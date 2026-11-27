#include "hfs_http.h"
#include "hfs.h"
#include "hfs_fs.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <windows.h>

#define READ_TIMEOUT_MS   30000
#define MAX_BODY_SIZE     (512LL * 1024 * 1024)

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    size_t consumed;
} req_stream;

static int rs_init(req_stream *rs)
{
    rs->capacity = 16384;
    rs->data = (char *)malloc(rs->capacity);
    if (!rs->data) return -1;
    rs->length = 0;
    rs->consumed = 0;
    rs->data[0] = '\0';
    return 0;
}

static void rs_free(req_stream *rs)
{
    if (rs->data) free(rs->data);
    rs->data = NULL;
    rs->length = 0;
    rs->capacity = 0;
    rs->consumed = 0;
}

static int rs_grow(req_stream *rs, size_t need)
{
    if (rs->length + need + 1 > rs->capacity) {
        size_t cap = rs->capacity;
        char *tmp;
        while (rs->length + need + 1 > cap) cap *= 2;
        tmp = (char *)realloc(rs->data, cap);
        if (!tmp) return -1;
        rs->data = tmp;
        rs->capacity = cap;
    }
    return 0;
}

static void rs_compact(req_stream *rs)
{
    if (rs->consumed == 0) return;
    memmove(rs->data, rs->data + rs->consumed, rs->length - rs->consumed);
    rs->length -= rs->consumed;
    rs->consumed = 0;
    rs->data[rs->length] = '\0';
}

static int rs_recv_more(req_stream *rs, hfs_socket *client)
{
    int n;
    if (rs->consumed > 0 && rs->length - rs->consumed < 8192) {
        rs_compact(rs);
    }
    if (rs_grow(rs, 8192) != 0) return -1;
    n = hfs_net_recv(client, rs->data + rs->length, (int)(rs->capacity - rs->length - 1),
                     READ_TIMEOUT_MS);
    if (n <= 0) return n;
    rs->length += (size_t)n;
    rs->data[rs->length] = '\0';
    return n;
}

static char *rs_find(req_stream *rs, const char *marker)
{
    if (rs->consumed >= rs->length) return NULL;
    return strstr(rs->data + rs->consumed, marker);
}

static void log_msg(hfs_server *server, const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (server->log) {
        time_t t = time(NULL);
        struct tm tmv;
        char ts[64];
        if (localtime_s(&tmv, &t) == 0) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
        else strcpy(ts, "-");
        fprintf(server->log, "[%s] %s\n", ts, buf);
        fflush(server->log);
    }
    if (server->verbose) {
        printf("%s\n", buf);
        fflush(stdout);
    }
}

static void send_all_or_close(hfs_socket *client, const char *buf, int size)
{
    hfs_net_send_all(client, buf, size);
}

static int g_keep_alive = 0;

static void send_response(hfs_socket *client, int status, const char *reason, const char *ctype,
                          const char *extra, const char *body, size_t body_len){
    char header[2048];
    int n;
    n = snprintf(header, sizeof(header),
                 "HTTP/1.1 %d %s\r\n"
                 "Server: hfs/%s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %u\r\n"
                 "Connection: %s\r\n"
                 "%s"
                 "\r\n",
                 status, reason, HFS_VERSION, ctype, (unsigned int)body_len,
                 g_keep_alive ? "keep-alive" : "close", extra ? extra : "");
    if (n <= 0) return;
    send_all_or_close(client, header, n);
    if (body && body_len > 0) send_all_or_close(client, body, (int)body_len);
}

static void send_error(hfs_socket *client, int status, const char *reason, const char *message)
{
    char body[4096];
    char esc[1024];
    int n;
    hfs_util_escape_html(message, esc, sizeof(esc));
    n = snprintf(body, sizeof(body),
                 "<!DOCTYPE html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
                 "<title>%d %s</title><style>"
                 "body{margin:0;padding:48px;font-family:'Segoe UI',Tahoma,Arial,'Microsoft YaHei',sans-serif;"
                 "background:#f4f6f8;color:#22313f}"
                 ".box{max-width:520px;margin:0 auto;background:#fff;border-radius:10px;padding:32px;"
                 "box-shadow:0 2px 14px rgba(20,40,70,.08)}"
                 "h1{margin:0 0 10px;font-size:22px;color:#d93025}p{margin:0 0 18px;color:#55606b;line-height:1.7}"
                 "a{color:#1f6feb;text-decoration:none}a:hover{text-decoration:underline}"
                 "</style></head><body><div class=\"box\"><h1>%d %s</h1><p>%s</p>"
                 "<p><a href=\"javascript:history.back()\">Go back</a></p></div></body></html>\n",
                 status, reason, status, reason, esc);
    if (n > 0) send_response(client, status, reason, "text/html; charset=utf-8", NULL, body, (size_t)n);
}

static int parse_request_line(char *line, hfs_request *req)
{
    char *sp1, *sp2;
    memset(req, 0, sizeof(*req));
    sp1 = strchr(line, ' ');
    if (!sp1) return -1;
    *sp1 = '\0';
    sp2 = strchr(sp1 + 1, ' ');
    if (!sp2) return -1;
    *sp2 = '\0';
    strncpy(req->method, line, sizeof(req->method) - 1);
    strncpy(req->target, sp1 + 1, sizeof(req->target) - 1);
    strncpy(req->version, sp2 + 1, sizeof(req->version) - 1);
    if (req->method[0] == '\0' || req->target[0] == '\0') return -1;
    return 0;
}

