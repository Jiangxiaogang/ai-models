#ifndef HANDLER_H
#define HANDLER_H
#include "http.h"
#include "config.h"
void handle_get(SOCKET s, const Config *cfg, Request *req);
void handle_put(SOCKET s, const Config *cfg, Request *req);
#endif
