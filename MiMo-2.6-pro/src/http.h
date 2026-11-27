#ifndef HTTP_H
#define HTTP_H

#include "net.h"

typedef struct {
    char method[8];
    char path[2048];
    long long content_length;
    int expect_continue;
    int chunked;
} http_request;

int http_parse(const char *head, int len, http_request *req);
void http_reply(SOCKET s, int code, const char *ctype, long long clen, const char *location);
void send_error(SOCKET s, int code);
const char *mime_type(const char *path);

#endif
