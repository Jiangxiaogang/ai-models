#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

typedef struct {
    unsigned short port;
    char root[MAXPATH];
    char index_name[128];
} Config;

int config_load(Config *cfg, const char *path, const char *base_dir);

#endif
