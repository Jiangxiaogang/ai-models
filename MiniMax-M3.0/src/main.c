#include "hfs.h"
#include "hfs_config.h"
#include "hfs_http.h"
#include "hfs_net.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile int g_running = 1;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void print_usage(const char *exe)
{
    printf("hfs - HTTP file transfer server %s\n\n", HFS_VERSION);
    printf("Usage: %s [options]\n\n", exe);
    printf("Options:\n");
    printf("  -c <file>   configuration file (default: %s)\n", HFS_CONFIG_FILE);
    printf("  -p <port>   override listen port\n");
    printf("  -r <dir>    override document root\n");
    printf("  -a <addr>   override listen address\n");
    printf("  -g          generate a default configuration file and exit\n");
    printf("  -v          verbose console output\n");
    printf("  -h          show this help\n");
}

static int write_default_config(const char *path)
{
    hfs_config cfg;
    hfs_config_defaults(&cfg);
    return hfs_config_save(&cfg, path);
}

int main(int argc, char **argv)
{
    hfs_config cfg;
    hfs_server server;
    hfs_socket listener, client;
    char config_path[HFS_MAX_PATH] = HFS_CONFIG_FILE;
    char port_override[32] = "";
    char root_override[HFS_MAX_PATH] = "";
    char addr_override[64] = "";
    int verbose = 0;
    int i;
    FILE *log = NULL;
    int rc;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--generate-config") == 0) {
            const char *out = (i + 1 < argc && argv[i + 1][0] != '-') ? argv[++i] : config_path;
            if (write_default_config(out) != 0) {
                fprintf(stderr, "cannot write configuration file: %s\n", out);
                return 1;
            }
            printf("configuration file written: %s\n", out);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            strncpy(config_path, argv[++i], sizeof(config_path) - 1);
            config_path[sizeof(config_path) - 1] = '\0';
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            strncpy(port_override, argv[++i], sizeof(port_override) - 1);
            port_override[sizeof(port_override) - 1] = '\0';
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            strncpy(root_override, argv[++i], sizeof(root_override) - 1);
            root_override[sizeof(root_override) - 1] = '\0';
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            strncpy(addr_override, argv[++i], sizeof(addr_override) - 1);
            addr_override[sizeof(addr_override) - 1] = '\0';
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    rc = hfs_config_load(&cfg, config_path);
    if (rc == -1) {
        hfs_config_defaults(&cfg);
        printf("configuration file '%s' not found, using defaults\n", config_path);
        hfs_config_save(&cfg, config_path);
    } else if (rc == -2) {
        fprintf(stderr, "configuration file '%s' contains an invalid value\n", config_path);
        return 1;
    }

    if (port_override[0]) {
        int port = atoi(port_override);
        if (port < 1 || port > 65535) {
            fprintf(stderr, "invalid port: %s\n", port_override);
            return 1;
        }
        cfg.listen_port = port;
    }
    if (addr_override[0]) {
        strncpy(cfg.listen_address, addr_override, sizeof(cfg.listen_address) - 1);
        cfg.listen_address[sizeof(cfg.listen_address) - 1] = '\0';
    }
    if (root_override[0]) {
        strncpy(cfg.document_root, root_override, sizeof(cfg.document_root) - 1);
        cfg.document_root[sizeof(cfg.document_root) - 1] = '\0';
    }

    if (hfs_config_validate(&cfg) != 0) {
        fprintf(stderr, "document_root is not a valid directory: %s\n", cfg.document_root);
        return 1;
    }

    log = fopen("hfs.log", "a");
    if (!log) verbose = 1;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (hfs_net_init() != 0) {
        fprintf(stderr, "cannot initialize network subsystem\n");
        if (log) fclose(log);
        return 1;
    }

    rc = hfs_net_listen(cfg.listen_address, cfg.listen_port, &listener);
    if (rc != 0) {
        fprintf(stderr, "cannot listen on %s:%d (error %d)\n", cfg.listen_address, cfg.listen_port, rc);
        hfs_net_cleanup();
        if (log) fclose(log);
        return 1;
    }

    hfs_http_init(&server, &cfg, log);
    server.verbose = verbose;

    printf("hfs %s started\n", HFS_VERSION);
    printf("  listen    : http://%s:%d/\n", cfg.listen_address, cfg.listen_port);
    printf("  root      : %s\n", cfg.document_root);
    printf("  index file: %s (auto_index=%d)\n", cfg.index_file, cfg.auto_index);
    printf("  upload    : %s   download: %s\n", cfg.enable_upload ? "on" : "off",
           cfg.enable_download ? "on" : "off");
    printf("  config    : %s\n", config_path);
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (g_running) {
        if (hfs_net_accept(&listener, &client) != 0) {
            if (!g_running) break;
            continue;
        }
        hfs_http_handle(&server, &client);
        hfs_net_close(&client);
    }

    printf("\nhfs stopped.\n");
    fflush(stdout);
    hfs_net_close(&listener);
    hfs_net_cleanup();
    if (log) fclose(log);
    return 0;
}
