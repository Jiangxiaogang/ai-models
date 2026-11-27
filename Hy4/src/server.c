#include "common.h"
#include <process.h>

#define RECV_BUF 65536

static Config g_cfg;

typedef struct {
    SOCKET s;
} Conn;

static void handle_client(Conn *c)
{
    char *buf;
    char *head;
    long long used = 0;
    long long head_len = 0;
    long long buffered;
    int n;
    Request req;
    const char *sep;

    buf = (char *)malloc(RECV_BUF + 1);
    if (!buf) { closesocket(c->s); free(c); return; }
    for (;;) {
        if (used >= RECV_BUF) break;
        n = recv(c->s, buf + (int)used, (int)(RECV_BUF - used), 0);
        if (n <= 0) break;
        used += n;
        buf[used] = 0;
        sep = strstr(buf, "\r\n\r\n");
        if (sep) {
            head_len = (long long)(sep + 4 - buf);
            break;
        }
    }
    if (head_len == 0) { free(buf); closesocket(c->s); free(c); return; }
    head = (char *)malloc((size_t)head_len + 1);
    if (!head) { free(buf); closesocket(c->s); free(c); return; }
    memcpy(head, buf, (size_t)head_len);
    head[head_len] = 0;

    if (!http_parse(&req, head, head_len)) {
        http_send_error(c->s, 400, "Bad Request");
    } else {
        req.sock = c->s;
        if (strcmp(req.method, "GET") == 0) {
            char path[MAX_PATH * 2];
            char dec[2048];
            long long size;
            if (util_url_decode(dec, sizeof(dec), req.raw_path) < 0 ||
                strstr(dec, "..") != NULL) {
                http_send_error(c->s, 403, "Forbidden");
            } else if (util_resolve_safe(path, sizeof(path), g_cfg.root, dec) &&
                       util_file_size(path, &size)) {
                http_send_file(c->s, path, size);
            } else if (util_resolve_safe(path, sizeof(path), g_cfg.root, dec) &&
                       util_is_dir(path)) {
                page_index(c->s, &req, &g_cfg);
            } else {
                http_send_error(c->s, 404, "Not Found");
            }
        } else if (strcmp(req.method, "PUT") == 0) {
            buffered = used - head_len;
            page_upload(c->s, &req, &g_cfg, buf, head_len, buffered);
        } else if (strcmp(req.method, "HEAD") == 0) {
            char path[MAX_PATH * 2];
            char dec[2048];
            long long size;
            ResponseHead h;
            if (util_url_decode(dec, sizeof(dec), req.raw_path) >= 0 &&
                util_resolve_safe(path, sizeof(path), g_cfg.root, dec) &&
                util_file_size(path, &size)) {
                h.status = 200;
                h.reason = "OK";
                h.type = util_mime(path);
                h.length = size;
                h.no_body = 1;
                h.extra = NULL;
                http_send_head(c->s, &h);
            } else {
                http_send_error(c->s, 404, "Not Found");
            }
        } else {
            http_send_error(c->s, 405, "Method Not Allowed");
        }
    }
    free(head);
    free(buf);
    shutdown(c->s, SD_SEND);
    closesocket(c->s);
    free(c);
}

static unsigned __stdcall thread_proc(void *arg)
{
    handle_client((Conn *)arg);
    _endthreadex(0);
    return 0;
}

void server_set_config(const Config *cfg)
{
    g_cfg = *cfg;
}

static int server_accept(SOCKET listen_sock)
{
    Conn *c;
    SOCKET s = accept(listen_sock, NULL, NULL);
    unsigned tid;
    HANDLE th;
    if (s == INVALID_SOCKET) return 1;
    c = (Conn *)malloc(sizeof(Conn));
    if (!c) { closesocket(s); return 1; }
    c->s = s;
    th = (HANDLE)_beginthreadex(NULL, 0, thread_proc, c, 0, &tid);
    if (!th) { free(c); closesocket(s); return 1; }
    CloseHandle(th);
    return 1;
}

int server_run(const Config *cfg)
{
    WSADATA wsa;
    SOCKET listen_sock = INVALID_SOCKET;
    struct sockaddr_in addr;
    char host[256];
    DWORD hostlen = sizeof(host);
    int on = 1;

    server_set_config(cfg);
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        printf("socket failed\n");
        WSACleanup();
        return 1;
    }
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(cfg->port);
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("bind failed on port %u\n", (unsigned)cfg->port);
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed\n");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    gethostname(host, (int)hostlen);
    printf("CFileServer\n");
    printf("  root : %s\n", cfg->root);
    printf("  port : %u\n", (unsigned)cfg->port);
    printf("  open : http://127.0.0.1:%u/\n", (unsigned)cfg->port);
    printf("  config: server.ini (port / root)\n");
    printf("  press Ctrl+C to stop\n\n");

    while (server_accept(listen_sock)) {
    }
    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
