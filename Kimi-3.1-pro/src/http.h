#ifndef HTTP_H
#define HTTP_H

#include <winsock2.h>

void handle_client(SOCKET client, const char *root);

#endif
