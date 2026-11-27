#include "net.h"

SOCKET net_listen(unsigned short port) {
    SOCKET s;
    int yes = 1;
    struct sockaddr_in addr;

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, (const struct sockaddr *)&addr, (int)sizeof addr) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (listen(s, 64) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

void conn_init(Conn *c, SOCKET fd, char *buf, int cap) {
    int timeout = 120000;

    c->fd = fd;
    c->buf = buf;
    c->cap = cap;
    c->pos = 0;
    c->len = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
}

void conn_close(Conn *c) {
    if (c->fd != INVALID_SOCKET) {
        closesocket(c->fd);
        c->fd = INVALID_SOCKET;
    }
}

static int conn_fill(Conn *c) {
    int n;

    if (c->pos < c->len) {
        return 1;
    }
    n = recv(c->fd, c->buf, c->cap, 0);
    if (n <= 0) {
        return 0;
    }
    c->pos = 0;
    c->len = n;
    return 1;
}

static int conn_getc(Conn *c) {
    if (!conn_fill(c)) {
        return -1;
    }
    return (unsigned char)c->buf[c->pos++];
}

int conn_getline(Conn *c, char *out, int cb) {
    int n = 0;

    for (;;) {
        int ch = conn_getc(c);
        if (ch < 0) {
            return n ? n : -1;
        }
        if (ch == '\n') {
            break;
        }
        if (n >= cb - 1) {
            return -2;
        }
        out[n++] = (char)ch;
    }
    if (n > 0 && out[n - 1] == '\r') {
        n--;
    }
    out[n] = 0;
    return n;
}

int conn_read(Conn *c, char *out, int n) {
    int total = 0;

    while (total < n) {
        int avail;
        int want;
        if (!conn_fill(c)) {
            break;
        }
        avail = c->len - c->pos;
        want = n - total;
        if (want > avail) {
            want = avail;
        }
        memcpy(out + total, c->buf + c->pos, (size_t)want);
        c->pos += want;
        total += want;
    }
    return total;
}

int conn_send(Conn *c, const char *data, int n) {
    int total = 0;

    while (total < n) {
        int r = send(c->fd, data + total, n - total, 0);
        if (r <= 0) {
            return 0;
        }
        total += r;
    }
    return 1;
}
