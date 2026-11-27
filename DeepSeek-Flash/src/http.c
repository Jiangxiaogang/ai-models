#include "http.h"
#include "util.h"

typedef struct {
    char method[16];
    char target[MAXPATH * 2];
    char url[MAXPATH * 2];
    char path[MAXPATH * 2];
    long long len;
    int expect100;
    int head;
} Request;

static const char PAGE_TOP[] =
    "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>文件共享</title><style>"
    "*{box-sizing:border-box}"
    "body{margin:0;font-family:system-ui,-apple-system,'Segoe UI','Microsoft YaHei',sans-serif;background:#f5f6f8;color:#2c3038}"
    ".wrap{max-width:980px;margin:0 auto;padding:24px 16px 64px}"
    "h1{font-size:20px;font-weight:600;margin:0 0 12px}"
    ".crumb{margin:0 0 16px;font-size:14px;color:#6b7280;word-break:break-all}"
    ".crumb a{color:#2563eb;text-decoration:none}"
    ".crumb a:hover{text-decoration:underline}"
    ".drop{border:2px dashed #c7ccd6;border-radius:12px;padding:26px;background:#fff;text-align:center;color:#6b7280;cursor:pointer;transition:background .2s,border-color .2s;user-select:none}"
    ".drop.over{border-color:#2563eb;background:#eff6ff;color:#2563eb}"
    ".drop strong{color:#2563eb;font-weight:600}"
    ".ups{margin:12px 0 20px}"
    ".up{background:#fff;border-radius:8px;padding:10px 12px;margin-bottom:8px;box-shadow:0 1px 3px rgba(0,0,0,.06)}"
    ".up .top{display:flex;justify-content:space-between;gap:12px;font-size:13px;margin-bottom:6px}"
    ".up .nm{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
    ".up .st{color:#6b7280;white-space:nowrap;font-variant-numeric:tabular-nums}"
    ".bar{height:6px;background:#e5e7eb;border-radius:3px;overflow:hidden}"
    ".bar i{display:block;height:100%;width:0;background:#2563eb;transition:width .15s linear}"
    "table{width:100%;border-collapse:collapse;background:#fff;border-radius:10px;overflow:hidden;box-shadow:0 1px 3px rgba(0,0,0,.06)}"
    "th,td{padding:11px 14px;text-align:left;font-size:14px;border-bottom:1px solid #f0f1f3}"
    "th{background:#fafbfc;color:#6b7280;font-weight:500}"
    "tbody tr:last-child td{border-bottom:0}"
    "tbody tr:hover td{background:#fafbff}"
    "td a{color:#2563eb;text-decoration:none}"
    "td a:hover{text-decoration:underline}"
    "a.dir{font-weight:600}"
    ".sz,.tm{color:#9aa1ac;white-space:nowrap;font-variant-numeric:tabular-nums}"
    ".empty{padding:40px;text-align:center;color:#9aa1ac}"
    "@media(max-width:600px){.tm{display:none}}"
    "</style></head><body><div class=\"wrap\"><h1>文件共享</h1><div class=\"crumb\">";

static const char PAGE_UPLOAD[] =
    "<input type=\"file\" id=\"picker\" multiple style=\"display:none\">"
    "<div class=\"drop\" id=\"drop\">拖拽文件到此处上传，或 <strong>点击选择文件</strong></div>"
    "<div class=\"ups\" id=\"ups\"></div>"
    "<table><thead><tr><th>名称</th><th>大小</th><th>修改时间</th></tr></thead><tbody>";

