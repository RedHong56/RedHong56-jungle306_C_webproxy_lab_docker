#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void doit(int fd){
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  char *hostname_ptr, *colon_ptr, *slash_ptr, *port_ptr;
  int server_fd;
  ssize_t n = 0;
  rio_t rio;

  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  if (!strstr(buf,"http://"))
  {
    printf("Not http://");
    return;
  }
  sscanf(buf, "%s %s %s", method, uri, version);

  // 2.URL 파싱 (hostname, port, path 분리)
  hostname_ptr = uri+7; //http:// 뒤
  colon_ptr = strchr(hostname_ptr,':'); // 이거 없으면 port 80
  port_ptr = colon_ptr +1;
  slash_ptr = strchr(hostname_ptr,'/'); // 이거 없으면 route
  size_t hostlen = colon_ptr - hostname_ptr;
  size_t portlen = slash_ptr - port_ptr;
  size_t pathlen = strlen(slash_ptr);
  memcpy(hostname, hostname_ptr, hostlen);
  memcpy(port, port_ptr, portlen);
  memcpy(path, slash_ptr, pathlen);
  hostname[hostlen]= '\0';
  port[portlen]= '\0';
  path[pathlen]= '\0';
  printf("host: %s, port: %s, path: %s", hostname, port, path);

  // 3. 원격 서버 접속 (Open_clientfd 호출)
  server_fd = open_clientfd(hostname, port);

  // 4. 요청 헤더 재구성 및 전달
  sprintf(buf, "%s %s HTTP/1.0\r\n",method, path);
  Rio_writen(server_fd, buf, strlen(buf));
  sprintf(buf, "Host: %s:%s\r\n",hostname, port);
  Rio_writen(server_fd, buf, strlen(buf));
  Rio_writen(server_fd, user_agent_hdr, strlen(user_agent_hdr));
  sprintf(buf, "Connection: close\r\n");
  Rio_writen(server_fd, buf, strlen(buf));
  sprintf(buf, "Proxy-Connection: close\r\n");
  Rio_writen(server_fd, buf, strlen(buf));
  sprintf(buf, "\r\n");
  Rio_writen(server_fd, buf, strlen(buf));

  // 5. 응답 중계 (Relay)
  while ((n=Rio_readn(server_fd, buf, MAXLINE)) > 0)
  {
    Rio_writen(fd, buf, n);
  }
}

void *thread(void *vargp) // 인자 상자에서 열어서 처리
{
  int connfd = *((int*)vargp);
  Pthread_detach(pthread_self()); // 메인 스레드와 분리하고 자동 회수 -> 메모리 누스 방지
  Free(vargp); // 메-누
  doit(connfd); // 중계 렛고
  Close(connfd); // 작업 후 닫기
  return NULL;
}

int main(int argc, char **argv)
{
  int listen_fd, conn_fd;
  socklen_t client_len;
  struct sockaddr_storage clientaddr;
  char client_host[MAXLINE], client_port[MAXLINE];
  pthread_t tid; //Thread ID
  // char *server_host[MAXLINE], *server_port[MAXLINE], *server_path[MAXLINE], buf[MAXLINE];
  if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
  // 1. listen-> accept로 client 정보 받기
  listen_fd = Open_listenfd(argv[1]);
  while (1)
  {
    client_len = sizeof(clientaddr); //client 주소 정보
    conn_fd = Accept(listen_fd, (SA *)&clientaddr, &client_len); //clientaddr update
    Getnameinfo((SA *)&clientaddr, client_len, client_host, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_host, client_port);
    // doit(conn_fd);
    // Close(conn_fd); 
    int *conn_fd_ptr = malloc(sizeof(int));
    *conn_fd_ptr = conn_fd;
    Pthread_create(&tid, NULL, thread, conn_fd_ptr);
  }
  printf("%s", user_agent_hdr);
  return 0;
}