#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_MAX 4096

struct config {
    int port;
    char root[1024];
};

int config_load(struct config *cfg, const char *path);

#endif
