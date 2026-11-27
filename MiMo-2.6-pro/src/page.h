#ifndef PAGE_H
#define PAGE_H

#include "util.h"

void page_head(xbuf *b, const char *urlpath, int has_parent);
void page_row(xbuf *b, const char *name, int is_dir, const char *size_text, const char *time_text);
void page_tail(xbuf *b);

#endif
