#include <stdio.h>

#include "config.h"
#include "net.h"
#include "server.h"

int main(int argc, char **argv) {
    config cfg;
    config_defaults(&cfg);
    const char *path = argc > 1 ? argv[1] : "fileserver.ini";
    int rc = config_load(&cfg, path);
    if (rc < 0) {
        fprintf(stderr, "invalid config: %s\n", path);
        return 1;
    }
    if (net_init() != 0) {
        fprintf(stderr, "winsock init failed\n");
        return 1;
    }
    printf("fileserver 1.0\n");
    printf("config: %s\n", rc > 0 ? path : "defaults");
    printf("root: %s\n", cfg.root);
    printf("port: %d\n", cfg.port);
    if (server_run(&cfg) != 0) {
        fprintf(stderr, "cannot listen on port %d\n", cfg.port);
        net_cleanup();
        return 1;
    }
    net_cleanup();
    return 0;
}
