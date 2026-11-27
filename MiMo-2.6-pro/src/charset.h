#ifndef CHARSET_H
#define CHARSET_H

#include <stddef.h>

int ansi_to_utf8(const char *in, char *out, size_t outsz);
int utf8_to_ansi(const char *in, char *out, size_t outsz);

#endif
