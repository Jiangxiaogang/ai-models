#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "server.h"
#include "config.h"
#include "http.h"
#include "handler.h"
#include "page.h"
#include "util.h"

static int read_header(SOCKET s, char *line, int cap)
{
    return http_recv_line(s, line, cap);
}

static void serve(SOCKET s, const Config *cfg)
{
    Request req;
    memset(&req, 0, sizeof(req));
    req.sock = s;
    char line[8192];
    if (!read_header(s, line, sizeof(line))) return;
    char method[16], url[MAX_PATH_LEN], ver[32];
    if (sscanf_s(line, "%15s %2047s %31s", method, (unsigned)sizeof(method),
                 url, (unsigned)sizeof(url), ver, (unsigned)sizeof(ver)) != 3) {
        http_error(s, 400, "Bad Request", "Malformed request line");
        return;
    }
    strcpy(req.method, method);
    url_decode(req.url, sizeof(req.url), url);
    req.content_length = 0;
    req.chunked = 0;
    char hline[8192];
    while (read_header(s, hline, sizeof(hline)) && hline[0]) {
        char *colon = strchr(hline, ':');
        if (!colon) continue;
        *colon = 0;
        char *v = colon + 1;
        while (*v == ' ' || *v == '\t') v++;
        if (!_stricmp(hline, "Content-Length"))
            req.content_length = _strtoui64(v, NULL, 10);
        else if (!_stricmp(hline, "Transfer-Encoding") && strstr(v, "chunked"))
            req.chunked = 1;
        else if (!_stricmp(hline, "Range") && strlen(v) < sizeof(req.range))
            strcpy(req.range, v);
    }
    req.body.sock = s;
    req.body.remaining = req.chunked ? 0 : req.content_length;
    req.body.chunked = req.chunked;
    req.body.chunk_rem = 0;
    if (!strcmp(req.method, "GET") || !strcmp(req.method, "HEAD"))
        handle_get(s, cfg, &req);
    else if (!strcmp(req.method, "PUT") || !strcmp(req.method, "POST"))
        handle_put(s, cfg, &req);
    else
        http_error(s, 405, "Method Not Allowed", "Unsupported method");
}

static DWORD WINAPI client_thread(LPVOID p)
{
    struct { SOCKET s; const Config *cfg; } *a = p;
    serve(a->s, a->cfg);
    shutdown(a->s, SD_BOTH);
    closesocket(a->s);
    free(a);
    return 0;
}

int server_run(const Config *cfg)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
        printf("WSAStartup failed\n");
        return 1;
    }
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    int reuse = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(cfg->port);
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    if (listen(listen_sock, 16) == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    printf("HTTP server listening on port %d, root: %s\n", cfg->port, cfg->root);
    printf("Open http://127.0.0.1:%d/ in your browser\n", cfg->port);
    while (1) {
        SOCKET c = accept(listen_sock, NULL, NULL);
        if (c == INVALID_SOCKET) break;
        struct { SOCKET s; const Config *cfg; } *a = malloc(sizeof(*a));
        a->s = c;
        a->cfg = cfg;
        HANDLE t = CreateThread(NULL, 0, client_thread, a, 0, NULL);
        if (t) CloseHandle(t);
        else { closesocket(c); free(a); }
    }
    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
