#ifndef NET_H
#define NET_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stddef.h>
#include <winsock2.h>

int net_init(void);
void net_cleanup(void);
SOCKET net_listen(int port);
SOCKET net_accept(SOCKET lst);
int net_recv(SOCKET s, void *buf, int len);
int net_send_all(SOCKET s, const void *buf, size_t len);
void net_close(SOCKET s);

#endif
