#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "charset.h"
#include "handler.h"
#include "http.h"
#include "page.h"
#include "util.h"

#define HEAD_MAX 16384
#define FS_MAX 2560

typedef struct {
    char name[512];
    unsigned long long size;
    int is_dir;
    char size_text[32];
    char time_text[32];
} dent;

static void fmt_size(unsigned long long n, char *out, size_t outsz) {
    if (n < 1024ULL) snprintf(out, outsz, "%llu B", n);
    else if (n < 1048576ULL) snprintf(out, outsz, "%.1f KB", n / 1024.0);
    else if (n < 1073741824ULL) snprintf(out, outsz, "%.1f MB", n / 1048576.0);
    else snprintf(out, outsz, "%.2f GB", n / 1073741824.0);
}

static void fmt_time(const FILETIME *ft, char *out, size_t outsz) {
    FILETIME local;
    SYSTEMTIME st;
    if (!FileTimeToLocalFileTime(ft, &local) || !FileTimeToSystemTime(&local, &st)) {
        snprintf(out, outsz, "%s", "-");
        return;
    }
    snprintf(out, outsz, "%04u-%02u-%02u %02u:%02u",
        (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
        (unsigned)st.wHour, (unsigned)st.wMinute);
}

static int dent_cmp(const void *a, const void *b) {
    const dent *x = (const dent *)a;
    const dent *y = (const dent *)b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return _stricmp(x->name, y->name);
}

static int map_path(char *fs, size_t fsz, const char *root, const char *urlpath) {
    if (strstr(urlpath, "..")) return -1;
    const char *p = urlpath;
    while (*p == '/') p++;
    char rel[2048];
    size_t n = 0;
    for (; *p; p++) {
        char c = *p;
        if (c == '/') c = '\\';
        if (c == '\\') {
            if (n > 0 && rel[n - 1] == '\\') continue;
        } else if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|' || (unsigned char)c < 32) {
            return -1;
        }
        if (n + 1 >= sizeof rel) return -1;
        rel[n++] = c;
    }
    while (n > 0 && rel[n - 1] == '\\') n--;
    rel[n] = 0;
    char ansi[2048];
    if (utf8_to_ansi(rel, ansi, sizeof ansi) != 0) {
        snprintf(ansi, sizeof ansi, "%s", rel);
    }
    int r = n > 0 ? snprintf(fs, fsz, "%s\\%s", root, ansi) : snprintf(fs, fsz, "%s", root);
    return (r > 0 && (size_t)r < fsz) ? 0 : -1;
}

static void send_redirect(SOCKET s, const char *location) {
    http_reply(s, 301, "text/html; charset=utf-8", 0, location);
}

static void send_file(SOCKET s, const char *fs, int head_only) {
    HANDLE h = CreateFileA(fs, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        send_error(s, 404);
        return;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        send_error(s, 500);
        return;
    }
    http_reply(s, 200, mime_type(fs), size.QuadPart, NULL);
    if (!head_only) {
        char buf[65536];
        DWORD rd;
        while (ReadFile(h, buf, (DWORD)sizeof buf, &rd, NULL) && rd > 0) {
            if (net_send_all(s, buf, rd) != 0) break;
        }
    }
    CloseHandle(h);
}

static void send_listing(SOCKET s, const char *fs, const char *urlpath, int head_only) {
    char pattern[FS_MAX];
    int pr = snprintf(pattern, sizeof pattern, "%s\\*", fs);
    if (pr <= 0 || (size_t)pr >= sizeof pattern) {
        send_error(s, 400);
        return;
    }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        send_error(s, 404);
        return;
    }
    dent *ents = NULL;
    size_t count = 0;
    size_t cap = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 32;
            dent *tmp = (dent *)realloc(ents, cap * sizeof *ents);
            if (!tmp) break;
            ents = tmp;
        }
        dent *d = &ents[count];
        if (ansi_to_utf8(fd.cFileName, d->name, sizeof d->name) != 0) {
            snprintf(d->name, sizeof d->name, "%s", fd.cFileName);
        }
        d->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        d->size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        fmt_size(d->size, d->size_text, sizeof d->size_text);
        fmt_time(&fd.ftLastWriteTime, d->time_text, sizeof d->time_text);
        count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (count > 1) qsort(ents, count, sizeof *ents, dent_cmp);
    xbuf b;
    xb_init(&b);
    page_head(&b, urlpath, strcmp(urlpath, "/") != 0);
    for (size_t i = 0; i < count; i++) {
        page_row(&b, ents[i].name, ents[i].is_dir, ents[i].size_text, ents[i].time_text);
    }
    page_tail(&b);
    free(ents);
    http_reply(s, 200, "text/html; charset=utf-8", (long long)b.len, NULL);
    if (!head_only && b.len > 0) net_send_all(s, b.data, b.len);
    xb_free(&b);
}

