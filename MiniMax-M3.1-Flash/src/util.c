#include "util.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

int sbuf_init(sbuf *b)
{
    b->data = (char *)malloc(4096);
    if (!b->data)
        return -1;
    b->len = 0;
    b->cap = 4096;
    b->data[0] = '\0';
    return 0;
}

static int sbuf_reserve(sbuf *b, size_t need)
{
    if (b->len + need + 1 <= b->cap)
        return 0;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->len + need + 1)
        cap *= 2;
    char *p = (char *)realloc(b->data, cap);
    if (!p)
        return -1;
    b->data = p;
    b->cap = cap;
    return 0;
}

int sbuf_addn(sbuf *b, const char *s, size_t n)
{
    if (sbuf_reserve(b, n) != 0)
        return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

int sbuf_add(sbuf *b, const char *s)
{
    return sbuf_addn(b, s, strlen(s));
}

int sbuf_addf(sbuf *b, _Printf_format_string_ const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char stackbuf[1024];
    int n = vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return -1;
    if ((size_t)n < sizeof(stackbuf))
        return sbuf_addn(b, stackbuf, (size_t)n);
    char *heap = (char *)malloc((size_t)n + 1);
    if (!heap)
        return -1;
    va_start(ap, fmt);
    vsnprintf(heap, (size_t)n + 1, fmt, ap);
    va_end(ap);
    int r = sbuf_addn(b, heap, (size_t)n);
    free(heap);
    return r;
}

void sbuf_free(sbuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

int str_ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb)
            return 0;
    }
    return *a == *b;
}

int str_ihas_prefix(const char *s, const char *prefix)
{
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix))
            return 0;
        s++;
        prefix++;
    }
    return 1;
}

char *str_trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return s;
}

int hex_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int url_decode(const char *src, char *dst, size_t cap)
{
    size_t o = 0;
    while (*src) {
        if (*src == '%') {
            int hi = hex_value((unsigned char)src[1]);
            if (hi < 0)
                return -1;
            int lo = hex_value((unsigned char)src[2]);
            if (lo < 0)
                return -1;
            if (o + 1 >= cap)
                return -1;
            dst[o++] = (char)((hi << 4) | lo);
            src += 3;
        } else {
            if (o + 1 >= cap)
                return -1;
            dst[o++] = *src++;
        }
    }
    dst[o] = '\0';
    return (int)o;
}

int url_encode(const char *src, char *dst, size_t cap)
{
    static const char *hexdigits = "0123456789ABCDEF";
    size_t o = 0;
    for (; *src; src++) {
        unsigned char c = (unsigned char)*src;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            if (o + 1 >= cap)
                return -1;
            dst[o++] = (char)c;
        } else {
            if (o + 3 >= cap)
                return -1;
            dst[o++] = '%';
            dst[o++] = hexdigits[c >> 4];
            dst[o++] = hexdigits[c & 15];
        }
    }
    dst[o] = '\0';
    return (int)o;
}

static int is_reserved_name(const char *name, size_t len)
{
    static const char *reserved[] = {"CON", "PRN", "AUX", "NUL",
                                     "COM1", "COM2", "COM3", "COM4", "COM5",
                                     "COM6", "COM7", "COM8", "COM9",
                                     "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
                                     "LPT6", "LPT7", "LPT8", "LPT9"};
    size_t n = len;
    const char *dot = strchr(name, '.');
    if (dot)
        n = (size_t)(dot - name);
    if (n == 0 || n > 4)
        return 0;
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strlen(reserved[i]) == n && str_ieq(name, reserved[i]))
            return 1;
    }
    return 0;
}

static void sanitize_segment(char *seg, size_t len)
{
    static const char *bad = "<>:\"|?*";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)seg[i];
        if (c < 32 || strchr(bad, (char)c))
            seg[i] = '_';
    }
    while (len > 0 && (seg[len - 1] == ' ' || seg[len - 1] == '.'))
        seg[--len] = '_';
    if (is_reserved_name(seg, len))
        seg[len] = '_';
}

int path_safe(const char *uri, char *out, size_t cap)
{
    char decoded[UTIL_PATH_MAX];
    if (url_decode(uri, decoded, sizeof(decoded)) < 0)
        return -1;
    for (char *p = decoded; *p; p++) {
        if (*p == '\\')
            *p = '/';
    }
    out[0] = '\0';
    size_t olen = 0;
    const char *p = decoded;
    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.')
            continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (olen == 0)
                return -1;
            while (olen > 0 && out[olen - 1] != '/')
                olen--;
            if (olen > 0)
                olen--;
            out[olen] = '\0';
            continue;
        }
        if (len > 255)
            return -1;
        if (olen + len + 2 > cap)
            return -1;
        if (olen)
            out[olen++] = '/';
        memcpy(out + olen, start, len);
        olen += len;
        out[olen] = '\0';
        sanitize_segment(out + (olen - len), len);
    }
    if (olen == 0) {
        if (cap < 2)
            return -1;
        out[0] = '.';
        out[1] = '\0';
    }
    return 0;
}

