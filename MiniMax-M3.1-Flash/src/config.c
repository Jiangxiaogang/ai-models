#include "config.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_default(app_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = 8080;
    cfg->max_upload = 1073741824LL;
    cfg->index_auto = 1;
    snprintf(cfg->root, sizeof(cfg->root), "%s", ".");
    snprintf(cfg->upload_dir, sizeof(cfg->upload_dir), "%s", "uploads");
}

static int parse_ll(const char *s, long long *out)
{
    char *end = NULL;
    long long v = _strtoi64(s, &end, 10);
    if (end == s)
        return -1;
    *out = v;
    return 0;
}

static void abs_path(const char *in, char *out, size_t cap)
{
    char full[UTIL_PATH_MAX * 2];
    DWORD n = GetFullPathNameA(in, (DWORD)sizeof(full), full, NULL);
    if (n == 0 || n >= sizeof(full)) {
        snprintf(out, cap, "%s", in);
        return;
    }
    snprintf(out, cap, "%s", full);
}

static int apply_entry(const char *section, const char *key, const char *value, app_config *cfg)
{
    if (strcmp(section, "server") == 0) {
        if (strcmp(key, "port") == 0) {
            long long v = 0;
            if (parse_ll(value, &v) != 0 || v < 1 || v > 65535)
                return -1;
            cfg->port = (int)v;
            return 0;
        }
        if (strcmp(key, "root") == 0) {
            abs_path(value, cfg->root, sizeof(cfg->root));
            return 0;
        }
        if (strcmp(key, "index") == 0) {
            cfg->index_auto = str_ieq(value, "on") || str_ieq(value, "1") ||
                             str_ieq(value, "true") || str_ieq(value, "yes");
            return 0;
        }
    } else if (strcmp(section, "upload") == 0) {
        if (strcmp(key, "dir") == 0) {
            snprintf(cfg->upload_dir, sizeof(cfg->upload_dir), "%s", value);
            return 0;
        }
        if (strcmp(key, "max_size") == 0) {
            long long v = 0;
            if (parse_ll(value, &v) != 0 || v < 0)
                return -1;
            cfg->max_upload = v;
            return 0;
        }
    }
    return 0;
}

int config_load(const char *file, app_config *cfg)
{
    config_default(cfg);
    char section[64] = "server";
    char line[UTIL_LINE_MAX];
    int lineno = 0;
    FILE *fp = NULL;
    if (fopen_s(&fp, file, "rb") != 0 || !fp)
        return 1;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *s = str_trim(line);
        if (*s == '\0' || *s == ';' || *s == '#')
            continue;
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end)
                continue;
            *end = '\0';
            snprintf(section, sizeof(section), "%s", s + 1);
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *key = str_trim(s);
        char *value = str_trim(eq + 1);
        if (apply_entry(section, key, value, cfg) != 0) {
            fprintf(stderr, "[config] line %d: invalid value for '%s'\n", lineno, key);
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}

int config_dump(const app_config *cfg)
{
    char root[UTIL_PATH_MAX];
    snprintf(root, sizeof(root), "%s", cfg->root);
    printf("[config] root       : %s\n", root);
    printf("[config] upload dir : %s\n", cfg->upload_dir);
    printf("[config] max upload : %lld bytes\n", cfg->max_upload);
    printf("[config] auto index : %s\n", cfg->index_auto ? "on" : "off");
    return 0;
}
