#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

#define CONFIG_FILE "httpd.conf"
#define CONFIG_PORT_DEFAULT 8080
#define CONFIG_ROOT_DEFAULT "files"

typedef struct {
    int port;
    char root[MAX_PATH];
} Config;

int config_load(Config *cfg);

#endif
