#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "handler.h"
#include "page.h"

static void send_file(struct http *r, const wchar_t *wpath, const char *urlfname)
{
    HANDLE h = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        http_reply(r, 404, "text/html", "<html><body><h1>404</h1></body></html>", 36);
        return;
    }
    LARGE_INTEGER sz;
    GetFileSizeEx(h, &sz);
    char head[1024];
    struct strbuf cd;
    sb_init(&cd);
    sb_str(&cd, "attachment; filename=\"");
    char asc[1024];
    int i;
    for (i = 0; urlfname[i] && i < 900; i++)
        asc[i] = (unsigned char)urlfname[i] < 128 && urlfname[i] != '"' &&
                 urlfname[i] != '\\' ? urlfname[i] : '_';
    asc[i] = 0;
    sb_str(&cd, asc);
    sb_str(&cd, "\"; filename*=UTF-8''");
    sb_url(&cd, urlfname);
    int n = _snprintf(head, sizeof(head),
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
        "Content-Length: %I64d\r\nContent-Disposition: %s\r\nConnection: close\r\n\r\n",
        sz.QuadPart, cd.p);
    http_send(r, head, n);
    char buf[65536];
    DWORD got;
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got > 0)
        http_send(r, buf, (int)got);
    CloseHandle(h);
    sb_free(&cd);
}

static int cmp_name(const void *a, const void *b)
{
    return _wcsicmp(*(const wchar_t **)a, *(const wchar_t **)b);
}