static const char PAGE_SCRIPT[] =
    "<script>(function(){"
    "var dir=location.pathname;if(dir.charAt(dir.length-1)!=='/')dir+='/';"
    "var picker=document.getElementById('picker');"
    "var drop=document.getElementById('drop');"
    "var ups=document.getElementById('ups');"
    "var queue=[],busy=false;"
    "function stop(e){e.preventDefault();e.stopPropagation();}"
    "function add(list){for(var i=0;i<list.length;i++)queue.push(list[i]);pump();}"
    "function pump(){if(busy||!queue.length)return;busy=true;"
    "var f=queue.shift();upload(f,function(){busy=false;"
    "if(!queue.length)setTimeout(function(){location.reload();},600);else pump();});}"
    "function fmt(n){if(n>=1073741824)return(n/1073741824).toFixed(1)+' GB';"
    "if(n>=1048576)return(n/1048576).toFixed(1)+' MB';"
    "if(n>=1024)return(n/1024).toFixed(1)+' KB';return Math.round(n)+' B';}"
    "function upload(file,done){"
    "var row=document.createElement('div');row.className='up';"
    "row.innerHTML='<div class=top><span class=nm></span><span class=st>准备中</span></div><div class=bar><i></i></div>';"
    "var nm=row.querySelector('.nm'),st=row.querySelector('.st'),bar=row.querySelector('.bar i');"
    "nm.textContent=file.name;ups.appendChild(row);"
    "var xhr=new XMLHttpRequest(),t0=Date.now(),lastT=t0,lastB=0,finished=false;"
    "function finish(){if(finished)return;finished=true;"
    "if(xhr.status>=200&&xhr.status<300){bar.style.width='100%';"
    "var sec=(Date.now()-t0)/1000;"
    "st.textContent='完成 '+fmt(file.size)+(sec>0?' · '+fmt(file.size/sec)+'/s':'');}"
    "else{st.textContent='失败 · '+(xhr.status?'HTTP '+xhr.status:'网络错误');}"
    "done();}"
    "xhr.open('PUT',dir+encodeURIComponent(file.name));"
    "xhr.upload.onprogress=function(e){if(!e.lengthComputable)return;"
    "var now=Date.now(),dt=(now-lastT)/1000;"
    "var speed=dt>0.02?(e.loaded-lastB)/dt:0;"
    "if(dt>0.02){lastT=now;lastB=e.loaded;}"
    "var pct=e.total?e.loaded*100/e.total:0;bar.style.width=pct.toFixed(1)+'%';"
    "st.textContent=pct.toFixed(1)+'% · '+fmt(e.loaded)+' / '+fmt(e.total)"
    "+(speed>0?' · '+fmt(speed)+'/s':'');};"
    "xhr.onload=finish;xhr.onerror=finish;xhr.send(file);}"
    "picker.addEventListener('change',function(){add(picker.files);picker.value='';});"
    "drop.addEventListener('click',function(){picker.click();});"
    "['dragenter','dragover'].forEach(function(t){drop.addEventListener(t,function(e){stop(e);drop.classList.add('over');});});"
    "drop.addEventListener('dragleave',function(e){stop(e);drop.classList.remove('over');});"
    "drop.addEventListener('drop',function(e){stop(e);drop.classList.remove('over');add(e.dataTransfer.files);});"
    "})();</script>";

static const char PAGE_BOTTOM[] = "</tbody></table></div>";
static const char PAGE_END[] = "</body></html>";

static const char *reason_of(int code) {
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 301: return "Moved Permanently";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 500: return "Internal Server Error";
    default: return "Error";
    }
}

static const char *mime_lookup(const char *ext) {
    static const struct {
        const char *e;
        const char *t;
    } tab[] = {
        {"html", "text/html; charset=utf-8"},
        {"htm", "text/html; charset=utf-8"},
        {"css", "text/css; charset=utf-8"},
        {"js", "application/javascript; charset=utf-8"},
        {"mjs", "application/javascript; charset=utf-8"},
        {"json", "application/json; charset=utf-8"},
        {"txt", "text/plain; charset=utf-8"},
        {"md", "text/markdown; charset=utf-8"},
        {"xml", "application/xml; charset=utf-8"},
        {"csv", "text/csv; charset=utf-8"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"webp", "image/webp"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"ogg", "audio/ogg"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"rar", "application/vnd.rar"},
        {"7z", "application/x-7z-compressed"},
        {"gz", "application/gzip"},
        {"tar", "application/x-tar"},
        {"exe", "application/vnd.microsoft.portable-executable"},
        {"wasm", "application/wasm"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"otf", "font/otf"}
    };
    size_t i;

    for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        if (strcmp(ext, tab[i].e) == 0) {
            return tab[i].t;
        }
    }
    return "application/octet-stream";
}

