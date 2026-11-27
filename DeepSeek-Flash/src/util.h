#ifndef UTIL_H
#define UTIL_H

#include "common.h"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

int  buf_init(Buf *b, size_t cap);
void buf_free(Buf *b);
int  buf_addn(Buf *b, const char *s, size_t n);
int  buf_add(Buf *b, const char *s);
int  buf_addf(Buf *b, const char *fmt, ...);

char *str_trim(char *s);
int   str_ieq(const char *a, const char *b);

int utf8_to_wide(const char *s, wchar_t *w, int cch);
int wide_to_utf8(const wchar_t *w, char *s, int cb);

void url_decode(char *s);
void url_encode(const char *s, char *out, int cb);
void html_escape_to(const char *s, char *out, int cb);

int path_normalize(const char *urlpath, char *out, int cb);
int path_join8(const char *root, const char *rel, char *out, int cb);
int path_join(const char *root, const char *rel, wchar_t *out, int cch);

int file_exists(const wchar_t *path);
int is_directory(const wchar_t *path);
int dir_create_all(const wchar_t *path);

void format_size(long long n, char *out, int cb);
void format_time(const FILETIME *ft, char *out, int cb);
void exe_dir(char *out, int cb);

#endif
