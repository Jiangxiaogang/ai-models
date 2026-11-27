#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "page.h"

void sb_init(struct strbuf *sb)
{
    sb->cap = 256;
    sb->len = 0;
    sb->p = (char *)malloc(sb->cap);
    sb->p[0] = 0;
}

void sb_free(struct strbuf *sb)
{
    free(sb->p);
    sb->p = NULL;
}

void sb_put(struct strbuf *sb, const char *data, size_t n)
{
    if (sb->len + n + 1 > sb->cap) {
        while (sb->len + n + 1 > sb->cap)
            sb->cap *= 2;
        sb->p = (char *)realloc(sb->p, sb->cap);
    }
    memcpy(sb->p + sb->len, data, n);
    sb->len += n;
    sb->p[sb->len] = 0;
}

void sb_str(struct strbuf *sb, const char *s)
{
    sb_put(sb, s, strlen(s));
}

void sb_fmt(struct strbuf *sb, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0)
        n = 0;
    if ((size_t)n >= sizeof(tmp))
        n = sizeof(tmp) - 1;
    sb_put(sb, tmp, (size_t)n);
}

void sb_url(struct strbuf *sb, const char *s)
{
    static const char hx[] = "0123456789ABCDEF";
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c <= ' ' || c >= 0x80 || c == '%' || c == '&' || c == '?' ||
            c == '#' || c == '+' || c == '"' || c == '<' || c == '>' ||
            c == '\\' || c == '^' || c == '`' || c == '{' || c == '}' ||
            c == '|' || c == '\'' || c == ';')
            sb_fmt(sb, "%%%c%c", hx[c >> 4], hx[c & 15]);
        else
            sb_put(sb, s, 1);
    }
}

void sb_html(struct strbuf *sb, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '<': sb_str(sb, "&lt;"); break;
        case '>': sb_str(sb, "&gt;"); break;
        case '"': sb_str(sb, "&quot;"); break;
        case '&': sb_str(sb, "&amp;"); break;
        default: sb_put(sb, s, 1);
        }
    }
}

void sb_utf8(struct strbuf *sb, const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n > 1) {
        char *tmp = (char *)malloc((size_t)n);
        WideCharToMultiByte(CP_UTF8, 0, w, -1, tmp, n, NULL, NULL);
        sb_put(sb, tmp, (size_t)n - 1);
        free(tmp);
    }
}
