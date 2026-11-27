#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <io.h>
#include "handler.h"
#include "http.h"

#define MAX_PATH_LEN 520
#define MAX_BODY     (64 * 1024 * 1024LL)

static void safe_path(char *out, const char *root, const char *path, int size) {
	int rl = (int)strlen(root);
	int need_slash = (root[rl - 1] != '/' && root[rl - 1] != '\\');
	snprintf(out, size, "%s%s%s", root, need_slash ? "/" : "", path);
	{
		int i;
		for (i = 0; out[i]; i++)
			if (out[i] == '/') out[i] = '\\';
	}
}

static void dir_listing(SOCKET s, const char *abspath, const char *urlpath) {
	struct _finddata_t fd;
	intptr_t h;
	char pattern[MAX_PATH_LEN];
	char body[65536];
	int blen = 0;

	blen += snprintf(body + blen, sizeof(body) - blen,
		"<html><head><meta charset=\"utf-8\"><title>Index of %s</title>"
		"<style>body{font-family:Arial,sans-serif;max-width:800px;margin:20px auto;}"
		"table{border-collapse:collapse;width:100%%}"
		"td,th{padding:8px 12px;text-align:left;border-bottom:1px solid #eee}"
		"tr:hover{background:#f5f5f5}a{color:#2196F3;text-decoration:none}"
		".size{text-align:right}.upload{margin:10px 0}"
		".upload a{padding:8px 16px;background:#4CAF50;color:#fff;border-radius:4px}"
		"</style></head><body><h1>Index of %s</h1>"
		"<div class=\"upload\"><a href=\"/__upload.html?p=%s\">Upload File</a></div>"
		"<table><tr><th>Name</th><th>Size</th><th>Modified</th></tr>",
		urlpath, urlpath, urlpath);

	if (urlpath[0] && strcmp(urlpath, "/")) {
		const char *parent = "/";
		char *p, tmp[1024];
		strcpy(tmp, urlpath);
		p = strrchr(tmp, '/');
		if (p && p != tmp) { *p = 0; parent = tmp; }
		blen += snprintf(body + blen, sizeof(body) - blen,
			"<tr><td><a href=\"%s\">[..]</a></td><td></td><td></td></tr>", parent);
	}

	snprintf(pattern, sizeof(pattern), "%s\\*", abspath);
	h = _findfirst(pattern, &fd);
	if (h != -1) {
		do {
			if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
			if (fd.attrib & _A_HIDDEN) continue;
			if (blen + (int)strlen(fd.name) + 256 >= (int)sizeof(body) - 512) break;
			if (fd.attrib & _A_SUBDIR) {
				blen += snprintf(body + blen, sizeof(body) - blen,
					"<tr><td><a href=\"%s%s/\">%s/</a></td><td class=\"size\">-</td><td>-</td></tr>",
					urlpath, fd.name, fd.name);
			} else {
				char sz[32] = "-";
				if (fd.size < 1024) snprintf(sz, sizeof(sz), "%ld B", fd.size);
				else if (fd.size < 1048576) snprintf(sz, sizeof(sz), "%.1f KB", fd.size / 1024.0);
				else snprintf(sz, sizeof(sz), "%.1f MB", fd.size / 1048576.0);
				blen += snprintf(body + blen, sizeof(body) - blen,
					"<tr><td><a href=\"%s%s\">%s</a></td><td class=\"size\">%s</td><td>-</td></tr>",
					urlpath, fd.name, fd.name, sz);
			}
		} while (_findnext(h, &fd) == 0);
		_findclose(h);
	}

	blen += snprintf(body + blen, sizeof(body) - blen, "</table></body></html>");
	http_send_str(s, 200, "text/html; charset=utf-8", body);
}

