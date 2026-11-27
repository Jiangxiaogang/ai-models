#pragma comment(lib, "ws2_32.lib")

#include <string.h>

#include "net.h"

int net_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
}

void net_cleanup(void) {
    WSACleanup();
}

SOCKET net_listen(int port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(s, (struct sockaddr *)&addr, (int)sizeof addr) != 0 || listen(s, 128) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

SOCKET net_accept(SOCKET lst) {
    return accept(lst, NULL, NULL);
}

int net_recv(SOCKET s, void *buf, int len) {
    return recv(s, (char *)buf, len, 0);
}

int net_send_all(SOCKET s, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        int chunk = len > 65536 ? 65536 : (int)len;
        int n = send(s, p, chunk, 0);
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

void net_close(SOCKET s) {
    closesocket(s);
}
