#include "common.h"
#include <ctype.h>

char *util_strtrim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = 0;
    return s;
}

int util_u8_to_w(const char *u8, wchar_t *w, size_t cap)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, (int)cap);
    if (n <= 0) {
        if (cap) w[0] = 0;
        return 0;
    }
    return n - 1;
}

int util_w_to_u8(const wchar_t *w, char *u8, size_t cap)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, u8, (int)cap, NULL, NULL);
    if (n <= 0) {
        if (cap) u8[0] = 0;
        return 0;
    }
    return n - 1;
}

void util_path_join(char *dst, size_t cap, const char *a, const char *b)
{
    size_t n;
    if (!a || !b || cap == 0) {
        if (cap) dst[0] = 0;
        return;
    }
    n = strlen(a);
    if (n > 0 && (a[n - 1] == '/' || a[n - 1] == '\\')) n--;
    if (n >= cap) n = cap - 1;
    memcpy(dst, a, n);
    dst[n] = 0;
    if (b[0] != '/' && b[0] != '\\' && n + 1 < cap) {
        dst[n++] = '\\';
        dst[n] = 0;
    }
    if (n < cap) strncat(dst, b, cap - n - 1);
}

void util_path_native(char *p)
{
    char *q;
    for (q = p; *q; q++) {
        if (*q == '/') *q = '\\';
    }
}

int util_url_decode(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    static const char hex[] = "0123456789ABCDEF";
    const char *hi;
    const char *lo;
    char c;
    while (*src && o + 1 < cap) {
        if (*src == '%' && src[1] && src[2] &&
            (hi = strchr(hex, (char)toupper((unsigned char)src[1]))) != NULL &&
            (lo = strchr(hex, (char)toupper((unsigned char)src[2]))) != NULL) {
            c = (char)(((int)(hi - hex) << 4) | (int)(lo - hex));
            if (c == 0) return -1;
            dst[o++] = c;
            src += 3;
        } else {
            dst[o++] = *src++;
        }
    }
    dst[o] = 0;
    return (int)o;
}

int util_resolve_safe(char *full, size_t cap, const char *root, const char *rel)
{
    char joined[MAX_PATH * 2];
    char canon_root[MAX_PATH * 2];
    char canon_full[MAX_PATH * 2];
    wchar_t wjoined[MAX_PATH * 2];
    wchar_t wroot[MAX_PATH * 2];
    wchar_t wcanon_root[MAX_PATH * 2];
    wchar_t wcanon_full[MAX_PATH * 2];
    size_t i;
    size_t n;
    size_t rlen;
    DWORD r;
    const char *p;

    p = rel;
    while (*p == '/' || *p == '\\') p++;
    util_path_join(joined, sizeof(joined), root, p);
    util_path_native(joined);
    for (i = 0; joined[i]; i++) {
        if (joined[i] == ':' && i != 1) return 0;
    }
    if (!util_u8_to_w(joined, wjoined, MAX_PATH * 2)) return 0;
    if (!util_u8_to_w(root, wroot, MAX_PATH * 2)) return 0;
    r = GetFullPathNameW(wroot, MAX_PATH * 2, wcanon_root, NULL);
    if (r == 0 || r >= MAX_PATH * 2) return 0;
    r = GetFullPathNameW(wjoined, MAX_PATH * 2, wcanon_full, NULL);
    if (r == 0 || r >= MAX_PATH * 2) return 0;
    if (!util_w_to_u8(wcanon_root, canon_root, sizeof(canon_root))) return 0;
    if (!util_w_to_u8(wcanon_full, canon_full, sizeof(canon_full))) return 0;
    util_path_native(canon_root);
    util_path_native(canon_full);
    rlen = strlen(canon_root);
    while (rlen > 1 && canon_root[rlen - 1] == '\\') canon_root[--rlen] = 0;
    if (_strnicmp(canon_full, canon_root, rlen) != 0) return 0;
    if (canon_full[rlen] != 0 && canon_full[rlen] != '\\') return 0;
    n = strlen(canon_full);
    while (n > 1 && canon_full[n - 1] == '\\') canon_full[--n] = 0;
    if (n + 1 > cap) return 0;
    memcpy(full, canon_full, n + 1);
    return 1;
}