static void mime_of(const wchar_t *wpath, char *out, int cb) {
    const wchar_t *dot = wcsrchr(wpath, L'.');
    const wchar_t *slash = wcsrchr(wpath, L'\\');
    char ext[16];
    int i;

    if (!dot || (slash && dot < slash)) {
        snprintf(out, (size_t)cb, "%s", "application/octet-stream");
        return;
    }
    dot++;
    for (i = 0; dot[i] && i < 15; i++) {
        wchar_t c = dot[i];
        ext[i] = (c >= L'A' && c <= L'Z') ? (char)(c - L'A' + L'a') : (char)c;
    }
    ext[i] = 0;
    snprintf(out, (size_t)cb, "%s", mime_lookup(ext));
}

static void make_disposition(const wchar_t *wpath, int download, char *out, int cb) {
    const wchar_t *name = wcsrchr(wpath, L'\\');
    char u8[MAXPATH];
    char enc[MAXPATH * 3];
    char asc[MAXPATH];
    int i;

    if (name) {
        name++;
    } else {
        name = wpath;
    }
    if (!wide_to_utf8(name, u8, (int)sizeof u8)) {
        u8[0] = 0;
    }
    url_encode(u8, enc, (int)sizeof enc);
    for (i = 0; u8[i]; i++) {
        unsigned char ch = (unsigned char)u8[i];
        asc[i] = (ch < 0x20 || ch >= 0x7f || ch == '"' || ch == '\\' || ch == ';') ? '_' : (char)ch;
    }
    asc[i] = 0;
    snprintf(out, (size_t)cb, "%s; filename=\"%s\"; filename*=UTF-8''%s",
             download ? "attachment" : "inline", asc, enc);
}

static int send_response(Conn *c, int code, const char *ctype, const char *extra,
                         const char *body, int bodylen, int head) {
    char hdr[2048];
    int n;

    n = snprintf(hdr, sizeof hdr,
                 "HTTP/1.1 %d %s\r\n"
                 "Server: FileServer/1.0\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "%s"
                 "\r\n",
                 code, reason_of(code), ctype, bodylen, extra ? extra : "");
    if (n <= 0 || (size_t)n >= sizeof hdr) {
        return 0;
    }
    if (!conn_send(c, hdr, n)) {
        return 0;
    }
    if (!head && body && bodylen > 0) {
        return conn_send(c, body, bodylen);
    }
    return 1;
}

static void send_error(Conn *c, int code, const char *msg) {
    char esc[512];
    char body[2048];

    html_escape_to(msg, esc, (int)sizeof esc);
    snprintf(body, sizeof body,
             "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
             "<title>%d</title><style>"
             "body{margin:0;font-family:system-ui,'Microsoft YaHei',sans-serif;background:#f4f5f7;color:#2c3038}"
             ".b{max-width:440px;margin:15vh auto;background:#fff;border-radius:12px;padding:32px;"
             "box-shadow:0 4px 24px rgba(0,0,0,.07);text-align:center}"
             "h1{margin:0 0 8px;font-size:40px;color:#e05252}p{color:#666;margin:0 0 18px}"
             "a{color:#2563eb;text-decoration:none}"
             "</style></head><body><div class=\"b\"><h1>%d</h1><p>%s</p>"
             "<p><a href=\"/\">返回首页</a></p></div></body></html>",
             code, code, esc);
    send_response(c, code, "text/html; charset=utf-8", NULL, body, (int)strlen(body), 0);
}

