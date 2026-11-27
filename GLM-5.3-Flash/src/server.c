#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"
#include "http.h"

#define MAX_PATH_ROOT MAX_PATH

typedef struct {
    SOCKET sock;
    char root[MAX_PATH_ROOT];
} ClientTask;

static DWORD WINAPI client_main(LPVOID arg)
{
    ClientTask *task = (ClientTask *)arg;
    http_serve(task->sock, task->root);
    shutdown(task->sock, SD_BOTH);
    closesocket(task->sock);
    free(task);
    return 0;
}

int server_run(const char *root, int port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[error] WSAStartup failed\n");
        return 1;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        printf("[error] create socket failed\n");
        return 1;
    }
    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)port);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[error] bind port %d failed\n", port);
        return 1;
    }
    if (listen(listener, 16) == SOCKET_ERROR) {
        printf("[error] listen failed\n");
        return 1;
    }
    printf("[http ] serving \"%s\" at http://localhost:%d/\n", root, port);
    printf("[http ] press Ctrl+C to stop\n");
    for (;;) {
        SOCKET client = accept(listener, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
        BOOL nodelay = TRUE;
        setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
        ClientTask *task = (ClientTask *)malloc(sizeof(ClientTask));
        if (!task) {
            closesocket(client);
            continue;
        }
        task->sock = client;
        strcpy(task->root, root);
        HANDLE thread = CreateThread(NULL, 0, client_main, task, 0, NULL);
        if (thread) {
            CloseHandle(thread);
        } else {
            closesocket(client);
            free(task);
        }
    }
}
