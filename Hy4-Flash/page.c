#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "page.h"
#include "http.h"
#include "util.h"

static char *app(char *p, char *end, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (p >= end) return p;
    va_start(ap, fmt);
    n = _vsnprintf_s(p, (size_t)(end - p), _TRUNCATE, fmt, ap);
    va_end(ap);
    return n < 0 ? end : p + n;
}

static const char UPLOAD_UI[] =
"<hr><h2>Upload</h2><form id=f><input type=file id=file name=file>"
"<button type=submit>Upload</button></form>"
"<div id=bar style=\"width:460px;height:18px;border:1px solid #ccc;margin:14px 0\">"
"<div id=fill style=\"height:100%;width:0;background:#4c8bf5\"></div></div>"
"<div id=msg style=color:#555>Ready.</div>"
"<script>"
"var f=document.getElementById('f'),file=document.getElementById('file');"
"f.onsubmit=function(e){e.preventDefault();"
"if(!file.files.length){alert('choose a file');return;}"
"var name=file.files[0].name;"
"var x=new XMLHttpRequest();x.open('PUT',location.href+encodeURIComponent(name),true);"
"x.setRequestHeader('Content-Type','application/octet-stream');"
"var t0=0,last=0,lastB=0,speed=0;"
"x.upload.onprogress=function(ev){"
"if(!ev.lengthComputable)return;"
"var now=Date.now();if(!t0)t0=now;"
"var pct=ev.loaded/ev.total*100;"
"document.getElementById('fill').style.width=pct.toFixed(1)+'%';"
"if(now-last>=300){speed=(ev.loaded-lastB)/((now-last)/1000);last=now;lastB=ev.loaded;}"
"var mb=function(b){return (b/1048576).toFixed(2)+' MB';};"
"document.getElementById('msg').textContent="
"mb(ev.loaded)+' / '+mb(ev.total)+' '+pct.toFixed(1)+'% '+mb(speed)+'/s '"
"+(speed>0?((ev.total-ev.loaded)/speed).toFixed(0)+'s left':'');"
"};"
"x.onload=function(){document.getElementById('msg').textContent="
"x.status==200||x.status==201?'Upload complete.':'Error '+x.status;};"
"x.onerror=function(){document.getElementById('msg').textContent='Network error';};"
"x.send(file.files[0]);};"
"</script>";

static void html_escape(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    while (*src && i + 6 < cap) {
        switch (*src) {
            case '&': memcpy(dst + i, "&amp;", 5); i += 5; break;
            case '<': memcpy(dst + i, "&lt;", 4); i += 4; break;
            case '>': memcpy(dst + i, "&gt;", 4); i += 4; break;
            case '"': memcpy(dst + i, "&quot;", 6); i += 6; break;
            default: dst[i++] = *src; break;
        }
        src++;
    }
    dst[i] = 0;
}

void page_upload(SOCKET s)
{
    static char body[8192];
    char *p = body, *end = body + sizeof(body);
    p = app(p, end, "<!doctype html><html><head><meta charset=utf-8><title>Upload</title>"
        "<style>body{font:14px Arial;margin:40px;color:#333}h1{font-size:22px}</style>"
        "</head><body><h1>File Upload</h1>");
    size_t n = strlen(UPLOAD_UI);
    if (p + n < end) { memcpy(p, UPLOAD_UI, n); p += n; }
    http_head(s, 200, "OK", "text/html; charset=utf-8", (long long)(p - body), NULL);
    http_send_all(s, body, (int)(p - body));
}

void page_index(SOCKET s, const char *url, const char *path)
{
    static char body[262144];
    char *p = body, *end = body + sizeof(body);
    WIN32_FIND_DATAA fdata;
    char pattern[MAX_PATH_LEN];
    size_t ulen = strlen(url);
    char slash = ulen ? url[ulen - 1] : '/';
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*", path);
    HANDLE h = FindFirstFileA(pattern, &fdata);
    p = app(p, end, "<!doctype html><html><head><meta charset=utf-8><title>Index of %s</title>"
        "<style>body{font:14px Arial;margin:30px}a{color:#06c;text-decoration:none}"
        "a:hover{text-decoration:underline}li{margin:3px 0}.sz{color:#888;margin-left:12px}</style>"
        "</head><body><h2>Index of %s</h2><hr><ul>", url, url);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(fdata.cFileName, ".") || !strcmp(fdata.cFileName, "..")) continue;
            char name[MAX_PATH_LEN], e[MAX_PATH_LEN * 3];
            html_escape(name, sizeof(name), fdata.cFileName);
            html_escape(e, sizeof(e), name);
            char full[MAX_PATH_LEN];
            _snprintf_s(full, sizeof(full), _TRUNCATE, "%s\\%s", path, fdata.cFileName);
            DWORD attr = GetFileAttributesA(full);
            char suffix[8] = "";
            long long sz = ((long long)fdata.nFileSizeHigh << 32) | fdata.nFileSizeLow;
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                strcpy(suffix, "/");
                sz = -1;
            }
            p = app(p, end, "<li><a href=\"%s%s%s\">%s%s</a>",
                url, slash == '/' ? "" : "/", e, name, suffix);
            if (sz >= 0) p = app(p, end, "<span class=sz>(%lld bytes)</span>", sz);
            p = app(p, end, "</li>");
        } while (FindNextFileA(h, &fdata));
        FindClose(h);
    }
    p = app(p, end, "</ul>");
    size_t n = strlen(UPLOAD_UI);
    if (p + n < end) { memcpy(p, UPLOAD_UI, n); p += n; }
    http_head(s, 200, "OK", "text/html; charset=utf-8", (long long)(p - body), NULL);
    http_send_all(s, body, (int)(p - body));
}
