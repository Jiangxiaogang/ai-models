#ifndef HTTP_H
#define HTTP_H
#include <winsock2.h>
#include <windows.h>
#include "defs.h"

typedef struct {
    SOCKET sock;
    long long remaining;
    int chunked;
    long long chunk_rem;
} BodyReader;

typedef struct {
    SOCKET sock;
    char method[16];
    char url[MAX_PATH_LEN];
    long long content_length;
    int chunked;
    char range[128];
    BodyReader body;
} Request;

int http_send_all(SOCKET s, const char *buf, int len);
int http_recv_line(SOCKET s, char *buf, int cap);
int body_read(BodyReader *br, char *buf, int want);
void http_head(SOCKET s, int code, const char *reason, const char *ctype, long long len, const char *extra);
void http_error(SOCKET s, int code, const char *reason, const char *msg);
#endif
