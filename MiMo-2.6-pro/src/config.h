#ifndef CONFIG_H
#define CONFIG_H

#define CFG_PATH_MAX 260

typedef struct {
    int port;
    char root[CFG_PATH_MAX];
} config;

void config_defaults(config *cfg);
int config_load(config *cfg, const char *path);

#endif
