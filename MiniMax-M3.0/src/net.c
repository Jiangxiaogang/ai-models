#include "hfs_net.h"

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

static int wsa_ready = 0;

int hfs_net_init(void)
{
    WSADATA wsa;
    if (wsa_ready) return 0;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
    wsa_ready = 1;
    return 0;
}

void hfs_net_cleanup(void)
{
    if (wsa_ready) {
        WSACleanup();
        wsa_ready = 0;
    }
}

int hfs_net_listen(const char *address, int port, hfs_socket *sock)
{
    SOCKET s;
    struct sockaddr_in addr;
    int opt = 1;

    memset(sock, 0, sizeof(*sock));

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (address && address[0] && strcmp(address, "0.0.0.0") != 0) {
        if (InetPton(AF_INET, address, &addr.sin_addr) != 1) addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return -2;
    }
    if (listen(s, 32) == SOCKET_ERROR) {
        closesocket(s);
        return -3;
    }
    sock->handle = (void *)(INT_PTR)s;
    sock->connected = 1;
    return 0;
}

int hfs_net_accept(hfs_socket *listener, hfs_socket *client)
{
    SOCKET s;
    memset(client, 0, sizeof(*client));
    s = accept((SOCKET)(INT_PTR)listener->handle, NULL, NULL);
    if (s == INVALID_SOCKET) return -1;
    client->handle = (void *)(INT_PTR)s;
    client->connected = 1;
    return 0;
}

int hfs_net_recv(hfs_socket *sock, char *buf, int size, int timeout_ms)
{
    fd_set fds;
    struct timeval tv;
    int rc;

    if (size <= 0) return 0;
    if (timeout_ms >= 0) {
        FD_ZERO(&fds);
        FD_SET((SOCKET)(INT_PTR)sock->handle, &fds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        rc = select(0, &fds, NULL, NULL, &tv);
        if (rc == 0) return 0;
        if (rc < 0) return -1;
    }
    return recv((SOCKET)(INT_PTR)sock->handle, buf, size, 0);
}

int hfs_net_send(hfs_socket *sock, const char *buf, int size)
{
    if (size <= 0) return 0;
    return send((SOCKET)(INT_PTR)sock->handle, buf, size, 0);
}

int hfs_net_send_all(hfs_socket *sock, const char *buf, int size)
{
    int sent = 0;
    while (sent < size) {
        int n = hfs_net_send(sock, buf + sent, size - sent);
        if (n <= 0) return -1;
        sent += n;
    }
    return sent;
}

void hfs_net_close(hfs_socket *sock)
{
    if (sock->handle) {
        closesocket((SOCKET)(INT_PTR)sock->handle);
        sock->handle = NULL;
    }
    sock->connected = 0;
}

int hfs_net_peer_port(hfs_socket *sock)
{
    struct sockaddr_in addr;
    int len = sizeof(addr);
    if (getpeername((SOCKET)(INT_PTR)sock->handle, (struct sockaddr *)&addr, &len) == SOCKET_ERROR)
        return 0;
    return ntohs(addr.sin_port);
}

const char *hfs_net_peer_addr(hfs_socket *sock)
{
    static char buf[64];
    struct sockaddr_in addr;
    int len = sizeof(addr);
    buf[0] = '\0';
    if (getpeername((SOCKET)(INT_PTR)sock->handle, (struct sockaddr *)&addr, &len) == SOCKET_ERROR)
        return buf;
    if (InetNtop(AF_INET, &addr.sin_addr, buf, sizeof(buf)) == NULL) buf[0] = '\0';
    return buf;
}
