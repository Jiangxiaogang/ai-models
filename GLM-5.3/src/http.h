#ifndef HTTP_H
#define HTTP_H

#include <windows.h>

#define HTTP_REQ_MAX 16384

struct http {
    SOCKET sock;
    char req[HTTP_REQ_MAX];
    int reqlen;
    char method[16];
    char url[4096];
    char path[4096];
    unsigned long long content_len;
    int body;
    int head;
};

int http_recv_request(struct http *r);
int http_parse(struct http *r);
void http_send(struct http *r, const char *data, int len);
void http_reply(struct http *r, int code, const char *type, const char *body, int len);
void http_redirect(struct http *r, const char *loc);

#endif
