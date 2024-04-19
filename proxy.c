#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);

int parse_uri(char *uri, char *request_ip, char *port, char *filename);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(char *method, char *request_ip, char *user_agent_hdr, int clientfd, char *filename);
void server_to_client(int clientfd, int fd);

int main(int argc, char **argv) {
    int listenfd, connfd; // 서버소켓,클라이언트 소켓 fd
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]); // 듣기 소켓 오픈

    // 무한 서버 루프 실행
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                       // 반복적으로 연결 요청 접수
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 소켓 주소를 호스트 이름 및 서비스 이름으로 변환
        //(클라이언트 주소, 클라이언트 주소의 크기, 호스트 이름, 호스트 이름 버퍼 크기, 포트번호, 포트 번호 최대 크기, 추가옵션)
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);  // 트랜젝션 수행
        Close(connfd); // 연결 종료
    }
}

/*
 * doit - 한개의 HTTP 트랜젝션 처리
 */
void doit(int fd) {

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], request_ip[MAXLINE], port[MAXLINE];
    rio_t rio, rio_com;
    int clientfd;
    // 요청 라인을 읽고 분석
    Rio_readinitb(&rio, fd);           // fd의 주소값을 rio로
    Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인 읽기(한줄)

    printf("Request headers:\n");
    printf("%s", buf);

    sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인에서 메서드, URI, HTTP 버전 추출

    // GET,HEAD 메소드에 대해서만 적용
    if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0) {
        clienterror(fd, method, "5O1", "Not implemented", "Tiny does not implement this method");
        return;
    }

    // uri 파싱
    parse_uri(uri, request_ip, port, filename); // URI를 요청 ip, port, file이름 추출

    clientfd = Open_clientfd(request_ip, port);

    // 요청 헤더를 읽고 서버에 전송
    read_requesthdrs(method, request_ip, user_agent_hdr, clientfd, filename);

    //서버에게 응답을 받고 클라이언트에게 전송
    server_to_client(clientfd,fd);
   
}
int parse_uri(char *uri, char *request_ip, char *port, char *filename) {
    /*
    uri 파싱 조건
    요청ip,filename,port,http_version
    filename과 port는 있을 수도있고 없을수도있음
    URI_examle= http://request_ip:port/path

    */
}

void read_requesthdrs(char *method, char *request_ip, char *user_agent_hdr, int clientfd, char *filename) {
    char buf[MAXLINE]; // 임시 버프

    /*자른 데이터를 헤더로 만들어 buf로 tiny에게 전달*/
    // 앞줄을 하나씩 물고와요 한다.
    sprintf(buf, "%s %s %s\r\n", method, filename, "HTTP/1.0");
    sprintf(buf, "%sHost: %s\r\n", buf, request_ip);
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%sConnection: %s\r\n", buf, "close");
    sprintf(buf, "%sProxy-Connection: %s\r\n\r\n", buf, "close");

    // 서버로 보낸다.
    Rio_writen(clientfd, buf, strlen(buf));
}
void server_to_client(int clientfd, int fd){

}
