/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

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
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // 요청 라인을 읽고 분석
    Rio_readinitb(&rio, fd);           // 초기화
    Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인 읽기
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인에서 메서드, URI, HTTP 버전 추출

    // GET 메소드에 대해서만 적용
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "SOI", "Not implemented", "Tiny does not implement this method");
        return;
    }

    read_requesthdrs(&rio); // 요청 헤더 읽기

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs); // URI를 파일이름과 CGI로 추출(정적파일인지 체킹)

    // 요청된 파일 존재 X -> 404반환
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }
    // 정적파일일 경우
    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 일반 파일인지, 권한이 있는지 확인
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size); // 정적파일 제공
    }
    // 동적 파일일 경우
    else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 일반 파일인지, 권한이 있는지 확인
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs); // 동적파일 제공
    }
}

