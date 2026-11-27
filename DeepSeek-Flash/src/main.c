#include "common.h"
#include "config.h"
#include "net.h"
#include "http.h"
#include "util.h"

static volatile LONG g_stop = 0;

static BOOL WINAPI on_ctrl(DWORD type) {
    (void)type;
    g_stop = 1;
    return TRUE;
}

static void logmsg(const char *fmt, ...) {
    SYSTEMTIME st;
    va_list ap;

    GetLocalTime(&st);
    printf("[%02u:%02u:%02u] ", (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

int main(int argc, char **argv) {
    Config cfg;
    char cfgpath[MAXPATH];
    char base[MAXPATH];
    WSADATA wsa;
    SOCKET srv;
    wchar_t wroot[MAXPATH * 2];

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"HTTP 文件传输服务器");

    exe_dir(base, (int)sizeof base);
    if (argc > 1) {
        snprintf(cfgpath, sizeof cfgpath, "%s", argv[1]);
    } else {
        snprintf(cfgpath, sizeof cfgpath, "%s\\config.ini", base);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        logmsg("Winsock 初始化失败");
        return 1;
    }
    if (!config_load(&cfg, cfgpath, base)) {
        logmsg("未找到配置文件，使用默认设置");
    }

    logmsg("配置文件: %s", cfgpath);
    logmsg("主目录:   %s", cfg.root);
    logmsg("监听端口: %u", (unsigned)cfg.port);

    if (utf8_to_wide(cfg.root, wroot, (int)(MAXPATH * 2)) && !is_directory(wroot)) {
        if (!dir_create_all(wroot)) {
            logmsg("错误: 无法创建主目录 %s", cfg.root);
            WSACleanup();
            return 1;
        }
        logmsg("已创建主目录: %s", cfg.root);
    }

    srv = net_listen(cfg.port);
    if (srv == INVALID_SOCKET) {
        logmsg("错误: 监听端口 %u 失败，错误码 %d", (unsigned)cfg.port, WSAGetLastError());
        logmsg("请修改配置文件中的 port 后重试");
        WSACleanup();
        printf("按回车键退出...");
        (void)getchar();
        return 1;
    }

    SetConsoleCtrlHandler(on_ctrl, TRUE);
    logmsg("服务已启动: http://127.0.0.1:%u/", (unsigned)cfg.port);
    logmsg("按 Ctrl+C 停止服务");

    while (!g_stop) {
        fd_set rfds;
        struct timeval tv;
        struct sockaddr_in addr;
        int alen = (int)sizeof addr;
        SOCKET cli;
        char buf[16384];
        char ip[INET_ADDRSTRLEN];
        Conn conn;
        int status;

        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if (select(0, &rfds, NULL, NULL, &tv) <= 0) {
            continue;
        }
        cli = accept(srv, (struct sockaddr *)&addr, &alen);
        if (cli == INVALID_SOCKET) {
            continue;
        }
        conn_init(&conn, cli, buf, (int)sizeof buf);
        status = http_handle(&conn, &cfg);
        conn_close(&conn);
        if (status > 0) {
            if (!inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof ip)) {
                snprintf(ip, sizeof ip, "%s", "-");
            }
            logmsg("%s -> %d", ip, status);
        }
    }

    logmsg("正在关闭服务");
    closesocket(srv);
    WSACleanup();
    return 0;
}
