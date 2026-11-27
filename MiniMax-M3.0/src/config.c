#include "hfs_config.h"
#include "hfs.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void copy_value(char *dst, size_t dst_size, const char *src)
{
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

void hfs_config_defaults(hfs_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    copy_value(cfg->listen_address, sizeof(cfg->listen_address), "0.0.0.0");
    cfg->listen_port = 8080;
    copy_value(cfg->document_root, sizeof(cfg->document_root), ".");
    copy_value(cfg->index_file, sizeof(cfg->index_file), "index.html");
    cfg->auto_index = 1;
    cfg->enable_upload = 1;
    cfg->enable_download = 1;
    copy_value(cfg->default_charset, sizeof(cfg->default_charset), "utf-8");
    cfg->max_upload_size = 0;
}

static void trim(char *s)
{
    char *p = s;
    size_t len;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static int parse_bool(const char *s)
{
    if (_stricmp(s, "1") == 0 || _stricmp(s, "true") == 0 || _stricmp(s, "yes") == 0 ||
        _stricmp(s, "on") == 0) return 1;
    return 0;
}

static int apply_pair(hfs_config *cfg, const char *key, const char *value)
{
    if (_stricmp(key, "listen_address") == 0) {
        copy_value(cfg->listen_address, sizeof(cfg->listen_address), value);
    } else if (_stricmp(key, "listen_port") == 0) {
        int port = atoi(value);
        if (port < 1 || port > 65535) return -1;
        cfg->listen_port = port;
    } else if (_stricmp(key, "document_root") == 0) {
        copy_value(cfg->document_root, sizeof(cfg->document_root), value);
    } else if (_stricmp(key, "index_file") == 0) {
        copy_value(cfg->index_file, sizeof(cfg->index_file), value);
    } else if (_stricmp(key, "auto_index") == 0) {
        cfg->auto_index = parse_bool(value);
    } else if (_stricmp(key, "enable_upload") == 0) {
        cfg->enable_upload = parse_bool(value);
    } else if (_stricmp(key, "enable_download") == 0) {
        cfg->enable_download = parse_bool(value);
    } else if (_stricmp(key, "default_charset") == 0) {
        copy_value(cfg->default_charset, sizeof(cfg->default_charset), value);
    } else if (_stricmp(key, "max_upload_size") == 0) {
        long long v = _strtoi64(value, NULL, 10);
        if (v < 0) return -1;
        cfg->max_upload_size = (int)v;
    } else {
        return 1;
    }
    return 0;
}

int hfs_config_load(hfs_config *cfg, const char *path)
{
    FILE *f;
    char line[HFS_MAX_LINE];
    int line_no = 0;

    hfs_config_defaults(cfg);

    f = fopen(path, "r");
    if (!f) return -1;

    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *value;
        int rc;
        line_no++;
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = line;
        value = eq + 1;
        trim(key);
        trim(value);
        if (key[0] == '[' && key[strlen(key) - 1] == ']') continue;
        rc = apply_pair(cfg, key, value);
        if (rc < 0) {
            fclose(f);
            return -2;
        }
    }
    fclose(f);
    return 0;
}

int hfs_config_save(const hfs_config *cfg, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# hfs configuration\n");
    fprintf(f, "listen_address = %s\n", cfg->listen_address);
    fprintf(f, "listen_port = %d\n", cfg->listen_port);
    fprintf(f, "document_root = %s\n", cfg->document_root);
    fprintf(f, "index_file = %s\n", cfg->index_file);
    fprintf(f, "auto_index = %d\n", cfg->auto_index);
    fprintf(f, "enable_upload = %d\n", cfg->enable_upload);
    fprintf(f, "enable_download = %d\n", cfg->enable_download);
    fprintf(f, "default_charset = %s\n", cfg->default_charset);
    fprintf(f, "max_upload_size = %d\n", cfg->max_upload_size);
    fclose(f);
    return 0;
}

int hfs_config_validate(hfs_config *cfg)
{
    char abs[HFS_MAX_PATH * 2];
    size_t len;

    len = strlen(cfg->document_root);
    if (len == 0) return -1;
    if (len + 3 > sizeof(abs)) return -1;
    strcpy(abs, cfg->document_root);
    if (abs[len - 1] == '/' || abs[len - 1] == '\\') abs[len - 1] = '\0';
    copy_value(cfg->document_root, sizeof(cfg->document_root), abs);

    if (!hfs_util_is_directory(cfg->document_root)) return -1;
    return 0;
}
