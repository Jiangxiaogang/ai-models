#include "hfs_fs.h"
#include "hfs.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <windows.h>

typedef struct {
    char name[256];
    char href[512];
    long long size;
    long long mtime;
    int is_dir;
} fs_entry;

static int entry_cmp(const void *a, const void *b)
{
    const fs_entry *x = (const fs_entry *)a;
    const fs_entry *y = (const fs_entry *)b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return _stricmp(x->name, y->name);
}

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} strbuf;

static int sb_reserve(strbuf *sb, size_t extra)
{
    if (sb->length + extra + 1 > sb->capacity) {
        size_t cap = sb->capacity ? sb->capacity : 4096;
        char *tmp;
        while (sb->length + extra + 1 > cap) cap *= 2;
        tmp = (char *)realloc(sb->data, cap);
        if (!tmp) return -1;
        sb->data = tmp;
        sb->capacity = cap;
    }
    return 0;
}

static int sb_append(strbuf *sb, const char *src)
{
    size_t n = strlen(src);
    if (sb_reserve(sb, n) != 0) return -1;
    memcpy(sb->data + sb->length, src, n + 1);
    sb->length += n;
    return 0;
}

static int sb_appendf(strbuf *sb, const char *fmt, ...)
{
    char tmp[2048];
    va_list args;
    int n;
    va_start(args, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (n < 0) return -1;
    if ((size_t)n >= sizeof(tmp)) n = (int)sizeof(tmp) - 1;
    return sb_append(sb, tmp);
}

static void format_size(long long size, char *dst, size_t dst_size)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = (double)size;
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        u++;
    }
    if (u == 0) snprintf(dst, dst_size, "%lld %s", size, units[u]);
    else snprintf(dst, dst_size, "%.2f %s", v, units[u]);
}

static int collect_entries(const char *fs_path, fs_entry **out, int *count)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pattern[HFS_MAX_PATH * 2];
    fs_entry *list = NULL;
    int n = 0, cap = 0;

    if (hfs_util_path_join(fs_path, "*", pattern, sizeof(pattern)) != 0) return -1;
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;

    do {
        char child[HFS_MAX_PATH * 2];
        struct stat st;
        fs_entry e;

        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (hfs_util_path_join(fs_path, fd.cFileName, child, sizeof(child)) != 0) continue;
        if (stat(child, &st) != 0) continue;

        memset(&e, 0, sizeof(e));
        strncpy(e.name, fd.cFileName, sizeof(e.name) - 1);
        e.size = (long long)st.st_size;
        e.mtime = (long long)st.st_mtime;
        e.is_dir = (st.st_mode & S_IFMT) == S_IFDIR;

        if (n >= cap) {
            int new_cap = cap ? cap * 2 : 64;
            fs_entry *tmp = (fs_entry *)realloc(list, (size_t)new_cap * sizeof(fs_entry));
            if (!tmp) {
                free(list);
                FindClose(h);
                return -1;
            }
            list = tmp;
            cap = new_cap;
        }
        list[n++] = e;
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    qsort(list, (size_t)n, sizeof(fs_entry), entry_cmp);
    *out = list;
    *count = n;
    return 0;
}

static int build_href(const char *url_path, const char *name, int is_dir, char *dst, size_t dst_size)
{
    char enc[512];
    size_t pos;
    if (hfs_util_url_encode(name, enc, sizeof(enc)) != 0) return -1;
    pos = strlen(url_path);
    if (pos > 0 && url_path[pos - 1] == '/') pos--;
    if (pos + strlen(enc) + 3 > dst_size) return -1;
    memcpy(dst, url_path, pos);
    dst[pos] = '/';
    strcpy(dst + pos + 1, enc);
    if (is_dir) {
        size_t l = strlen(dst);
        if (l + 2 > dst_size) return -1;
        dst[l] = '/';
        dst[l + 1] = '\0';
    }
    return 0;
}

static const char *INDEX_CSS =
    "*{box-sizing:border-box}"
    "body{margin:0;padding:24px;font-family:'Segoe UI',Tahoma,Arial,'Microsoft YaHei',sans-serif;"
    "background:#f4f6f8;color:#22313f}"
    ".box{max-width:980px;margin:0 auto;background:#fff;border-radius:10px;overflow:hidden;"
    "box-shadow:0 2px 14px rgba(20,40,70,.08)}"
    ".bar{padding:18px 24px;background:#1f6feb;color:#fff}"
    ".bar h1{margin:0 0 6px;font-size:18px;font-weight:600}"
    ".bar p{margin:0;font-size:13px;opacity:.85;word-break:break-all}"
    ".tbl{width:100%;border-collapse:collapse;font-size:14px}"
    ".tbl th{text-align:left;padding:12px 16px;background:#f0f3f7;color:#55606b;"
    "font-weight:600;font-size:13px;border-bottom:1px solid #e2e7ee}"
    ".tbl td{padding:11px 16px;border-bottom:1px solid #eef1f5}"
    ".tbl tr:hover td{background:#f7fafd}"
    ".tbl a{color:#1f6feb;text-decoration:none}"
    ".tbl a:hover{text-decoration:underline}"
    ".dir{font-weight:600}"
    ".muted{color:#8b96a3}"
    ".up{padding:11px 16px;font-size:13px}"
    ".up a{color:#1f6feb;text-decoration:none}"
    "form.up{display:inline}"
    ".foot{padding:14px 24px;font-size:12px;color:#8b96a3;border-top:1px solid #eef1f5}";

