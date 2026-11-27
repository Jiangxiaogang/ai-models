#ifndef HTTP_H
#define HTTP_H

#include <winsock2.h>

typedef struct {
	char method[16];
	char path[1024];
	long content_length;
} request_t;

int http_recv_req(SOCKET s, request_t *req, char *body_data, int *body_len);
int http_send_str(SOCKET s, int code, const char *mime, const char *body);
int http_send_file(SOCKET s, const char *fname);
int http_send_error(SOCKET s, int code);
long http_recv_body_to_file(SOCKET s, long content_length, const char *fname);

#endif