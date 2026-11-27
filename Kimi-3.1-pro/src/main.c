#include "config.h"
#include "http.h"

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char executable[SERVER_PATH_CAP];
    char config_path[SERVER_PATH_CAP];
    char root[SERVER_PATH_CAP];
    server_config config;
    WSADATA wsa;
    SOCKET server;
    struct sockaddr_in address;
    DWORD length;
    char *slash;
    BOOL reuse = 1;

    if (argc > 2) {
        fprintf(stderr, "Usage: httpserver.exe [config-file]\n");
        return 1;
    }
    length = GetModuleFileNameA(NULL, executable, sizeof(executable));
    if (!length || length >= sizeof(executable)) {
        fprintf(stderr, "Cannot locate the executable.\n");
        return 1;
    }
    slash = strrchr(executable, '\\');
    if (!slash) {
        fprintf(stderr, "Cannot locate the executable directory.\n");
        return 1;
    }
    *slash = '\0';
    if (argc == 2) {
        if (strlen(argv[1]) >= sizeof(config_path)) {
            fprintf(stderr, "The configuration path is too long.\n");
            return 1;
        }
        strcpy(config_path, argv[1]);
    } else {
        int written = snprintf(config_path, sizeof(config_path), "%s\\server.ini", executable);
        if (written < 0 || (size_t)written >= sizeof(config_path)) {
            fprintf(stderr, "The configuration path is too long.\n");
            return 1;
        }
    }
    if (!load_config(config_path, &config)) {
        fprintf(stderr, "Invalid configuration: %s\n", config_path);
        return 1;
    }
    if (!resolve_root(config_path, &config, root, sizeof(root))) {
        fprintf(stderr, "Cannot open or create the root directory.\n");
        return 1;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        fprintf(stderr, "Cannot create a socket.\n");
        WSACleanup();
        return 1;
    }
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(config.port);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        fprintf(stderr, "Cannot bind port %u.\n", (unsigned int)config.port);
        closesocket(server);
        WSACleanup();
        return 1;
    }
    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "Cannot listen on port %u.\n", (unsigned int)config.port);
        closesocket(server);
        WSACleanup();
        return 1;
    }
    printf("HTTP file server\nPort: %u\nRoot: %s\n", (unsigned int)config.port, root);
    fflush(stdout);
    for (;;) {
        SOCKET client = accept(server, NULL, NULL);

        if (client == INVALID_SOCKET) {
            fprintf(stderr, "Accept failed.\n");
            continue;
        }
        handle_client(client, root);
        closesocket(client);
    }
}
