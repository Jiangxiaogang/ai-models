#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "page.h"
#include "util.h"

#define PAGE_CAP (4 << 20)
#define ENTRY_MAX 10000

static const char PAGE_CSS[] =
    "body{font-family:'Segoe UI',Arial,sans-serif;max-width:860px;margin:24px auto;padding:0 16px;color:#222}"
    "h1{font-size:20px;border-bottom:2px solid #2b7a78;padding-bottom:8px}"
    ".upload{background:#f5f7f7;border:1px solid #ddd;border-radius:8px;padding:14px 16px;margin:16px 0}"
    "#name{margin:8px 0;font-weight:600;min-height:18px}"
    "#bar{height:14px;background:#e3e8e8;border-radius:7px;overflow:hidden}"
    "#fill{height:100%;width:0;background:#2b9d8f;transition:width .2s}"
    "#stat{margin-top:6px;font-size:13px;color:#555;min-height:16px}"
    "table{width:100%;border-collapse:collapse;font-size:14px;margin-top:12px}"
    "th{text-align:left;border-bottom:2px solid #2b7a78;padding:6px 10px}"
    "td{border-bottom:1px solid #e8e8e8;padding:6px 10px}"
    "tr:hover td{background:#f2f8f8}"
    "a{color:#1d6f6d;text-decoration:none}"
    "a:hover{text-decoration:underline}"
    ".dir{font-weight:600}"
    ".num{text-align:right;white-space:nowrap}"
    ".time{white-space:nowrap;color:#777}";

static const char PAGE_JS[] =
    "function fmtSize(n){"
    "if(n<1024)return n+' B';"
    "if(n<1048576)return (n/1024).toFixed(1)+' KB';"
    "if(n<1073741824)return (n/1048576).toFixed(1)+' MB';"
    "return (n/1073741824).toFixed(2)+' GB';}"
    "function uploadAll(){"
    "var files=document.getElementById('file').files;"
    "if(files.length)sendOne(files,0);}"
    "function sendOne(files,i){"
    "if(i>=files.length){setTimeout(function(){location.reload();},300);return;}"
    "var file=files[i];"
    "var name=document.getElementById('name');"
    "var fill=document.getElementById('fill');"
    "var stat=document.getElementById('stat');"
    "name.textContent='Uploading: '+file.name;"
    "fill.style.width='0';"
    "var t0=Date.now(),lastT=t0,lastL=0,speed=0;"
    "var xhr=new XMLHttpRequest();"
    "xhr.open('PUT',location.pathname+encodeURIComponent(file.name));"
    "xhr.upload.onprogress=function(e){"
    "var now=Date.now();"
    "if(now-lastT>=400){if(lastT>t0)speed=(e.loaded-lastL)*1000/(now-lastT);lastT=now;lastL=e.loaded;}"
    "var p=e.total?e.loaded/e.total:0;"
    "fill.style.width=(p*100).toFixed(1)+'%';"
    "stat.textContent=fmtSize(e.loaded)+' / '+fmtSize(e.total)+'   '+fmtSize(speed)+'/s';};"
    "xhr.onload=function(){"
    "if(xhr.status>=200&&xhr.status<300)sendOne(files,i+1);"
    "else{name.textContent='Upload failed: HTTP '+xhr.status;stat.textContent='';}};"
    "xhr.onerror=function(){name.textContent='Upload failed: network error';stat.textContent='';};"
    "xhr.send(file);}"
    "document.body.ondragover=function(e){e.preventDefault();};"
    "document.body.ondrop=function(e){e.preventDefault();"
    "if(e.dataTransfer.files.length)sendOne(e.dataTransfer.files,0);};";

typedef struct {
    char ansi[MAX_PATH];
    char utf8[1024];
    int isDir;
    long long size;
    FILETIME ft;
} Entry;

static void append(char *buf, int *pos, const char *text)
{
    int n = (int)strlen(text);
    int room = PAGE_CAP - 1 - *pos;
    if (n > room) n = room > 0 ? room : 0;
    if (n > 0) {
        memcpy(buf + *pos, text, (size_t)n);
        *pos += n;
    }
    buf[*pos] = 0;
}

static void append_escaped(char *buf, int *pos, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '&': append(buf, pos, "&amp;"); break;
        case '<': append(buf, pos, "&lt;"); break;
        case '>': append(buf, pos, "&gt;"); break;
        case '"': append(buf, pos, "&quot;"); break;
        default: {
            char one[2];
            one[0] = *s;
            one[1] = 0;
            append(buf, pos, one);
        }
        }
    }
}

static void append_url_name(char *buf, int *pos, const char *utf8Name)
{
    char piece[4] = "%..";
    for (const char *p = utf8Name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            piece[0] = (char)c;
            piece[1] = 0;
            append(buf, pos, piece);
        } else {
            piece[0] = '%';
            piece[1] = HEX_UPPER[c >> 4];
            piece[2] = HEX_UPPER[c & 15];
            append(buf, pos, piece);
        }
    }
}

