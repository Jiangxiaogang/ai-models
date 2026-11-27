#ifndef UPLOAD_H
#define UPLOAD_H

#include "util.h"
#include "http.h"
#include "config.h"

int upload_page_build(const app_config *cfg, sbuf *out);
int upload_handle_put(conn *c, const http_request *req, const app_config *cfg);

#endif