int path_join(char *out, size_t cap, const char *base, const char *rel)
{
    size_t blen = strlen(base);
    while (blen > 1 && (base[blen - 1] == '/' || base[blen - 1] == '\\'))
        blen--;
    if (!rel || !*rel || strcmp(rel, ".") == 0) {
        if (blen + 1 > cap)
            return -1;
        memcpy(out, base, blen);
        out[blen] = '\0';
        return 0;
    }
    size_t rlen = strlen(rel);
    if (blen + 1 + rlen + 1 > cap)
        return -1;
    memcpy(out, base, blen);
    out[blen] = '/';
    memcpy(out + blen + 1, rel, rlen);
    out[blen + 1 + rlen] = '\0';
    return 0;
}

int file_is_dir(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int file_exists(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
}

long long file_size(const char *path)
{
    struct _stat64 st;
    if (_stat64(path, &st) != 0)
        return -1;
    return (long long)st.st_size;
}

long long file_mtime(const char *path)
{
    struct _stat64 st;
    if (_stat64(path, &st) != 0)
        return 0;
    return (long long)st.st_mtime;
}

int dir_make_all(const char *path)
{
    char part[UTIL_PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len + 1 > sizeof(part))
        return -1;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || path[i] == '/' || path[i] == '\\') {
            memcpy(part, path, i);
            part[i] = '\0';
            if (i > 0 && !file_is_dir(part) && !CreateDirectoryA(part, NULL) && !file_is_dir(part))
                return -1;
        }
    }
    return 0;
}

static const char *mime_table[][2] = {
    {"html", "text/html; charset=utf-8"},
    {"htm", "text/html; charset=utf-8"},
    {"txt", "text/plain; charset=utf-8"},
    {"log", "text/plain; charset=utf-8"},
    {"md", "text/markdown; charset=utf-8"},
    {"css", "text/css; charset=utf-8"},
    {"csv", "text/csv; charset=utf-8"},
    {"js", "application/javascript; charset=utf-8"},
    {"json", "application/json; charset=utf-8"},
    {"xml", "application/xml; charset=utf-8"},
    {"pdf", "application/pdf"},
    {"zip", "application/zip"},
    {"gz", "application/gzip"},
    {"tar", "application/x-tar"},
    {"7z", "application/x-7z-compressed"},
    {"rar", "application/vnd.rar"},
    {"exe", "application/octet-stream"},
    {"dll", "application/octet-stream"},
    {"bin", "application/octet-stream"},
    {"iso", "application/octet-stream"},
    {"img", "application/octet-stream"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"bmp", "image/bmp"},
    {"ico", "image/x-icon"},
    {"svg", "image/svg+xml"},
    {"webp", "image/webp"},
    {"mp3", "audio/mpeg"},
    {"wav", "audio/wav"},
    {"flac", "audio/flac"},
    {"ogg", "audio/ogg"},
    {"mp4", "video/mp4"},
    {"webm", "video/webm"},
    {"mkv", "video/x-matroska"},
    {"avi", "video/x-msvideo"},
    {"mov", "video/quicktime"},
    {"ttf", "font/ttf"},
    {"otf", "font/otf"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"wasm", "application/wasm"},
    {NULL, NULL}};

const char *mime_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot)
        return "application/octet-stream";
    const char *ext = dot + 1;
    for (int i = 0; mime_table[i][0]; i++) {
        if (str_ieq(ext, mime_table[i][0]))
            return mime_table[i][1];
    }
    return "application/octet-stream";
}

void fmt_bytes(long long bytes, char *out, size_t cap)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = (double)bytes;
    int u = 0;
    if (v < 0)
        v = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        u++;
    }
    if (u == 0)
        snprintf(out, cap, "%lld B", bytes);
    else
        snprintf(out, cap, "%.2f %s", v, units[u]);
}

void fmt_speed(double bytes_per_sec, char *out, size_t cap)
{
    if (bytes_per_sec < 1.0)
        snprintf(out, cap, "--");
    else if (bytes_per_sec < 1024.0)
        snprintf(out, cap, "%.0f B/s", bytes_per_sec);
    else if (bytes_per_sec < 1024.0 * 1024.0)
        snprintf(out, cap, "%.1f KB/s", bytes_per_sec / 1024.0);
    else
        snprintf(out, cap, "%.2f MB/s", bytes_per_sec / (1024.0 * 1024.0));
}

int fmt_http_date(long long unix_time, char *out, size_t cap)
{
    time_t t = (time_t)unix_time;
    struct tm tmv;
    if (gmtime_s(&tmv, &t) != 0)
        return -1;
    return strftime(out, cap, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
}

int fmt_escape_html(const char *src, char *dst, size_t cap)
{
    size_t o = 0;
    for (; *src; src++) {
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
            size_t rl = strlen(rep);
            if (o + rl + 1 > cap)
                return -1;
            memcpy(dst + o, rep, rl);
            o += rl;
        } else {
            if (o + 2 > cap)
                return -1;
            dst[o++] = *src;
        }
    }
    dst[o] = '\0';
    return (int)o;
}
