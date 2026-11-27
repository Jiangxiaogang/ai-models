#include "http.h"

#include <stdio.h>
#include <string.h>

void conn_init(conn *c, SOCKET s)
{
    c->s = s;
    c->len = 0;
    c->pos = 0;
}

int conn_fill(conn *c)
{
    if (c->pos > 0) {
        memmove(c->buf, c->buf + c->pos, c->len - c->pos);
        c->len -= c->pos;
        c->pos = 0;
    }
    if (c->len >= sizeof(c->buf))
        return -1;
    int n = recv(c->s, c->buf + c->len, (int)(sizeof(c->buf) - c->len), 0);
    if (n <= 0)
        return -1;
    c->len += (size_t)n;
    return n;
}

int conn_read_line(conn *c, char *out, size_t cap)
{
    for (;;) {
        for (size_t i = c->pos; i < c->len; i++) {
            if (c->buf[i] == '\n') {
                size_t start = c->pos;
                size_t n = i - start;
                if (n > 0 && c->buf[start + n - 1] == '\r')
                    n--;
                if (n + 1 > cap)
                    return -2;
                memcpy(out, c->buf + start, n);
                out[n] = '\0';
                c->pos = i + 1;
                return (int)n;
            }
        }
        if (c->len - c->pos >= sizeof(c->buf))
            return -2;
        if (conn_fill(c) <= 0)
            return -1;
    }
}

int conn_read_headers(conn *c, char *out, size_t cap)
{
    char line[UTIL_LINE_MAX];
    size_t total = 0;
    out[0] = '\0';
    for (;;) {
        int n = conn_read_line(c, line, sizeof(line));
        if (n < 0)
            return -1;
        if (n == 0)
            return (int)total;
        if (total + (size_t)n + 2 > cap)
            return -2;
        memcpy(out + total, line, (size_t)n);
        total += (size_t)n;
        out[total++] = '\n';
        out[total] = '\0';
    }
}

int conn_read_body(conn *c, void *buf, size_t n)
{
    char *p = (char *)buf;
    size_t got = 0;
    if (c->pos < c->len) {
        size_t avail = c->len - c->pos;
        size_t take = avail < n ? avail : n;
        memcpy(p, c->buf + c->pos, take);
        c->pos += take;
        got = take;
    }
    while (got < n) {
        int r = recv(c->s, p + got, (int)(n - got), 0);
        if (r <= 0)
            return -1;
        got += (size_t)r;
    }
    return (int)got;
}

int conn_send_all(conn *c, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t sent = 0;
    while (sent < n) {
        int r = send(c->s, p + sent, (int)(n - sent), 0);
        if (r <= 0)
            return -1;
        sent += (size_t)r;
    }
    return 0;
}

int conn_send_text(conn *c, const char *s)
{
    return conn_send_all(c, s, strlen(s));
}

static int prefix_ieq(const char *s, const char *prefix, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char a = s[i];
        char b = prefix[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}

int http_header_get(const http_request *req, const char *name, char *out, size_t cap)
{
    const char *p = req->headers;
    size_t nlen = strlen(name);
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
        if (linelen > nlen && prefix_ieq(p, name, nlen) && p[nlen] == ':') {
            const char *v = p + nlen + 1;
            while (*v == ' ' || *v == '\t')
                v++;
            size_t vlen = eol ? (size_t)(eol - v) : strlen(v);
            if (vlen >= cap)
                vlen = cap - 1;
            memcpy(out, v, vlen);
            out[vlen] = '\0';
            return 0;
        }
        if (!eol)
            break;
        p = eol + 1;
    }
    return -1;
}

int http_query_get(const char *query, const char *name, char *out, size_t cap)
{
    size_t nlen = strlen(name);
    const char *p = query;
    while (*p) {
        const char *amp = strchr(p, '&');
        const char *eq = strchr(p, '=');
        const char *end = amp ? amp : p + strlen(p);
        if (eq && eq < end) {
            size_t klen = (size_t)(eq - p);
            if (klen == nlen && prefix_ieq(p, name, nlen)) {
                size_t vlen = (size_t)(end - eq - 1);
                char tmp[HTTP_TARGET_MAX];
                if (vlen >= sizeof(tmp))
                    vlen = sizeof(tmp) - 1;
                memcpy(tmp, eq + 1, vlen);
                tmp[vlen] = '\0';
                for (size_t i = 0; i < vlen; i++) {
                    if (tmp[i] == '+')
                        tmp[i] = ' ';
                }
                if (url_decode(tmp, out, cap) < 0)
                    return -1;
                return 0;
            }
        }
        if (!amp)
            break;
        p = amp + 1;
    }
    return -1;
}

