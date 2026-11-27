#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"
#include "page.h"
#include "util.h"

#define HEAD_CAP 8192
#define BODY_CHUNK 65536
#define PATH_MAX_URL 2048

typedef struct {
    char method[8];
    char rawPath[PATH_MAX_URL];
    long long contentLength;
} Request;

static void send_all(SOCKET sock, const char *data, long long len)
{
    while (len > 0) {
        int chunk = len > BODY_CHUNK ? BODY_CHUNK : (int)len;
        int sent = send(sock, data, chunk, 0);
        if (sent <= 0) return;
        data += sent;
        len -= sent;
    }
}

static void respond(SOCKET sock, int code, const char *reason, const char *type,
                    const char *extra, const char *body, long long bodyLen)
{
    char head[512];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Server: mini-httpd\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lld\r\n"
                     "%s"
                     "Connection: close\r\n"
                     "\r\n",
                     code, reason, type, bodyLen, extra ? extra : "");
    if (n > 0 && n < (int)sizeof(head)) send_all(sock, head, n);
    if (body) send_all(sock, body, bodyLen);
}

static void respond_error(SOCKET sock, int code, const char *reason)
{
    char body[256];
    int n = snprintf(body, sizeof(body),
                     "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>%d %s</title></head>"
                     "<body style=\"font-family:Arial\"><h1>%d %s</h1><hr><em>mini-httpd</em></body></html>",
                     code, reason, code, reason);
    respond(sock, code, reason, "text/html; charset=utf-8", NULL, body, n);
}

static void drain_body(SOCKET sock, long long already, long long total)
{
    char tmp[4096];
    long long remain = total > already ? total - already : 0;
    while (remain > 0) {
        int want = remain < (long long)sizeof(tmp) ? (int)remain : (int)sizeof(tmp);
        int n = recv(sock, tmp, want, 0);
        if (n <= 0) break;
        remain -= n;
    }
}

static int recv_head(SOCKET sock, char *buf, int cap, int *bodyStart)
{
    int total = 0;
    for (;;) {
        if (total >= cap) return -1;
        int n = recv(sock, buf + total, cap - total, 0);
        if (n <= 0) return -1;
        total += n;
        buf[total] = 0;
        char *end = strstr(buf, "\r\n\r\n");
        if (end) {
            *bodyStart = (int)(end - buf) + 4;
            return total;
        }
    }
}

static void parse_request(char *head, Request *req)
{
    memset(req, 0, sizeof(*req));
    char *headers = "";
    char *eol = strstr(head, "\r\n");
    if (eol) {
        *eol = 0;
        headers = eol + 2;
    }
    char *sp1 = strchr(head, ' ');
    if (!sp1) return;
    *sp1 = 0;
    snprintf(req->method, sizeof(req->method), "%s", head);
    char *sp2 = strchr(sp1 + 1, ' ');
    if (sp2) *sp2 = 0;
    snprintf(req->rawPath, sizeof(req->rawPath), "%s", sp1 + 1);
    char *query = strchr(req->rawPath, '?');
    if (query) *query = 0;
    for (char *h = headers; *h; ) {
        char *eolH = strstr(h, "\r\n");
        size_t lineLen = eolH ? (size_t)(eolH - h) : strlen(h);
        if (lineLen > 15 && _strnicmp(h, "content-length:", 15) == 0) {
            req->contentLength = atoll(h + 15);
        }
        if (!eolH) break;
        h = eolH + 2;
    }
}

static void build_full(const char *root, const char *path, char *out, int cap)
{
    if (strcmp(path, "/") == 0) {
        snprintf(out, (size_t)cap, "%s", root);
    } else {
        snprintf(out, (size_t)cap, "%s%s", root, path);
    }
}

static void make_parents(const char *path)
{
    char tmp[PATH_MAX_URL + MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char keep = *p;
            *p = 0;
            CreateDirectoryA(tmp, NULL);
            *p = keep;
        }
    }
}

