#ifndef CONFIG_H
#define CONFIG_H

#include "util.h"

typedef struct {
    int port;
    char root[UTIL_PATH_MAX];
    char upload_dir[256];
    long long max_upload;
    int index_auto;
} app_config;

void config_default(app_config *cfg);
int config_load(const char *file, app_config *cfg);
int config_dump(const app_config *cfg);

#endif
