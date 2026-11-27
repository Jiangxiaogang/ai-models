#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "defs.h"
#include "config.h"
#include "server.h"

int main(void)
{
    Config cfg;
    char exe[MAX_PATH_LEN];
    char cfgfile[MAX_PATH_LEN];
    GetModuleFileNameA(NULL, exe, MAX_PATH_LEN);
    char *slash = strrchr(exe, '\\');
    if (slash) {
        *(slash + 1) = 0;
        _snprintf_s(cfgfile, sizeof(cfgfile), _TRUNCATE, "%sconfig.txt", exe);
    } else {
        strcpy(cfgfile, "config.txt");
    }
    if (!config_load(cfgfile, &cfg))
        printf("config.txt not found, using defaults (port 8080, root .)\n");
    return server_run(&cfg);
}
