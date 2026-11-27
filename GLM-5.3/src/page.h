#ifndef PAGE_H
#define PAGE_H

#include <stddef.h>

struct strbuf {
    char *p;
    size_t len;
    size_t cap;
};

void sb_init(struct strbuf *sb);
void sb_free(struct strbuf *sb);
void sb_put(struct strbuf *sb, const char *data, size_t n);
void sb_str(struct strbuf *sb, const char *s);
void sb_fmt(struct strbuf *sb, const char *fmt, ...);
void sb_url(struct strbuf *sb, const char *s);
void sb_html(struct strbuf *sb, const char *s);
void sb_utf8(struct strbuf *sb, const wchar_t *w);

#endif