static void handle_get(SOCKET sock, const char *root, const char *rawPath)
{
    char path[PATH_MAX_URL];
    char ansiPath[PATH_MAX_URL];
    url_decode(path, rawPath);
    if (!utf8_to_ansi(path, ansiPath, (int)sizeof(ansiPath))) {
        respond_error(sock, 400, "Bad Request");
        return;
    }
    if (path_unsafe(ansiPath)) {
        respond_error(sock, 403, "Forbidden");
        return;
    }
    char full[PATH_MAX_URL + MAX_PATH];
    build_full(root, ansiPath, full, (int)sizeof(full));
    DWORD attr = GetFileAttributesA(full);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        respond_error(sock, 404, "Not Found");
        return;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        size_t len = strlen(ansiPath);
        if (len == 0 || ansiPath[len - 1] != '/') {
            char extra[PATH_MAX_URL + 32];
            snprintf(extra, sizeof(extra), "Location: %s/\r\n", rawPath);
            respond(sock, 301, "Moved Permanently", "text/plain", extra, "Moved\n", 6);
            return;
        }
        char *html = page_index_html(rawPath, full);
        if (!html) {
            respond_error(sock, 500, "Internal Server Error");
            return;
        }
        respond(sock, 200, "OK", "text/html; charset=utf-8", NULL, html, (long long)strlen(html));
        free(html);
        return;
    }
    HANDLE file = CreateFileA(full, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        respond_error(sock, 404, "Not Found");
        return;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        respond_error(sock, 500, "Internal Server Error");
        return;
    }
    const char *name = strrchr(rawPath, '/');
    name = name ? name + 1 : rawPath;
    char extra[PATH_MAX_URL + 64];
    snprintf(extra, sizeof(extra),
             "Content-Disposition: attachment; filename*=UTF-8''%s\r\n", name);
    respond(sock, 200, "OK", mime_of(full), extra, NULL, size.QuadPart);
    char chunk[BODY_CHUNK];
    long long remain = size.QuadPart;
    while (remain > 0) {
        DWORD want = remain < (long long)sizeof(chunk) ? (DWORD)remain : (DWORD)sizeof(chunk);
        DWORD got = 0;
        if (!ReadFile(file, chunk, want, &got, NULL) || got == 0) break;
        send_all(sock, chunk, got);
        remain -= got;
    }
    CloseHandle(file);
}

static void handle_put(SOCKET sock, const char *root, const char *rawPath,
                       const char *buf, int total, int bodyStart, long long contentLength)
{
    char path[PATH_MAX_URL];
    char ansiPath[PATH_MAX_URL];
    url_decode(path, rawPath);
    if (!utf8_to_ansi(path, ansiPath, (int)sizeof(ansiPath))) {
        drain_body(sock, total - bodyStart, contentLength);
        respond_error(sock, 400, "Bad Request");
        return;
    }
    size_t len = strlen(ansiPath);
    if (path_unsafe(ansiPath) || len < 2 || ansiPath[len - 1] == '/') {
        drain_body(sock, total - bodyStart, contentLength);
        respond_error(sock, 400, "Bad Request");
        return;
    }
    char full[PATH_MAX_URL + MAX_PATH];
    build_full(root, ansiPath, full, (int)sizeof(full));
    DWORD attr = GetFileAttributesA(full);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        drain_body(sock, total - bodyStart, contentLength);
        respond_error(sock, 409, "Conflict");
        return;
    }
    make_parents(full);
    HANDLE file = CreateFileA(full, GENERIC_WRITE, FILE_SHARE_READ,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        drain_body(sock, total - bodyStart, contentLength);
        respond_error(sock, 403, "Forbidden");
        return;
    }
    long long remain = contentLength;
    long long have = total - bodyStart;
    long long take = have < remain ? have : remain;
    if (take > 0) {
        DWORD written = 0;
        WriteFile(file, buf + bodyStart, (DWORD)take, &written, NULL);
        remain -= written;
    }
    char chunk[BODY_CHUNK];
    while (remain > 0) {
        int want = remain < (long long)sizeof(chunk) ? (int)remain : (int)sizeof(chunk);
        int n = recv(sock, chunk, want, 0);
        if (n <= 0) break;
        DWORD written = 0;
        if (!WriteFile(file, chunk, (DWORD)n, &written, NULL)) break;
        remain -= written;
    }
    CloseHandle(file);
    if (remain > 0) {
        DeleteFileA(full);
        return;
    }
    char extra[PATH_MAX_URL + 32];
    snprintf(extra, sizeof(extra), "Location: %s\r\n", rawPath);
    respond(sock, 201, "Created", "text/plain", extra, "Created\n", 8);
}

void http_serve(SOCKET sock, const char *root)
{
    char buf[HEAD_CAP + BODY_CHUNK];
    int bodyStart = 0;
    int total = recv_head(sock, buf, (int)sizeof(buf) - 1, &bodyStart);
    if (total <= 0) return;
    buf[total] = 0;
    Request req;
    parse_request(buf, &req);
    if (strcmp(req.method, "GET") == 0) {
        handle_get(sock, root, req.rawPath);
    } else if (strcmp(req.method, "PUT") == 0) {
        if (req.contentLength < 0) req.contentLength = 0;
        handle_put(sock, root, req.rawPath, buf, total, bodyStart, req.contentLength);
    } else {
        drain_body(sock, total - bodyStart, req.contentLength);
        respond_error(sock, 405, "Method Not Allowed");
    }
}
