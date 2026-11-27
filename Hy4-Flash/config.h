#ifndef CONFIG_H
#define CONFIG_H
#include "defs.h"
typedef struct {
    unsigned short port;
    char root[MAX_PATH_LEN];
} Config;
int config_load(const char *file, Config *cfg);
#endif
