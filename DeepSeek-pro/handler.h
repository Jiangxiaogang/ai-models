#ifndef HANDLER_H
#define HANDLER_H

#include <winsock2.h>
#include "config.h"
#include "http.h"

void handle_request(SOCKET s, const request_t *req, const char *body_data, int body_len, const config_t *cfg);

#endif