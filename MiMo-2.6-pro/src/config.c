#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "config.h"

void config_defaults(config *cfg) {
    cfg->port = 8080;
    if (GetFullPathNameA(".", CFG_PATH_MAX, cfg->root, NULL) == 0) {
        cfg->root[0] = '.';
        cfg->root[1] = 0;
    }
}

static int is_directory(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    *end = 0;
    return s;
}

static int parse_text(config *cfg, char *text) {
    char *p = text;
    while (*p) {
        char *eol = strchr(p, '\n');
        if (eol) *eol = 0;
        char *line = trim(p);
        char *eq = strchr(line, '=');
        if (*line && *line != '#' && *line != ';' && eq) {
            *eq = 0;
            char *key = trim(line);
            char *val = trim(eq + 1);
            if (_stricmp(key, "port") == 0) {
                cfg->port = atoi(val);
            } else if (_stricmp(key, "root") == 0) {
                DWORD n = GetFullPathNameA(val, CFG_PATH_MAX, cfg->root, NULL);
                if (n == 0 || n >= CFG_PATH_MAX) return -1;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return 0;
}

int config_load(config *cfg, const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    char text[8192];
    DWORD rd = 0;
    BOOL ok = ReadFile(h, text, (DWORD)sizeof text - 1, &rd, NULL);
    CloseHandle(h);
    if (!ok) return -1;
    text[rd] = 0;
    if (parse_text(cfg, text) != 0) return -1;
    if (cfg->port <= 0 || cfg->port > 65535) return -1;
    if (!is_directory(cfg->root)) return -1;
    return 1;
}