static void parse_headers(char *headers, char *headers_end, hfs_request *req)
{
    char *line = headers;
    while (line && line < headers_end && *line && req->header_count < HFS_MAX_HEADERS) {
        char *eol = strstr(line, "\r\n");
        char *colon;
        char *next;
        if (!eol || eol > headers_end) break;
        *eol = '\0';
        next = eol + 2;
        colon = strchr(line, ':');
        if (colon) {
            char *value;
            *colon = '\0';
            value = colon + 1;
            while (*value == ' ' || *value == '\t') value++;
            strncpy(req->headers[req->header_count].name, line, sizeof(req->headers[0].name) - 1);
            strncpy(req->headers[req->header_count].value, value, sizeof(req->headers[0].value) - 1);
            req->header_count++;
        }
        line = next;
    }
}

static const char *find_header(const hfs_request *req, const char *name)
{
    int i;
    for (i = 0; i < req->header_count; i++) {
        if (_stricmp(req->headers[i].name, name) == 0) return req->headers[i].value;
    }
    return NULL;
}

static int split_url(const char *target, char *path, size_t path_size, char *query, size_t query_size)
{
    const char *q = strchr(target, '?');
    const char *hash = strchr(target, '#');
    size_t len;

    if (hash) {
        len = (size_t)(hash - target);
    } else {
        len = strlen(target);
    }
    if (q && (hash == NULL || q < hash)) {
        len = (size_t)(q - target);
    }
    if (len + 1 > path_size) return -1;
    memcpy(path, target, len);
    path[len] = '\0';

    if (query && query_size > 0) {
        query[0] = '\0';
        if (q) {
            const char *end = (hash && hash > q) ? hash : q + strlen(q);
            size_t qlen = (size_t)(end - q - 1);
            if (qlen + 1 > query_size) qlen = query_size - 1;
            memcpy(query, q + 1, qlen);
            query[qlen] = '\0';
        }
    }
    return 0;
}

static int decode_path(const char *raw, char *dst, size_t dst_size)
{
    char tmp[HFS_MAX_URL];
    if (hfs_util_url_decode(raw, tmp, sizeof(tmp)) != 0) return -1;
    if (hfs_util_normalize_path(tmp, dst, dst_size) != 0) return -1;
    return 0;
}

static void mime_of(const char *path, char *dst, size_t dst_size)
{
    hfs_util_mime_type(path, dst, dst_size);
}

static void send_file(hfs_socket *client, const char *path, const hfs_request *req)
{
    FILE *f;
    long long size = 0;
    char ctype[128];
    char header[1024];
    int n;
    long long offset = 0;
    long long remaining;
    int partial = 0;
    int head_only = (_stricmp(req->method, "HEAD") == 0);
    const char *range = find_header(req, "Range");
    char buf[64 * 1024];

    if (hfs_util_file_size(path, &size) != 0) {
        send_error(client, 500, "Internal Server Error", "Cannot stat file");
        return;
    }
    f = fopen(path, "rb");
    if (!f) {
        send_error(client, 403, "Forbidden", "Cannot open file");
        return;
    }
    mime_of(path, ctype, sizeof(ctype));

    if (range && range[0] == 'b') {
        const char *eq = strchr(range, '=');
        if (eq) {
            long long start = 0, end = size - 1;
            const char *dash = strchr(eq + 1, '-');
            if (dash) {
                char sbuf[32];
                size_t slen = (size_t)(dash - (eq + 1));
                if (slen > 0 && slen < sizeof(sbuf)) {
                    memcpy(sbuf, eq + 1, slen);
                    sbuf[slen] = '\0';
                    start = _strtoi64(sbuf, NULL, 10);
                    if (dash[1]) end = _strtoi64(dash + 1, NULL, 10);
                    else end = size - 1;
                    if (start >= 0 && end < size && end >= start) {
                        partial = 1;
                        offset = start;
                    }
                }
            }
        }
    }

    remaining = size - offset;
    if (partial) {
        n = snprintf(header, sizeof(header),
                     "HTTP/1.1 206 Partial Content\r\n"
                     "Server: hfs/%s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lld\r\n"
                     "Content-Range: bytes %lld-%lld/%lld\r\n"
                     "Accept-Ranges: bytes\r\n"
                     "Connection: %s\r\n\r\n",
                     HFS_VERSION, ctype, remaining, offset, offset + remaining - 1, size,
                     g_keep_alive ? "keep-alive" : "close");
    } else {
        n = snprintf(header, sizeof(header),
                     "HTTP/1.1 200 OK\r\n"
                     "Server: hfs/%s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lld\r\n"
                     "Accept-Ranges: bytes\r\n"
                     "Connection: %s\r\n\r\n",
                     HFS_VERSION, ctype, size, g_keep_alive ? "keep-alive" : "close");
    }
    send_all_or_close(client, header, n);

    if (!head_only && remaining > 0) {
        if (offset > 0 && fseek(f, (long)offset, SEEK_SET) != 0) {
            fclose(f);
            return;
        }
        while (remaining > 0) {
            size_t want = sizeof(buf);
            size_t got;
            if ((long long)want > remaining) want = (size_t)remaining;
            got = fread(buf, 1, want, f);
            if (got == 0) break;
            if (hfs_net_send_all(client, buf, (int)got) < 0) break;
            remaining -= (long long)got;
        }
    }
    fclose(f);
}