static int serve_file(Conn *c, const wchar_t *wpath, int head, int download) {
    FILE *fp;
    long long size;
    char ctype[128];
    char disp[MAXPATH * 4];
    char hdr[2048];
    char *buf;
    int n;

    fp = _wfopen(wpath, L"rb");
    if (!fp) {
        send_error(c, 500, "无法打开文件");
        return 500;
    }
    _fseeki64(fp, 0, SEEK_END);
    size = _ftelli64(fp);
    _fseeki64(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        send_error(c, 500, "无法读取文件");
        return 500;
    }
    mime_of(wpath, ctype, (int)sizeof ctype);
    make_disposition(wpath, download, disp, (int)sizeof disp);
    n = snprintf(hdr, sizeof hdr,
                 "HTTP/1.1 200 OK\r\n"
                 "Server: FileServer/1.0\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %lld\r\n"
                 "Content-Disposition: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 ctype, size, disp);
    if (n <= 0 || (size_t)n >= sizeof hdr) {
        fclose(fp);
        send_error(c, 500, "响应头过长");
        return 500;
    }
    if (!conn_send(c, hdr, n)) {
        fclose(fp);
        return 200;
    }
    if (head) {
        fclose(fp);
        return 200;
    }
    buf = (char *)malloc(IOBUFSIZE);
    if (!buf) {
        fclose(fp);
        return 200;
    }
    for (;;) {
        size_t got = fread(buf, 1, IOBUFSIZE, fp);
        if (got == 0) {
            break;
        }
        if (!conn_send(c, buf, (int)got)) {
            break;
        }
    }
    free(buf);
    fclose(fp);
    return 200;
}

static void add_breadcrumb(Buf *b, const char *rel) {
    char acc[MAXPATH * 3];
    const char *p = rel;

    acc[0] = 0;
    buf_add(b, "<a href=\"/\">根目录</a>");
    while (*p) {
        char seg[MAXPATH];
        char enc[MAXPATH * 3];
        char esc[MAXPATH * 6];
        const char *q = p;
        size_t n;
        size_t cur;

        while (*p && *p != '\\') {
            p++;
        }
        n = (size_t)(p - q);
        if (n >= sizeof seg) {
            n = sizeof seg - 1;
        }
        memcpy(seg, q, n);
        seg[n] = 0;
        url_encode(seg, enc, (int)sizeof enc);
        cur = strlen(acc);
        snprintf(acc + cur, sizeof acc - cur, "%s%s", cur ? "/" : "", enc);
        html_escape_to(seg, esc, (int)sizeof esc);
        buf_addf(b, " / <a href=\"/%s/\">%s</a>", acc, esc);
        if (*p == '\\') {
            p++;
        }
    }
}

static int list_dir(Conn *c, const wchar_t *wdir, const char *rel, int head) {
    Buf b;
    wchar_t pattern[MAXPATH * 2];
    int pass;
    int any = 0;

    if (!buf_init(&b, 16384)) {
        send_error(c, 500, "内存不足");
        return 500;
    }
    buf_add(&b, PAGE_TOP);
    add_breadcrumb(&b, rel);
    buf_add(&b, "</div>");
    buf_add(&b, PAGE_UPLOAD);
    if (rel[0]) {
        buf_add(&b, "<tr><td><a class=\"dir\" href=\"../\">上级目录</a></td>"
                    "<td class=\"sz\">-</td><td class=\"tm\">-</td></tr>");
    }
    wcscpy(pattern, wdir);
    if (wcslen(pattern) + 2 < MAXPATH * 2) {
        wcscat(pattern, L"\\*");
    }
    for (pass = 0; pass < 2; pass++) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            break;
        }
        do {
            int isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            char name_u8[MAXPATH];
            char name_enc[MAXPATH * 3];
            char name_esc[MAXPATH * 6];
            char size_s[32];
            char time_s[32];
            long long fsize;

            if ((pass == 0) != (isdir != 0)) {
                continue;
            }
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
                continue;
            }
            if (!wide_to_utf8(fd.cFileName, name_u8, (int)sizeof name_u8)) {
                continue;
            }
            url_encode(name_u8, name_enc, (int)sizeof name_enc);
            html_escape_to(name_u8, name_esc, (int)sizeof name_esc);
            format_time(&fd.ftLastWriteTime, time_s, (int)sizeof time_s);
            if (isdir) {
                buf_addf(&b, "<tr><td><a class=\"dir\" href=\"%s/\">%s/</a></td>"
                             "<td class=\"sz\">-</td><td class=\"tm\">%s</td></tr>",
                         name_enc, name_esc, time_s);
            } else {
                fsize = ((long long)fd.nFileSizeHigh << 32) | (long long)fd.nFileSizeLow;
                format_size(fsize, size_s, (int)sizeof size_s);
                buf_addf(&b, "<tr><td><a href=\"%s\">%s</a></td>"
                             "<td class=\"sz\">%s</td><td class=\"tm\">%s</td></tr>",
                         name_enc, name_esc, size_s, time_s);
            }
            any = 1;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (!any) {
        buf_add(&b, "<tr><td class=\"empty\" colspan=\"3\">目录为空</td></tr>");
    }
    buf_add(&b, PAGE_BOTTOM);
    buf_add(&b, PAGE_SCRIPT);
    buf_add(&b, PAGE_END);
    if (!send_response(c, 200, "text/html; charset=utf-8", NULL, b.data, (int)b.len, head)) {
        buf_free(&b);
        return 200;
    }
    buf_free(&b);
    return 200;
}

