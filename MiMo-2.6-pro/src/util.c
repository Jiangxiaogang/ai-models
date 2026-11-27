#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void xb_init(xbuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void xb_free(xbuf *b) {
    free(b->data);
    xb_init(b);
}

static void xb_reserve(xbuf *b, size_t n) {
    if (b->len + n + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + n + 1) cap *= 2;
    char *data = (char *)realloc(b->data, cap);
    if (!data) exit(1);
    b->data = data;
    b->cap = cap;
}

void xb_add(xbuf *b, const char *s, size_t n) {
    xb_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

void xb_addf(xbuf *b, const char *fmt, ...) {
    char tmp[8192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof tmp) n = (int)sizeof tmp - 1;
    xb_add(b, tmp, (size_t)n);
}

void html_escape(xbuf *b, const char *s) {
    for (; *s; s++) {
        switch (*s) {
        case '&': XB(b, "&amp;"); break;
        case '<': XB(b, "&lt;"); break;
        case '>': XB(b, "&gt;"); break;
        case '"': XB(b, "&quot;"); break;
        case '\'': XB(b, "&#39;"); break;
        default: xb_add(b, s, 1);
        }
    }
}

static const char hex_digits[] = "0123456789ABCDEF";

void url_encode(xbuf *b, const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        int plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
        if (plain) {
            xb_add(b, s, 1);
        } else {
            char enc[3];
            enc[0] = '%';
            enc[1] = hex_digits[c >> 4];
            enc[2] = hex_digits[c & 15];
            xb_add(b, enc, 3);
        }
    }
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int url_decode(char *s) {
    char *out = s;
    for (; *s; s++) {
        int v;
        if (*s == '%') {
            int hi = s[1] ? hex_value(s[1]) : -1;
            int lo = s[1] ? hex_value(s[2]) : -1;
            if (hi < 0 || lo < 0) return -1;
            v = hi * 16 + lo;
            s += 2;
        } else {
            v = (unsigned char)*s;
        }
        if (v == 0) return -1;
        *out++ = (char)v;
    }
    *out = 0;
    return 0;
}

int find_head_end(const char *s, int len) {
    for (int i = 0; i < len; i++) {
        if (s[i] == '\r' && i + 3 < len && s[i + 1] == '\n' && s[i + 2] == '\r' && s[i + 3] == '\n') return i + 4;
        if (s[i] == '\n' && i + 1 < len && s[i + 1] == '\n') return i + 2;
    }
    return -1;
}
