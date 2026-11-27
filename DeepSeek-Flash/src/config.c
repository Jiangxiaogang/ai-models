#include "config.h"
#include "util.h"

static void unquote(char *s) {
    size_t n = strlen(s);

    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        memmove(s, s + 1, n - 2);
        s[n - 2] = 0;
    }
}

static int is_absolute_path(const char *p) {
    if (p[0] == '\\') {
        return 1;
    }
    return isalpha((unsigned char)p[0]) && p[1] == ':';
}

int config_load(Config *cfg, const char *path, const char *base_dir) {
    FILE *fp;
    char line[1024];
    size_t rl;

    memset(cfg, 0, sizeof *cfg);
    cfg->port = 8080;
    snprintf(cfg->root, sizeof cfg->root, "%s", "www");
    snprintf(cfg->index_name, sizeof cfg->index_name, "%s", "index.html");

    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }

    while (fgets(line, (int)sizeof line, fp)) {
        char *s = str_trim(line);
        char *eq;
        char *key;
        char *val;

        if (*s == 0 || *s == '#' || *s == ';') {
            continue;
        }
        eq = strchr(s, '=');
        if (!eq) {
            continue;
        }
        *eq = 0;
        key = str_trim(s);
        val = str_trim(eq + 1);
        unquote(val);
        if (str_ieq(key, "port")) {
            int p = atoi(val);
            if (p > 0 && p < 65536) {
                cfg->port = (unsigned short)p;
            }
        } else if (str_ieq(key, "root")) {
            if (*val) {
                snprintf(cfg->root, sizeof cfg->root, "%s", val);
            }
        } else if (str_ieq(key, "index")) {
            snprintf(cfg->index_name, sizeof cfg->index_name, "%s", val);
        }
    }
    fclose(fp);

    rl = strlen(cfg->root);
    while (rl > 3 && cfg->root[rl - 1] == '\\') {
        cfg->root[--rl] = 0;
    }
    if (!is_absolute_path(cfg->root)) {
        char tmp[MAXPATH];
        if (path_join8(base_dir, cfg->root, tmp, (int)sizeof tmp)) {
            snprintf(cfg->root, sizeof cfg->root, "%s", tmp);
        }
    }
    return 1;
}
