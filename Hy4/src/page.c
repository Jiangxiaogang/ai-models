#include "common.h"
#include <stdarg.h>

typedef struct {
    char *p;
    size_t len;
    size_t cap;
} Buf;

static int buf_init(Buf *b)
{
    b->cap = 32768;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (!b->p) return 0;
    b->p[0] = 0;
    return 1;
}

static int buf_add(Buf *b, const char *fmt, ...)
{
    va_list ap;
    int n;
    size_t need;
    char *np;
    va_start(ap, fmt);
    n = _vscprintf(fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    need = b->len + (size_t)n + 1;
    if (need > b->cap) {
        while (b->cap < need) b->cap *= 2;
        np = (char *)realloc(b->p, b->cap);
        if (!np) return 0;
        b->p = np;
    }
    va_start(ap, fmt);
    vsnprintf_s(b->p + b->len, b->cap - b->len, _TRUNCATE, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
    return 1;
}

static void buf_free(Buf *b)
{
    free(b->p);
    b->p = NULL;
    b->len = 0;
    b->cap = 0;
}

static const char *css =
    "*{box-sizing:border-box}body{margin:0;padding:24px;background:#f5f6f8;color:#222;"
    "font-family:system-ui,-apple-system,'Segoe UI',sans-serif}"
    ".box{max-width:960px;margin:0 auto;background:#fff;border:1px solid #e3e5e8;"
    "border-radius:10px;padding:24px;box-shadow:0 2px 10px rgba(0,0,0,.05)}"
    "h1{font-size:20px;margin:0 0 4px}a{color:#1a6fd4;text-decoration:none}"
    "a:hover{text-decoration:underline}.muted{color:#888;font-size:13px;margin-bottom:18px}"
    "table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:9px 8px;"
    "border-bottom:1px solid #eee;font-size:14px}th{color:#666;font-weight:600;font-size:12px}"
    "td.sz,td.tm{color:#777;white-space:nowrap}.up{border-top:1px solid #eee;margin-top:22px;padding-top:20px}"
    ".row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}"
    "input[type=file]{flex:1;min-width:200px}button{border:0;background:#1a6fd4;color:#fff;"
    "padding:9px 18px;border-radius:6px;cursor:pointer;font-size:14px}button:disabled{background:#aaa;cursor:not-allowed}"
    ".bar{height:16px;background:#e9edf2;border-radius:8px;overflow:hidden;margin-top:14px}"
    ".fill{height:100%;width:0;background:linear-gradient(90deg,#1a6fd4,#37b24d);transition:width .15s}"
    "#stat{margin-top:10px;font-size:13px;color:#444;line-height:1.7}"
    ".ok{color:#2b8a3e}.err{color:#c92a2a}";

static const char *js_upload =
    "var DIR=%s;"
    "function fmt(b){var u=['B','KB','MB','GB','TB'],i=0,v=b;"
    "while(v>=1024&&i<4){v/=1024;i++}return (i?v.toFixed(1):v)+' '+u[i]}"
    "function up(){var fi=document.getElementById('f').files[0];"
    "if(!fi){alert('请先选择文件');return}"
    "var btn=document.getElementById('b'),bar=document.getElementById('bar'),"
    "st=document.getElementById('stat'),url=DIR.replace(/\\/$/,'')+'/'+encodeURIComponent(fi.name);"
    "btn.disabled=true;var t0=Date.now(),last=t0,lastB=0;"
    "var x=new XMLHttpRequest();x.open('PUT',url,true);"
    "x.setRequestHeader('Content-Type','application/octet-stream');"
    "x.upload.onprogress=function(e){if(!e.lengthComputable)return;"
    "var now=Date.now(),dt=(now-last)/1000,db=e.loaded-lastB,sp=dt>0?db/dt:0;"
    "var el=(now-t0)/1000,avg=el>0?e.loaded/el:0,p=e.loaded/e.total*100;"
    "bar.style.width=p.toFixed(1)+'%%';"
    "st.innerHTML='进度 '+p.toFixed(1)+'%% ('+fmt(e.loaded)+' / '+fmt(e.total)+')<br>'"
    "+'速度 '+fmt(sp)+'/s　平均 '+fmt(avg)+'/s';"
    "last=now;lastB=e.loaded};"
    "x.onload=function(){btn.disabled=false;"
    "if(x.status>=200&&x.status<300){bar.style.width='100%%';st.className='ok';"
    "st.innerHTML='上传完成：'+fmt(fi.size)+'，耗时 '+((Date.now()-t0)/1000).toFixed(1)+' 秒。';"
    "setTimeout(function(){location.reload()},900)}"
    "else{st.className='err';st.innerHTML='上传失败：HTTP '+x.status+' '+x.responseText}};"
    "x.onerror=function(){btn.disabled=false;st.className='err';st.innerHTML='网络错误'};"
    "x.send(fi)}";

static int is_index(const char *name)
{
    return util_ieq(name, "index.html") || util_ieq(name, "index.htm");
}

static void page_header(Buf *b, const char *title, const char *extra_head)
{
    buf_add(b,
        "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>%s - CFileServer</title><style>%s</style>%s</head>"
        "<body><div class=\"box\">",
        title, css, extra_head ? extra_head : "");
}

static int js_string(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)src;
    while (*p && o + 8 < cap) {
        switch (*p) {
        case '\\': memcpy(dst + o, "\\\\", 2); o += 2; break;
        case '"':  memcpy(dst + o, "\\\"", 2); o += 2; break;
        case '\'': memcpy(dst + o, "\\'", 2); o += 2; break;
        case '<':  memcpy(dst + o, "\\x3c", 4); o += 4; break;
        case '>':  memcpy(dst + o, "\\x3e", 4); o += 4; break;
        case '&':  memcpy(dst + o, "\\x26", 4); o += 4; break;
        case '\r': memcpy(dst + o, "\\r", 2); o += 2; break;
        case '\n': memcpy(dst + o, "\\n", 2); o += 2; break;
        case '\t': memcpy(dst + o, "\\t", 2); o += 2; break;
        default:
            if (*p < 0x20) { o += (size_t)sprintf_s(dst + o, cap - o, "\\x%02x", *p); }
            else dst[o++] = (char)*p;
            break;
        }
        p++;
    }
    dst[o] = 0;
    return (int)o;
}

void page_index(SOCKET s, const Request *req, const Config *cfg)
{
    char dir[MAX_PATH * 2];
    char pattern[MAX_PATH * 2];
    char idx[MAX_PATH * 2];
    char dec[2048];
    char escdir[4096];
    char jsdir[4096];
    char escname[2048];
    char href[4096];
    char name[MAX_PATH * 3];
    char szbuf[64];
    char tmstr[64];
    Buf b;
    WIN32_FIND_DATAW fd;
    wchar_t wpattern[MAX_PATH * 2];
    HANDLE h;
    long long fsize = 0;
    int has_index = 0;
    ResponseHead rh;

    if (util_url_decode(dec, sizeof(dec), req->raw_path) < 0) {
        http_send_error(s, 400, "Bad Request");
        return;
    }
    if (strstr(dec, "..") != NULL) {
        http_send_error(s, 403, "Forbidden");
        return;
    }
    if (!util_resolve_safe(dir, sizeof(dir), cfg->root, dec)) {
        http_send_error(s, 403, "Forbidden");
        return;
    }
    util_path_join(idx, sizeof(idx), dir, "index.html");
    if (util_file_size(idx, &fsize)) {
        has_index = 1;
    } else {
        util_path_join(idx, sizeof(idx), dir, "index.htm");
        if (util_file_size(idx, &fsize)) has_index = 1;
    }
    if (has_index) {
        http_send_file(s, idx, fsize);
        return;
    }
    if (!util_is_dir(dir)) {
        http_send_error(s, 404, "Not Found");
        return;
    }

    util_escape_html(escdir, sizeof(escdir), dec);
    js_string(jsdir, sizeof(jsdir), dec);
    if (!buf_init(&b)) {
        http_send_error(s, 500, "Internal Server Error");
        return;
    }
    page_header(&b, escdir, NULL);
    buf_add(&b, "<h1>目录：%s</h1><div class=\"muted\">CFileServer / 文件列表</div>", escdir);
    buf_add(&b, "<table><tr><th>名称</th><th>大小</th><th>修改时间</th></tr>");

    util_path_join(pattern, sizeof(pattern), dir, "*");
    if (util_u8_to_w(pattern, wpattern, MAX_PATH * 2)) {
        h = FindFirstFileW(wpattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                int isdir;
                long long size;
                char timestr[64];
                if (!util_w_to_u8(fd.cFileName, name, sizeof(name))) continue;
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
                isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                size = ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                util_escape_html(escname, sizeof(escname), name);
                {
                    char base[4096];
                    char ehref[4096];
                    sprintf_s(base, sizeof(base), "%s%s%s",
                        dec, (dec[0] && dec[strlen(dec) - 1] == '/') ? "" : "/", name);
                    util_escape_attr(ehref, sizeof(ehref), base);
                    strcpy_s(href, sizeof(href), ehref);
                }
                {
                    SYSTEMTIME utc, loc;
                    struct tm tmv;
                    time_t tt;
                    FileTimeToSystemTime(&fd.ftLastWriteTime, &utc);
                    SystemTimeToTzSpecificLocalTime(NULL, &utc, &loc);
                    memset(&tmv, 0, sizeof(tmv));
                    tmv.tm_year = loc.wYear - 1900;
                    tmv.tm_mon = loc.wMonth - 1;
                    tmv.tm_mday = loc.wDay;
                    tmv.tm_hour = loc.wHour;
                    tmv.tm_min = loc.wMinute;
                    tmv.tm_sec = loc.wSecond;
                    tmv.tm_isdst = -1;
                    tt = mktime(&tmv);
                    util_time_str(timestr, sizeof(timestr), tt);
                }
                if (isdir) strcpy_s(szbuf, sizeof(szbuf), "&lt;目录&gt;");
                else util_size_str(szbuf, sizeof(szbuf), size);
                strcpy_s(tmstr, sizeof(tmstr), timestr);
                buf_add(&b,
                    "<tr><td><a href=\"%s\">%s%s</a></td><td class=\"sz\">%s</td><td class=\"tm\">%s</td></tr>",
                    href, escname, isdir ? "/" : "", szbuf, tmstr);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }

    {
        char script[8192];
        sprintf_s(script, sizeof(script), js_upload, jsdir);
        buf_add(&b,
            "</table><div class=\"up\">"
            "<h1 style=\"font-size:16px\">上传文件</h1>"
            "<div class=\"row\"><input type=\"file\" id=\"f\">"
            "<button id=\"b\" onclick=\"up()\">上传到本目录</button></div>"
            "<div class=\"bar\"><div class=\"fill\" id=\"bar\"></div></div>"
            "<div id=\"stat\">选择文件后点击上传。</div></div>"
            "<script>%s</script></div></body></html>", script);
    }
    rh.status = 200;
    rh.reason = "OK";
    rh.type = "text/html; charset=utf-8";
    rh.length = (long long)b.len;
    rh.no_body = 0;
    rh.extra = NULL;
    http_send_head(s, &rh);
    http_send_all(s, b.p, (long long)b.len);
    buf_free(&b);
}

int page_upload(SOCKET s, Request *req, const Config *cfg, const char *head, long long head_len, long long buffered)
{
    char dec[2048];
    char dir[MAX_PATH * 2];
    char rel[2048];
    char name[MAX_PATH * 3];
    char target[MAX_PATH * 2];
    char escname[2048];
    char body[1024];
    long long received = 0;
    int aborted = 0;
    int r;

    if (util_url_decode(dec, sizeof(dec), req->raw_path) < 0) {
        http_send_error(s, 400, "Bad Request");
        return 0;
    }
    if (strstr(dec, "..") != NULL || strchr(dec, ':') != NULL) {
        http_send_error(s, 403, "Forbidden");
        return 0;
    }
    util_base_name(name, sizeof(name), dec);
    if (!*name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        http_send_error(s, 400, "Bad Request");
        return 0;
    }
    strcpy_s(rel, sizeof(rel), dec);
    {
        char *slash = strrchr(rel, '/');
        char *bslash = strrchr(rel, '\\');
        char *cut = (slash && slash > bslash) ? slash : bslash;
        if (cut) {
            if (cut == rel) rel[1] = 0;
            else *cut = 0;
        } else {
            rel[0] = 0;
        }
    }
    if (!util_resolve_safe(dir, sizeof(dir), cfg->root, rel)) {
        http_send_error(s, 403, "Forbidden");
        return 0;
    }
    util_path_join(target, sizeof(target), dir, name);
    strcpy_s(req->path, sizeof(req->path), target);

    r = http_recv_body(s, req, head, head_len, buffered, &received, &aborted);
    if (r < 0) {
        http_send_error(s, 500, "Internal Server Error");
        return 0;
    }
    if (r == 0) {
        http_send_error(s, 400, "Bad Request");
        return 0;
    }
    util_escape_html(escname, sizeof(escname), name);
    sprintf_s(body, sizeof(body),
        "{\"ok\":true,\"name\":\"%s\",\"size\":%lld}", escname, received);
    return http_send_text(s, 201, "Created", "application/json; charset=utf-8", body);
}
