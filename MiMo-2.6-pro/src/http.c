#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "util.h"

typedef struct {
    int code;
    const char *reason;
    const char *desc;
} status_entry;

static const status_entry statuses[] = {
    {200, "OK", "请求成功"},
    {201, "Created", "文件已上传"},
    {301, "Moved Permanently", "已重定向"},
    {400, "Bad Request", "请求无效"},
    {403, "Forbidden", "禁止访问"},
    {404, "Not Found", "找不到指定的文件或目录"},
    {405, "Method Not Allowed", "不支持的请求方法"},
    {411, "Length Required", "缺少 Content-Length 请求头"},
    {431, "Request Header Fields Too Large", "请求头过大"},
    {500, "Internal Server Error", "服务器内部错误"}
};

static const status_entry *find_status(int code) {
    for (size_t i = 0; i < sizeof statuses / sizeof statuses[0]; i++) {
        if (statuses[i].code == code) return &statuses[i];
    }
    return NULL;
}

const char *http_reason(int code) {
    const status_entry *e = find_status(code);
    return e ? e->reason : "Error";
}

void http_reply(SOCKET s, int code, const char *ctype, long long clen, const char *location) {
    xbuf b;
    xb_init(&b);
    xb_addf(&b, "HTTP/1.1 %d %s\r\n", code, http_reason(code));
    XB(&b, "Server: fileserver/1.0\r\n");
    xb_addf(&b, "Content-Type: %s\r\n", ctype);
    xb_addf(&b, "Content-Length: %lld\r\n", clen);
    if (location) {
        XB(&b, "Location: ");
        xb_add(&b, location, strlen(location));
        XB(&b, "\r\n");
    }
    XB(&b, "Connection: close\r\n\r\n");
    net_send_all(s, b.data, b.len);
    xb_free(&b);
}

void send_error(SOCKET s, int code) {
    const status_entry *e = find_status(code);
    xbuf b;
    xb_init(&b);
    XB(&b, "<!doctype html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\"><title>");
    xb_addf(&b, "%d", code);
    XB(&b, "</title></head><body><h1>");
    xb_addf(&b, "%d %s", code, http_reason(code));
    XB(&b, "</h1><p>");
    if (e) xb_add(&b, e->desc, strlen(e->desc));
    XB(&b, "</p></body></html>\n");
    http_reply(s, code, "text/html; charset=utf-8", (long long)b.len, NULL);
    net_send_all(s, b.data, b.len);
    xb_free(&b);
}

typedef struct {
    const char *ext;
    const char *type;
} mime_entry;

static const mime_entry mimes[] = {
    {"html", "text/html; charset=utf-8"},
    {"htm", "text/html; charset=utf-8"},
    {"css", "text/css; charset=utf-8"},
    {"js", "application/javascript; charset=utf-8"},
    {"json", "application/json; charset=utf-8"},
    {"txt", "text/plain; charset=utf-8"},
    {"log", "text/plain; charset=utf-8"},
    {"md", "text/plain; charset=utf-8"},
    {"csv", "text/plain; charset=utf-8"},
    {"xml", "application/xml"},
    {"pdf", "application/pdf"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"webp", "image/webp"},
    {"bmp", "image/bmp"},
    {"svg", "image/svg+xml"},
    {"ico", "image/x-icon"},
    {"mp3", "audio/mpeg"},
    {"wav", "audio/wav"},
    {"flac", "audio/flac"},
    {"mp4", "video/mp4"},
    {"mkv", "video/x-matroska"},
    {"avi", "video/x-msvideo"},
    {"mov", "video/quicktime"},
    {"wmv", "video/x-ms-wmv"},
    {"zip", "application/zip"},
    {"7z", "application/x-7z-compressed"},
    {"rar", "application/vnd.rar"},
    {"gz", "application/gzip"},
    {"tar", "application/x-tar"},
    {"bz2", "application/x-bzip2"},
    {"iso", "application/x-iso9660-image"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"ttf", "font/ttf"},
    {"otf", "font/otf"},
    {"wasm", "application/wasm"},
    {"apk", "application/vnd.android.package-archive"},
    {"exe", "application/octet-stream"},
    {"dll", "application/octet-stream"},
    {"bin", "application/octet-stream"}
};

const char *mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || strchr(dot, '\\') || strchr(dot, '/')) return "application/octet-stream";
    char ext[12];
    size_t n = 0;
    for (const char *p = dot + 1; *p && n + 1 < sizeof ext; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        ext[n++] = c;
    }
    ext[n] = 0;
    for (size_t i = 0; i < sizeof mimes / sizeof mimes[0]; i++) {
        if (strcmp(ext, mimes[i].ext) == 0) return mimes[i].type;
    }
    return "application/octet-stream";
}

static void parse_header(const char *line, http_request *req) {
    const char *colon = strchr(line, ':');
    if (!colon) return;
    size_t n = (size_t)(colon - line);
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (n == 14 && _strnicmp(line, "Content-Length", 14) == 0) {
        req->content_length = strtoll(v, NULL, 10);
    } else if (n == 6 && _strnicmp(line, "Expect", 6) == 0 && strstr(v, "100")) {
        req->expect_continue = 1;
    } else if (n == 17 && _strnicmp(line, "Transfer-Encoding", 17) == 0 && strstr(v, "chunked")) {
        req->chunked = 1;
    }
}

static int copy_token(const char **pp, char *out, size_t outsz) {
    const char *p = *pp;
    size_t n = 0;
    while (*p == ' ') p++;
    while (*p && *p != ' ') {
        if (n + 1 >= outsz) return -1;
        out[n++] = *p++;
    }
    out[n] = 0;
    *pp = p;
    return n > 0 ? 0 : -1;
}

int http_parse(const char *head, int len, http_request *req) {
    memset(req, 0, sizeof *req);
    req->content_length = -1;
    const char *end = head + len;
    const char *eol = memchr(head, '\n', (size_t)len);
    if (!eol) return -1;
    int ll = (int)(eol - head);
    if (ll > 0 && head[ll - 1] == '\r') ll--;
    char line[2176];
    if (ll <= 0 || ll >= (int)sizeof line) return -1;
    memcpy(line, head, (size_t)ll);
    line[ll] = 0;
    char target[2048];
    const char *lp = line;
    if (copy_token(&lp, req->method, sizeof req->method) != 0) return -1;
    if (copy_token(&lp, target, sizeof target) != 0) return -1;
    char *query = strchr(target, '?');
    if (query) *query = 0;
    if (url_decode(target) != 0) return -1;
    size_t plen = strlen(target);
    if (plen == 0 || plen >= sizeof req->path) return -1;
    memcpy(req->path, target, plen + 1);
    const char *p = eol + 1;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        int hl = nl ? (int)(nl - p) : (int)(end - p);
        if (hl > 0 && p[hl - 1] == '\r') hl--;
        if (hl <= 0) break;
        if (hl < 1024) {
            char buf[1024];
            memcpy(buf, p, (size_t)hl);
            buf[hl] = 0;
            parse_header(buf, req);
        }
        if (!nl) break;
        p = nl + 1;
    }
    return 0;
}
