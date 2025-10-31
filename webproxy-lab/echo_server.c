#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)
{
    int listenfd, confd;
    socklen_t clientlen;
    struct sockaddr_storge clientaddr; // 어떤한 주소라도 받을수있는 충분한 공간
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if(argc != 2)
    {
        fprintf(stderr, "usage: %s<port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(confd);
        Close(connfd);
    }
    exit(0);
}