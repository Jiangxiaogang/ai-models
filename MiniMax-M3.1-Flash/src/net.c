#include "net.h"

#include <stdio.h>

void net_error_text(int code, char *out, size_t cap)
{
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, (DWORD)code, 0, out, (DWORD)cap, NULL);
    if (n == 0) {
        snprintf(out, cap, "winsock error %d", code);
        return;
    }
    while (n > 0 && (out[n - 1] == '\r' || out[n - 1] == '\n' || out[n - 1] == ' '))
        out[--n] = '\0';
}

int net_startup(char *err, size_t cap)
{
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        net_error_text(rc, err, cap);
        return -1;
    }
    return 0;
}

void net_cleanup(void)
{
    WSACleanup();
}

void net_set_timeout(SOCKET s, int seconds)
{
    DWORD ms = (DWORD)seconds * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, (int)sizeof(ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&ms, (int)sizeof(ms));
}

SOCKET net_listen(int port, char *err, size_t cap)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        net_error_text(WSAGetLastError(), err, cap);
        return INVALID_SOCKET;
    }
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, (int)sizeof(reuse));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        net_error_text(WSAGetLastError(), err, cap);
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (listen(s, 64) == SOCKET_ERROR) {
        net_error_text(WSAGetLastError(), err, cap);
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

void net_close(SOCKET s)
{
    if (s != INVALID_SOCKET)
        closesocket(s);
}
