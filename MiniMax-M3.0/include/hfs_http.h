#ifndef HFS_HTTP_H
#define HFS_HTTP_H

#include <stdio.h>

#include "hfs_config.h"
#include "hfs_net.h"

typedef struct {
    hfs_config config;
    FILE *log;
    int verbose;
} hfs_server;

int hfs_http_init(hfs_server *server, const hfs_config *cfg, FILE *log);
void hfs_http_handle(hfs_server *server, hfs_socket *client);

#endif
