#ifndef HANDLER_H
#define HANDLER_H

#include <winsock2.h>
#include "config.h"
#include "http.h"

typedef struct {
    SOCKET s;
    app_config cfg;
} client_ctx;

int handle_client(SOCKET s, const app_config *cfg);
unsigned __stdcall client_thread(void *arg);

#endif
