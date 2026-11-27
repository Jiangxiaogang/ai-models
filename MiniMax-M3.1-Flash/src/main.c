#include "config.h"
#include "net.h"
#include "handler.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

static volatile int g_running = 1;
static SOCKET g_listener = INVALID_SOCKET;

static BOOL WINAPI console_handler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_running = 0;
        if (g_listener != INVALID_SOCKET)
            closesocket(g_listener);
        return TRUE;
    }
    return FALSE;
}

static void print_local_ip(int port)
{
    char host[256];
    DWORD len = (DWORD)sizeof(host);
    if (GetComputerNameA(host, &len) == 0)
        return;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char ip[64];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res)
        return;
    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)))
        printf("[net]   局域网访问: http://%s:%d/\n", ip, port);
    freeaddrinfo(res);
}

int main(int argc, char **argv)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleA("MiniFileServer - HTTP 文件传输服务器");
    const char *cfg_file = (argc > 1) ? argv[1] : "config.ini";
    app_config cfg;
    int rc = config_load(cfg_file, &cfg);
    if (rc < 0) {
        fprintf(stderr, "[config] 配置文件解析失败: %s\n", cfg_file);
        return 1;
    }
    if (rc == 1)
        printf("[config] 未找到配置文件 %s, 使用内置默认配置\n", cfg_file);
    config_dump(&cfg);
    if (dir_make_all(cfg.root) != 0) {
        fprintf(stderr, "[config] 无法创建主目录: %s\n", cfg.root);
        return 1;
    }
    char err[NET_ERR_MAX];
    if (net_startup(err, sizeof(err)) != 0) {
        fprintf(stderr, "[net] Winsock 初始化失败: %s\n", err);
        return 1;
    }
    g_listener = net_listen(cfg.port, err, sizeof(err));
    if (g_listener == INVALID_SOCKET) {
        fprintf(stderr, "[net] 监听端口 %d 失败: %s\n", cfg.port, err);
        net_cleanup();
        return 1;
    }
    SetConsoleCtrlHandler(console_handler, TRUE);
    printf("[net]   监听端口  : %d\n", cfg.port);
    printf("[net]   本机访问  : http://127.0.0.1:%d/\n", cfg.port);
    print_local_ip(cfg.port);
    printf("[net]   上传页面  : http://127.0.0.1:%d/upload.html\n", cfg.port);
    printf("[net]   按 Ctrl+C 停止服务\n");
    fflush(stdout);
    while (g_running) {
        struct sockaddr_in peer;
        int plen = (int)sizeof(peer);
        SOCKET s = accept(g_listener, (struct sockaddr *)&peer, &plen);
        if (s == INVALID_SOCKET) {
            if (!g_running)
                break;
            continue;
        }
        client_ctx *ctx = (client_ctx *)malloc(sizeof(client_ctx));
        if (!ctx) {
            net_close(s);
            continue;
        }
        ctx->s = s;
        ctx->cfg = cfg;
        unsigned thread = _beginthreadex(NULL, 0, client_thread, ctx, 0, NULL);
        if (thread)
            CloseHandle((HANDLE)thread);
    }
    printf("[net]   服务器已停止\n");
    fflush(stdout);
    net_cleanup();
    return 0;
}
