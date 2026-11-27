#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "config.h"
#include "http.h"
#include "handler.h"

#pragma comment(lib, "ws2_32.lib")

static volatile int running = 1;

static void sig_handler(int sig) {
	(void)sig;
	running = 0;
}

static void print_banner(unsigned short port, const char *root) {
	printf("HTTP File Server\n");
	printf("Listening on http://localhost:%hu\n", port);
	printf("Root directory: %s\n", root);
	printf("Press Ctrl+C to stop.\n");
}

int main(int argc, char **argv) {
	config_t cfg;
	WSADATA wsa;
	SOCKET lsock, csock;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	int yes = 1;
	const char *cfgpath = "config.txt";

	if (argc > 1) cfgpath = argv[1];
	if (config_load(&cfg, cfgpath) != 0) {
		fprintf(stderr, "Warning: cannot load %s, using defaults.\n", cfgpath);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "WSAStartup failed\n");
		return 1;
	}

	lsock = socket(AF_INET, SOCK_STREAM, 0);
	if (lsock == INVALID_SOCKET) {
		fprintf(stderr, "socket failed\n");
		WSACleanup();
		return 1;
	}

	setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(cfg.port);

	if (bind(lsock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
		fprintf(stderr, "bind failed. Port %hu may be in use.\n", cfg.port);
		closesocket(lsock);
		WSACleanup();
		return 1;
	}

	if (listen(lsock, SOMAXCONN) == SOCKET_ERROR) {
		fprintf(stderr, "listen failed\n");
		closesocket(lsock);
		WSACleanup();
		return 1;
	}

	signal(SIGINT, sig_handler);
	print_banner(cfg.port, cfg.root);

	while (running) {
		fd_set fds;
		struct timeval tv = { 0, 500000 };
		FD_ZERO(&fds);
		FD_SET(lsock, &fds);
		if (select(0, &fds, NULL, NULL, &tv) > 0) {
			csock = accept(lsock, (struct sockaddr *)&addr, &addrlen);
			if (csock != INVALID_SOCKET) {
				request_t req;
				int body_len = 0;
				memset(&req, 0, sizeof(req));
				if (http_recv_req(csock, &req, NULL, &body_len) == 0) {
					handle_request(csock, &req, NULL, body_len, &cfg);
				}
				closesocket(csock);
			}
		}
	}

	printf("\nShutting down...\n");
	closesocket(lsock);
	WSACleanup();
	return 0;
}