#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "handler.h"
#include "http.h"

struct thread_arg {
    SOCKET sock;
    struct config cfg;
};

static DWORD WINAPI client_thread(LPVOID param)
{
    struct thread_arg *ta = (struct thread_arg *)param;
    struct http r;
    r.sock = ta->sock;
    if (http_recv_request(&r) == 0)
        handle_request(&r, &ta->cfg);
    closesocket(ta->sock);
    free(ta);
    return 0;
}

int server_run(struct config *cfg)
{
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0)
        return -1;
    SOCKET ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == INVALID_SOCKET)
        return -1;
    struct sockaddr_in sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons((u_short)cfg->port);
    if (bind(ls, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) {
        printf("bind %d failed: %d\n", cfg->port, WSAGetLastError());
        return -1;
    }
    if (listen(ls, 16) == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        return -1;
    }
    for (;;) {
        SOCKET cs = accept(ls, NULL, NULL);
        if (cs == INVALID_SOCKET)
            continue;
        struct thread_arg *ta = (struct thread_arg *)malloc(sizeof(*ta));
        if (!ta) {
            closesocket(cs);
            continue;
        }
        ta->sock = cs;
        ta->cfg = *cfg;
        if (!CreateThread(NULL, 0, client_thread, ta, 0, NULL)) {
            closesocket(cs);
            free(ta);
        }
    }
}
