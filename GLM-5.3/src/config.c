#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

int config_load(struct config *cfg, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    cfg->port = 0;
    cfg->root[0] = 0;
    char buf[CONFIG_MAX];
    while (fgets(buf, sizeof(buf), fp)) {
        char *p = strchr(buf, '=');
        if (!p)
            continue;
        *p = 0;
        char *key = buf;
        while (*key == ' ' || *key == '\t')
            key++;
        char *e = key + strlen(key);
        while (e > key && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
            *--e = 0;
        char *val = p + 1;
        while (*val == ' ' || *val == '\t')
            val++;
        e = val + strlen(val);
        while (e > val && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
            *--e = 0;
        if (!strcmp(key, "port"))
            cfg->port = atoi(val);
        else if (!strcmp(key, "root"))
            strcpy(cfg->root, val);
    }
    fclose(fp);
    if (cfg->port < 1 || cfg->port > 65535 || !cfg->root[0])
        return -1;
    return 0;
}