int hfs_fs_build_index(const hfs_config *cfg, const char *url_path, const char *fs_path,
                       hfs_buffer *out)
{
    fs_entry *entries = NULL;
    int count = 0, i;
    strbuf sb;
    long long total = 0;
    int dirs = 0, files = 0;
    char datebuf[64];
    long long now = (long long)time(NULL);

    if (collect_entries(fs_path, &entries, &count) != 0) return -1;

    memset(&sb, 0, sizeof(sb));
    sb_append(&sb, "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"utf-8\">\n");
    sb_append(&sb, "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
    sb_append(&sb, "<title>Index of ");
    {
        char esc[1024];
        hfs_util_escape_html(url_path, esc, sizeof(esc));
        sb_append(&sb, esc);
    }
    sb_append(&sb, "</title>\n<style>");
    sb_append(&sb, INDEX_CSS);
    sb_append(&sb, "</style>\n</head>\n<body>\n<div class=\"box\">\n");
    sb_append(&sb, "<div class=\"bar\"><h1>Index of ");
    {
        char esc[1024];
        hfs_util_escape_html(url_path, esc, sizeof(esc));
        sb_append(&sb, esc);
        sb_append(&sb, "</h1><p>");
        hfs_util_escape_html(fs_path, esc, sizeof(esc));
        sb_append(&sb, esc);
        sb_append(&sb, "</p></div>\n");
    }

    sb_append(&sb, "<table class=\"tbl\"><thead><tr><th>Name</th><th>Size</th>"
                   "<th>Modified</th></tr></thead><tbody>\n");

    if (strcmp(url_path, "/") != 0) {
        sb_append(&sb, "<tr><td class=\"dir\"><a href=\"../\">../</a></td><td class=\"muted\">-</td>"
                       "<td class=\"muted\">-</td></tr>\n");
    }

    for (i = 0; i < count; i++) {
        char href[600];
        char esc_name[600];
        char sizebuf[64];
        char shown[300];

        if (build_href(url_path, entries[i].name, entries[i].is_dir, href, sizeof(href)) != 0) continue;
        hfs_util_escape_html(entries[i].name, esc_name, sizeof(esc_name));
        strncpy(shown, esc_name, sizeof(shown) - 1);
        shown[sizeof(shown) - 1] = '\0';
        if (entries[i].is_dir) {
            size_t l = strlen(shown);
            if (l + 6 < sizeof(shown)) strcat(shown, "/");
            dirs++;
        } else {
            format_size(entries[i].size, sizebuf, sizeof(sizebuf));
            total += entries[i].size;
            files++;
        }
        hfs_util_http_date(entries[i].mtime, datebuf, sizeof(datebuf));
        sb_appendf(&sb, "<tr><td><a href=\"%s\"%s>%s</a></td><td>%s</td><td>%s</td></tr>\n",
                   href, entries[i].is_dir ? " class=\"dir\"" : "", shown,
                   entries[i].is_dir ? "-" : sizebuf, datebuf);
    }
    sb_append(&sb, "</tbody></table>\n");

    if (cfg->enable_upload) {
        char esc[1024];
        char link[1200];
        char up_url[1024];
        size_t base = strlen(url_path);
        while (base > 1 && url_path[base - 1] == '/') base--;
        snprintf(up_url, sizeof(up_url), "%.*s", (int)base, url_path);
        snprintf(link, sizeof(link), "%s/__upload", up_url);
        hfs_util_escape_html(link, esc, sizeof(esc));
        sb_append(&sb, "<div class=\"up\"><a href=\"");
        sb_append(&sb, esc);
        sb_append(&sb, "\">Upload file</a> &nbsp;|&nbsp; <a href=\"../\">Up to parent</a></div>\n");
    }

    {
        char totalbuf[64];
        format_size(total, totalbuf, sizeof(totalbuf));
        hfs_util_http_date(now, datebuf, sizeof(datebuf));
        sb_appendf(&sb, "<div class=\"foot\">%d directories, %d files, total %s &nbsp;|&nbsp; %s</div>\n",
                   dirs, files, totalbuf, datebuf);
    }

    sb_append(&sb, "</div>\n</body>\n</html>\n");

    free(entries);

    if (!sb.data) return -1;
    out->data = sb.data;
    out->length = sb.length;
    return 0;
}

void hfs_fs_buffer_free(hfs_buffer *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->length = 0;
}
