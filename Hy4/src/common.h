#ifndef COMMON_H
#define COMMON_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    unsigned short port;
    char root[MAX_PATH];
} Config;

typedef struct {
    SOCKET sock;
    char method[16];
    char raw_path[2048];
    char path[MAX_PATH * 2];
    char version[16];
    long long content_length;
    int has_content_length;
    char content_type[128];
    char host[256];
    char *body;
    long long body_len;
    char query[1024];
} Request;

typedef struct {
    int status;
    const char *reason;
    const char *type;
    long long length;
    int no_body;
    const char *extra;
} ResponseHead;

int config_load(const char *file, Config *cfg);
const char *config_arg(int argc, char **argv, const char *key);

char *util_strtrim(char *s);
int util_u8_to_w(const char *u8, wchar_t *w, size_t cap);
int util_w_to_u8(const wchar_t *w, char *u8, size_t cap);
void util_path_join(char *dst, size_t cap, const char *a, const char *b);
void util_path_native(char *p);
int util_url_decode(char *dst, size_t cap, const char *src);
int util_resolve_safe(char *full, size_t cap, const char *root, const char *rel);
int util_is_dir(const char *p);
int util_file_size(const char *p, long long *size);
const char *util_mime(const char *name);
void util_escape_html(char *dst, size_t cap, const char *src);
void util_escape_attr(char *dst, size_t cap, const char *src);
void util_time_str(char *dst, size_t cap, time_t t);
void util_size_str(char *dst, size_t cap, long long n);
void util_gmt_str(char *dst, size_t cap);
int util_ieq(const char *a, const char *b);
void util_base_name(char *dst, size_t cap, const char *p);
void util_lower(char *dst, size_t cap, const char *src);

int http_parse(Request *req, const char *head, long long head_len);
long long http_head_field(const char *head, const char *name, char *dst, size_t cap);
int http_send_head(SOCKET s, const ResponseHead *h);
int http_send_all(SOCKET s, const char *buf, long long len);
int http_recv_body(SOCKET s, Request *req, const char *head, long long head_len, long long buffered, long long *received, int *aborted);
int http_send_file(SOCKET s, const char *path, long long size);
int http_send_text(SOCKET s, int status, const char *reason, const char *type, const char *text);
int http_send_error(SOCKET s, int status, const char *reason);

void page_index(SOCKET s, const Request *req, const Config *cfg);
int page_upload(SOCKET s, Request *req, const Config *cfg, const char *head, long long head_len, long long buffered);

int server_run(const Config *cfg);
void server_set_config(const Config *cfg);

#endif
