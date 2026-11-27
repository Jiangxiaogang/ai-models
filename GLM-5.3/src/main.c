#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "config.h"
#include "server.h"

int main(int argc, char **argv)
{
    const char *cf = argc > 1 ? argv[1] : "server.ini";
    struct config cfg;
    if (config_load(&cfg, cf) != 0) {
        printf("load %s failed: port=1-65535 root=dir\n", cf);
        return 1;
    }
    printf("http://127.0.0.1:%d/  root=%s\n", cfg.port, cfg.root);
    return server_run(&cfg) == 0 ? 0 : 1;
}