static void recv_upload(SOCKET s, const char *fs, const char *body, int body_len, const http_request *req) {
    HANDLE h = CreateFileA(fs, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        send_error(s, 403);
        return;
    }
    if (req->expect_continue) {
        net_send_all(s, "HTTP/1.1 100 Continue\r\n\r\n", 25);
    }
    long long remain = req->content_length;
    int ok = 1;
    if (body_len > 0 && remain > 0) {
        DWORD want = (DWORD)((long long)body_len < remain ? body_len : remain);
        DWORD wr = 0;
        if (!WriteFile(h, body, want, &wr, NULL)) ok = 0;
        remain -= wr;
    }
    while (ok && remain > 0) {
        char buf[65536];
        int want = (int)(remain > (long long)sizeof buf ? sizeof buf : remain);
        int n = net_recv(s, buf, want);
        if (n <= 0) {
            ok = 0;
            break;
        }
        DWORD wr = 0;
        if (!WriteFile(h, buf, (DWORD)n, &wr, NULL) || wr != (DWORD)n) ok = 0;
        remain -= n;
    }
    CloseHandle(h);
    if (!ok) {
        DeleteFileA(fs);
        send_error(s, 500);
        return;
    }
    const char *msg = "上传完成";
    http_reply(s, 201, "text/plain; charset=utf-8", (long long)strlen(msg), NULL);
    net_send_all(s, msg, strlen(msg));
}

static void drain_body(SOCKET s, long long content_length) {
    char buf[4096];
    long long remain = content_length;
    while (remain > 0) {
        int want = (int)(remain > (long long)sizeof buf ? sizeof buf : remain);
        int n = net_recv(s, buf, want);
        if (n <= 0) return;
        remain -= n;
    }
}

static void serve(SOCKET s, const char *fs, const char *urlpath, int head_only) {
    DWORD attr = GetFileAttributesA(fs);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        send_error(s, 404);
        return;
    }
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        send_file(s, fs, head_only);
        return;
    }
    size_t len = strlen(urlpath);
    if (len == 0 || urlpath[len - 1] != '/') {
        xbuf loc;
        xb_init(&loc);
        url_encode(&loc, urlpath);
        XB(&loc, "/");
        send_redirect(s, loc.data);
        xb_free(&loc);
        return;
    }
    char index[FS_MAX];
    int n = snprintf(index, sizeof index, "%s\\index.html", fs);
    if (n > 0 && (size_t)n < sizeof index && GetFileAttributesA(index) != INVALID_FILE_ATTRIBUTES) {
        send_file(s, index, head_only);
        return;
    }
    send_listing(s, fs, urlpath, head_only);
}

void handle_conn(SOCKET s, const char *root) {
    char head[HEAD_MAX];
    int total = 0;
    int hdr_end = -1;
    while (hdr_end < 0) {
        int room = (int)(sizeof(head) - 1 - (size_t)total);
        if (room <= 0) {
            send_error(s, 431);
            return;
        }
        int n = net_recv(s, head + total, room);
        if (n <= 0) return;
        total += n;
        head[total] = 0;
        hdr_end = find_head_end(head, total);
    }
    http_request req;
    if (http_parse(head, hdr_end, &req) != 0) {
        send_error(s, 400);
        return;
    }
    printf("%s %s\n", req.method, req.path);
    if (req.chunked) {
        send_error(s, 411);
        return;
    }
    char fs[FS_MAX];
    if (map_path(fs, sizeof fs, root, req.path) != 0) {
        send_error(s, 400);
        return;
    }
    int head_only = strcmp(req.method, "HEAD") == 0;
    if (strcmp(req.method, "GET") == 0 || head_only) {
        if (req.content_length > 0) drain_body(s, req.content_length);
        serve(s, fs, req.path, head_only);
    } else if (strcmp(req.method, "PUT") == 0) {
        if (req.content_length < 0) {
            send_error(s, 411);
            return;
        }
        recv_upload(s, fs, head + hdr_end, total - hdr_end, &req);
    } else {
        if (req.content_length > 0) drain_body(s, req.content_length);
        send_error(s, 405);
    }
}