static int mkdir_parent(const char *root, const char *rel) {
    char parent[MAXPATH];
    wchar_t wparent[MAXPATH * 2];
    char *slash = strrchr(rel, '\\');

    if (!slash) {
        return 1;
    }
    memcpy(parent, rel, (size_t)(slash - rel));
    parent[slash - rel] = 0;
    if (!parent[0]) {
        return 1;
    }
    if (!path_join(root, parent, wparent, (int)(MAXPATH * 2))) {
        return 0;
    }
    return dir_create_all(wparent);
}

static int handle_put(Conn *c, const Config *cfg, Request *r) {
    char rel[MAXPATH];
    wchar_t wpath[MAXPATH * 2];
    wchar_t wtmp[MAXPATH * 2 + 8];
    FILE *fp;
    char *buf;
    long long remain;
    int ok = 1;
    size_t ulen = strlen(r->path);

    if (!path_normalize(r->path, rel, (int)sizeof rel)) {
        send_error(c, 400, "非法路径");
        return 400;
    }
    if (!rel[0]) {
        send_error(c, 400, "上传目标不能是根目录");
        return 400;
    }
    if (ulen == 0 || r->path[ulen - 1] == '/') {
        send_error(c, 400, "上传目标必须是文件");
        return 400;
    }
    if (r->len < 0) {
        send_error(c, 411, "缺少 Content-Length");
        return 411;
    }
    if (!path_join(cfg->root, rel, wpath, (int)(MAXPATH * 2))) {
        send_error(c, 400, "路径过长");
        return 400;
    }
    if (is_directory(wpath)) {
        send_error(c, 409, "目标是一个目录");
        return 409;
    }
    if (!mkdir_parent(cfg->root, rel)) {
        send_error(c, 500, "无法创建目标目录");
        return 500;
    }
    wcscpy(wtmp, wpath);
    wcscat(wtmp, L".part");
    if (r->expect100) {
        if (!conn_send(c, "HTTP/1.1 100 Continue\r\n\r\n", 25)) {
            return 100;
        }
    }
    fp = _wfopen(wtmp, L"wb");
    if (!fp) {
        send_error(c, 500, "无法创建文件");
        return 500;
    }
    buf = (char *)malloc(IOBUFSIZE);
    if (!buf) {
        fclose(fp);
        _wremove(wtmp);
        send_error(c, 500, "内存不足");
        return 500;
    }
    remain = r->len;
    while (remain > 0) {
        int want = remain > IOBUFSIZE ? IOBUFSIZE : (int)remain;
        int got = conn_read(c, buf, want);
        if (got <= 0) {
            ok = 0;
            break;
        }
        if (fwrite(buf, 1, (size_t)got, fp) != (size_t)got) {
            ok = 0;
            break;
        }
        remain -= got;
    }
    free(buf);
    if (fclose(fp) != 0) {
        ok = 0;
    }
    if (!ok) {
        _wremove(wtmp);
        send_error(c, 400, "上传未完成");
        return 400;
    }
    if (!MoveFileExW(wtmp, wpath, MOVEFILE_REPLACE_EXISTING)) {
        _wremove(wtmp);
        send_error(c, 500, "保存文件失败");
        return 500;
    }
    {
        char body[128];
        int n = snprintf(body, sizeof body, "{\"ok\":true,\"size\":%lld}", r->len);
        send_response(c, 201, "application/json; charset=utf-8", NULL, body, n, 0);
    }
    return 201;
}

static int join_rel(const char *rel, const char *name, char *out, int cb) {
    int n;

    if (rel[0]) {
        n = snprintf(out, (size_t)cb, "%s\\%s", rel, name);
    } else {
        n = snprintf(out, (size_t)cb, "%s", name);
    }
    return n > 0 && n < cb;
}

