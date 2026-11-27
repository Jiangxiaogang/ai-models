#ifndef UTIL_H
#define UTIL_H

#include <sal.h>
#include <stddef.h>

#define UTIL_PATH_MAX 1024
#define UTIL_LINE_MAX 8192

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sbuf;

int sbuf_init(sbuf *b);
int sbuf_add(sbuf *b, const char *s);
int sbuf_addn(sbuf *b, const char *s, size_t n);
int sbuf_addf(sbuf *b, _Printf_format_string_ const char *fmt, ...);
void sbuf_free(sbuf *b);

int str_ieq(const char *a, const char *b);
int str_ihas_prefix(const char *s, const char *prefix);
char *str_trim(char *s);

int hex_value(int c);
int url_decode(const char *src, char *dst, size_t cap);
int url_encode(const char *src, char *dst, size_t cap);

int path_safe(const char *uri, char *out, size_t cap);
int path_join(char *out, size_t cap, const char *base, const char *rel);

int file_is_dir(const char *path);
int file_exists(const char *path);
long long file_size(const char *path);
long long file_mtime(const char *path);
int dir_make_all(const char *path);

const char *mime_of(const char *path);
void fmt_bytes(long long bytes, char *out, size_t cap);
void fmt_speed(double bytes_per_sec, char *out, size_t cap);
int fmt_http_date(long long unix_time, char *out, size_t cap);
int fmt_escape_html(const char *src, char *dst, size_t cap);

#endif
