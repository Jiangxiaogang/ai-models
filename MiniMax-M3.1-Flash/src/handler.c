#include "handler.h"

#include "net.h"
#include "listing.h"
#include "upload.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_request(const http_request *req, int code)
{
    char stamp[32];
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d", (int)st.wHour, (int)st.wMinute, (int)st.wSecond);
    printf("[%s] %s %s -> %d\n", stamp, req->method, req->path, code);
    fflush(stdout);
}

static int find_index(const char *dir, char *out, size_t cap)
{
    static const char *names[] = {"index.html", "index.htm"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char candidate[UTIL_PATH_MAX];
        if (path_join(candidate, sizeof(candidate), dir, names[i]) != 0)
            continue;
        if (file_exists(candidate) && !file_is_dir(candidate)) {
            snprintf(out, cap, "%s", candidate);
            return 0;
        }
    }
    return -1;
}

static int serve_file(conn *c, const http_request *req, const char *disk, int head_only)
{
    long long size = file_size(disk);
    if (size < 0)
        return http_send_error(c, 404, "文件不存在");
    long long start = 0;
    long long end = size - 1;
    int code = 200;
    char extra[256];
    extra[0] = '\0';
    if (req->has_range) {
        if (req->range_start >= size && size > 0) {
            char range[128];
            snprintf(range, sizeof(range), "Content-Range: bytes */%lld\r\n", size);
            http_send_head(c, 416, "text/plain; charset=utf-8", 0, range);
            return 0;
        }
        start = req->range_start;
        end = (req->range_end < 0) ? size - 1 : req->range_end;
        if (end > size - 1)
            end = size - 1;
        if (end < start)
            end = start;
        code = 206;
        snprintf(extra, sizeof(extra),
                 "Content-Range: bytes %lld-%lld/%lld\r\nAccept-Ranges: bytes\r\n", start, end, size);
    } else {
        snprintf(extra, sizeof(extra), "Accept-Ranges: bytes\r\n");
    }
    char date[64];
    if (fmt_http_date(file_mtime(disk), date, sizeof(date)) != 0)
        snprintf(date, sizeof(date), "-");
    char head_extra[512];
    snprintf(head_extra, sizeof(head_extra), "%sLast-Modified: %s\r\n", extra, date);
    long long length = size > 0 ? end - start + 1 : 0;
    if (http_send_head(c, code, mime_of(disk), length, head_extra) != 0)
        return -1;
    if (head_only || length <= 0)
        return 0;
    FILE *fp = NULL;
    if (fopen_s(&fp, disk, "rb") != 0 || !fp)
        return -1;
    char *buf = (char *)malloc(HTTP_BODY_CHUNK);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (start > 0 && _fseeki64(fp, start, SEEK_SET) != 0) {
        free(buf);
        fclose(fp);
        return -1;
    }
    long long left = length;
    int ok = 1;
    while (left > 0) {
        size_t want = (left < (long long)HTTP_BODY_CHUNK) ? (size_t)left : (size_t)HTTP_BODY_CHUNK;
        size_t got = fread(buf, 1, want, fp);
        if (got == 0) {
            ok = 0;
            break;
        }
        if (conn_send_all(c, buf, got) != 0) {
            ok = 0;
            break;
        }
        left -= (long long)got;
    }
    free(buf);
    fclose(fp);
    return ok ? 0 : -1;
}

static int serve_buffer(conn *c, int code, const char *ctype, const char *data, size_t len, int head_only)
{
    if (http_send_head(c, code, ctype, (long long)len, NULL) != 0)
        return -1;
    if (head_only || len == 0)
        return 0;
    return conn_send_all(c, data, len);
}

static int serve_upload_page(conn *c, const app_config *cfg, int head_only)
{
    sbuf page;
    if (sbuf_init(&page) != 0)
        return http_send_error(c, 500, "内存不足");
    upload_page_build(cfg, &page);
    int rc = serve_buffer(c, 200, "text/html; charset=utf-8", page.data, page.len, head_only);
    sbuf_free(&page);
    return rc;
}