static int handle_get(Conn *c, const Config *cfg, Request *r) {
    char rel[MAXPATH];
    wchar_t wpath[MAXPATH * 2];
    DWORD attr;
    size_t ulen = strlen(r->url);

    if (!path_normalize(r->path, rel, (int)sizeof rel)) {
        send_error(c, 400, "非法路径");
        return 400;
    }
    if (!path_join(cfg->root, rel, wpath, (int)(MAXPATH * 2))) {
        send_error(c, 400, "路径过长");
        return 400;
    }
    attr = GetFileAttributesW(wpath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        send_error(c, 404, "文件或目录不存在");
        return 404;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (ulen == 0 || r->url[ulen - 1] != '/') {
            char extra[MAXPATH * 2 + 32];
            char body[MAXPATH * 2 + 64];
            snprintf(extra, sizeof extra, "Location: %s/\r\n", r->url);
            snprintf(body, sizeof body, "<a href=\"%s/\">跳转</a>", r->url);
            send_response(c, 301, "text/html; charset=utf-8", extra, body, (int)strlen(body), r->head);
            return 301;
        }
        if (cfg->index_name[0]) {
            char relidx[MAXPATH];
            wchar_t widx[MAXPATH * 2];
            if (join_rel(rel, cfg->index_name, relidx, (int)sizeof relidx) &&
                path_join(cfg->root, relidx, widx, (int)(MAXPATH * 2)) &&
                file_exists(widx)) {
                return serve_file(c, widx, r->head, 0);
            }
        }
        return list_dir(c, wpath, rel, r->head);
    }
    return serve_file(c, wpath, r->head, 1);
}

static void parse_header(Request *r, const char *line) {
    const char *colon = strchr(line, ':');
    char name[64];
    const char *val;
    size_t n;

    if (!colon) {
        return;
    }
    n = (size_t)(colon - line);
    if (n >= sizeof name) {
        n = sizeof name - 1;
    }
    memcpy(name, line, n);
    name[n] = 0;
    val = colon + 1;
    while (*val == ' ' || *val == '\t') {
        val++;
    }
    if (str_ieq(name, "Content-Length")) {
        r->len = _strtoi64(val, NULL, 10);
        if (r->len < 0) {
            r->len = -1;
        }
    } else if (str_ieq(name, "Expect") && _strnicmp(val, "100-continue", 12) == 0) {
        r->expect100 = 1;
    }
}

static int split_request_line(Request *r, char *line) {
    char *p = line;
    char *m = p;
    char *t;

    while (*p && *p != ' ') {
        p++;
    }
    if (!*p) {
        return 0;
    }
    *p++ = 0;
    t = p;
    while (*p && *p != ' ') {
        p++;
    }
    *p = 0;
    if (!m[0] || !t[0]) {
        return 0;
    }
    snprintf(r->method, sizeof r->method, "%s", m);
    snprintf(r->target, sizeof r->target, "%s", t);
    return 1;
}

static int read_request(Conn *c, Request *r) {
    char line[MAXLINE];
    int n;
    int count = 0;

    memset(r, 0, sizeof *r);
    r->len = -1;
    n = conn_getline(c, line, (int)sizeof line);
    if (n <= 0) {
        return 0;
    }
    if (!split_request_line(r, line)) {
        return -1;
    }
    for (;;) {
        n = conn_getline(c, line, (int)sizeof line);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            break;
        }
        parse_header(r, line);
        if (++count > 100) {
            return -1;
        }
    }
    return 1;
}

static int parse_target(Request *r) {
    char *q;

    if (r->target[0] != '/') {
        return 0;
    }
    snprintf(r->url, sizeof r->url, "%s", r->target);
    q = strchr(r->url, '?');
    if (q) {
        *q = 0;
    }
    q = strchr(r->url, '#');
    if (q) {
        *q = 0;
    }
    snprintf(r->path, sizeof r->path, "%s", r->url);
    url_decode(r->path);
    for (q = r->path; *q; q++) {
        if (*q == '\\') {
            *q = '/';
        }
    }
    return 1;
}

int http_handle(Conn *c, const Config *cfg) {
    Request r;
    int rc = read_request(c, &r);

    if (rc <= 0) {
        return rc;
    }
    if (strcmp(r.method, "HEAD") == 0) {
        r.head = 1;
    } else if (strcmp(r.method, "GET") != 0 && strcmp(r.method, "PUT") != 0) {
        send_response(c, 405, "text/html; charset=utf-8", "Allow: GET, HEAD, PUT\r\n",
                      "Method Not Allowed", 18, 0);
        return 405;
    }
    if (!parse_target(&r)) {
        send_error(c, 400, "请求路径无效");
        return 400;
    }
    if (strcmp(r.method, "PUT") == 0) {
        return handle_put(c, cfg, &r);
    }
    return handle_get(c, cfg, &r);
}
