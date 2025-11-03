/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <strings.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp, int *keep_alive);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int *keep_alive, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs ,int *keep_alive, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, int *keep_alive);

int main(int argc, char **argv) // 인자수, 인자포인터
{
  int listenfd, connfd; //듣기소켓, 연결소켓
  char hostname[MAXLINE], port[MAXLINE]; //호스트 문자열, 포트 문자열
  socklen_t clientlen; //IPv4
  struct sockaddr_storage clientaddr; // 클라이언트 주소

  /* Check command line args */
  if (argc != 2) //프로그램 + port
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); //port
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}
////////////////////////////////DO IT//////////////////////////////////////////////
void doit(int fd) //연결 소켓
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE]; //resource 파일 시스템 경로 매핑 문자열 , cgi인자 즉, ? 뒤에 부분 저장 문자열
  rio_t rio;
  int n, keep_alive = 1;
  Rio_readinitb(&rio, fd);
  while (keep_alive)
  {
    if((n = Rio_readlineb(&rio, buf, MAXLINE)) == 0){
      printf("Client closed connection.\n");
      break;
    }
    // printf("Request headers:\n");
    printf("%s",buf); // 받은 버퍼확인
    sscanf(buf,"%s %s %s", method, uri, version); // buf에 있는 것들 각 변수에 담아주고
    if (strcmp(version, "HTTP/1.0") == 0)
      keep_alive = 0; // 종료


    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD") ) // 메소드 확인 "GET" 아닐 시 ret;
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method", &keep_alive);
    break;
  }

  read_requesthdrs(&rio,&keep_alive);  // 요청 헤더 라인들을 한 줄씩 읽어 소비

  is_static = parse_uri(uri, filename, cgiargs); // 정적인지 확인
  if (stat(filename, &sbuf) < 0)//stat: 메타데이터를 커널에서 조회 -> 경로가 존재하는지, 무엇인지를 확정
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't read the file", &keep_alive); // 그런 파일 없다~
    return;
  }

  if (is_static) // 정적 콘텐츠
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t read the file", &keep_alive);
      return;
    }
    serve_static(fd,filename, sbuf.st_size, &keep_alive, method);

  }
  else { // 동적 콘텐츠
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) 
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program", &keep_alive);
      return;
    }
    serve_dynamic(fd, filename, cgiargs, &keep_alive, method);
  }
  }
}

