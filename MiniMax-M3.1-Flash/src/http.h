#ifndef HTTP_H
#define HTTP_H

#include <winsock2.h>
#include "util.h"

#define HTTP_TARGET_MAX 2048
#define HTTP_HEADER_MAX 16384
#define HTTP_BODY_CHUNK 65536

typedef struct {
    char method[16];
    char target[HTTP_TARGET_MAX];
    char path[HTTP_TARGET_MAX];
    char query[HTTP_TARGET_MAX];
    char version[16];
    long long content_length;
    int has_content_length;
    int expect_continue;
    int has_range;
    long long range_start;
    long long range_end;
    char headers[HTTP_HEADER_MAX];
} http_request;

typedef struct {
    SOCKET s;
    char buf[16384];
    size_t len;
    size_t pos;
} conn;

void conn_init(conn *c, SOCKET s);
int conn_fill(conn *c);
int conn_read_line(conn *c, char *out, size_t cap);
int conn_read_headers(conn *c, char *out, size_t cap);
int conn_read_body(conn *c, void *buf, size_t n);
int conn_send_all(conn *c, const void *buf, size_t n);
int conn_send_text(conn *c, const char *s);

int http_parse_request(conn *c, http_request *req);
int http_header_get(const http_request *req, const char *name, char *out, size_t cap);
int http_query_get(const char *query, const char *name, char *out, size_t cap);
const char *http_status_text(int code);
int http_send_head(conn *c, int code, const char *ctype, long long length, const char *extra);
int http_send_error(conn *c, int code, const char *detail);
int http_send_html(conn *c, int code, const char *title, const char *body);

#endif
