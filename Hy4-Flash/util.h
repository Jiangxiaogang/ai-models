#ifndef UTIL_H
#define UTIL_H
#include <stdio.h>
#include "defs.h"
#include "config.h"
void url_decode(char *dst, size_t cap, const char *src);
int safe_path(const Config *cfg, const char *url, char *out, size_t cap);
const char *mime_type(const char *path);
#endif
