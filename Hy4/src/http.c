#include "common.h"
#include <ctype.h>

static void lower_copy(char *dst, size_t cap, const char *src, size_t len)
{
    size_t i;
    if (len >= cap) len = cap - 1;
    for (i = 0; i < len; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[len] = 0;
}

long long http_head_field(const char *head, const char *name, char *dst, size_t cap)
{
    const char *p = head;
    const char *e = strstr(head, "\r\n\r\n");
    size_t nlen = strlen(name);
    const char *next;
    const char *colon;
    const char *v;
    size_t l;

    if (!e) e = head + strlen(head);
    if (dst && cap) dst[0] = 0;
    p = strstr(head, "\r\n");
    if (!p || p > e) return -1;
    p += 2;
    while (p < e) {
        next = strstr(p, "\r\n");
        if (!next || next > e) next = e;
        if ((size_t)(next - p) > nlen) {
            colon = p + nlen;
            if (*colon == ':' && _strnicmp(p, name, nlen) == 0) {
                v = colon + 1;
                while (v < next && (*v == ' ' || *v == '\t')) v++;
                l = (size_t)(next - v);
                if (dst && cap) {
                    if (l >= cap) l = cap - 1;
                    memcpy(dst, v, l);
                    dst[l] = 0;
                }
                return (long long)(next - v);
            }
        }
        p = next + 2;
    }
    return -1;
}

int http_parse(Request *req, const char *head, long long head_len)
{
    const char *sp1;
    const char *sp2;
    const char *crlf;
    char cl[64];
    char ct[128];
    char host[256];
    char q[1024];
    const char *qm;
    size_t n;

    memset(req, 0, sizeof(*req));
    req->sock = INVALID_SOCKET;
    req->content_length = 0;
    req->has_content_length = 0;
    strcpy_s(req->version, sizeof(req->version), "HTTP/1.1");
    strcpy_s(req->content_type, sizeof(req->content_type), "application/octet-stream");

    (void)head_len;
    crlf = strstr(head, "\r\n");
    if (!crlf) return 0;
    sp1 = strchr(head, ' ');
    if (!sp1 || sp1 > crlf) return 0;
    sp2 = strchr(sp1 + 1, ' ');
    if (!sp2 || sp2 > crlf) return 0;

    n = (size_t)(sp1 - head);
    if (n >= sizeof(req->method)) n = sizeof(req->method) - 1;
    memcpy(req->method, head, n);
    req->method[n] = 0;

    n = (size_t)(sp2 - sp1 - 1);
    if (n >= sizeof(req->raw_path)) n = sizeof(req->raw_path) - 1;
    memcpy(req->raw_path, sp1 + 1, n);
    req->raw_path[n] = 0;

    n = (size_t)(crlf - sp2 - 1);
    if (n >= sizeof(req->version)) n = sizeof(req->version) - 1;
    memcpy(req->version, sp2 + 1, n);
    req->version[n] = 0;

    qm = strchr(req->raw_path, '?');
    if (qm) {
        strcpy_s(q, sizeof(q), qm + 1);
        *(char *)qm = 0;
    } else {
        q[0] = 0;
    }
    strcpy_s(req->query, sizeof(req->query), q);

    if (http_head_field(head, "Content-Length", cl, sizeof(cl)) >= 0) {
        req->content_length = _atoi64(cl);
        req->has_content_length = 1;
        if (req->content_length < 0) req->content_length = 0;
    }
    if (http_head_field(head, "Content-Type", ct, sizeof(ct)) >= 0) {
        lower_copy(ct, sizeof(ct), ct, strlen(ct));
        strcpy_s(req->content_type, sizeof(req->content_type), ct);
    }
    if (http_head_field(head, "Host", host, sizeof(host)) >= 0) {
        strcpy_s(req->host, sizeof(req->host), host);
    }
    return 1;
}

int http_send_all(SOCKET s, const char *buf, long long len)
{
    long long sent = 0;
    int n;
    while (sent < len) {
        long long left = len - sent;
        int want = (int)(left > 16384 ? 16384 : left);
        n = send(s, buf + (size_t)sent, want, 0);
        if (n <= 0) return 0;
        sent += n;
    }
    return 1;
}

int http_send_head(SOCKET s, const ResponseHead *h)
{
    char buf[1024];
    char date[64];
    int n;
    util_gmt_str(date, sizeof(date));
    n = sprintf_s(buf, sizeof(buf),
        "HTTP/1.1 %d %s\r\n"
        "Server: CFileServer\r\n"
        "Date: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "%s\r\n",
        h->status, h->reason, date,
        h->type ? h->type : "application/octet-stream",
        h->length,
        h->extra ? h->extra : "");
    if (n < 0) return 0;
    return http_send_all(s, buf, n);
}

int http_send_text(SOCKET s, int status, const char *reason, const char *type, const char *text)
{
    ResponseHead h;
    long long len = (long long)strlen(text);
    h.status = status;
    h.reason = reason;
    h.type = type;
    h.length = len;
    h.no_body = 0;
    h.extra = NULL;
    if (!http_send_head(s, &h)) return 0;
    return http_send_all(s, text, len);
}

int http_send_error(SOCKET s, int status, const char *reason)
{
    char body[512];
    sprintf_s(body, sizeof(body),
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>%d %s</title></head>"
        "<body style=\"font-family:system-ui,sans-serif;padding:40px\">"
        "<h1>%d %s</h1><p>CFileServer</p></body></html>",
        status, reason, status, reason);
    return http_send_text(s, status, reason, "text/html; charset=utf-8", body);
}

int http_send_file(SOCKET s, const char *path, long long size)
{
    ResponseHead h;
    HANDLE f = INVALID_HANDLE_VALUE;
    wchar_t w[MAX_PATH * 2];
    char buf[65536];
    DWORD got = 0;
    long long left = size;
    int ok = 1;

    if (!util_u8_to_w(path, w, MAX_PATH * 2)) return http_send_error(s, 404, "Not Found");
    f = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) return http_send_error(s, 404, "Not Found");
    h.status = 200;
    h.reason = "OK";
    h.type = util_mime(path);
    h.length = size;
    h.no_body = 0;
    h.extra = "Accept-Ranges: none\r\n";
    if (!http_send_head(s, &h)) ok = 0;
    while (ok && left > 0) {
        DWORD want = (DWORD)(left > (long long)sizeof(buf) ? sizeof(buf) : left);
        if (!ReadFile(f, buf, want, &got, NULL) || got == 0) break;
        if (!http_send_all(s, buf, got)) { ok = 0; break; }
        left -= got;
    }
    CloseHandle(f);
    return ok;
}