static void serve_upload_page(SOCKET s, const char *urlpath) {
	char page[4096];
	snprintf(page, sizeof(page),
		"<html><head><meta charset=\"utf-8\"><title>Upload File</title>"
		"<style>"
		"*{box-sizing:border-box;margin:0;padding:0}"
		"body{font-family:Arial;background:#f0f2f5;min-height:100vh;display:flex;align-items:center;justify-content:center}"
		".c{background:#fff;padding:40px;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,.1);width:460px;max-width:95%%}"
		"h2{color:#333;margin-bottom:24px;text-align:center}"
		".file-wrap{position:relative;margin-bottom:24px}"
		".file-wrap input[type=file]{position:absolute;inset:0;opacity:0;cursor:pointer}"
		".file-wrap label{display:block;padding:40px 20px;border:2px dashed #d9d9d9;border-radius:8px;text-align:center;color:#888;cursor:pointer;transition:all .2s}"
		".file-wrap label:hover,.file-wrap.drag label{border-color:#4CAF50;color:#4CAF50;background:#f6fef6}"
		".file-wrap.has label{border-style:solid;color:#333}"
		"#fn{display:block;margin-top:8px;font-size:13px;color:#666;word-break:break-all}"
		"#bar-wrap{height:6px;background:#e8e8e8;border-radius:6px;margin-bottom:12px;overflow:hidden;display:none}"
		"#bar{height:100%%;width:0%%;background:linear-gradient(90deg,#4CAF50,#45a049);border-radius:6px;transition:width .1s}"
		"#info{text-align:center;font-size:13px;color:#888;margin-bottom:12px;display:none}"
		"#btn{width:100%%;padding:12px;background:#4CAF50;color:#fff;border:none;border-radius:6px;font-size:16px;cursor:pointer;transition:.2s}"
		"#btn:hover{background:#45a049}#btn:disabled{opacity:.6;cursor:not-allowed}"
		"#msg{margin-top:12px;text-align:center;font-size:13px;color:#388E3C;display:none}"
		"</style></head><body>"
		"<div class=\"c\"><h2>Upload File</h2>"
		"<div class=\"file-wrap\" id=\"fw\">"
		"<input type=\"file\" id=\"fi\" multiple>"
		"<label for=\"fi\">Click or drag files here</label>"
		"</div><span id=\"fn\"></span>"
		"<div id=\"bar-wrap\"><div id=\"bar\"></div></div>"
		"<div id=\"info\"></div>"
		"<button id=\"btn\" onclick=\"start()\">Upload</button>"
		"<div id=\"msg\"></div></div>"
		"<script>"
		"var u='%s';if(!u.endsWith('/'))u+='/';"
		"var f=null;"
		"document.getElementById('fi').onchange=function(e){f=e.target.files[0];if(f){document.getElementById('fn').textContent=f.name;document.getElementById('fw').classList.add('has');}};"
		"var fw=document.getElementById('fw');"
		"fw.ondragover=function(e){e.preventDefault();fw.classList.add('drag');};"
		"fw.ondragleave=function(){fw.classList.remove('drag');};"
		"fw.ondrop=function(e){e.preventDefault();fw.classList.remove('drag');var fs=e.dataTransfer.files;if(fs.length)document.getElementById('fi').files=fs;document.getElementById('fi').onchange({target:{files:fs}});};"
		"function fmt(v){if(v<1024)return v.toFixed(0)+' B';if(v<1048576)return(v/1024).toFixed(1)+' KB';return(v/1048576).toFixed(1)+' MB';}"
		"function start(){if(!f)return alert('Please select a file');"
		"var bar=document.getElementById('bar'),bw=document.getElementById('bar-wrap'),info=document.getElementById('info'),btn=document.getElementById('btn'),msg=document.getElementById('msg');"
		"bw.style.display='block';info.style.display='block';msg.style.display='none';btn.disabled=true;"
		"var x=new XMLHttpRequest(),start=Date.now(),last=0,lastT=start;"
		"x.upload.onprogress=function(e){if(!e.lengthComputable)return;"
		"var now=Date.now(),dt=(now-lastT)/1000;if(dt>=0.3){"
		"var pct=(e.loaded/e.total*100).toFixed(0);bar.style.width=pct+'%%';"
		"var sp=(e.loaded-last)/dt;"
		"info.textContent=pct+'%%  '+fmt(e.loaded)+' / '+fmt(e.total)+'  '+fmt(sp)+'/s';last=e.loaded;lastT=now;}};"
		"x.open('PUT',u+encodeURIComponent(f.name),true);"
		"x.onload=function(){if(x.status>=200&&x.status<300){msg.style.display='block';msg.textContent='Upload success!';bar.style.width='100%%';info.textContent='Done in '+((Date.now()-start)/1000).toFixed(1)+'s';}"
		"else{msg.style.display='block';msg.style.color='#D32F2F';msg.textContent='Error: '+x.status;}btn.disabled=false;};"
		"x.onerror=function(){msg.style.display='block';msg.style.color='#D32F2F';msg.textContent='Network error';btn.disabled=false;};"
		"x.send(f);}"
		"</script></body></html>",
		urlpath);
	http_send_str(s, 200, "text/html; charset=utf-8", page);
}

void handle_request(SOCKET s, const request_t *req, const char *body_data, int body_len, const config_t *cfg) {
	char abspath[MAX_PATH_LEN];
	char tmp[MAX_PATH_LEN];
	struct _stat st;
	(void)body_data;
	(void)body_len;

	if (strcmp(req->method, "GET") == 0) {
		if (strncmp(req->path, "/__upload.html", 14) == 0) {
			const char *up = req->path + 14;
			if (*up == '?' && strncmp(up + 1, "p=", 2) == 0) up += 3;
			snprintf(tmp, sizeof(tmp), "%s/", up[0] ? up : "/");
			serve_upload_page(s, tmp);
			return;
		}

		safe_path(abspath, cfg->root, req->path, sizeof(abspath));
		if (_stat(abspath, &st) < 0) {
			http_send_error(s, 404);
			return;
		}
		if (st.st_mode & _S_IFDIR) {
			if (req->path[strlen(req->path) - 1] != '/') {
				char loc[2048];
				int len = (int)strlen(req->path);
				memcpy(loc, req->path, len);
				loc[len] = '/';
				loc[len + 1] = 0;
				{
					char esc[4096];
					int hlen = snprintf(esc, sizeof(esc),
						"HTTP/1.1 301 Moved\r\nLocation: %s\r\nConnection: close\r\nServer: DeepSeek-HTTP\r\n\r\n", loc);
					send(s, esc, hlen, 0);
				}
				return;
			}
			dir_listing(s, abspath, req->path);
			return;
		}
		http_send_file(s, abspath);
	} else if (strcmp(req->method, "PUT") == 0) {
		if (req->content_length <= 0 || req->content_length > MAX_BODY) {
			http_send_error(s, req->content_length > MAX_BODY ? 413 : 400);
			return;
		}
		safe_path(abspath, cfg->root, req->path, sizeof(abspath));
		{
			long written = http_recv_body_to_file(s, req->content_length, abspath);
			if (written == req->content_length) {
				http_send_str(s, 201, "text/plain; charset=utf-8", "OK");
			} else {
				http_send_error(s, 500);
			}
		}
	} else {
		http_send_error(s, 501);
	}
}