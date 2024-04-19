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
        clienterror(fd, method, "5O1", "Not implemented", "Tiny does not implement this method");
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

/*
 * clienterror - 에러응답
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    // http response body 설정
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n",body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    // http response출력
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
 * read_requesthdrs - 요청 헤더 읽기
 */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}
/*
* parse_uri - URI를 분석하여 정적 or 동적 파일 판별 후 처리  
*/
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    //정적 파일일 경우
    if (!strstr(uri, "cgi-bin")) { //cgi-bin 포함인지 확인
        strcpy(cgiargs, "");
        strcpy(filename, "."); 
        strcat(filename, uri); //filname에 uri 저장
        if (uri[strlen(uri) - 1] == '/') // URI가 /로 끝나면
            strcat(filename, "home.html");// 기본 파일 이름 추가
        return 1;
    } 
    //동적 파일일 경우
    else {
        ptr = index(uri, '?');//cgi 인자 찾기
        //cgi 인자가 존재할 경우
        if (ptr) {
            strcpy(cgiargs, ptr + 1); //cgi 인자 추출 후 null삽입
            *ptr = '\0';
        } else
            strcpy(cgiargs, ""); //cgi 인자가 없다면 빈문자열 삽입
        strcpy(filename, ".");
        strcat(filename, uri);//filname에 uri 저장
        return 0;
    }
}

/*
* serve_static - 정적 파일 전달
*/
void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    //응답헤더 전송
    get_filetype(filename, filetype); //파일의 확장자 추출
    sprintf(buf, "HTTP/1.0 200 0K\r\n");
    sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent—length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    //response body 전송
    srcfd = Open(filename, O_RDONLY, 0); //파일을 열고
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //매모리 매핑 수행
    Close(srcfd); //파일 닫기
    Rio_writen(fd, srcp, filesize); //파일 읽어들이기
    Munmap(srcp, filesize); //매모리 매핑 해제
}
/*
 * get_filetype — 파일 확장자 추출(content-type)
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

/*
* serve_dynamic- 동적 파일 전달
*/
void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptyList[] = {NULL};

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    //자식 프로세스 생성후 분리
    if (Fork() == 0) {

        setenv("QUERY_STRING", cgiargs, 1); //cgi 인자를 QUERY_STRING으로 설정(추후 환경변수 생성해야함)
        Dup2(fd, STDOUT_FILENO); // 표준 출력을 fd(클라이언트 소켓 파일 디스크립터)로 지정 =>동적 파일 생성하는 출력 클라이언트에게 전송
        Execve(filename, emptyList, environ); //동적 파일 실행(새로운 프로세스 실행)
    }
    Wait(NULL);
}