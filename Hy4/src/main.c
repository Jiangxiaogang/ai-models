#include "common.h"

int main(int argc, char **argv)
{
    Config cfg;
    const char *v;
    long port;
    char cfgfile[MAX_PATH];

    strcpy_s(cfgfile, sizeof(cfgfile), "server.ini");
    if (argc > 1 && argv[1][0] != '-') strcpy_s(cfgfile, sizeof(cfgfile), argv[1]);
    config_load(cfgfile, &cfg);

    v = config_arg(argc, argv, "--port");
    if (!v) v = config_arg(argc, argv, "-p");
    if (v) {
        port = strtol(v, NULL, 10);
        if (port > 0 && port <= 65535) cfg.port = (unsigned short)port;
    }
    v = config_arg(argc, argv, "--root");
    if (!v) v = config_arg(argc, argv, "-r");
    if (v) {
        char tmp[MAX_PATH * 2];
        wchar_t w[MAX_PATH * 2];
        wchar_t canon[MAX_PATH * 2];
        DWORD r;
        strcpy_s(tmp, sizeof(tmp), v);
        util_path_native(tmp);
        if (util_u8_to_w(tmp, w, MAX_PATH * 2)) {
            r = GetFullPathNameW(w, MAX_PATH * 2, canon, NULL);
            if (r > 0 && r < MAX_PATH * 2 && util_w_to_u8(canon, cfg.root, sizeof(cfg.root))) {
                util_path_native(cfg.root);
            }
        }
    }
    return server_run(&cfg);
}
