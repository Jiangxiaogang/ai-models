#ifndef HTTP_H
#define HTTP_H

#include <winsock2.h>

void http_serve(SOCKET sock, const char *root);

#endif