static void parse_range(http_request *req)
{
    char value[256];
    if (http_header_get(req, "Range", value, sizeof(value)) != 0)
        return;
    const char *p = value;
    while (*p == ' ')
        p++;
    if (str_ihas_prefix(p, "bytes="))
        p += 6;
    char *dash = strchr(p, '-');
    if (!dash)
        return;
    *dash = '\0';
    char *end = NULL;
    long long start = _strtoi64(p, &end, 10);
    if (end == p || *end != '\0')
        return;
    long long last = _strtoi64(dash + 1, &end, 10);
    if (end == dash + 1)
        last = -1;
    if (last != -1 && last < start)
        return;
    req->has_range = 1;
    req->range_start = start;
    req->range_end = last;
}

int http_parse_request(conn *c, http_request *req)
{
    char line[UTIL_LINE_MAX];
    memset(req, 0, sizeof(*req));
    int n = conn_read_line(c, line, sizeof(line));
    if (n <= 0)
        return -1;
    char *sp1 = strchr(line, ' ');
    if (!sp1)
        return -1;
    *sp1 = '\0';
    char *sp2 = strchr(sp1 + 1, ' ');
    if (!sp2)
        return -1;
    *sp2 = '\0';
    snprintf(req->method, sizeof(req->method), "%s", line);
    snprintf(req->target, sizeof(req->target), "%s", sp1 + 1);
    snprintf(req->version, sizeof(req->version), "%s", sp2 + 1);
    char *query = strchr(req->target, '?');
    if (query) {
        *query = '\0';
        snprintf(req->query, sizeof(req->query), "%s", query + 1);
    }
    const char *path = req->target;
    const char *scheme = strstr(req->target, "://");
    if (scheme) {
        const char *host_end = scheme + 3;
        const char *slash = strchr(host_end, '/');
        path = slash ? slash : host_end + strlen(host_end);
    }
    snprintf(req->path, sizeof(req->path), "%s", path);
    if (conn_read_headers(c, req->headers, sizeof(req->headers)) < 0)
        return -1;
    char value[256];
    if (http_header_get(req, "Content-Length", value, sizeof(value)) == 0) {
        char *end = NULL;
        long long len = _strtoi64(value, &end, 10);
        if (end != value && len >= 0) {
            req->content_length = len;
            req->has_content_length = 1;
        }
    }
    if (http_header_get(req, "Expect", value, sizeof(value)) == 0) {
        if (str_ihas_prefix(value, "100-continue"))
            req->expect_continue = 1;
    }
    parse_range(req);
    return 0;
}

const char *http_status_text(int code)
{
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 416: return "Range Not Satisfiable";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default: return "OK";
    }
}

int http_send_head(conn *c, int code, const char *ctype, long long length, const char *extra)
{
    char head[1024];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lld\r\n"
                     "Connection: close\r\n"
                     "Server: MiniFileServer/1.0\r\n"
                     "%s"
                     "\r\n",
                     code, http_status_text(code), ctype, length, extra ? extra : "");
    if (n < 0 || n >= (int)sizeof(head))
        return -1;
    return conn_send_all(c, head, (size_t)n);
}

int http_send_html(conn *c, int code, const char *title, const char *body)
{
    char page[2048];
    int n = snprintf(page, sizeof(page),
                     "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
                     "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                     "<title>%d %s</title>"
                     "<style>body{font-family:Segoe UI,Microsoft YaHei,sans-serif;background:#0f1419;color:#d7dde5;"
                     "margin:0;padding:40px;display:flex;justify-content:center}"
                     ".box{max-width:640px;width:100%%}.code{font-size:64px;font-weight:700;color:#4f9cf9}"
                     "h1{font-size:22px;margin:8px 0 16px}p{color:#8b98a9;line-height:1.7}"
                     "a{color:#4f9cf9;text-decoration:none}a:hover{text-decoration:underline}</style>"
                     "</head><body><div class=\"box\"><div class=\"code\">%d</div><h1>%s</h1><p>%s</p>"
                     "<p><a href=\"/\">&larr; 返回首页</a></p></div></body></html>",
                     code, title, code, title, body);
    if (n < 0 || n >= (int)sizeof(page))
        return -1;
    if (http_send_head(c, code, "text/html; charset=utf-8", (long long)n, NULL) != 0)
        return -1;
    return conn_send_all(c, page, (size_t)n);
}

int http_send_error(conn *c, int code, const char *detail)
{
    char body[256];
    snprintf(body, sizeof(body), "%s", detail ? detail : "");
    return http_send_html(c, code, http_status_text(code), body);
}