static void send_directory(hfs_server *server, hfs_socket *client, const char *url_path,
                           const char *fs_path)
{
    hfs_buffer buf;
    char index_path[HFS_MAX_PATH * 2];
    char ctype[128];
    hfs_request head_req;

    if (server->config.index_file[0]) {
        if (hfs_util_path_join(fs_path, server->config.index_file, index_path, sizeof(index_path)) == 0 &&
            !hfs_util_is_directory(index_path) && hfs_util_exists(index_path)) {
            memset(&head_req, 0, sizeof(head_req));
            send_file(client, index_path, &head_req);
            return;
        }
    }
    if (!server->config.auto_index) {
        send_error(client, 403, "Forbidden", "Directory listing is disabled");
        return;
    }
    if (hfs_fs_build_index(&server->config, url_path, fs_path, &buf) != 0) {
        send_error(client, 500, "Internal Server Error", "Cannot build directory index");
        return;
    }
    mime_of("index.html", ctype, sizeof(ctype));
    send_response(client, 200, "OK", ctype, NULL, buf.data, buf.length);
    hfs_fs_buffer_free(&buf);
}

static int parse_multipart_boundary(const char *ctype, char *dst, size_t dst_size)
{
    const char *p = strstr(ctype, "boundary=");
    size_t len = 0;
    if (!p) return -1;
    p += 9;
    if (*p == '"') p++;
    while (*p && *p != '"' && *p != ';' && *p != '\r' && *p != '\n') {
        if (len + 1 >= dst_size) return -1;
        dst[len++] = *p++;
    }
    dst[len] = '\0';
    if (len == 0) return -1;
    return 0;
}

static void sanitize_filename(const char *name, char *dst, size_t dst_size)
{
    size_t i, len = strlen(name);
    size_t out = 0;
    if (len == 0) {
        strcpy(dst, "upload.bin");
        return;
    }
    for (i = 0; i < len; i++) {
        char c = name[i];
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|') {
            c = '_';
        }
        if (out + 1 >= dst_size) break;
        dst[out++] = c;
    }
    while (out > 0 && (dst[out - 1] == ' ' || dst[out - 1] == '.')) out--;
    if (out == 0) {
        strcpy(dst, "upload.bin");
        return;
    }
    dst[out] = '\0';
}

static void format_speed(double bytes_per_sec, char *dst, size_t dst_size)
{
    static const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    double v = bytes_per_sec;
    int u = 0;
    while (v >= 1024.0 && u < 3) {
        v /= 1024.0;
        u++;
    }
    if (bytes_per_sec <= 0.0) snprintf(dst, dst_size, "--");
    else if (u == 0) snprintf(dst, dst_size, "%.0f %s", v, units[u]);
    else snprintf(dst, dst_size, "%.2f %s", v, units[u]);
}

