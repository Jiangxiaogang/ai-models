#include <stdio.h>
#include "config.h"
#include "server.h"

int main(void)
{
    Config cfg;
    int found = config_load(&cfg);
    if (cfg.port < 1 || cfg.port > 65535) {
        printf("[error] invalid port %d in %s\n", cfg.port, CONFIG_FILE);
        return 1;
    }
    if (found) {
        printf("[config] %s: port=%d root=\"%s\"\n", CONFIG_FILE, cfg.port, cfg.root);
    } else {
        printf("[config] %s not found, defaults: port=%d root=\"%s\"\n",
               CONFIG_FILE, cfg.port, cfg.root);
    }
    return server_run(cfg.root, cfg.port);
}
