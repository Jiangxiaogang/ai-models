#include "config.h"

#include <windows.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static int copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);

    if (length >= capacity) {
        return 0;
    }
    memcpy(destination, source, length + 1);
    return 1;
}

static int set_port(server_config *config, const char *text)
{
    char *end;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno || end == text || *trim(end) || value == 0 || value > 65535) {
        return 0;
    }
    config->port = (unsigned short)value;
    return 1;
}

int load_config(const char *path, server_config *config)
{
    char line[2048];
    FILE *file;

    config->port = 8080;
    strcpy(config->root, "www");
    file = fopen(path, "r");
    if (!file) {
        return 1;
    }
    while (fgets(line, sizeof(line), file)) {
        char *key;
        char *value;

        if (!strchr(line, '\n') && !feof(file)) {
            fclose(file);
            return 0;
        }
        key = trim(line);
        if (!*key || *key == '#' || *key == ';') {
            continue;
        }
        value = strchr(key, '=');
        if (!value) {
            fclose(file);
            return 0;
        }
        *value++ = '\0';
        key = trim(key);
        value = trim(value);
        if (_stricmp(key, "port") == 0) {
            if (!set_port(config, value)) {
                fclose(file);
                return 0;
            }
        } else if (_stricmp(key, "root") == 0) {
            if (!*value || !copy_text(config->root, sizeof(config->root), value)) {
                fclose(file);
                return 0;
            }
        }
    }
    if (ferror(file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static int is_absolute(const char *path)
{
    return (isalpha((unsigned char)path[0]) && path[1] == ':') ||
           path[0] == '\\' || path[0] == '/';
}

int resolve_root(const char *config_path, const server_config *config, char *root, size_t capacity)
{
    char full_config[SERVER_PATH_CAP];
    char combined[SERVER_PATH_CAP + sizeof(config->root)];
    const char *source = config->root;
    char *slash;
    char *other;
    DWORD length;
    DWORD attributes;

    if (is_absolute(source)) {
        if (!copy_text(combined, sizeof(combined), source)) {
            return 0;
        }
    } else {
        length = GetFullPathNameA(config_path, (DWORD)sizeof(full_config), full_config, NULL);
        if (!length || length >= sizeof(full_config)) {
            return 0;
        }
        slash = strrchr(full_config, '\\');
        other = strrchr(full_config, '/');
        if (other && (!slash || other > slash)) {
            slash = other;
        }
        if (!slash) {
            return 0;
        }
        *slash = '\0';
        {
            int written = snprintf(combined, sizeof(combined), "%s\\%s", full_config, source);
            if (written < 0 || (size_t)written >= sizeof(combined)) {
                return 0;
            }
        }
    }
    length = GetFullPathNameA(combined, (DWORD)capacity, root, NULL);
    if (!length || length >= capacity) {
        return 0;
    }
    if (!CreateDirectoryA(root, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return 0;
    }
    attributes = GetFileAttributesA(root);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