static void send_progress_page(hfs_socket *client, const char *filename, long long total,
                               const char *status_text, int done, long long received,
                               const char *redirect)
{
    char body[16384];
    char esc_name[512];
    double percent = total > 0 ? ((double)received * 100.0 / (double)total) : 100.0;
    char speedbuf[32];
    int n;

    hfs_util_escape_html(filename, esc_name, sizeof(esc_name));
    format_speed(0.0, speedbuf, sizeof(speedbuf));

    n = snprintf(body, sizeof(body),
                 "<!DOCTYPE html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>Upload progress</title><style>"
                 "*{box-sizing:border-box}"
                 "body{margin:0;padding:32px;font-family:'Segoe UI',Tahoma,Arial,'Microsoft YaHei',sans-serif;"
                 "background:#f4f6f8;color:#22313f}"
                 ".box{max-width:680px;margin:0 auto;background:#fff;border-radius:10px;padding:28px;"
                 "box-shadow:0 2px 14px rgba(20,40,70,.08)}"
                 "h1{margin:0 0 6px;font-size:19px;font-weight:600}"
                 ".sub{margin:0 0 20px;font-size:13px;color:#8b96a3;word-break:break-all}"
                 ".bar{height:16px;background:#e9edf2;border-radius:8px;overflow:hidden}"
                 ".fill{height:100%%;background:#1f6feb;border-radius:8px;transition:width .2s}"
                 ".fill.done{background:#28a745}"
                 ".row{display:flex;justify-content:space-between;margin-top:10px;font-size:13px;color:#55606b;"
                 "gap:12px;flex-wrap:wrap}"
                 ".grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 18px;margin-top:14px;font-size:13px}"
                 ".grid span{color:#8b96a3}.grid b{color:#22313f;font-weight:600}"
                 ".st{margin-top:18px;padding:12px 14px;border-radius:8px;font-size:14px}"
                 ".st.ok{background:#e7f6ec;color:#1e7b34}"
                 ".st.err{background:#fdecea;color:#c5221f}"
                 ".st.run{background:#e8f1fd;color:#1a56c4}"
                 "a{color:#1f6feb;text-decoration:none}a:hover{text-decoration:underline}"
                 "</style></head><body><div class=\"box\">"
                 "<h1>Uploading %s</h1><p class=\"sub\">%s</p>"
                 "<div class=\"bar\"><div class=\"fill%s\" id=\"fill\" style=\"width:%.2f%%\"></div></div>"
                 "<div class=\"row\"><span id=\"pct\">%.2f%%</span>"
                 "<span id=\"num\">%lld / %lld bytes</span></div>"
                 "<div class=\"grid\">"
                 "<div><span>Speed</span> <b id=\"spd\">%s</b></div>"
                 "<div><span>Elapsed</span> <b id=\"eta\">0.0 s</b></div>"
                 "</div>"
                 "<div class=\"st %s\" id=\"st\">%s</div>",
                 esc_name, esc_name, done ? " done" : "", percent, percent, received, total,
                 speedbuf, done ? "ok" : "run", status_text);

    if (n < 0 || (size_t)n >= sizeof(body)) return;

    if (done) {
        if (redirect) {
            char esc_redirect[1024];
            hfs_util_escape_html(redirect, esc_redirect, sizeof(esc_redirect));
            n += snprintf(body + n, sizeof(body) - (size_t)n,
                          "<p style=\"margin-top:18px;font-size:14px\"><a href=\"%s\">Back to listing</a></p>",
                          esc_redirect);
        }
    } else {
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "<script>"
                      "(function(){"
                      "var f=document.getElementById('fill'),p=document.getElementById('pct'),"
                      "n=document.getElementById('num'),s=document.getElementById('spd'),"
                      "e=document.getElementById('eta'),st=document.getElementById('st');"
                      "var t0=Date.now(),last=0,lastT=t0;"
                      "window.hfsProgress=function(loaded,total){"
                      "var now=Date.now(),dt=(now-lastT)/1000,db=loaded-last;"
                      "if(dt>0.15){var v=db/dt;s.textContent=(v/1024).toFixed(2)+' KB/s';}"
                      "last=loaded;lastT=now;"
                      "var pc=total>0?(loaded*100/total):100;"
                      "f.style.width=pc.toFixed(2)+'%%';p.textContent=pc.toFixed(2)+'%%';"
                      "n.textContent=loaded.toLocaleString()+' / '+total.toLocaleString()+' bytes';"
                      "e.textContent=((now-t0)/1000).toFixed(1)+' s';};"
                      "window.hfsDone=function(msg){st.className='st ok';st.textContent=msg;"
                      "f.className='fill done';f.style.width='100%%';"
                      "p.textContent='100.00%%';e.textContent=((Date.now()-t0)/1000).toFixed(1)+' s';};"
                      "window.hfsFail=function(msg){st.className='st err';st.textContent=msg;};"
                      "})();"
                      "</script>");
    }
    n += snprintf(body + n, sizeof(body) - (size_t)n, "</div></body></html>\n");
    if (n < 0) return;
    send_response(client, 200, "OK", "text/html; charset=utf-8", "Cache-Control: no-store\r\n",
                  body, (size_t)n);
}