int http_recv_body(SOCKET s, Request *req, const char *head, long long head_len, long long buffered, long long *received, int *aborted)
{
    HANDLE f = INVALID_HANDLE_VALUE;
    wchar_t w[MAX_PATH * 2];
    char buf[65536];
    long long total = 0;
    long long need;
    long long chunk;
    int n;
    DWORD wrote = 0;
    const char *extra = head + head_len;
    long long avail = buffered;
    int ok = 1;

    *received = 0;
    *aborted = 0;
    req->body = NULL;
    req->body_len = 0;
    need = req->content_length;
    if (need > 0) {
        if (!util_u8_to_w(req->path, w, MAX_PATH * 2)) return -1;
        f = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (f == INVALID_HANDLE_VALUE) return -1;
    }

    if (avail > need) avail = need;
    if (avail > 0) {
        if (f != INVALID_HANDLE_VALUE && !WriteFile(f, extra, (DWORD)avail, &wrote, NULL)) ok = 0;
        total = avail;
    }
    while (ok && total < need) {
        chunk = need - total;
        if (chunk > (long long)sizeof(buf)) chunk = sizeof(buf);
        n = recv(s, buf, (int)chunk, 0);
        if (n <= 0) { *aborted = 1; ok = 0; break; }
        if (f != INVALID_HANDLE_VALUE && !WriteFile(f, buf, (DWORD)n, &wrote, NULL)) { ok = 0; break; }
        total += n;
    }
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    *received = total;
    if (!ok && f != INVALID_HANDLE_VALUE) {
        if (util_u8_to_w(req->path, w, MAX_PATH * 2)) DeleteFileW(w);
    }
    return ok ? 1 : 0;
}
