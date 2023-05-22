/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

sem_t log_mutex;

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
void *thread_routine(void *thread_arg);
void proxy_manager(int connfd, struct sockaddr_in *sockaddr);
int proxy_send(int clientfd, rio_t *conn_rio, char *request_header, size_t length, char *method);
size_t proxy_receive(int connfd, rio_t *client_rio);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
int Rio_writen_w(int fd, void *usrbuf, size_t n);

struct threadArg
{
    int connfd;
    struct sockaddr_in clientaddr;
};

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    int listenfd, connfd, clientlen;
    struct sockaddr_in clientaddr;
    char hostname[MAXLINE], port[MAXLINE];
    struct threadArg *arg;
    pthread_t tid;

    listenfd = Open_listenfd(argv[1]);
    clientlen = sizeof(struct sockaddr_in);

    /* ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);

    /* init sem */
    Sem_init(&log_mutex, 0, 1);

    // listen to client
    while (1)
    {
        arg = Malloc(sizeof(struct threadArg));
        arg->connfd = Accept(listenfd, (SA *)&(arg->clientaddr), &clientlen);
        Pthread_create(&tid, NULL, &thread_routine, arg);
    }

    Close(listenfd);
    exit(1);
}

void *thread_routine(void *arg)
{
    struct threadArg *t_arg = (struct threadArg *)arg;

    // detach
    Pthread_detach(Pthread_self());

    // manager
    proxy_manager(t_arg->connfd, &(t_arg->clientaddr));

    Close(t_arg->connfd);
    return NULL;
}

/*
 * proxy_manager - manage proxy send and receive
 */
void proxy_manager(int connfd, struct sockaddr_in *clientaddr)
{
    char req_header[MAXLINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE];
    int clientfd;
    rio_t conn_rio, client_rio;
    size_t byte_size = 0, content_length = 0;

    /* read request line */
    Rio_readinitb(&conn_rio, connfd);
    if (Rio_readlineb_w(&conn_rio, buf, MAXLINE) == 0)
        return;
    sscanf(buf, "%s %s %s", method, uri, version);
    if(parse_uri(uri, hostname, pathname, port) == -1)
        return;

    /* read and set request header */
    sprintf(req_header, "%s /%s %s\r\n", method, pathname, version);
    size_t n = Rio_readlineb_w(&conn_rio, buf, MAXLINE);
    while (n != 0)
    {
        if (strncasecmp(buf, "Content-Length", 14) == 0)
        {
            sscanf(buf + 15, "%zu", &content_length);
        }
        strcat(req_header, buf);

        if (strncmp(buf, "\r\n", 2) == 0)
            break;

        n = Rio_readlineb_w(&conn_rio, buf, MAXLINE);
    }
    if(n == 0)
        return;

    /* open clientfd */
    clientfd = open_clientfd(hostname, port);
    if (clientfd < 0)
    {
        return;
    }
    Rio_readinitb(&client_rio, clientfd);

    /* send reqest to server */
    if (proxy_send(clientfd, &conn_rio, req_header, content_length, method) == 0)
    {
        byte_size = proxy_receive(connfd, &client_rio);
    }

    /*  log */
    format_log_entry(buf, clientaddr, uri, byte_size);
    P(&log_mutex);
    printf("%s\n", buf);
    V(&log_mutex);

    Close(clientfd);
}

/*
 * proxy_send - send request to server
 */
int proxy_send(int clientfd, rio_t *conn_rio, char *req_header, size_t length, char *method)
{
    char buf[MAXLINE];
    size_t n = 0;

    /* send request header */
    Rio_writen_w(clientfd, req_header, strlen(req_header));

    /* send request body */
    for (int i = 0; i < length; ++i)
    {
        if (Rio_readnb_w(conn_rio, buf, 1) == 0)
            return -1;
        if (Rio_writen_w(clientfd, buf, 1) == -1)
            return -1;
    }

    return 0;
}

/*
 * proxy_receive - receive response from server and send to connfd
 */
size_t proxy_receive(int connfd, rio_t *client_rio){
    char buf[MAXLINE];
    size_t byte_size = 0, content_length = 0;

    /* get response header */
    size_t n = Rio_readlineb_w(client_rio, buf, MAXLINE);
    while (n != 0)
    {
        byte_size += n;
        if (strncasecmp(buf, "Content-Length", 14) == 0)
        {
            sscanf(buf + 15, "%zu", &content_length);
        }
        Rio_writen_w(connfd, buf, n);

        if (strncmp(buf, "\r\n", 2) == 0)
            break;

        n = Rio_readlineb_w(client_rio, buf, MAXLINE);
    }

    /* receive response body */
    if (n != 0)
    {
        for (int i = 0; i < content_length; ++i)
        {
            Rio_readnb_w(client_rio, buf, 1);
            Rio_writen_w(connfd, buf, 1);
            ++byte_size;
        }
    }else{
        return 0;
    }

    return byte_size;
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0)
    {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':')
    {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    }
    else
    {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL)
    {
        pathname[0] = '\0';
    }
    else
    {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    char host[INET_ADDRSTRLEN];

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (inet_ntop(AF_INET, &sockaddr->sin_addr, host, sizeof(host)) == NULL)
        unix_error("Convert sockaddr_in to string representation failed\n");

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %s %s %zu", time_str, host, uri, size);
}

/*
 * Rio error checking wrappers
 */
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
    {
        fprintf(stderr, "Rio_readnb error\n");
        return 0;
    }
    return rc;
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
    {
        fprintf(stderr, "Rio_readlineb error\n");
        return 0;
    }
    return rc;
}

int Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
    {
        fprintf(stderr, "Rio_writen error\n");
        return -1;
    }
    return 0;
}
