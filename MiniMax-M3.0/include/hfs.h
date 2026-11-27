#ifndef HFS_H
#define HFS_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#define HFS_VERSION       "1.0"
#define HFS_MAX_PATH      1024
#define HFS_MAX_URL       2048
#define HFS_MAX_HEADERS   64
#define HFS_MAX_LINE      2048

typedef struct {
    char name[64];
    char value[512];
} hfs_header;

typedef struct {
    char method[16];
    char target[HFS_MAX_URL];
    char version[16];
    hfs_header headers[HFS_MAX_HEADERS];
    int header_count;
    long long content_length;
    int has_content_length;
    int keep_alive;
} hfs_request;

int hfs_util_url_decode(const char *src, char *dst, size_t dst_size);
int hfs_util_url_encode(const char *src, char *dst, size_t dst_size);
int hfs_util_path_join(const char *base, const char *rel, char *dst, size_t dst_size);
int hfs_util_path_to_absolute(const char *base, const char *rel, char *dst, size_t dst_size);
int hfs_util_normalize_path(const char *src, char *dst, size_t dst_size);
void hfs_util_http_date(long long t, char *dst, size_t dst_size);
int hfs_util_mime_type(const char *path, char *dst, size_t dst_size);
int hfs_util_is_directory(const char *path);
int hfs_util_file_size(const char *path, long long *size);
int hfs_util_exists(const char *path);
void hfs_util_escape_html(const char *src, char *dst, size_t dst_size);

#endif
