#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "handler.h"
#include "http.h"
#include "config.h"
#include "util.h"
#include "page.h"

static int parse_range(const char *v, long long size, long long *start, long long *end)
{
    if (!v || strncmp(v, "bytes=", 6)) return 0;
    v += 6;
    char *dash = strchr(v, '-');
    if (!dash) return 0;
    *start = _strtoui64(v, NULL, 10);
    *end = dash[1] ? _strtoui64(dash + 1, NULL, 10) : size - 1;
    if (*start > *end || *start >= size) return 0;
    if (*end >= size) *end = size - 1;
    return 1;
}

void handle_get(SOCKET s, const Config *cfg, Request *req)
{
    char path[MAX_PATH_LEN];
    if (!safe_path(cfg, req->url, path, sizeof(path))) {
        http_error(s, 403, "Forbidden", "Invalid path");
        return;
    }
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        http_error(s, 404, "Not Found", "File not found");
        return;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        char idx[MAX_PATH_LEN];
        _snprintf_s(idx, sizeof(idx), _TRUNCATE, "%s\\index.html", path);
        if (GetFileAttributesA(idx) != INVALID_FILE_ATTRIBUTES) {
            Request r = *req;
            _snprintf_s(r.url, sizeof(r.url), _TRUNCATE, "%sindex.html",
                req->url[ strlen(req->url) - 1] == '/' ? req->url : "");
            char tmp[MAX_PATH_LEN];
            _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s/index.html", req->url);
            strcpy(r.url, tmp);
            handle_get(s, cfg, &r);
            return;
        }
        page_index(s, req->url, path);
        return;
    }
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        http_error(s, 403, "Forbidden", "Cannot open file");
        return;
    }
    LARGE_INTEGER sz;
    GetFileSizeEx(h, &sz);
    long long size = sz.QuadPart;
    long long start = 0, end = size - 1;
    int partial = size > 0 && parse_range(req->range, size, &start, &end);
    long long len = partial ? end - start + 1 : size;
    char extra[256] = "";
    if (partial) _snprintf_s(extra, sizeof(extra), _TRUNCATE,
        "Content-Range: bytes %lld-%lld/%lld\r\n", start, end, size);
    http_head(s, partial ? 206 : 200, partial ? "Partial Content" : "OK",
              mime_type(path), len, extra[0] ? extra : NULL);
    LARGE_INTEGER pos;
    pos.QuadPart = start;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    char buf[32768];
    long long left = len;
    DWORD got;
    while (left > 0) {
        DWORD want = (DWORD)(left < (long long)sizeof(buf) ? left : (long long)sizeof(buf));
        if (!ReadFile(h, buf, want, &got, NULL) || got == 0) break;
        if (!http_send_all(s, buf, (int)got)) break;
        left -= got;
    }
    CloseHandle(h);
}

void handle_put(SOCKET s, const Config *cfg, Request *req)
{
    char path[MAX_PATH_LEN];
    if (!safe_path(cfg, req->url, path, sizeof(path))) {
        http_error(s, 403, "Forbidden", "Invalid path");
        return;
    }
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        http_error(s, 403, "Forbidden", "Target is a directory");
        return;
    }
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        http_error(s, 500, "Internal Server Error", "Cannot create file");
        return;
    }
    char buf[32768];
    int n;
    DWORD written;
    while ((n = body_read(&req->body, buf, sizeof(buf))) > 0) {
        if (!WriteFile(h, buf, (DWORD)n, &written, NULL) || written != (DWORD)n) {
            CloseHandle(h);
            http_error(s, 500, "Internal Server Error", "Write failed");
            return;
        }
    }
    CloseHandle(h);
    http_head(s, 201, "Created", "text/plain; charset=utf-8", 2, NULL);
    http_send_all(s, "OK", 2);
}
