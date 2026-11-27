#include "common.h"

static void set_root(Config *cfg, const char *v)
{
    char tmp[MAX_PATH * 2];
    wchar_t w[MAX_PATH * 2];
    wchar_t canon[MAX_PATH * 2];
    DWORD r;
    if (!v || !*v) return;
    strcpy_s(tmp, sizeof(tmp), v);
    util_path_native(tmp);
    if (!util_u8_to_w(tmp, w, MAX_PATH * 2)) return;
    r = GetFullPathNameW(w, MAX_PATH * 2, canon, NULL);
    if (r == 0 || r >= MAX_PATH * 2) return;
    if (!util_w_to_u8(canon, cfg->root, sizeof(cfg->root))) return;
    util_path_native(cfg->root);
}

int config_load(const char *file, Config *cfg)
{
    FILE *f;
    char line[1024];
    char *p;
    char *v;
    char key[64];
    char val[512];
    char *eq;
    wchar_t wcur[MAX_PATH];
    char cur[MAX_PATH * 3];
    long port;
    DWORD r;

    cfg->port = 8080;
    r = GetCurrentDirectoryW(MAX_PATH, wcur);
    if (r == 0 || r >= MAX_PATH || !util_w_to_u8(wcur, cur, sizeof(cur))) {
        strcpy_s(cfg->root, sizeof(cfg->root), ".");
    } else {
        strcpy_s(cfg->root, sizeof(cfg->root), cur);
        util_path_native(cfg->root);
    }

    if (fopen_s(&f, file, "r") != 0 || !f) return 0;
    while (fgets(line, sizeof(line), f)) {
        p = util_strtrim(line);
        if (!*p || *p == '#' || *p == ';' || *p == '[') continue;
        eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        strcpy_s(key, sizeof(key), util_strtrim(p));
        v = util_strtrim(eq + 1);
        strcpy_s(val, sizeof(val), v);
        if (util_ieq(key, "port")) {
            port = strtol(val, NULL, 10);
            if (port > 0 && port <= 65535) cfg->port = (unsigned short)port;
        } else if (util_ieq(key, "root")) {
            set_root(cfg, val);
        }
    }
    fclose(f);
    return 1;
}

const char *config_arg(int argc, char **argv, const char *key)
{
    int i;
    for (i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], key) == 0) return argv[i + 1];
    }
    return NULL;
}
