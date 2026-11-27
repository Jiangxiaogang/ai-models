#ifndef NET_H
#define NET_H

#include <winsock2.h>
#include <stddef.h>

#define NET_ERR_MAX 256

int net_startup(char *err, size_t cap);
void net_cleanup(void);
SOCKET net_listen(int port, char *err, size_t cap);
void net_close(SOCKET s);
void net_set_timeout(SOCKET s, int seconds);
void net_error_text(int code, char *out, size_t cap);

#endif