////////////////////////////////CLIENT ERROR//////////////////////////////////////////////
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, int *keep_alive)
{
  //ex: clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>"); //body라는 버퍼안에 넣기 
  sprintf(body, "%s<body bgcolor=" "ffffff"">\r\n",body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);


/* Print the HTTP response */
  if (*keep_alive)
  {
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: keep-alive\r\n"); // <-- 이 줄
    Rio_writen(fd, buf, strlen(buf));
  }
  else
  {
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n"); // <-- 이 줄이
    Rio_writen(fd, buf, strlen(buf));
  }
  // --- 공통 헤더 ---
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
////////////////////////////////READ_REQUEST_HDRS//////////////////////////////////////////////
// HTTP 요청 헤더를 읽어들이는 함수입니다.
void read_requesthdrs(rio_t *rp, int *keep_alive) // 'rp'는 클라이언트와 연결된 RIO 버퍼(물통)입니다.
{
  
  char buf[MAXLINE]; // HTTP 헤더 한 줄을 임시로 저장할 버퍼

  Rio_readlineb(rp, buf, MAXLINE);  // 요청 라인을 읽어서 buf에 저장
  // printf("%s", buf); // 저장한 요청 라인 출력
  
  while (strcmp(buf, "\r\n"))  // 헤더(Header) 읽기 루프 "buf의 내용이 \r\n (빈 줄)이 아닐 동안 계속 반복
  {    
    printf("%s", buf);     // 방금 읽은 헤더 라인을 서버 콘솔 창에 출력합니다.
    if(strstr(buf, "Connection: close")){
      *keep_alive = 0;
    }
    Rio_readlineb(rp, buf, MAXLINE); 
  }
  return; 
}
////////////////////////////////PARSE_URI//////////////////////////////////////////////
int parse_uri(char *uri, char *filename, char *cgiargs) // 로컬 파일 경로(filename)와 CGI 인자(cgiargs)를 채움, 동적.정적 판단 리턴
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) // 정적 콘텐츠 판단: URI에 "cgi-bin"이 없으면 정적으로 간주 (부분집합 포함?)
  { 
    strcpy(cgiargs, "");       
    strcpy(filename, ".");
    strcat(filename, uri); // filename = "." + uri  → 예: "./index.html"

    // URI가 '/'로 끝나면 디폴트 문서로 보정
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");  // 예: "/"" → "./home.html"
    return 1;// 1= 정적
  }
  else
  { //cgi-bin 포함
    
  
    ptr = index(uri, '?'); // 쿼리 문자열 시작 위치 탐색

    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // '?' 다음을 CGI 인자로 복사하고
      *ptr = '\0';            // uri = "cgi-bin/exec?arg=..." → "cgi-bin/exec"
    }
    else
      strcpy(cgiargs, "");    // 쿼리 없음
    strcpy(filename, ".");
    strcat(filename, uri);    // filename = "." + "cgi-bin/exec"
    return 0;                 // 동적 요청
  }
}// 반환값: 1 = 정적(static), 0 = 동적(dynamic)
////////////////////////////////SERVE_STATIC//////////////////////////////////////////////
void serve_static(int fd, char *filename, int filesize, int *keep_alive, char *method)
{
  int srcfd; // 소스 식별자받
  char *srcp, filetype[MAXLINE]; //소스 포인터, 파일타입 버퍼?

  char buf[MAXBUF];
  char *p = buf;
  int n;
  int remaining = sizeof(buf);
  // printf("--- A: serve_static 시작 ---\n"); //
  /* Send response headers to client */
  get_filetype(filename, filetype); // filetype 버퍼에 타입 저장
  // printf("--- B: get_filetype 완료 ---\n"); 
  if (*keep_alive) // 1.1 일 때
  {
    n = snprintf(p, remaining, "HTTP/1.1 200 OK\r\n"); //p에 글자 삽입 / remaining은 쓸 수 있는 최대 크기
    p += n;
    remaining -= n;

    n = snprintf(p, remaining, "Connection: keep-alive\r\n");
    p += n;
    remaining -= n;
  }else{
    n = snprintf(p, remaining, "HTTP/1.0 200 OK\r\n"); //p에 글자 삽입 / remaining은 쓸 수 있는 최대 크기
    p += n;
    remaining -= n;

    n = snprintf(p, remaining, "Connection: close\r\n");
    p += n;
    remaining -= n;
  }
  /* Build the HTTP response headers correctly - use separate buffers or append */
  n = snprintf(p, remaining, "Server: Tiny Web Server\r\n");
  p += n;
  remaining -= n;

  n = snprintf(p, remaining, "Content-length: %d\r\n", filesize);
  p += n;
  remaining -= n;

  n = snprintf(p, remaining, "Content-type: %s\r\n\r\n", filetype);
  p += n;
  remaining -= n;
  // printf("--- C: 헤더 생성 완료, 전송 시작 ---\n");
  Rio_writen(fd, buf, strlen(buf)); // 버퍼 길이 만큼 연결 소켓에 써주기
  // printf("--- D: 헤더 전송 완료 ---\n");
  printf("Response headers:\n");
  printf("%s", buf); // 버퍼 내용 출력 해주고

  
  /* Send response body to client */
  if (strcasecmp(method, "GET") == 0)
  {
    srcfd = Open(filename, O_RDONLY, 0); // 요청한 파일을 열기 / O_RDONLY 읽기 전용 / srcfd = 소스 파일 디스크립터
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리 맵: 파일을 read해서 메모리로 복사하는 대신 srcfd의 내용물(filesize) 만큼 프로그램의 메모리 주소에 연결(매핑)
    char *alloc = calloc(filesize, 1);
    Rio_readn(srcfd, alloc, filesize);
    Close(srcfd); // 메모리 매핑 했으니 닫기
    Rio_writen(fd, alloc, filesize); //filesize 만큼 전부 전송
    // Munmap(srcp, filesize); // 전송하고나서 unmap
    free(alloc);
  }
}
////////////////////////////////GET_FILETYPE//////////////////////////////////////////////
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, ".video/mp4");
  else
    strcpy(filetype, "text/plain");
}
////////////////////////////////SERVE_DYNAMIC//////////////////////////////////////////////
void serve_dynamic(int fd, char *filename, char *cgiargs, int *keep_alive, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part od HTTP response*/
  if (*keep_alive)
    {
      sprintf(buf, "HTTP/1.1 200 OK\r\n");
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Server: Tiny Web Server\r\n");
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Connection: keep-alive\r\n"); 
      Rio_writen(fd, buf, strlen(buf));
    }else{
      sprintf(buf, "HTTP/1.0 200 OK\r\n");
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Server: Tiny Web Server\r\n");
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Connection: close\r\n"); 
      Rio_writen(fd, buf, strlen(buf));
    }
  if (strcasecmp(method, "GET") == 0)
  {    
    if (Fork() == 0) // 만약 내가 복제본이라면
      {
        /* Real server wowuld set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); // setenv로 추가한 "QUERY_STRING=10&20
        Dup2(fd, STDOUT_FILENO); // Redirect stdout to client 출력을 클라이언트로만 해준뒤
        Execve(filename, emptylist, environ); // 파일 name을 실행하는데 environ은 뭐지?
      }
        Wait(NULL); // 부모 프로세스는 if 문 건너뛰고
  }
}