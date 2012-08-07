#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define DEBUGx
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

static const char *default_user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *default_accept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *default_accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *default_connect = "Connection: close\r\n";
static const char *default_proxy = "Proxy-Connection: close\r\n";
static const char *default_version = "HTTP/1.0\r\n";

typedef struct {
        char* host;
        int port;
        char* request;
}req_t;


void doit(int fd);
void get_request(rio_t *rp, req_t *req);
void extract_header(char *raw, char *header, char *value);
void add_to_header(char *key, char *value, char *header);
void append_header(char *hdr_line,char *header);
int parse_uri(char *uri, char *host, int *port,char *short_uri);
int parse_reqline(char *header,char *reqline, char* host, int *port);
void set_header(char *key, char *value, char *header);
void extract_host_port(char *value,char* host,int* port);
void forward_to_server(int server_fd);



int main(int argc, char **argv)
{

        int listenfd, connfd, port, clientlen;
        struct sockaddr_in clientaddr;

        if (argc != 2) {
                fprintf(stderr, "usage: %s <port>\n", argv[0]);
                exit(1);
        }
        port = atoi(argv[1]);

        listenfd = Open_listenfd(port);
        while (1) {
                printf("new connection!\n");
                clientlen = sizeof(clientaddr);
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
                doit(connfd);
                Close(connfd);
        }

        return 0;
}


void get_request(rio_t *rp, req_t* req)
{
        char buf[MAXLINE];
        char key[MAXLINE],value[MAXLINE];
        char raw[MAXLINE];
        int has_host = 0;

        char request[MAXLINE];
        req->request = request;

        //printf("request address %p\n",request);

        /*initialize all!*/
        *request = 0;
        *raw = 0;
        *buf = 0;
        *key = 0;
        *value = 0;


        char host[MAXLINE];
        int port;

        *host = 0;
        port = 80;

        //char method[MAXLINE], uri[MAXLINE], version[MAXLINE],short_uri[MAXLINE];

        Rio_readlineb(rp, buf, MAXLINE);
        //printf("%s",buf);
        strcat(raw,buf);
        parse_reqline(request,buf,host,&port);


        append_header((char *)default_user_agent,request);
        append_header((char *)default_accept,request);
        append_header((char *)default_accept_encoding,request);
        append_header((char *)default_connect,request);
        append_header((char *)default_proxy,request);


        while (strcmp(buf, "\r\n")) {
                *key = '\0';
                *value = '\0';
                Rio_readlineb(rp, buf, MAXLINE);
                //dbg_printf("raw header line %slen%d\n",buf,strlen(buf));
                //printf("%s",buf);
                strcat(raw,buf);
                //printf("raw %s len %d\n",buf,strlen(buf));
                if (!strcmp(buf, "\r\n")) {
                        printf("break!\n");
                        break;
                }
                extract_header(buf,key,value);
                if (*key != '\0' && *value!='\0') {
                        if (!strcmp(key,"Host")) {
                                dbg_printf("contain host\n");
                                extract_host_port(value,host,&port);
                                has_host = 1;
                        }
                        set_header(key,value,request);
                }
        }
        //printf("final header\n");
        if (!has_host) {
                char host_hdr[MAXLINE];
                if (port != 80) {
                        sscanf(host_hdr,"Host: %s:%d\r\n",host,&port);
                } else {
                        sscanf(host_hdr,"Host: %s\r\n",host);
                }
                append_header(host_hdr,request);
        }

        req->host = host;
        req->port = port;
        append_header("\r\n",request);
        printf("raw:\n%s",raw);
        printf("modified:\n%s",request);
        return;
}

void doit(int fd)
{
        rio_t rio;
        rio_t s_rio;
        req_t req;
        int server_fd;
        //char request[MAXLINE];
        Rio_readinitb(&rio,fd);
        get_request(&rio,&req);


        server_fd = Open_clientfd(req.host,req.port);
        Rio_readinitb(&s_rio,server_fd);
        Rio_writen(server_fd,req.request,strlen(req.request));

        ssize_t nread;
        char buf[MAXLINE];
        while((nread = Rio_readnb(&s_rio,buf,MAXLINE))!=0)
        {
            Rio_writen(fd,buf,nread);
        }

        //printf("in req:\n%s%s %d\n",req.request,req.host,req.port);
}

void append_header(char *hdr_line,char *header)
{
        strcat(header,hdr_line);
}

void extract_header(char *raw, char *header, char *value)
{
        char *ptr;
        char t_value[MAXLINE];

        ptr = strstr(raw,":");
        if (ptr != NULL) {
                *ptr = 0;
                strcpy(header,raw);
                strcpy(t_value,ptr+2);
                ptr = strstr(t_value,"\r");
                *ptr = 0;
                strcpy(value,t_value);
        } else return;
}

void add_to_header(char *key, char *value, char *header)
{
        char hdr_line[MAXLINE];
        sprintf(hdr_line,"%s: %s\r\n",key,value);
        dbg_printf("formed header:%slen:%d\n",hdr_line,strlen(hdr_line));
        append_header(hdr_line,header);

}

int parse_uri(char *uri, char *host, int *port, char *short_uri)
{
        *host = 0;
        *port = 80;
        char *http_ptr,*slash_ptr;
        char host_t[MAXLINE],port_str[MAXLINE];
        http_ptr = strstr(uri,"http://");
        if (http_ptr == NULL) {
                strcpy(short_uri,uri);
                return 0;
        } else {
                //printf
                http_ptr += 7;
                //printf("without http %s\n",http_ptr);
                slash_ptr = strchr(http_ptr,'/');
                *slash_ptr = 0;
                strcpy(host_t,http_ptr);
                //printf("host_t is %s\n",host_t);
                *slash_ptr = '/';
                strcpy(short_uri,slash_ptr);
                //printf("short_uri is %s\n",short_uri);



                char *ptr;
                ptr = strstr(host_t,":");
                if (ptr != NULL) {
                        *ptr = 0;
                        strcpy(port_str,ptr+1);
                        *port = atoi(port_str);
                }

                strcpy(host,host_t);
                return 1;

        }
}

int parse_reqline(char *header,char *reqline, char* host, int *port)
{
        char method[MAXLINE], uri[MAXLINE], version[MAXLINE],short_uri[MAXLINE];
        sscanf(reqline, "%s %s %s", method, uri, version);

        int found_host;
        found_host = parse_uri(uri,host,port,short_uri);
        char new_req[MAXLINE];
        sprintf(new_req, "%s %s %s",method, short_uri,default_version);
        append_header(new_req,header);
        return found_host;

}


void set_header(char *key, char *value, char *header)
{
        if (!strcmp(key,"User-Agent")|| !strcmp(key,"Accept")|| !strcmp(key,"Accept-Encoding")
                        || !strcmp(key,"Connection") || !strcmp(key, "Proxy-Connection")) {
                dbg_printf("Ignore header %s\n",key);
        } else {
                add_to_header(key,value,header);
        }


}

void extract_host_port(char *value,char* host,int* port)
{
        char t_value[MAXLINE];
        strcpy(t_value,value);
        char *ptr;

        *port = 80;

        ptr = strstr(t_value,":");
        if (ptr!=NULL) {
                *ptr = 0;
                strcpy(host,t_value);
                *port = atoi(ptr+1);
        }
}