static void send_live_upload_page(hfs_socket *client, const char *url_path)
{
    char body[16384];
    char esc_path[1024];
    int n;

    hfs_util_escape_html(url_path, esc_path, sizeof(esc_path));

    n = snprintf(body, sizeof(body),
                 "<!DOCTYPE html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>Upload</title><style>"
                 "*{box-sizing:border-box}"
                 "body{margin:0;padding:32px;font-family:'Segoe UI',Tahoma,Arial,'Microsoft YaHei',sans-serif;"
                 "background:#f4f6f8;color:#22313f}"
                 ".box{max-width:680px;margin:0 auto;background:#fff;border-radius:10px;padding:28px;"
                 "box-shadow:0 2px 14px rgba(20,40,70,.08)}"
                 "h1{margin:0 0 6px;font-size:19px;font-weight:600}"
                 ".sub{margin:0 0 20px;font-size:13px;color:#8b96a3;word-break:break-all}"
                 "label{display:block;margin-bottom:10px;font-size:14px}"
                 "input[type=file]{display:block;width:100%%;padding:10px;border:1px dashed #c3cbd6;"
                 "border-radius:8px;background:#fafbfc;font-size:13px}"
                 "button{margin-top:16px;padding:10px 22px;border:0;border-radius:6px;background:#1f6feb;"
                 "color:#fff;font-size:14px;cursor:pointer}"
                 "button:disabled{background:#a9bdd6;cursor:not-allowed}"
                 ".bar{height:16px;background:#e9edf2;border-radius:8px;overflow:hidden;margin-top:22px;"
                 "display:none}"
                 ".bar.show{display:block}"
                 ".fill{height:100%%;background:#1f6feb;border-radius:8px;transition:width .15s;width:0}"
                 ".fill.done{background:#28a745}"
                 ".row{display:flex;justify-content:space-between;margin-top:10px;font-size:13px;color:#55606b;"
                 "gap:12px;flex-wrap:wrap}"
                 ".grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 18px;margin-top:14px;font-size:13px;"
                 "display:none}"
                 ".grid.show{display:grid}"
                 ".grid span{color:#8b96a3}.grid b{color:#22313f;font-weight:600}"
                 ".st{margin-top:18px;padding:12px 14px;border-radius:8px;font-size:14px;display:none}"
                 ".st.show{display:block}"
                 ".st.ok{background:#e7f6ec;color:#1e7b34}"
                 ".st.err{background:#fdecea;color:#c5221f}"
                 ".st.run{background:#e8f1fd;color:#1a56c4}"
                 "a{color:#1f6feb;text-decoration:none}a:hover{text-decoration:underline}"
                 "</style></head><body><div class=\"box\">"
                 "<h1>Upload</h1><p class=\"sub\">Target: %s</p>"
                 "<form id=\"f\" method=\"post\" enctype=\"multipart/form-data\">"
                 "<label>Select a file</label>"
                 "<input type=\"file\" name=\"file\" id=\"pick\" required>"
                 "<button type=\"submit\" id=\"go\">Upload</button></form>"
                 "<div class=\"bar\" id=\"bar\"><div class=\"fill\" id=\"fill\"></div></div>"
                 "<div class=\"row\"><span id=\"pct\">0.00%%</span>"
                 "<span id=\"num\">0 / 0 bytes</span></div>"
                 "<div class=\"grid\" id=\"grid\">"
                 "<div><span>Speed</span> <b id=\"spd\">--</b></div>"
                 "<div><span>Elapsed</span> <b id=\"eta\">0.0 s</b></div>"
                 "<div><span>Size</span> <b id=\"size\">--</b></div>"
                 "<div><span>Remaining</span> <b id=\"rem\">--</b></div>"
                 "</div>"
                 "<div class=\"st\" id=\"st\"></div>"
                 "<script>"
                 "(function(){"
                 "var f=document.getElementById('f'),pick=document.getElementById('pick'),"
                 "go=document.getElementById('go'),bar=document.getElementById('bar'),"
                 "fill=document.getElementById('fill'),pct=document.getElementById('pct'),"
                 "num=document.getElementById('num'),spd=document.getElementById('spd'),"
                 "eta=document.getElementById('eta'),size=document.getElementById('size'),"
                 "rem=document.getElementById('rem'),grid=document.getElementById('grid'),"
                 "st=document.getElementById('st');"
                 "function fmt(b){return b.toLocaleString();}"
                 "f.addEventListener('submit',function(ev){"
                 "  ev.preventDefault();"
                 "  if(!pick.files.length){st.className='st show err';st.textContent='Please select a file.';return;}"
                 "  var file=pick.files[0],fd=new FormData();fd.append('file',file,file.name);"
                 "  var xhr=new XMLHttpRequest();xhr.open('POST','%s',true);"
                 "  xhr.upload.onprogress=function(e){"
                 "    if(!e.lengthComputable)return;"
                 "    var now=Date.now();"
                 "    if(window._lt===undefined){window._lt=now;window._lb=0;}"
                 "    var dt=(now-window._lt)/1000,db=e.loaded-window._lb;"
                 "    if(dt>0.2){var v=db/dt;spd.textContent=(v/1024).toFixed(2)+' KB/s';"
                 "      window._lt=now;window._lb=e.loaded;}"
                 "    var pc=e.loaded*100/e.total;"
                 "    fill.style.width=pc.toFixed(2)+'%%';"
                 "    pct.textContent=pc.toFixed(2)+'%%';"
                 "    num.textContent=fmt(e.loaded)+' / '+fmt(e.total)+' bytes';"
                 "    bar.classList.add('show');grid.classList.add('show');"
                 "    size.textContent=fmt(e.total)+' bytes';"
                 "    rem.textContent=(e.total-e.loaded)>0?fmt(e.total-e.loaded)+' bytes':'0 bytes';"
                 "    eta.textContent=((now-window._t0)/1000).toFixed(1)+' s';"
                 "  };"
                 "  xhr.onload=function(){"
                 "    st.className='st show '+(xhr.status>=200&&xhr.status<300?'ok':'err');"
                 "    st.textContent=(xhr.status>=200&&xhr.status<300)?"
                 "      'Upload completed successfully.':'Upload failed (HTTP '+xhr.status+').';"
                 "    if(xhr.status>=200&&xhr.status<300){fill.className='fill done';fill.style.width='100%%';"
                 "      pct.textContent='100.00%%';}"
                 "    go.disabled=false;};"
                 "  xhr.onerror=function(){st.className='st show err';st.textContent='Network error.';go.disabled=false;};"
                 "  window._t0=Date.now();"
                 "  bar.classList.add('show');grid.classList.add('show');"
                 "  st.className='st show run';st.textContent='Uploading...';"
                 "  go.disabled=true;xhr.send(fd);"
                 "});"
                 "})();"
                 "</script></div></body></html>\n",
                 esc_path, esc_path);

    if (n < 0 || (size_t)n >= sizeof(body)) return;
    send_response(client, 200, "OK", "text/html; charset=utf-8", "Cache-Control: no-store\r\n",
                  body, (size_t)n);
}

