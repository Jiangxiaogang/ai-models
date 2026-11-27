#include "listing.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LISTING_MAX_ENTRIES 4096

typedef struct {
    char name[256];
    long long size;
    long long mtime;
    int is_dir;
} entry;

static int entry_cmp(const void *a, const void *b)
{
    const entry *x = (const entry *)a;
    const entry *y = (const entry *)b;
    if (x->is_dir != y->is_dir)
        return y->is_dir - x->is_dir;
    int r = _stricmp(x->name, y->name);
    if (r != 0)
        return r;
    return strcmp(x->name, y->name);
}

static int collect(const char *disk_dir, entry *items, int max_items)
{
    char pattern[UTIL_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*", disk_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    int count = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        if (count >= max_items)
            break;
        entry *e = &items[count];
        snprintf(e->name, sizeof(e->name), "%s", fd.cFileName);
        e->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        ULARGE_INTEGER ticks;
        ticks.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        ticks.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        e->size = e->is_dir ? 0 : (long long)((__int64)fd.nFileSizeHigh << 32 | fd.nFileSizeLow);
        e->mtime = (long long)ticks.QuadPart;
        count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    qsort(items, (size_t)count, sizeof(entry), entry_cmp);
    return count;
}

int listing_build(const char *url_path, const char *disk_dir, sbuf *out)
{
    entry *items = (entry *)malloc(sizeof(entry) * LISTING_MAX_ENTRIES);
    if (!items)
        return -1;
    int count = collect(disk_dir, items, LISTING_MAX_ENTRIES);
    char title[512];
    char safe[512];
    snprintf(safe, sizeof(safe), "%s", url_path[0] ? url_path : "/");
    if (fmt_escape_html(safe, title, sizeof(title)) < 0)
        snprintf(title, sizeof(title), "/");
    char up[UTIL_PATH_MAX];
    snprintf(up, sizeof(up), "%s", url_path);
    size_t ulen = strlen(up);
    while (ulen > 1 && up[ulen - 1] == '/')
        up[--ulen] = '\0';
    char *slash = strrchr(up, '/');
    if (slash)
        up[slash - up + 1] = '\0';
    else
        snprintf(up, sizeof(up), "/");
    sbuf_addf(out,
              "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
              "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
              "<title>Index of %s</title>"
              "<style>body{font-family:Segoe UI,Microsoft YaHei,sans-serif;background:#0f1419;color:#d7dde5;margin:0}"
              "header{padding:22px 30px;background:#161c24;border-bottom:1px solid #232c38;display:flex;"
              "align-items:center;gap:16px;flex-wrap:wrap}"
              "h1{font-size:18px;margin:0;font-weight:600;color:#e8edf4}"
              "nav{margin-left:auto;display:flex;gap:14px}"
              "a{color:#4f9cf9;text-decoration:none}a:hover{text-decoration:underline}"
              "table{width:100%;border-collapse:collapse;font-size:14px}"
              "th{text-align:left;color:#8b98a9;font-weight:600;padding:10px 30px;border-bottom:1px solid #232c38}"
              "td{padding:9px 30px;border-bottom:1px solid #1b222b}"
              "tr:hover td{background:#161c24}"
              ".name a{color:#d7dde5}.dir a{color:#f0b429;font-weight:600}"
              ".num{text-align:right;color:#9aa7b6;white-space:nowrap}"
              ".meta{color:#6f7c8c}.tag{font-size:12px;color:#0f1419;background:#4f9cf9;border-radius:4px;padding:1px 7px}"
              "footer{padding:16px 30px;color:#6f7c8c;font-size:13px}"
              "</style></head><body>"
              "<header><h1>Index of %s</h1><nav>"
              "<a href=\"/upload.html\">上传文件</a><a href=\"/\">根目录</a>"
              "</nav></header><table><thead><tr><th>名称</th><th>大小</th><th>修改时间</th><th>操作</th></tr></thead><tbody>",
              title,
              title);
    if (strcmp(up, "/") != 0 && strcmp(up, url_path) != 0)
        sbuf_addf(out, "<tr><td class=\"name dir\"><a href=\"%s\">..</a></td><td class=\"num\">-</td>"
                       "<td class=\"meta\">-</td><td>-</td></tr>",
                  up);
    for (int i = 0; i < count; i++) {
        const entry *e = &items[i];
        char esc[512];
        char enc[1024];
        if (fmt_escape_html(e->name, esc, sizeof(esc)) < 0)
            snprintf(esc, sizeof(esc), "?");
        if (url_encode(e->name, enc, sizeof(enc)) < 0)
            snprintf(enc, sizeof(enc), "?");
        char sizebuf[32];
        fmt_bytes(e->size, sizebuf, sizeof(sizebuf));
        char datebuf[64];
        if (fmt_http_date(e->mtime / 10000000LL - 11644473600LL, datebuf, sizeof(datebuf)) != 0)
            snprintf(datebuf, sizeof(datebuf), "-");
        char href[1400];
        snprintf(href, sizeof(href), "%s%s%s", url_path, enc, e->is_dir ? "/" : "");
        if (e->is_dir) {
            sbuf_addf(out,
                      "<tr><td class=\"name dir\"><a href=\"%s\">%s/</a></td><td class=\"num\">-</td>"
                      "<td class=\"meta\">%s</td><td><a href=\"%s\">打开</a></td></tr>",
                      href, esc, datebuf, href);
        } else {
            sbuf_addf(out,
                      "<tr><td class=\"name\"><a href=\"%s\">%s</a></td><td class=\"num\">%s</td>"
                      "<td class=\"meta\">%s</td><td><a href=\"%s\" download>下载</a></td></tr>",
                      href, esc, sizebuf, datebuf, href);
        }
    }
    sbuf_addf(out, "</tbody></table><footer>共 %d 项 &nbsp;|&nbsp; 上传文件请访问 /upload.html</footer></body></html>",
              count);
    free(items);
    return 0;
}
