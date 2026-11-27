#ifndef UTIL_H
#define UTIL_H

void url_decode(char *dst, const char *src);
int path_unsafe(const char *path);
int utf8_to_ansi(const char *src, char *dst, int dstCap);
int ansi_to_utf8(const char *src, char *dst, int dstCap);
const char *mime_of(const char *name);
void human_size(long long n, char *dst, int dstCap);

extern const char HEX_UPPER[];

#endif
