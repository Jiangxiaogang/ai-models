#ifndef HTTP_H
#define HTTP_H

#include "common.h"
#include "config.h"
#include "net.h"

int http_handle(Conn *c, const Config *cfg);

#endif
