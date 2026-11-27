#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = 0;
    return s;
}

int config_load(Config *cfg)
{
    cfg->port = CONFIG_PORT_DEFAULT;
    strcpy(cfg->root, CONFIG_ROOT_DEFAULT);
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) return 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(line);
        char *val = trim(eq + 1);
        if (strcmp(key, "port") == 0) {
            cfg->port = atoi(val);
        } else if (strcmp(key, "root") == 0 && val[0]) {
            strcpy(cfg->root, val);
        }
    }
    fclose(fp);
    char full[MAX_PATH];
    if (GetFullPathNameA(cfg->root, MAX_PATH, full, NULL)) strcpy(cfg->root, full);
    CreateDirectoryA(cfg->root, NULL);
    return 1;
}
