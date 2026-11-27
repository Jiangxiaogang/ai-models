#ifndef HANDLER_H
#define HANDLER_H

#include "http.h"
#include "config.h"

void handle_request(struct http *r, struct config *cfg);

#endif
