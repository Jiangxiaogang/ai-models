#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "http.h"

int http_send_all(SOCKET s, const char *buf, int len)
{
    while (len > 0) {
        int n = send(s, buf, len, 0);
        if (n <= 0) return 0;
        buf += n;
        len -= n;
    }
    return 1;
}

int http_recv_line(SOCKET s, char *buf, int cap)
{
    int i = 0;
    char c;
    while (i < cap - 1) {
        if (recv(s, &c, 1, 0) != 1) return 0;
        if (c == '\r') continue;
        if (c == '\n') { buf[i] = 0; return 1; }
        buf[i++] = c;
    }
    buf[i] = 0;
    return 0;
}

int body_read(BodyReader *br, char *buf, int want)
{
    int n, got = 0;
    if (want > 8192) want = 8192;
    while (got < want) {
        if (br->chunked) {
            if (br->chunk_rem == 0) {
                char line[64];
                if (!http_recv_line(br->sock, line, sizeof(line))) return got;
                br->chunk_rem = strtol(line, NULL, 16);
                if (br->chunk_rem == 0) {
                    http_recv_line(br->sock, line, sizeof(line));
                    return got;
                }
            }
            n = (int)(br->chunk_rem < (want - got) ? br->chunk_rem : (want - got));
            n = recv(br->sock, buf + got, n, 0);
            if (n <= 0) return got;
            br->chunk_rem -= n;
        } else {
            if (br->remaining <= 0) return got;
            n = (int)(br->remaining < (want - got) ? br->remaining : (want - got));
            n = recv(br->sock, buf + got, n, 0);
            if (n <= 0) return got;
            br->remaining -= n;
        }
        got += n;
    }
    return got;
}

void http_head(SOCKET s, int code, const char *reason, const char *ctype, long long len, const char *extra)
{
    char h[1024];
    int n = _snprintf_s(h, sizeof(h), _TRUNCATE,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: close\r\n",
        code, reason, ctype, len);
    if (extra) n += _snprintf_s(h + n, sizeof(h) - (size_t)n, _TRUNCATE, "%s", extra);
    _snprintf_s(h + n, sizeof(h) - (size_t)n, _TRUNCATE, "\r\n");
    http_send_all(s, h, (int)strlen(h));
}

void http_error(SOCKET s, int code, const char *reason, const char *msg)
{
    char b[512];
    int len = _snprintf_s(b, sizeof(b), _TRUNCATE,
        "<!doctype html><meta charset=utf-8><title>%d %s</title><h1>%d %s</h1><p>%s</p>",
        code, reason, code, reason, msg);
    http_head(s, code, reason, "text/html; charset=utf-8", len, NULL);
    http_send_all(s, b, len);
}
