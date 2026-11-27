#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "defs.h"

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e >= s && isspace((unsigned char)*e)) *e-- = 0;
    return s;
}

int config_load(const char *file, Config *cfg)
{
    cfg->port = 8080;
    strcpy(cfg->root, ".");
    FILE *fp = fopen(file, "r");
    if (!fp) return 0;
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        char *p = trim(line);
        if (!*p || *p == '#') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(p);
        char *val = trim(eq + 1);
        if (!strcmp(key, "port")) {
            int n = atoi(val);
            if (n > 0 && n < 65536) cfg->port = (unsigned short)n;
        } else if (!strcmp(key, "root")) {
            if (*val) {
                size_t i, len = strlen(val);
                if (len >= sizeof(cfg->root)) len = sizeof(cfg->root) - 1;
                memcpy(cfg->root, val, len);
                cfg->root[len] = 0;
                for (i = 0; cfg->root[i]; i++)
                    if (cfg->root[i] == '/') cfg->root[i] = '\\';
            }
        }
    }
    fclose(fp);
    return 1;
}
