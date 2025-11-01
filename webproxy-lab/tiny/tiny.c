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
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

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

void doit(int fd) //연결 소켓
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE]; //resource 파일 시스템 경로 매핑 문자열 , cgi인자 즉, ? 뒤에 부분 저장 문자열
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(fd, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s",buf); // 받은 버퍼확인
  scanf(buf,"%s %s %s", method, uri, version); // buf에 있는 것들 각 변수에 담아주고

  if (strcasecmp(method, "GET")) // 메소드 확인 "GET" 아닐 시 ret;
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);  // 요청 헤더 라인들을 한 줄씩 읽어 소비

  is_static = parse_uri(uri, filename, cgiargs); // 정적인지 확인
  if (stat(filename, &sbuf) < 0)//stat: 메타데이터를 커널에서 조회 -> 경로가 존재하는지, 무엇인지를 확정
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't read the file"); // 그런 파일 없다~
    return;
  }

  if (is_static) // 정적 콘텐츠
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t read the file");
      return;
    }
    serve_static(fd,filename, sbuf.st_size);
  }
  else { // 동적 콘텐츠
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) 
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

int parse_uri(char *uri, char *filename, char *cgiargs) // 로컬 파일 경로(filename)와 CGI 인자(cgiargs)를 채움, 동적.정적 판단 리턴
{
  char *ptr;

  
  if (!strstr(uri, "cgi-bin")) // 정적 콘텐츠 판단: URI에 "cgi-bin"이 없으면 정적으로 간주
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