static int serve_listing(conn *c, const char *url_path, const char *disk, int head_only)
{
    sbuf page;
    if (sbuf_init(&page) != 0)
        return http_send_error(c, 500, "内存不足");
    listing_build(url_path, disk, &page);
    int rc = serve_buffer(c, 200, "text/html; charset=utf-8", page.data, page.len, head_only);
    sbuf_free(&page);
    return rc;
}

typedef struct {
    char rel[UTIL_PATH_MAX];
    char disk[UTIL_PATH_MAX];
    char index[UTIL_PATH_MAX];
    char location[UTIL_PATH_MAX];
    char extra[UTIL_PATH_MAX + 32];
} work_ctx;

static int serve_get(conn *c, const http_request *req, const app_config *cfg)
{
    int head_only = strcmp(req->method, "HEAD") == 0;
    const char *path = req->path;
    if (strcmp(path, "/upload.html") == 0 || strcmp(path, "/upload") == 0)
        return serve_upload_page(c, cfg, head_only);
    work_ctx *w = (work_ctx *)calloc(1, sizeof(work_ctx));
    if (!w)
        return http_send_error(c, 500, "内存不足");
    int rc;
    if (path_safe(path, w->rel, sizeof(w->rel)) != 0) {
        rc = http_send_error(c, 400, "非法的请求路径");
    } else if (path_join(w->disk, sizeof(w->disk), cfg->root, w->rel) != 0) {
        rc = http_send_error(c, 400, "路径过长");
    } else if (file_is_dir(w->disk)) {
        size_t plen = strlen(path);
        if (plen == 0 || path[plen - 1] != '/') {
            snprintf(w->location, sizeof(w->location), "%s/", path);
            snprintf(w->extra, sizeof(w->extra), "Location: %s\r\n", w->location);
            rc = http_send_head(c, 301, "text/plain; charset=utf-8", 0, w->extra);
        } else if (cfg->index_auto && find_index(w->disk, w->index, sizeof(w->index)) == 0) {
            rc = serve_file(c, req, w->index, head_only);
        } else if (!cfg->index_auto) {
            rc = http_send_error(c, 403, "目录索引已关闭");
        } else {
            rc = serve_listing(c, path, w->disk, head_only);
        }
    } else if (!file_exists(w->disk)) {
        rc = http_send_error(c, 404, "文件不存在");
    } else {
        rc = serve_file(c, req, w->disk, head_only);
    }
    free(w);
    return rc;
}

static int route(conn *c, const http_request *req, const app_config *cfg)
{
    if (strcmp(req->method, "GET") == 0 || strcmp(req->method, "HEAD") == 0)
        return serve_get(c, req, cfg);
    if (strcmp(req->method, "PUT") == 0) {
        if (strcmp(req->path, "/upload") == 0)
            return upload_handle_put(c, req, cfg);
        return http_send_error(c, 404, "未知的上传地址");
    }
    if (strcmp(req->method, "OPTIONS") == 0) {
        return http_send_head(c, 204, "text/plain; charset=utf-8", 0, "Allow: GET, HEAD, PUT, OPTIONS\r\n");
    }
    return http_send_error(c, 405, "不支持的请求方法");
}

int handle_client(SOCKET s, const app_config *cfg)
{
    conn *c = (conn *)malloc(sizeof(conn));
    http_request *req = (http_request *)malloc(sizeof(http_request));
    if (!c || !req) {
        free(c);
        free(req);
        return -1;
    }
    conn_init(c, s);
    net_set_timeout(s, 30);
    int rc = -1;
    if (http_parse_request(c, req) != 0) {
        http_send_error(c, 400, "请求格式错误");
    } else {
        log_request(req, 200);
        rc = route(c, req, cfg);
    }
    free(req);
    free(c);
    return rc;
}

unsigned __stdcall client_thread(void *arg)
{
    client_ctx *ctx = (client_ctx *)arg;
    handle_client(ctx->s, &ctx->cfg);
    net_close(ctx->s);
    free(ctx);
    return 0;
}
