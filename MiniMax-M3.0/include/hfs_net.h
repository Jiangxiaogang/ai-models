#ifndef HFS_NET_H
#define HFS_NET_H

#include <stddef.h>

typedef struct {
    void *handle;
    int connected;
} hfs_socket;

int hfs_net_init(void);
void hfs_net_cleanup(void);
int hfs_net_listen(const char *address, int port, hfs_socket *sock);
int hfs_net_accept(hfs_socket *listener, hfs_socket *client);
int hfs_net_recv(hfs_socket *sock, char *buf, int size, int timeout_ms);
int hfs_net_send(hfs_socket *sock, const char *buf, int size);
int hfs_net_send_all(hfs_socket *sock, const char *buf, int size);
void hfs_net_close(hfs_socket *sock);
int hfs_net_peer_port(hfs_socket *sock);
const char *hfs_net_peer_addr(hfs_socket *sock);

#endif