static void append_time(char *buf, int *pos, const SYSTEMTIME *st)
{
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%04u-%02u-%02u %02u:%02u",
             st->wYear, st->wMonth, st->wDay, st->wHour, st->wMinute);
    append(buf, pos, tmp);
}

static void append_row(char *buf, int *pos, const char *urlBase, const Entry *e)
{
    char link[2200];
    int lp = 0;
    append(link, &lp, urlBase);
    append_url_name(link, &lp, e->utf8);
    if (e->isDir) append(link, &lp, "/");
    append(buf, pos, "<tr><td><a href=\"");
    append(buf, pos, link);
    append(buf, pos, e->isDir ? "\" class=\"dir\">" : "\">");
    append_escaped(buf, pos, e->utf8);
    if (e->isDir) {
        append(buf, pos, "/</a></td><td class=\"num\"></td><td class=\"time\">");
    } else {
        char sz[32];
        human_size(e->size, sz, sizeof(sz));
        append(buf, pos, "</a></td><td class=\"num\">");
        append(buf, pos, sz);
        append(buf, pos, "</td><td class=\"time\">");
    }
    SYSTEMTIME utc, local;
    if (FileTimeToSystemTime(&e->ft, &utc) && SystemTimeToTzSpecificLocalTime(NULL, &utc, &local)) {
        append_time(buf, pos, &local);
    } else {
        append(buf, pos, "-");
    }
    append(buf, pos, "</td></tr>\n");
}

static int entry_cmp(const void *a, const void *b)
{
    const Entry *x = (const Entry *)a;
    const Entry *y = (const Entry *)b;
    if (x->isDir != y->isDir) return y->isDir - x->isDir;
    return _stricmp(x->ansi, y->ansi);
}

char *page_index_html(const char *urlPath, const char *diskPath)
{
    char *buf = (char *)malloc(PAGE_CAP);
    if (!buf) return NULL;
    int pos = 0;
    append(buf, &pos, "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n"
                      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
                      "<title>Index of ");
    append_escaped(buf, &pos, urlPath);
    append(buf, &pos, "</title>\n<style>");
    append(buf, &pos, PAGE_CSS);
    append(buf, &pos, "</style>\n</head>\n<body>\n<h1>Index of ");
    append_escaped(buf, &pos, urlPath);
    append(buf, &pos, "</h1>\n<div class=\"upload\">\n"
                      "<input type=\"file\" id=\"file\" multiple>\n"
                      "<button onclick=\"uploadAll()\">Upload</button>\n"
                      "<div id=\"name\"></div>\n"
                      "<div id=\"bar\"><div id=\"fill\"></div></div>\n"
                      "<div id=\"stat\"></div>\n"
                      "</div>\n"
                      "<table>\n<tr><th>Name</th><th>Size</th><th>Modified</th></tr>\n");
    if (strcmp(urlPath, "/") != 0) {
        int len = (int)strlen(urlPath);
        int cut = len - 1;
        while (cut > 0 && urlPath[cut - 1] != '/') cut--;
        char parent[2048];
        snprintf(parent, sizeof(parent), "%.*s", cut, urlPath);
        append(buf, &pos, "<tr><td><a class=\"dir\" href=\"");
        append(buf, &pos, parent);
        append(buf, &pos, "\">../</a></td><td class=\"num\"></td><td class=\"time\"></td></tr>\n");
    }
    Entry *entries = (Entry *)malloc(sizeof(Entry) * 64);
    int count = 0, cap = entries ? 64 : 0;
    if (entries) {
        char pattern[MAX_PATH + 4];
        snprintf(pattern, sizeof(pattern), "%s\\*", diskPath);
        WIN32_FIND_DATAA fd;
        HANDLE find = FindFirstFileA(pattern, &fd);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                if (count >= cap) {
                    int next = cap * 2;
                    Entry *grown = (Entry *)realloc(entries, sizeof(Entry) * (size_t)next);
                    if (!grown) break;
                    entries = grown;
                    cap = next;
                }
                Entry *e = &entries[count++];
                strcpy(e->ansi, fd.cFileName);
                ansi_to_utf8(e->ansi, e->utf8, (int)sizeof(e->utf8));
                e->isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                e->size = ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                e->ft = fd.ftLastWriteTime;
            } while (count < ENTRY_MAX && FindNextFileA(find, &fd));
            FindClose(find);
        }
        qsort(entries, (size_t)count, sizeof(Entry), entry_cmp);
        for (int i = 0; i < count; i++) append_row(buf, &pos, urlPath, &entries[i]);
        free(entries);
    }
    append(buf, &pos, "</table>\n<script>\n");
    append(buf, &pos, PAGE_JS);
    append(buf, &pos, "\n</script>\n</body>\n</html>\n");
    return buf;
}
