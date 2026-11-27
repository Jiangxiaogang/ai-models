#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#define SERVER_PATH_CAP 4096

typedef struct {
    unsigned short port;
    char root[1024];
} server_config;

int load_config(const char *path, server_config *config);
int resolve_root(const char *config_path, const server_config *config, char *root, size_t capacity);

#endif
