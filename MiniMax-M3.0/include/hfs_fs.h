#ifndef HFS_FS_H
#define HFS_FS_H

#include <stddef.h>

#include "hfs_config.h"

typedef struct {
    char *data;
    size_t length;
} hfs_buffer;

int hfs_fs_build_index(const hfs_config *cfg, const char *url_path, const char *fs_path,
                       hfs_buffer *out);
void hfs_fs_buffer_free(hfs_buffer *buf);

#endif