int util_is_dir(const char *p)
{
    wchar_t w[MAX_PATH * 2];
    DWORD a;
    if (!util_u8_to_w(p, w, MAX_PATH * 2)) return 0;
    a = GetFileAttributesW(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int util_file_size(const char *p, long long *size)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    wchar_t w[MAX_PATH * 2];
    if (!util_u8_to_w(p, w, MAX_PATH * 2)) return 0;
    if (!GetFileAttributesExW(w, GetFileExInfoStandard, &d)) return 0;
    if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0;
    *size = ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    return 1;
}

const char *util_mime(const char *name)
{
    static const struct { const char *ext; const char *mime; } map[] = {
        { ".html", "text/html; charset=utf-8" },
        { ".htm", "text/html; charset=utf-8" },
        { ".css", "text/css; charset=utf-8" },
        { ".js", "application/javascript; charset=utf-8" },
        { ".json", "application/json; charset=utf-8" },
        { ".txt", "text/plain; charset=utf-8" },
        { ".log", "text/plain; charset=utf-8" },
        { ".md", "text/plain; charset=utf-8" },
        { ".csv", "text/plain; charset=utf-8" },
        { ".xml", "application/xml; charset=utf-8" },
        { ".png", "image/png" },
        { ".jpg", "image/jpeg" },
        { ".jpeg", "image/jpeg" },
        { ".gif", "image/gif" },
        { ".bmp", "image/bmp" },
        { ".ico", "image/x-icon" },
        { ".svg", "image/svg+xml" },
        { ".webp", "image/webp" },
        { ".pdf", "application/pdf" },
        { ".zip", "application/zip" },
        { ".7z", "application/x-7z-compressed" },
        { ".rar", "application/vnd.rar" },
        { ".gz", "application/gzip" },
        { ".tar", "application/x-tar" },
        { ".mp3", "audio/mpeg" },
        { ".mp4", "video/mp4" },
        { ".wav", "audio/wav" },
        { ".avi", "video/x-msvideo" },
        { ".exe", "application/octet-stream" },
        { ".bin", "application/octet-stream" },
        { ".dll", "application/octet-stream" }
    };
    const char *dot = strrchr(name, '.');
    size_t i;
    char ext[32];
    if (!dot) return "application/octet-stream";
    util_lower(ext, sizeof(ext), dot);
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(ext, map[i].ext) == 0) return map[i].mime;
    }
    return "application/octet-stream";
}

void util_lower(char *dst, size_t cap, const char *src)
{
    size_t i;
    for (i = 0; i + 1 < cap && src[i]; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

void util_escape_html(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    const char *p = src;
    const char *r;
    while (*p && o + 8 < cap) {
        switch (*p) {
        case '&':  r = "&amp;"; break;
        case '<':  r = "&lt;"; break;
        case '>':  r = "&gt;"; break;
        case '"':  r = "&quot;"; break;
        case '\'': r = "&#39;"; break;
        default:   r = NULL; break;
        }
        if (r) {
            size_t l = strlen(r);
            memcpy(dst + o, r, l);
            o += l;
            p++;
        } else {
            dst[o++] = *p++;
        }
    }
    dst[o] = 0;
}

void util_escape_attr(char *dst, size_t cap, const char *src)
{
    char tmp[2048];
    util_escape_html(tmp, sizeof(tmp), src);
    strcpy_s(dst, cap, tmp);
}

void util_time_str(char *dst, size_t cap, time_t t)
{
    struct tm *p = localtime(&t);
    if (p) strftime(dst, cap, "%Y-%m-%d %H:%M:%S", p);
    else strcpy_s(dst, cap, "-");
}

void util_size_str(char *dst, size_t cap, long long n)
{
    static const char *u[] = { "B", "KB", "MB", "GB", "TB" };
    double v = (double)n;
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; i++; }
    if (i == 0) sprintf_s(dst, cap, "%lld %s", n, u[i]);
    else sprintf_s(dst, cap, "%.1f %s", v, u[i]);
}

void util_gmt_str(char *dst, size_t cap)
{
    time_t t = time(NULL);
    struct tm *p = gmtime(&t);
    if (p) strftime(dst, cap, "%a, %d %b %Y %H:%M:%S GMT", p);
    else strcpy_s(dst, cap, "");
}

int util_ieq(const char *a, const char *b)
{
    return _stricmp(a, b) == 0;
}

void util_base_name(char *dst, size_t cap, const char *p)
{
    const char *s = strrchr(p, '/');
    const char *t = strrchr(p, '\\');
    const char *b = s ? s + 1 : p;
    if (t && t + 1 > b) b = t + 1;
    strcpy_s(dst, cap, b);
}
