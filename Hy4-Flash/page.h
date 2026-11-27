#ifndef PAGE_H
#define PAGE_H
#include <winsock2.h>
#include "http.h"
void page_upload(SOCKET s);
void page_index(SOCKET s, const char *url, const char *path);
#endif
