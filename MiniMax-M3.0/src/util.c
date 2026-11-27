#include "hfs.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int hex_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int hfs_util_url_decode(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;
    while (*src) {
        if (out + 1 >= dst_size) return -1;
        if (*src == '%' && hex_value((unsigned char)src[1]) >= 0 &&
            hex_value((unsigned char)src[2]) >= 0) {
            dst[out++] = (char)((hex_value((unsigned char)src[1]) << 4) |
                                hex_value((unsigned char)src[2]));
            src += 3;
        } else if (*src == '+') {
            dst[out++] = ' ';
            src++;
        } else {
            dst[out++] = *src++;
        }
    }
    dst[out] = '\0';
    return 0;
}

int hfs_util_url_encode(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0;
    while (*src) {
        unsigned char c = (unsigned char)*src;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            if (out + 2 > dst_size) return -1;
            dst[out++] = (char)c;
        } else {
            if (out + 4 > dst_size) return -1;
            dst[out++] = '%';
            dst[out++] = hex[c >> 4];
            dst[out++] = hex[c & 15];
        }
        src++;
    }
    dst[out] = '\0';
    return 0;
}

int hfs_util_normalize_path(const char *src, char *dst, size_t dst_size)
{
    char work[HFS_MAX_PATH * 2];
    char *segments[256];
    int count = 0;
    size_t len, i, out = 0;
    char *p, *token;
    int root_slash;

    len = strlen(src);
    if (len + 1 > sizeof(work)) return -1;
    memcpy(work, src, len + 1);

    root_slash = (work[0] == '/');
    p = work;
    if (root_slash) p = work + 1;

    token = strtok(p, "/\\");
    while (token) {
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/\\");
            continue;
        }
        if (strcmp(token, "..") == 0) {
            if (count > 0) count--;
        } else {
            if (count >= 256) return -1;
            segments[count++] = token;
        }
        token = strtok(NULL, "/\\");
    }

    if (out + 1 > dst_size) return -1;
    dst[out] = '\0';

    if (root_slash) {
        if (out + 2 > dst_size) return -1;
        dst[out++] = '/';
        dst[out] = '\0';
    }

    for (i = 0; i < (size_t)count; i++) {
        size_t seg_len = strlen(segments[i]);
        if (i > 0) {
            if (out + 2 > dst_size) return -1;
            dst[out++] = '/';
            dst[out] = '\0';
        }
        if (out + seg_len + 1 > dst_size) return -1;
        memcpy(dst + out, segments[i], seg_len + 1);
        out += seg_len;
    }
    return 0;
}

int hfs_util_path_join(const char *base, const char *rel, char *dst, size_t dst_size)
{
    size_t base_len = strlen(base);
    size_t rel_len = strlen(rel);
    int need_sep = (base_len > 0 && base[base_len - 1] != '/' && base[base_len - 1] != '\\');
    if (base_len + need_sep + rel_len + 1 > dst_size) return -1;
    memcpy(dst, base, base_len);
    if (need_sep) dst[base_len++] = '/';
    memcpy(dst + base_len, rel, rel_len + 1);
    return 0;
}

int hfs_util_path_to_absolute(const char *base, const char *rel, char *dst, size_t dst_size)
{
    char joined[HFS_MAX_PATH * 2];
    if (hfs_util_path_join(base, rel, joined, sizeof(joined)) != 0) return -1;
    return hfs_util_normalize_path(joined, dst, dst_size);
}

void hfs_util_http_date(long long t, char *dst, size_t dst_size)
{
    time_t tt = (time_t)t;
    struct tm tmv;
    if (gmtime_s(&tmv, &tt) != 0) {
        strncpy(dst, "Thu, 01 Jan 1970 00:00:00 GMT", dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }
    strftime(dst, dst_size, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
}

int hfs_util_mime_type(const char *path, char *dst, size_t dst_size)
{
    const char *dot = strrchr(path, '.');
    if (!dot || !dot[1]) {
        strncpy(dst, "application/octet-stream", dst_size - 1);
        dst[dst_size - 1] = '\0';
        return 0;
    }
    dot++;
    if (_stricmp(dot, "html") == 0 || _stricmp(dot, "htm") == 0) strcpy(dst, "text/html; charset=utf-8");
    else if (_stricmp(dot, "txt") == 0) strcpy(dst, "text/plain; charset=utf-8");
    else if (_stricmp(dot, "css") == 0) strcpy(dst, "text/css; charset=utf-8");
    else if (_stricmp(dot, "js") == 0) strcpy(dst, "application/javascript; charset=utf-8");
    else if (_stricmp(dot, "json") == 0) strcpy(dst, "application/json; charset=utf-8");
    else if (_stricmp(dot, "xml") == 0) strcpy(dst, "application/xml; charset=utf-8");
    else if (_stricmp(dot, "csv") == 0) strcpy(dst, "text/csv; charset=utf-8");
    else if (_stricmp(dot, "md") == 0) strcpy(dst, "text/markdown; charset=utf-8");
    else if (_stricmp(dot, "jpg") == 0 || _stricmp(dot, "jpeg") == 0) strcpy(dst, "image/jpeg");
    else if (_stricmp(dot, "png") == 0) strcpy(dst, "image/png");
    else if (_stricmp(dot, "gif") == 0) strcpy(dst, "image/gif");
    else if (_stricmp(dot, "bmp") == 0) strcpy(dst, "image/bmp");
    else if (_stricmp(dot, "ico") == 0) strcpy(dst, "image/x-icon");
    else if (_stricmp(dot, "svg") == 0) strcpy(dst, "image/svg+xml");
    else if (_stricmp(dot, "webp") == 0) strcpy(dst, "image/webp");
    else if (_stricmp(dot, "mp3") == 0) strcpy(dst, "audio/mpeg");
    else if (_stricmp(dot, "wav") == 0) strcpy(dst, "audio/wav");
    else if (_stricmp(dot, "mp4") == 0) strcpy(dst, "video/mp4");
    else strcpy(dst, "application/octet-stream");
    dst[dst_size - 1] = '\0';
    return 0;
}

int hfs_util_is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

int hfs_util_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int hfs_util_file_size(const char *path, long long *size)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    *size = (long long)st.st_size;
    return 0;
}

void hfs_util_escape_html(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;
    while (*src) {
        const char *rep = NULL;
        switch (*src) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#39;"; break;
            default: break;
        }
        if (rep) {
            size_t n = strlen(rep);
            if (out + n + 1 > dst_size) break;
            memcpy(dst + out, rep, n);
            out += n;
        } else {
            if (out + 2 > dst_size) break;
            dst[out++] = *src;
        }
        src++;
    }
    dst[out] = '\0';
}