static void send_upload_error(hfs_socket *client, const char *filename, const char *message){
    char body[8192];
    char esc_name[512];
    char esc_msg[1024];
    int n;

    hfs_util_escape_html(filename, esc_name, sizeof(esc_name));
    hfs_util_escape_html(message, esc_msg, sizeof(esc_msg));

    n = snprintf(body, sizeof(body),
                 "<!DOCTYPE html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
                 "<title>Upload failed</title><style>"
                 "body{margin:0;padding:32px;font-family:'Segoe UI',Tahoma,Arial,'Microsoft YaHei',sans-serif;"
                 "background:#f4f6f8;color:#22313f}"
                 ".box{max-width:640px;margin:0 auto;background:#fff;border-radius:10px;padding:28px;"
                 "box-shadow:0 2px 14px rgba(20,40,70,.08)}"
                 "h1{margin:0 0 6px;font-size:19px;font-weight:600;color:#d93025}"
                 ".sub{margin:0 0 20px;font-size:13px;color:#8b96a3;word-break:break-all}"
                 ".st{margin-top:18px;padding:12px 14px;border-radius:8px;font-size:14px;"
                 "background:#fdecea;color:#c5221f}"
                 "a{color:#1f6feb;text-decoration:none}a:hover{text-decoration:underline}"
                 "</style></head><body><div class=\"box\">"
                 "<h1>Upload failed</h1><p class=\"sub\">%s</p>"
                 "<div class=\"st\">%s</div>"
                 "<p style=\"margin-top:18px;font-size:14px\"><a href=\"javascript:history.back()\">Go back</a></p>"
                 "</div></body></html>\n",
                 esc_name, esc_msg);
    if (n <= 0) return;
    send_response(client, 400, "Bad Request", "text/html; charset=utf-8", NULL, body, (size_t)n);
}

