#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} xbuf;

#define XB(b, s) xb_add((b), (s), sizeof(s) - 1)

void xb_init(xbuf *b);
void xb_free(xbuf *b);
void xb_add(xbuf *b, const char *s, size_t n);
void xb_addf(xbuf *b, const char *fmt, ...);
void html_escape(xbuf *b, const char *s);
void url_encode(xbuf *b, const char *s);
int url_decode(char *s);
int find_head_end(const char *s, int len);

#endif
