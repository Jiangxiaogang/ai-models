#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"

static int recv_line(SOCKET s, char *out, int max)
{
    int n = 0;
    char c;
    while (n + 1 < max) {
        int k = recv(s, &c, 1, 0);
        if (k != 1)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            out[n++] = c;
    }
    out[n] = 0;
    return n;
}

int http_recv_request(struct http *r)
{
    r->reqlen = 0;
    r->body = 0;
    r->content_len = 0;
    r->head = 0;
    for (;;) {
        int k = recv(r->sock, r->req + r->reqlen, HTTP_REQ_MAX - r->reqlen, 0);
        if (k <= 0)
            return -1;
        r->reqlen += k;
        r->req[r->reqlen] = 0;
        char *h = strstr(r->req, "\r\n\r\n");
        if (h) {
            r->head = (int)(h - r->req) + 4;
            r->body = r->reqlen - r->head;
            break;
        }
        if (r->reqlen >= HTTP_REQ_MAX)
            return -1;
    }
    return http_parse(r);
}

static void header_val(char *line, char **val)
{
    char *p = strchr(line, ':');
    if (!p) {
        *val = NULL;
        return;
    }
    *p = 0;
    char *v = p + 1;
    while (*v == ' ' || *v == '\t')
        v++;
    *val = v;
}

int http_parse(struct http *r)
{
    char line[8192];
    char *p = r->req;
    char *end = r->req + r->head;
    char *e = strstr(p, "\r\n");
    if (!e)
        return -1;
    memcpy(line, p, (size_t)(e - p));
    line[e - p] = 0;
    char *url = strchr(line, ' ');
    if (!url)
        return -1;
    *url++ = 0;
    char *ver = strchr(url, ' ');
    if (!ver)
        return -1;
    *ver = 0;
    if (strlen(line) >= sizeof(r->method))
        return -1;
    strcpy(r->method, line);
    if (strlen(url) >= sizeof(r->url))
        return -1;
    strcpy(r->url, url);
    p = e + 2;
    while (p < end) {
        char *le = strstr(p, "\r\n");
        if (!le || le >= end)
            break;
        size_t ll = (size_t)(le - p);
        if (ll >= sizeof(line))
            ll = sizeof(line) - 1;
        memcpy(line, p, ll);
        line[ll] = 0;
        char *val;
        header_val(line, &val);
        if (val && !_stricmp(line, "Content-Length"))
            r->content_len = _atoi64(val);
        p = le + 2;
    }
    return 0;
}

void http_send(struct http *r, const char *data, int len)
{
    int off = 0;
    while (off < len) {
        int k = send(r->sock, data + off, len - off, 0);
        if (k <= 0)
            return;
        off += k;
    }
}

void http_reply(struct http *r, int code, const char *type, const char *body, int len)
{
    char head[512];
    const char *reason = code == 200 ? "OK"
        : code == 404 ? "Not Found"
        : code == 400 ? "Bad Request"
        : code == 405 ? "Method Not Allowed"
        : code == 507 ? "Insufficient Storage"
        : code == 201 ? "Created"
        : "Error";
    int n = _snprintf(head, sizeof(head),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        code, reason, type, len);
    http_send(r, head, n);
    if (body && len > 0)
        http_send(r, body, len);
}

void http_redirect(struct http *r, const char *loc)
{
    char head[4352];
    int n = _snprintf(head, sizeof(head),
        "HTTP/1.1 301 Moved Permanently\r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        loc);
    http_send(r, head, n);
}
