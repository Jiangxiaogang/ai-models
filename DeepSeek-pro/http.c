#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "http.h"
#include "mime.h"

#define CHUNK 65536
static char gbuf[CHUNK];
static int  glen;
static int  gboff;

static int recv_hdrs(SOCKET s) {
	glen = 0;
	while (glen < CHUNK - 1) {
		int n = recv(s, gbuf + glen, CHUNK - 1 - glen, 0);
		if (n <= 0) return -1;
		glen += n;
		gbuf[glen] = 0;
		if (strstr(gbuf, "\r\n\r\n")) {
			gboff = (int)(strstr(gbuf, "\r\n\r\n") - gbuf) + 4;
			return glen;
		}
	}
	return -1;
}

static void url_dec(char *s) {
	char *r = s, *w = s, hi, lo;
	while (*r) {
		if (*r == '%' && r[1] && r[2]) {
			hi = r[1]; lo = r[2];
			if (hi >= '0' && hi <= '9') hi -= '0';
			else { hi = (char)((hi & 0xDF) - 'A' + 10); if ((unsigned char)hi > 15) hi = 0; }
			if (lo >= '0' && lo <= '9') lo -= '0';
			else { lo = (char)((lo & 0xDF) - 'A' + 10); if ((unsigned char)lo > 15) lo = 0; }
			*w++ = (char)((hi << 4) | lo);
			r += 3;
		} else if (*r == '+') { *w++ = ' '; r++; }
		else *w++ = *r++;
	}
	*w = 0;
}

static long parse_cl(const char *s) {
	const char *p;
	p = strstr(s, "Content-Length:");
	if (!p) p = strstr(s, "content-length:");
	if (!p) p = strstr(s, "CONTENT-LENGTH:");
	if (!p) return 0;
	p += 15;
	while (*p == ' ' || *p == '\t') p++;
	return atol(p);
}

int http_recv_req(SOCKET s, request_t *req, char *body_data, int *body_len) {
	char *end;
	if (recv_hdrs(s) < 0) return -1;
	end = strstr(gbuf, "\r\n");
	req->method[0] = 0;
	req->path[0] = 0;
	if (end) {
		int hdr_end = (int)(end - gbuf);
		int i = 0, j;
		while (i < hdr_end && gbuf[i] != ' ') i++;
		if (i < hdr_end && i < 15) {
			memcpy(req->method, gbuf, i);
			req->method[i] = 0;
			i++;
			j = i;
			while (j < hdr_end && gbuf[j] != ' ' && gbuf[j] != '?' && j - i < 1023) j++;
			memcpy(req->path, gbuf + i, j - i);
			req->path[j - i] = 0;
		}
	}
	url_dec(req->path);
	req->content_length = parse_cl(gbuf);
	if (body_data && body_len && req->content_length > 0) {
		int have = glen - gboff;
		int want = *body_len;
		if (want > (int)req->content_length) want = (int)req->content_length;
		if (have > want) have = want;
		if (have > 0) memcpy(body_data, gbuf + gboff, have);
		*body_len = have;
		while (*body_len < want) {
			int n = recv(s, body_data + *body_len, want - *body_len, 0);
			if (n <= 0) break;
			*body_len += n;
		}
	}
	return 0;
}

int http_send_str(SOCKET s, int code, const char *mime, const char *body) {
	char hdr[2048];
	int hlen, blen = (int)strlen(body);
	hlen = sprintf(hdr,
		"HTTP/1.1 %d OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"Server: DeepSeek-HTTP\r\n"
		"\r\n", code, mime, blen);
	send(s, hdr, hlen, 0);
	send(s, body, blen, 0);
	return 0;
}

int http_send_file(SOCKET s, const char *fname) {
	FILE *f;
	struct _stat st;
	char hdr[2048];
	int hlen;

	if (_stat(fname, &st) < 0) {
		http_send_error(s, 404);
		return -1;
	}
	f = fopen(fname, "rb");
	if (!f) { http_send_error(s, 500); return -1; }
	hlen = sprintf(hdr,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %ld\r\n"
		"Connection: close\r\n"
		"Server: DeepSeek-HTTP\r\n"
		"\r\n", mime_type(fname), st.st_size);
	send(s, hdr, hlen, 0);
	{
		char buf[CHUNK];
		long remain = st.st_size;
		while (remain > 0) {
			int rd = (int)(remain > CHUNK ? CHUNK : remain);
			int n = (int)fread(buf, 1, rd, f);
			if (n <= 0) break;
			send(s, buf, n, 0);
			remain -= n;
		}
	}
	fclose(f);
	return 0;
}

int http_send_error(SOCKET s, int code) {
	const char *msg;
	char body[512];
	switch (code) {
		case 400: msg = "Bad Request"; break;
		case 403: msg = "Forbidden"; break;
		case 404: msg = "Not Found"; break;
		case 405: msg = "Method Not Allowed"; break;
		case 413: msg = "Payload Too Large"; break;
		case 500: msg = "Internal Server Error"; break;
		case 501: msg = "Not Implemented"; break;
		default:  msg = "Error"; break;
	}
	sprintf(body,
		"<html><head><title>%d %s</title></head>"
		"<body><h1>%d %s</h1></body></html>",
		code, msg, code, msg);
	return http_send_str(s, code, "text/html; charset=utf-8", body);
}

long http_recv_body_to_file(SOCKET s, long content_length, const char *fname) {
	FILE *f;
	long remain = content_length, written = 0;
	int n;
	f = fopen(fname, "wb");
	if (!f) return 0;

	{
		int inbuf = glen - gboff;
		if (inbuf > 0) {
			if (inbuf > remain) inbuf = (int)remain;
			fwrite(gbuf + gboff, 1, inbuf, f);
			written += inbuf;
			remain -= inbuf;
		}
	}

	while (remain > 0) {
		char tmp[CHUNK];
		int rd = (int)(remain > CHUNK ? CHUNK : remain);
		n = recv(s, tmp, rd, 0);
		if (n <= 0) break;
		fwrite(tmp, 1, n, f);
		written += n;
		remain -= n;
	}
	fclose(f);
	return written;
}