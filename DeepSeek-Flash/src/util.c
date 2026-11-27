#include "util.h"

static int buf_reserve(Buf *b, size_t need) {
    size_t cap;
    char *p;

    if (need <= b->cap) {
        return 1;
    }
    cap = b->cap ? b->cap : 256;
    while (cap < need) {
        if (cap > (size_t)-1 / 2) {
            return 0;
        }
        cap *= 2;
    }
    p = (char *)realloc(b->data, cap);
    if (!p) {
        return 0;
    }
    b->data = p;
    b->cap = cap;
    return 1;
}

int buf_init(Buf *b, size_t cap) {
    memset(b, 0, sizeof *b);
    if (cap < 64) {
        cap = 64;
    }
    b->data = (char *)malloc(cap);
    if (!b->data) {
        return 0;
    }
    b->cap = cap;
    b->data[0] = 0;
    return 1;
}

void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

int buf_addn(Buf *b, const char *s, size_t n) {
    if (!s || n == 0) {
        return 1;
    }
    if (n > (size_t)-1 - b->len - 1) {
        return 0;
    }
    if (!buf_reserve(b, b->len + n + 1)) {
        return 0;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
    return 1;
}

int buf_add(Buf *b, const char *s) {
    return buf_addn(b, s, strlen(s));
}

int buf_addf(Buf *b, const char *fmt, ...) {
    char tmp[2048];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return 0;
    }
    if ((size_t)n < sizeof tmp) {
        return buf_addn(b, tmp, (size_t)n);
    }
    {
        char *big = (char *)malloc((size_t)n + 1);
        int r;
        if (!big) {
            return 0;
        }
        va_start(ap, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, ap);
        va_end(ap);
        r = buf_addn(b, big, (size_t)n);
        free(big);
        return r;
    }
}

char *str_trim(char *s) {
    char *e;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
        e--;
    }
    *e = 0;
    return s;
}

int str_ieq(const char *a, const char *b) {
    return _stricmp(a, b) == 0;
}

int utf8_to_wide(const char *s, wchar_t *w, int cch) {
    int n;

    if (cch <= 0) {
        return 0;
    }
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, w, cch);
    if (n <= 0) {
        w[0] = 0;
        return 0;
    }
    return 1;
}

int wide_to_utf8(const wchar_t *w, char *s, int cb) {
    int n;

    if (cb <= 0) {
        return 0;
    }
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, s, cb, NULL, NULL);
    if (n <= 0) {
        s[0] = 0;
        return 0;
    }
    return 1;
}

void url_decode(char *s) {
    char *d = s;

    while (*s) {
        if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            int hi = isdigit((unsigned char)s[1]) ? s[1] - '0' : (s[1] | 0x20) - 'a' + 10;
            int lo = isdigit((unsigned char)s[2]) ? s[2] - '0' : (s[2] | 0x20) - 'a' + 10;
            *d++ = (char)((hi << 4) | lo);
            s += 3;
        } else {
            *d++ = *s++;
        }
    }
    *d = 0;
}

void url_encode(const char *s, char *out, int cb) {
    static const char hex[] = "0123456789ABCDEF";
    int o = 0;

    if (cb <= 0) {
        return;
    }
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            if (o + 1 >= cb) {
                break;
            }
            out[o++] = (char)c;
        } else {
            if (o + 3 >= cb) {
                break;
            }
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 15];
        }
    }
    out[o] = 0;
}

void html_escape_to(const char *s, char *out, int cb) {
    int o = 0;

    if (cb <= 0) {
        return;
    }
    for (; *s && o < cb - 1; s++) {
        const char *r = NULL;
        switch (*s) {
        case '&': r = "&amp;"; break;
        case '<': r = "&lt;"; break;
        case '>': r = "&gt;"; break;
        case '"': r = "&quot;"; break;
        case '\'': r = "&#39;"; break;
        default: break;
        }
        if (r) {
            while (*r && o < cb - 1) {
                out[o++] = *r++;
            }
        } else {
            out[o++] = *s;
        }
    }
    out[o] = 0;
}

