#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <windows.h>
#include "util.h"
#include "config.h"

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    while (*src && i + 1 < cap) {
        if (*src == '%' && hexval(src[1]) >= 0 && hexval(src[2]) >= 0) {
            dst[i++] = (char)(hexval(src[1]) * 16 + hexval(src[2]));
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = 0;
}

int safe_path(const Config *conf, const char *url, char *out, size_t cap)
{
    char dec[MAX_PATH_LEN];
    char q[MAX_PATH_LEN];
    char base[MAX_PATH_LEN];
    char *p;
    url_decode(dec, sizeof(dec), url);
    p = strchr(dec, '?');
    if (p) *p = 0;
    for (p = dec; *p; p++) if (*p == '/') *p = '\\';
    _snprintf_s(q, sizeof(q), _TRUNCATE, "%s\\%s", conf->root, dec);
    if (!GetFullPathNameA(q, (DWORD)cap, out, NULL)) return 0;
    if (!GetFullPathNameA(conf->root, sizeof(base), base, NULL)) return 0;
    size_t n = strlen(base);
    if (_strnicmp(out, base, n) != 0) return 0;
    if (out[n] && out[n] != '\\') return 0;
    return 1;
}

const char *mime_type(const char *path)
{
    static const char *types[][2] = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".txt",  "text/plain; charset=utf-8"},
        {".css",  "text/css; charset=utf-8"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".xml",  "application/xml"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".ico",  "image/x-icon"},
        {".svg",  "image/svg+xml"},
        {".pdf",  "application/pdf"},
        {".zip",  "application/zip"},
        {".mp3",  "audio/mpeg"},
        {".mp4",  "video/mp4"},
        {".exe",  "application/octet-stream"},
        {".dll",  "application/octet-stream"}
    };
    char *dot = strrchr(path, '.');
    unsigned i;
    if (dot) for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if (!_stricmp(dot, types[i][0])) return types[i][1];
    return "application/octet-stream";
}
