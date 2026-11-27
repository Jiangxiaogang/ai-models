#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

const char HEX_UPPER[] = "0123456789ABCDEF";

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(char *dst, const char *src)
{
    while (*src) {
        if (*src == '%' && hex_val(src[1]) >= 0 && hex_val(src[2]) >= 0) {
            *dst++ = (char)(hex_val(src[1]) * 16 + hex_val(src[2]));
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

int path_unsafe(const char *path)
{
    char seg[256];
    int len = 0;
    while (*path) {
        char c = *path++;
        if (c == '/' || c == '\\') {
            seg[len] = 0;
            if (len > 0 && strcmp(seg, "..") == 0) return 1;
            len = 0;
        } else if (len < 255) {
            seg[len++] = c;
        }
    }
    seg[len] = 0;
    return len > 0 && strcmp(seg, "..") == 0;
}

int utf8_to_ansi(const char *src, char *dst, int dstCap)
{
    int wn = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (wn <= 0) {
        if (dstCap > 0) dst[0] = 0;
        return 0;
    }
    wchar_t *wide = (wchar_t *)malloc((size_t)wn * sizeof(wchar_t));
    if (!wide) {
        if (dstCap > 0) dst[0] = 0;
        return 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wide, wn);
    int n = WideCharToMultiByte(CP_ACP, 0, wide, -1, dst, dstCap, NULL, NULL);
    free(wide);
    if (n <= 0 && dstCap > 0) dst[0] = 0;
    return n;
}

int ansi_to_utf8(const char *src, char *dst, int dstCap)
{
    int wn = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
    if (wn <= 0) {
        if (dstCap > 0) dst[0] = 0;
        return 0;
    }
    wchar_t *wide = (wchar_t *)malloc((size_t)wn * sizeof(wchar_t));
    if (!wide) {
        if (dstCap > 0) dst[0] = 0;
        return 0;
    }
    MultiByteToWideChar(CP_ACP, 0, src, -1, wide, wn);
    int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, dst, dstCap, NULL, NULL);
    free(wide);
    if (n <= 0 && dstCap > 0) dst[0] = 0;
    return n;
}

const char *mime_of(const char *name)
{
    static const struct {
        const char *ext;
        const char *type;
    } TABLE[] = {
        { ".html", "text/html" },
        { ".htm",  "text/html" },
        { ".css",  "text/css" },
        { ".js",   "application/javascript" },
        { ".json", "application/json" },
        { ".xml",  "application/xml" },
        { ".txt",  "text/plain" },
        { ".md",   "text/plain" },
        { ".csv",  "text/plain" },
        { ".png",  "image/png" },
        { ".jpg",  "image/jpeg" },
        { ".jpeg", "image/jpeg" },
        { ".gif",  "image/gif" },
        { ".svg",  "image/svg+xml" },
        { ".ico",  "image/x-icon" },
        { ".webp", "image/webp" },
        { ".mp3",  "audio/mpeg" },
        { ".wav",  "audio/wav" },
        { ".mp4",  "video/mp4" },
        { ".pdf",  "application/pdf" },
        { ".zip",  "application/zip" },
        { ".7z",   "application/x-7z-compressed" },
        { NULL,    "application/octet-stream" },
    };
    const char *ext = strrchr(name, '.');
    if (ext) {
        for (int i = 0; TABLE[i].ext; i++) {
            if (_stricmp(ext, TABLE[i].ext) == 0) return TABLE[i].type;
        }
    }
    return "application/octet-stream";
}

void human_size(long long n, char *dst, int dstCap)
{
    if (n < 1024) {
        snprintf(dst, dstCap, "%lld B", n);
    } else if (n < 1024LL * 1024) {
        snprintf(dst, dstCap, "%.1f KB", n / 1024.0);
    } else if (n < 1024LL * 1024 * 1024) {
        snprintf(dst, dstCap, "%.1f MB", n / (1024.0 * 1024));
    } else {
        snprintf(dst, dstCap, "%.2f GB", n / (1024.0 * 1024 * 1024));
    }
}