int path_normalize(const char *urlpath, char *out, int cb) {
    size_t o = 0;
    const char *p = urlpath;

    if (cb < 2) {
        return 0;
    }
    while (*p == '/') {
        p++;
    }
    while (*p) {
        const char *q = p;
        size_t n;
        size_t i;
        while (*p && *p != '/') {
            p++;
        }
        n = (size_t)(p - q);
        if (n == 0) {
            while (*p == '/') {
                p++;
            }
            continue;
        }
        if (n == 2 && q[0] == '.' && q[1] == '.') {
            return 0;
        }
        if (n == 1 && q[0] == '.') {
            while (*p == '/') {
                p++;
            }
            continue;
        }
        if (o + (o ? 1 : 0) + n + 1 > (size_t)cb) {
            return 0;
        }
        if (o) {
            out[o++] = '\\';
        }
        for (i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)q[i];
            if (ch < 0x20 || ch == ':' || ch == '<' || ch == '>' || ch == '"' ||
                ch == '|' || ch == '?' || ch == '*') {
                return 0;
            }
            out[o++] = (char)ch;
        }
        while (*p == '/') {
            p++;
        }
    }
    out[o] = 0;
    return 1;
}

int path_join8(const char *root, const char *rel, char *out, int cb) {
    int n;

    if (rel && rel[0]) {
        n = snprintf(out, (size_t)cb, "%s\\%s", root, rel);
    } else {
        n = snprintf(out, (size_t)cb, "%s", root);
    }
    return n > 0 && n < cb;
}

int path_join(const char *root, const char *rel, wchar_t *out, int cch) {
    char full[MAXPATH * 2];

    if (!path_join8(root, rel, full, (int)sizeof full)) {
        return 0;
    }
    return utf8_to_wide(full, out, cch);
}

int file_exists(const wchar_t *path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

int is_directory(const wchar_t *path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int dir_create_all(const wchar_t *path) {
    wchar_t tmp[MAXPATH * 2];
    size_t len;
    size_t i;

    len = wcslen(path);
    if (len == 0 || len >= MAXPATH * 2) {
        return 0;
    }
    wcscpy(tmp, path);
    for (i = 1; i < len; i++) {
        wchar_t save;
        if (tmp[i] != L'\\' && tmp[i] != L'/') {
            continue;
        }
        if (i == 2 && tmp[1] == L':') {
            continue;
        }
        save = tmp[i];
        tmp[i] = 0;
        CreateDirectoryW(tmp, NULL);
        tmp[i] = save;
    }
    if (!CreateDirectoryW(tmp, NULL)) {
        return GetLastError() == ERROR_ALREADY_EXISTS || is_directory(tmp);
    }
    return 1;
}

void format_size(long long n, char *out, int cb) {
    const char *unit = "B";
    double v = (double)n;

    if (n >= 1024LL * 1024 * 1024) {
        v /= 1073741824.0;
        unit = "GB";
    } else if (n >= 1024LL * 1024) {
        v /= 1048576.0;
        unit = "MB";
    } else if (n >= 1024) {
        v /= 1024.0;
        unit = "KB";
    }
    if (unit[0] == 'B' || v >= 100.0) {
        snprintf(out, (size_t)cb, "%.0f %s", v, unit);
    } else {
        snprintf(out, (size_t)cb, "%.1f %s", v, unit);
    }
}

void format_time(const FILETIME *ft, char *out, int cb) {
    FILETIME local;
    SYSTEMTIME st;

    if (!FileTimeToLocalFileTime(ft, &local) || !FileTimeToSystemTime(&local, &st)) {
        snprintf(out, (size_t)cb, "-");
        return;
    }
    snprintf(out, (size_t)cb, "%04u-%02u-%02u %02u:%02u",
             (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
             (unsigned)st.wHour, (unsigned)st.wMinute);
}

void exe_dir(char *out, int cb) {
    wchar_t w[MAXPATH * 2];
    DWORD n = GetModuleFileNameW(NULL, w, (DWORD)(MAXPATH * 2));
    wchar_t *p;

    if (n == 0 || n >= (DWORD)(MAXPATH * 2)) {
        snprintf(out, (size_t)cb, ".");
        return;
    }
    p = wcsrchr(w, L'\\');
    if (p) {
        *p = 0;
    }
    if (!wide_to_utf8(w, out, cb)) {
        snprintf(out, (size_t)cb, ".");
    }
}