static int ensure_parent_dir(const char *path)
{
    char dir[HFS_MAX_PATH * 2];
    char *slash;
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    slash = strrchr(dir, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (dir[0] == '\0') return 0;
    CreateDirectoryA(dir, NULL);
    return 0;
}

static void handle_put(hfs_server *server, hfs_socket *client, hfs_request *req, req_stream *rs,
                       const char *url_path)
{
    char fs_path[HFS_MAX_PATH * 2];
    char target[HFS_MAX_PATH * 2];
    FILE *f = NULL;
    long long remaining;
    long long received = 0;
    const char *overwrite = find_header(req, "X-Overwrite");
    int allow_overwrite = (overwrite == NULL || _stricmp(overwrite, "0") != 0);
    char name_only[512];
    char esc_name[512];
    const char *slash;

    if (!server->config.enable_upload) {
        send_error(client, 403, "Forbidden", "Upload is disabled");
        return;
    }
    if (!req->has_content_length || req->content_length < 0) {
        send_error(client, 411, "Length Required", "Content-Length is required for PUT");
        return;
    }
    if (server->config.max_upload_size > 0 && req->content_length > server->config.max_upload_size) {
        send_error(client, 413, "Payload Too Large", "Upload exceeds max_upload_size");
        return;
    }
    if (req->content_length > MAX_BODY_SIZE) {
        send_error(client, 413, "Payload Too Large", "Upload body too large");
        return;
    }
    if (hfs_util_path_to_absolute(server->config.document_root, url_path, fs_path, sizeof(fs_path)) != 0) {
        send_error(client, 400, "Bad Request", "Invalid path");
        return;
    }
    if (hfs_util_is_directory(fs_path)) {
        send_error(client, 409, "Conflict", "Target is a directory");
        return;
    }
    if (!allow_overwrite && hfs_util_exists(fs_path)) {
        send_error(client, 409, "Conflict", "File already exists");
        return;
    }
    if (hfs_util_path_join(server->config.document_root, url_path, target, sizeof(target)) != 0) {
        send_error(client, 400, "Bad Request", "Invalid path");
        return;
    }
    ensure_parent_dir(target);

    slash = strrchr(url_path, '/');
    strncpy(name_only, slash ? slash + 1 : url_path, sizeof(name_only) - 1);
    name_only[sizeof(name_only) - 1] = '\0';
    if (name_only[0] == '\0') strcpy(name_only, "upload.bin");
    hfs_util_escape_html(name_only, esc_name, sizeof(esc_name));

    f = fopen(target, "wb");
    if (!f) {
        send_upload_error(client, esc_name, "Cannot create the target file");
        return;
    }

    remaining = req->content_length;
    while (remaining > 0) {
        size_t avail = rs->length - rs->consumed;
        size_t take;
        if (avail == 0) {
            int n = rs_recv_more(rs, client);
            if (n <= 0) break;
            continue;
        }
        take = avail;
        if ((long long)take > remaining) take = (size_t)remaining;
        if (fwrite(rs->data + rs->consumed, 1, take, f) != take) break;
        rs->consumed += take;
        remaining -= (long long)take;
        received += (long long)take;
    }
    fclose(f);

    if (received != req->content_length) {
        send_upload_error(client, esc_name, "Incomplete upload, the connection was interrupted");
        return;
    }
    log_msg(server, "PUT %s -> %s (%lld bytes)", url_path, target, received);
    send_progress_page(client, name_only, req->content_length, "Upload completed successfully.",
                       1, received, ".");
}

static void handle_multipart(hfs_server *server, hfs_socket *client, hfs_request *req, req_stream *rs,
                              const char *url_path, int use_part_name)
{
    const char *ctype = find_header(req, "Content-Type");
    char boundary[256];
    char delimiter[300];
    char filename[512];
    char target[HFS_MAX_PATH * 2];
    char name_only[512];
    char esc_name[512];
    FILE *f;
    long long total = req->content_length;
    long long received = 0;
    char *body = NULL;
    size_t body_len = 0;
    char *boundary_pos;
    char *headers_end;
    char *disp;
    char *name_start;
    char *name_end;

    if (!server->config.enable_upload) {
        send_error(client, 403, "Forbidden", "Upload is disabled");
        return;
    }
    if (!req->has_content_length || total < 0) {
        send_error(client, 411, "Length Required", "Content-Length is required");
        return;
    }
    if (!ctype || parse_multipart_boundary(ctype, boundary, sizeof(boundary)) != 0) {
        send_error(client, 400, "Bad Request", "Invalid multipart request");
        return;
    }
    snprintf(delimiter, sizeof(delimiter), "--%s", boundary);

    body = (char *)malloc((size_t)total + 2);
    if (!body) {
        send_error(client, 500, "Internal Server Error", "Out of memory");
        return;
    }

    while (received < total) {
        size_t avail = rs->length - rs->consumed;
        size_t take;
        if (avail == 0) {
            int n = rs_recv_more(rs, client);
            if (n <= 0) break;
            continue;
        }
        take = avail;
        if ((long long)take > total - received) take = (size_t)(total - received);
        memcpy(body + body_len, rs->data + rs->consumed, take);
        body_len += take;
        rs->consumed += take;
        received += (long long)take;
    }
    body[body_len] = '\0';

    if (received != total) {
        free(body);
        send_error(client, 400, "Bad Request", "Incomplete multipart body");
        return;
    }

    boundary_pos = strstr(body, delimiter);
    if (!boundary_pos) {
        free(body);
        send_error(client, 400, "Bad Request", "Malformed multipart body");
        return;
    }
    headers_end = strstr(boundary_pos, "\r\n\r\n");
    if (!headers_end) {
        free(body);
        send_error(client, 400, "Bad Request", "Malformed multipart body");
        return;
    }
    disp = strstr(boundary_pos, "filename=");
    if (!disp || disp > headers_end) {
        free(body);
        send_error(client, 400, "Bad Request", "No file part found in multipart body");
        return;
    }
    name_start = disp + 9;
    if (*name_start == '"') name_start++;
    name_end = name_start;
    while (*name_end && *name_end != '"' && *name_end != '\r' && *name_end != '\n') name_end++;
    {
        size_t n = (size_t)(name_end - name_start);
        if (n >= sizeof(filename)) n = sizeof(filename) - 1;
        memcpy(filename, name_start, n);
        filename[n] = '\0';
    }
    sanitize_filename(filename, name_only, sizeof(name_only));
    hfs_util_escape_html(name_only, esc_name, sizeof(esc_name));

    {
        char rel[HFS_MAX_URL];
        if (use_part_name) {
            size_t base = strlen(url_path);
            while (base > 1 && url_path[base - 1] == '/') base--;
            if (base == 0 || (base == 1 && url_path[0] == '/')) {
                snprintf(rel, sizeof(rel), "/%s", name_only);
            } else {
                snprintf(rel, sizeof(rel), "%.*s/%s", (int)base, url_path, name_only);
            }
        } else {
            strncpy(rel, url_path, sizeof(rel) - 1);
            rel[sizeof(rel) - 1] = '\0';
        }
        if (hfs_util_path_join(server->config.document_root, rel, target, sizeof(target)) != 0) {
            free(body);
            send_upload_error(client, esc_name, "Invalid upload path");
            return;
        }
    }
    ensure_parent_dir(target);

    f = fopen(target, "wb");
    if (!f) {
        free(body);
        send_upload_error(client, esc_name, "Cannot create the target file");
        return;
    }
    {
        char *content = headers_end + 4;
        char *next = strstr(content, delimiter);
        long long written = 0;
        if (next) {
            size_t len = (size_t)(next - content);
            if (len >= 2) len -= 2;
            written = (long long)fwrite(content, 1, len, f);
        }
        fclose(f);
        log_msg(server, "POST upload %s -> %s (%lld bytes)", name_only, target, written);
        send_progress_page(client, name_only, total, "Upload completed successfully.", 1, written, ".");
    }
    free(body);
}

static void handle_get(hfs_server *server, hfs_socket *client, hfs_request *req, const char *url_path)
{
    char fs_path[HFS_MAX_PATH * 2];

    if (!server->config.enable_download) {
        send_error(client, 403, "Forbidden", "Download is disabled");
        return;
    }
    if (hfs_util_path_to_absolute(server->config.document_root, url_path, fs_path, sizeof(fs_path)) != 0) {
        send_error(client, 400, "Bad Request", "Invalid path");
        return;
    }
    if (hfs_util_is_directory(fs_path)) {
        send_directory(server, client, url_path, fs_path);
        return;
    }
    if (!hfs_util_exists(fs_path)) {
        send_error(client, 404, "Not Found", "File not found");
        return;
    }
    send_file(client, fs_path, req);
}

void hfs_http_handle(hfs_server *server, hfs_socket *client)
{
    req_stream rs;
    hfs_request req;
    char url_path[HFS_MAX_URL];
    char query[HFS_MAX_URL];
    int served = 0;

    if (rs_init(&rs) != 0) return;
    g_keep_alive = 0;

    for (;;) {
        char *head_end;
        char *header_block;
        char *body_start;
        char *line_end;

        while (rs.consumed < rs.length) {
            head_end = rs_find(&rs, "\r\n\r\n");
            if (head_end) break;
            if (rs_recv_more(&rs, client) <= 0) {
                rs_free(&rs);
                return;
            }
        }
        if (rs.consumed >= rs.length) {
            if (rs_recv_more(&rs, client) <= 0) {
                rs_free(&rs);
                return;
            }
            head_end = rs_find(&rs, "\r\n\r\n");
            if (!head_end) {
                rs_free(&rs);
                return;
            }
        }

        head_end = rs_find(&rs, "\r\n\r\n");
        if (!head_end) {
            rs_free(&rs);
            return;
        }
        header_block = rs.data + rs.consumed;
        body_start = head_end + 4;

        line_end = strstr(header_block, "\r\n");
        if (!line_end) {
            send_error(client, 400, "Bad Request", "Malformed request line");
            break;
        }
        *line_end = '\0';
        if (parse_request_line(header_block, &req) != 0) {
            send_error(client, 400, "Bad Request", "Malformed request line");
            break;
        }
        *line_end = '\r';
        line_end[1] = '\n';
        parse_headers(line_end + 2, head_end, &req);

        {
            const char *cl = find_header(&req, "Content-Length");
            if (cl) {
                req.content_length = _strtoi64(cl, NULL, 10);
                req.has_content_length = 1;
            }
        }

        {
            const char *conn = find_header(&req, "Connection");
            int client_ka = (conn && _stricmp(conn, "keep-alive") == 0);
            int server_ka = (strcmp(req.version, "HTTP/1.0") != 0);
            req.keep_alive = client_ka && server_ka;
            g_keep_alive = req.keep_alive;
        }

        {
            const char *expect = find_header(&req, "Expect");
            if (expect && _stricmp(expect, "100-continue") == 0) {
                send_all_or_close(client, "HTTP/1.1 100 Continue\r\n\r\n", 25);
            }
        }

        if (split_url(req.target, url_path, sizeof(url_path), query, sizeof(query)) != 0 ||
            decode_path(url_path, url_path, sizeof(url_path)) != 0) {
            send_error(client, 400, "Bad Request", "Malformed request target");
            break;
        }

        log_msg(server, "%s %s from %s:%d", req.method, req.target, hfs_net_peer_addr(client),
                hfs_net_peer_port(client));

        if (_stricmp(req.method, "GET") == 0 || _stricmp(req.method, "HEAD") == 0) {
            const char *base = strrchr(url_path, '/');
            if (base && _stricmp(base + 1, "__upload") == 0) {
                if (!server->config.enable_upload) {
                    send_error(client, 403, "Forbidden", "Upload is disabled");
                } else {
                    send_live_upload_page(client, url_path);
                }
            } else {
                handle_get(server, client, &req, url_path);
            }
        } else if (_stricmp(req.method, "PUT") == 0) {
            rs.consumed = (size_t)(body_start - rs.data);
            handle_put(server, client, &req, &rs, url_path);
        } else if (_stricmp(req.method, "POST") == 0) {
            char post_path[HFS_MAX_URL];
            int use_part_name = 0;
            strncpy(post_path, url_path, sizeof(post_path) - 1);
            post_path[sizeof(post_path) - 1] = '\0';
            {
                char *base = strrchr(post_path, '/');
                if (base && _stricmp(base + 1, "__upload") == 0) {
                    *base = '\0';
                    if (post_path[0] == '\0') strcpy(post_path, "/");
                    use_part_name = 1;
                }
            }
            rs.consumed = (size_t)(body_start - rs.data);
            handle_multipart(server, client, &req, &rs, post_path, use_part_name);
        } else if (_stricmp(req.method, "DELETE") == 0) {
            char fs_path[HFS_MAX_PATH * 2];
            if (hfs_util_path_to_absolute(server->config.document_root, url_path, fs_path,
                                           sizeof(fs_path)) != 0) {
                send_error(client, 400, "Bad Request", "Invalid path");
            } else if (!hfs_util_exists(fs_path)) {
                send_error(client, 404, "Not Found", "File not found");
            } else if (remove(fs_path) != 0) {
                send_error(client, 500, "Internal Server Error", "Cannot delete file");
            } else {
                send_response(client, 200, "OK", "text/plain; charset=utf-8", NULL, "Deleted\n", 9);
            }
        } else {
            send_error(client, 501, "Not Implemented", "Method not supported");
        }

        served++;

        if (!req.keep_alive) break;

        {
            size_t body_off = (size_t)(body_start - rs.data);
            long long body_len = req.has_content_length ? req.content_length : 0;
            size_t end = body_off + (size_t)body_len;
            if (end > rs.length) end = rs.length;
            rs.consumed = end;
        }
    }

    g_keep_alive = 0;
    rs_free(&rs);
    (void)served;
}

int hfs_http_init(hfs_server *server, const hfs_config *cfg, FILE *log)
{
    memset(server, 0, sizeof(*server));
    server->config = *cfg;
    server->log = log;
    server->verbose = 0;
    return 0;
}
