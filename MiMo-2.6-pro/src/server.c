#define WIN32_LEAN_AND_MEAN

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "handler.h"
#include "net.h"
#include "server.h"

typedef struct {
    SOCKET sock;
    char root[CFG_PATH_MAX];
} conn;

static unsigned __stdcall client_thread(void *arg) {
    conn *c = (conn *)arg;
    handle_conn(c->sock, c->root);
    net_close(c->sock);
    free(c);
    return 0;
}

int server_run(const config *cfg) {
    SOCKET lst = net_listen(cfg->port);
    if (lst == INVALID_SOCKET) return -1;
    printf("listening on port %d\n", cfg->port);
    for (;;) {
        SOCKET s = net_accept(lst);
        if (s == INVALID_SOCKET) continue;
        conn *c = (conn *)malloc(sizeof *c);
        if (!c) {
            net_close(s);
            continue;
        }
        c->sock = s;
        snprintf(c->root, sizeof c->root, "%s", cfg->root);
        uintptr_t h = _beginthreadex(NULL, 0, client_thread, c, 0, NULL);
        if (h == 0) {
            net_close(s);
            free(c);
            continue;
        }
        CloseHandle((HANDLE)h);
    }
}
