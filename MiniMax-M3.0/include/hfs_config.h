#ifndef HFS_CONFIG_H
#define HFS_CONFIG_H

#include "hfs.h"

#define HFS_CONFIG_FILE "hfs.conf"

typedef struct {
    char listen_address[64];
    int listen_port;
    char document_root[HFS_MAX_PATH];
    char index_file[128];
    int auto_index;
    int enable_upload;
    int enable_download;
    char default_charset[32];
    int max_upload_size;
} hfs_config;

int hfs_config_load(hfs_config *cfg, const char *path);
int hfs_config_save(const hfs_config *cfg, const char *path);
void hfs_config_defaults(hfs_config *cfg);
int hfs_config_validate(hfs_config *cfg);

#endif
