#ifndef NET_H
#define NET_H

#include "common.h"

typedef struct {
    SOCKET fd;
    char *buf;
    int cap;
    int pos;
    int len;
} Conn;

SOCKET net_listen(unsigned short port);
void   conn_init(Conn *c, SOCKET fd, char *buf, int cap);
void   conn_close(Conn *c);
int    conn_getline(Conn *c, char *out, int cb);
int    conn_read(Conn *c, char *out, int n);
int    conn_send(Conn *c, const char *data, int n);

#endif