static void send_index(struct http *r, const wchar_t *wdir, const char *url)
{
    wchar_t pat[1024];
    _snwprintf(pat, 1024, L"%s\\*", wdir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    struct strbuf page;
    sb_init(&page);
    sb_str(&page, "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<title>Index of ");
    sb_html(&page, url);
    sb_str(&page, "</title></head><body style=\"font-family:Consolas,monospace;"
        "max-width:900px;margin:40px auto\">"
        "<h2>Index of ");
    sb_html(&page, url);
    sb_str(&page, "</h2><hr><table style=\"width:100%\">");
    wchar_t **names = NULL;
    int n = 0, cap = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (n == cap) {
                cap = cap ? cap * 2 : 64;
                names = (wchar_t **)realloc(names, (size_t)cap * sizeof(*names));
            }
            size_t wl = wcslen(fd.cFileName) + 1;
            names[n] = (wchar_t *)malloc(wl * sizeof(wchar_t));
            memcpy(names[n], fd.cFileName, wl * sizeof(wchar_t));
            n++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (names)
        qsort(names, (size_t)n, sizeof(*names), cmp_name);
    int i;
    for (i = 0; i < n; i++) {
        if (!_wcsicmp(names[i], L".") || !_wcsicmp(names[i], L".."))
            continue;
        wchar_t fq[1024];
        _snwprintf(fq, 1024, L"%s\\%s", wdir, names[i]);
        DWORD attr = GetFileAttributesW(fq);
        int isdir = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
        char u8n[1024];
        WideCharToMultiByte(CP_UTF8, 0, names[i], -1, u8n, sizeof(u8n), NULL, NULL);
        sb_str(&page, "<tr>");
        if (isdir) {
            sb_str(&page, "<td><a href=\"");
            sb_url(&page, u8n);
            sb_str(&page, "/\">");
            sb_html(&page, u8n);
            sb_str(&page, "/</a></td><td>DIR</td><td></td>");
        } else {
            WIN32_FILE_ATTRIBUTE_DATA fa;
            char df[32] = "";
            if (GetFileAttributesExW(fq, GetFileExInfoStandard, &fa)) {
                SYSTEMTIME st;
                FileTimeToSystemTime(&fa.ftLastWriteTime, &st);
                _snprintf(df, sizeof(df), "%04d-%02d-%02d %02d:%02d",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
            }
            char szs[32];
            _i64toa((__int64)fa.nFileSizeLow | ((__int64)fa.nFileSizeHigh << 32), szs, 10);
            sb_str(&page, "<td><a href=\"");
            sb_url(&page, u8n);
            sb_str(&page, "\">");
            sb_html(&page, u8n);
            sb_str(&page, "</a></td><td>");
            sb_str(&page, df);
            sb_str(&page, "</td><td style=\"text-align:right\">");
            sb_str(&page, szs);
            sb_str(&page, "</td>");
        }
        sb_str(&page, "</tr>");
    }
    for (i = 0; i < n; i++)
        free(names[i]);
    free(names);
    sb_str(&page, "</table><hr>"
        "<form onsubmit=\"return false\">"
        "<input type=\"file\" id=\"file\" required style=\"width:70%\"> "
        "<button type=\"submit\" id=\"btn\">Upload</button></form>"
        "<div id=\"st\" style=\"margin:12px 0\"></div>"
        "<script>var f=document.forms[0],fi=document.getElementById('file'),"
        "st=document.getElementById('st'),btn=document.getElementById('btn');"
        "f.onsubmit=function(){var file=fi.files[0];if(!file)return;"
        "var x=new XMLHttpRequest(),t0=Date.now(),ld=0;"
        "x.open('PUT','/'+encodeURIComponent(file.name));"
        "x.upload.onprogress=function(ev){if(!ev.lengthComputable)return;"
        "var now=Date.now(),dt=(now-t0)/1000,sp=dt>0?(ev.loaded-ld)/dt:0;"
        "ld=ev.loaded;t0=now;"
        "st.textContent=ev.loaded+' / '+ev.total+' bytes  '+sp.toFixed(0)+' B/s  '"
        "+(ev.loaded*100/ev.total).toFixed(1)+'%';};"
        "x.onload=function(){st.textContent='done: '+x.status;fi.value='';};"
        "x.onerror=function(){st.textContent='error';};"
        "x.send(file);};</script>"
        "</body></html>");
    http_reply(r, 200, "text/html; charset=utf-8", page.p, (int)page.len);
    sb_free(&page);
}

static void recv_body_to_file(struct http *r, const wchar_t *wpath)
{
    HANDLE h = CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        http_reply(r, 507, "text/html", "<html><body><h1>507</h1></body></html>", 36);
        return;
    }
    char buf[65536];
    unsigned long long total = r->content_len;
    unsigned long long done = (unsigned long long)r->body;
    if (done > total)
        done = total;
    if (done) {
        DWORD wr;
        WriteFile(h, r->req + r->head, (DWORD)done, &wr, NULL);
    }
    while (done < total) {
        unsigned long long want = total - done;
        if (want > sizeof(buf))
            want = sizeof(buf);
        int k = recv(r->sock, buf, (int)want, 0);
        if (k <= 0)
            break;
        DWORD wr;
        WriteFile(h, buf, (DWORD)k, &wr, NULL);
        done += (unsigned long long)k;
    }
    CloseHandle(h);
    http_reply(r, 201, "text/html", "<html><body>OK</body></html>", 29);
}

static void url_decode(char *s)
{
    char *o = s;
    while (*s) {
        if (*s == '%' && s[1] && s[2]) {
            int hi = s[1], lo = s[2];
            hi = hi >= '0' && hi <= '9' ? hi - '0' : (hi | 32) - 'a' + 10;
            lo = lo >= '0' && lo <= '9' ? lo - '0' : (lo | 32) - 'a' + 10;
            *o++ = (char)(hi * 16 + lo);
            s += 3;
        } else if (*s == '+') {
            *o++ = ' ';
            s++;
        } else
            *o++ = *s++;
    }
    *o = 0;
}

void handle_request(struct http *r, struct config *cfg)
{
    if (!strcmp(r->method, "POST")) {
        http_reply(r, 405, "text/html", "<html><body><h1>405</h1></body></html>", 36);
        return;
    }
    char url[4096];
    strcpy(url, r->url);
    char *q = strchr(url, '?');
    if (q)
        *q = 0;
    url_decode(url);
    wchar_t wurl[2048];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048);
    wchar_t wbase[1024];
    MultiByteToWideChar(CP_ACP, 0, cfg->root, -1, wbase, 1024);
    int bl = (int)wcslen(wbase);
    while (bl > 0 && wbase[bl - 1] == L'\\')
        wbase[--bl] = 0;
    wchar_t wpath[3072];
    _snwprintf(wpath, 3072, L"%s%s", wbase, wurl);
    char *slash = strrchr(url, '/');
    const char *rawname = slash && slash[1] ? slash + 1 : "";
    if (!strcmp(r->method, "PUT")) {
        if (!*rawname) {
            http_reply(r, 400, "text/html", "<html><body><h1>400</h1></body></html>", 36);
            return;
        }
        recv_body_to_file(r, wpath);
        return;
    }
    if (strcmp(r->method, "GET")) {
        http_reply(r, 405, "text/html", "<html><body><h1>405</h1></body></html>", 36);
        return;
    }
    DWORD attr = GetFileAttributesW(wpath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        http_reply(r, 404, "text/html", "<html><body><h1>404</h1></body></html>", 36);
        return;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        int ul = (int)strlen(url);
        if (ul < 1 || url[ul - 1] != '/') {
            char loc[4224];
            _snprintf(loc, sizeof(loc), "%s/", r->url);
            http_redirect(r, loc);
            return;
        }
        send_index(r, wpath, url);
    } else
        send_file(r, wpath, rawname);
}
