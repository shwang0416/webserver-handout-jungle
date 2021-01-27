/*
* tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
* GET method to serve static and dynamic content
*/
#include "csapp.h"
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);


int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    /* Check command-line args */
    // 인자가 두개가 아니면 에러메시지 반환
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // open_listenfd로 listen소켓의 fd, listenfd를 생성
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        // Accept 함수로 connfd 생성
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // 소켓을 바탕으로 클라이언트의 주소와 호스트명(클라)를 얻는다.
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        // 호스트명(클라), port 가 제대로 되었는지 확인할 수 있도록 출력
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        doit(connfd);
        //echo(connfd);
        Close(connfd);
    }
}
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], buf2[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    //int log;
    ssize_t n;

    /* Read request line and headers */
    // fd를 설정 clientfd로 rio_t 구조체를 초기화한다.
    Rio_readinitb(&rio, fd);        // fd와 rio 연결(fd의 내용을 rio로)
    // rio_t를 MAXLINE만큼 읽어서 buf에 저장
    Rio_readlineb(&rio, buf, MAXLINE);
    // if ((log = Open("request.txt",O_CREAT | O_RDONLY, 0)) < 1){
    //     printf("Error\n");
    // }
    // printf("n: %d\n", n);
    // printf("log: %d\n", log);
    // printf("%s\n", buf);
//    Rio_writen(log, buf, n);
    // 클라가 요청 헤더 (host:domain) 를 보낸 상태 ↓ 아래 실행
    printf("Request headers:\n");
    printf("%s", buf);
    // buf에 있는 데이터를 공백을 기준으로 각 변수에 나누어 저장함
    sscanf(buf, "%s %s %s", method, uri, version);
    // 만약 들어온 method가 GET이 아니면 클라 에러를 출력
    if (!strcasecmp(method, "GET"))                                 // 요청이 GET이면
    {
        // resquest 헤더를 읽어들임.
        read_requesthdrs(&rio);
        /* Parse URI from GET request */
        // 클라가 요청하는 파일이 static인지 dynamic인지 판단
        is_static = parse_uri(uri, filename, cgiargs);
        // 해당 파일이 디스크 안에 없으면 에러 메세지 반환
        if (stat(filename, &sbuf) < 0)
        {
            clienterror(fd, filename, "404", "Not found",
                        "Tiny couldn't find this file");
            return;
        }
        if (is_static)
        { /* Serve static content */
            // 보통 파일인가? 그리고 읽기 권한이 있는가
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
            {
                clienterror(fd, filename, "403", "Forbidden",
                            "Tiny couldn't read the file");
                return;
            }
            serve_static(fd, filename, sbuf.st_size);
        }
        else
        { /* Serve dynamic content */
            // 보통 파일인가? 그리고 읽기 권한이 있는가
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
            {
                clienterror(fd, filename, "403", "Forbidden",
                            "Tiny couldn't run the CGI program");
                return;
            }
            serve_dynamic(fd, filename, cgiargs);
        }
    }
    else if (!strcasecmp(method, "HEAD"))                           // 요청이 HEAD이면
    {
        /* Send response headers to client */
        sprintf(buf, "HTTP/1.0 200 OK\r\n");
        sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
        sprintf(buf, "%sConnection: close\r\n", buf);
        Rio_writen(fd, buf, strlen(buf));
    }
    else {
        clienterror(fd, method, "501", "Not implemented",
                    "Tiny does not implement this method");
        return;
    }
}
//
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
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
// 요청 헤더를 읽음
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}
// 클라이언트가 요청하는 파일이 static인지 dynamic인지 판단
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    if (!strstr(uri, "cgi-bin"))
    { /* Static content */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");
        return 1;
    }
    else
    { /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    // 클라는 rio_readlineb로 sprintf했던 것을 읽는다.
    printf("Response headers:\n");
    printf("%s", buf);
    /* Send response body to client */

    // file의 fd를 얻어온다.
    srcfd = Open(filename, O_RDONLY, 0);
    printf("srcfd: %d\n", srcfd);
    // 파일을 저장하기 위해서 메모리를 할당한다.
    // 930p 11.9
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    char *srcbuf = Malloc(filesize);
    Rio_readn(srcfd, srcbuf, filesize);


    // 저장했으니깐 fd 를 닫는다.
    Close(srcfd);

    // 클라한테 파일을 던져준다. 
    // Rio_writen(fd, srcp, filesize);
    Rio_writen(fd, srcbuf, filesize);
    //: 주소 srcp(or srcbuf)에서 시작하는 filesize 바이트를 클라이언트의 fd(연결식별자: connfd)로 복사한다.

    // 요청했던 메모리를 닫는다. 
    //Munmap(srcp, filesize);
    Free(srcbuf);
}
/*
 * get_filetype - Derive file type from filename
 */
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
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "image/mpg");
    else if (strstr(filename, ".mov"))
        strcpy(filetype, "video/mov");    
    else
        strcpy(filetype, "text/plain");
}
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = {NULL};
    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    //Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%sServer: Tiny Web Server\r\n",buf);
    Rio_writen(fd, buf, strlen(buf));
    if (Fork() == 0)
    { /* Child */
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);
        // 환경설정
        Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */
        // 나의 STDOUT을 fd에 쓰는 걸로 바꿔라!
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
}