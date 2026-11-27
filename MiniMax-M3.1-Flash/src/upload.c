#include "upload.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void page_style(sbuf *out)
{
    sbuf_add(out,
             "<style>\n"
             "*{box-sizing:border-box}\n"
             "body{margin:0;background:#0f1419;color:#d7dde5;font-family:'Segoe UI','Microsoft YaHei',sans-serif}\n"
             "header{padding:22px 30px;background:#161c24;border-bottom:1px solid #232c38;display:flex;align-items:center;gap:16px}\n"
             "h1{font-size:18px;margin:0;font-weight:600}\n"
             "nav{margin-left:auto;display:flex;gap:16px}\n"
             "a{color:#4f9cf9;text-decoration:none}a:hover{text-decoration:underline}\n"
             ".wrap{max-width:880px;margin:26px auto;padding:0 20px}\n"
             ".card{background:#161c24;border:1px solid #232c38;border-radius:10px;padding:22px}\n"
             ".picker{display:block;border:1px dashed #3a4654;border-radius:8px;padding:30px;text-align:center;"
             "cursor:pointer;color:#8b98a9;transition:background .15s,border-color .15s}\n"
             ".picker:hover{border-color:#4f9cf9;color:#d7dde5;background:#131a22}\n"
             ".picker input{display:none}\n"
             ".row{display:flex;gap:10px;margin-top:14px;align-items:center;flex-wrap:wrap}\n"
             ".row.top{margin-top:0}\n"
             "input[type=text]{flex:1;min-width:180px;background:#0f1419;border:1px solid #232c38;border-radius:6px;"
             "color:#d7dde5;padding:9px 12px;font-size:14px}\n"
             "input[type=text]:focus{outline:none;border-color:#4f9cf9}\n"
             "button{background:#4f9cf9;border:0;border-radius:6px;color:#08131f;font-weight:600;padding:10px 24px;"
             "font-size:14px;cursor:pointer}\n"
             "button:disabled{opacity:.5;cursor:not-allowed}\n"
             ".item{margin-top:14px;padding:14px;background:#111820;border:1px solid #232c38;border-radius:8px}\n"
             ".fname{font-size:14px;word-break:break-all}\n"
             ".pct{margin-left:auto;color:#4f9cf9;font-variant-numeric:tabular-nums}\n"
             ".bar{height:8px;background:#1b222b;border-radius:4px;margin:10px 0;overflow:hidden}\n"
             ".fill{height:100%;width:0;background:linear-gradient(90deg,#3d8bfd,#4f9cf9);transition:width .15s}\n"
             ".meta{font-size:12px;color:#8b98a9;margin-top:0}\n"
             ".meta span{margin-right:20px}\n"
             ".ok{color:#3fb950}.err{color:#f85149}\n"
             "</style>\n");
}

static void page_script(sbuf *out)
{
    sbuf_add(out,
             "<script>\n"
             "(function(){\n"
             "var input=document.getElementById('file'),dir=document.getElementById('dir'),btn=document.getElementById('btn');\n"
             "function fmt(b){if(b<1024)return b+' B';if(b<1048576)return (b/1024).toFixed(1)+' KB';"
             "if(b<1073741824)return (b/1048576).toFixed(2)+' MB';return (b/1073741824).toFixed(2)+' GB';}\n"
             "function sp(v){if(!v||v<=0)return '--';if(v<1024)return v.toFixed(0)+' B/s';"
             "if(v<1048576)return (v/1024).toFixed(1)+' KB/s';return (v/1048576).toFixed(2)+' MB/s';}\n"
             "function eta(s){if(!isFinite(s)||s<0||s>604800)return '--';var m=Math.floor(s/60),x=Math.floor(s%60);"
             "return m>0?m+' 分 '+x+' 秒':x+' 秒';}\n"
             "function addItem(f){\n"
             "var d=document.createElement('div');d.className='item';\n"
             "d.innerHTML='<div class=\"row top\"><span class=\"fname\"></span><span class=\"pct\">0%</span></div>'+\n"
             "'<div class=\"bar\"><div class=\"fill\"></div></div>'+\n"
             "'<div class=\"row meta\"><span class=\"stat\">0 B / 0 B</span><span class=\"sp\">速度 --</span>'+\n"
             "'<span class=\"tm\">剩余 --</span></div>';\n"
             "d.querySelector('.fname').textContent=f.name;\n"
             "document.getElementById('list').appendChild(d);\n"
             "return {el:d,f:f};\n"
             "}\n"
             "function upload(item){\n"
             "return new Promise(function(resolve,reject){\n"
             "var f=item.f,xhr=new XMLHttpRequest();\n"
             "var sub=dir.value.replace(/^\\/+|\\/+$/g,'');\n"
             "var target=(sub?sub+'/':'')+f.name;\n"
             "xhr.open('PUT','/upload?name='+encodeURIComponent(target),true);\n"
             "xhr.setRequestHeader('X-File-Name',f.name);\n"
             "var last=0,lastT=performance.now(),cur=0;\n"
             "xhr.upload.onprogress=function(e){\n"
             "if(!e.lengthComputable)return;\n"
             "var now=performance.now(),dt=(now-lastT)/1000;\n"
             "if(dt>0){var v=(e.loaded-last)/dt;last=e.loaded;lastT=now;cur=cur?cur*0.6+v*0.4:v;}\n"
             "var pct=e.total?Math.round(e.loaded/e.total*100):0;\n"
             "item.el.querySelector('.fill').style.width=pct+'%';\n"
             "item.el.querySelector('.pct').textContent=pct+'%';\n"
             "item.el.querySelector('.stat').textContent=fmt(e.loaded)+' / '+fmt(e.total);\n"
             "item.el.querySelector('.sp').textContent='速度 '+sp(cur);\n"
             "item.el.querySelector('.tm').textContent='剩余 '+eta(cur>0?(e.total-e.loaded)/cur:0);\n"
             "};\n"
             "xhr.onload=function(){\n"
             "if(xhr.status>=200&&xhr.status<300){\n"
             "item.el.querySelector('.stat').innerHTML='<span class=\"ok\">上传完成</span>';\n"
             "item.el.querySelector('.pct').textContent='100%';\n"
             "item.el.querySelector('.fill').style.width='100%';\n"
             "item.el.querySelector('.sp').textContent='';item.el.querySelector('.tm').textContent='';\n"
             "resolve();\n"
             "}else{\n"
             "item.el.querySelector('.stat').innerHTML='<span class=\"err\">失败 HTTP '+xhr.status+'</span>';\n"
             "reject();\n"
             "}\n"
             "};\n"
             "xhr.onerror=function(){\n"
             "item.el.querySelector('.stat').innerHTML='<span class=\"err\">网络错误</span>';reject();};\n"
             "xhr.ontimeout=function(){\n"
             "item.el.querySelector('.stat').innerHTML='<span class=\"err\">超时</span>';reject();};\n"
             "xhr.send(f);\n"
             "});\n"
             "}\n"
             "btn.onclick=function(){\n"
             "var files=input.files;\n"
             "if(!files||!files.length){alert('请先选择文件');return;}\n"
             "btn.disabled=true;\n"
             "var items=[],i;\n"
             "for(i=0;i<files.length;i++)items.push(addItem(files[i]));\n"
             "var chain=Promise.resolve();\n"
             "items.forEach(function(it){chain=chain.then(function(){return upload(it);}).catch(function(){});});\n"
             "chain.then(function(){btn.disabled=false;});\n"
             "};\n"
             "})();\n"
             "</script>\n");
}

int upload_page_build(const app_config *cfg, sbuf *out)
{
    char dir_esc[512];
    if (fmt_escape_html(cfg->upload_dir, dir_esc, sizeof(dir_esc)) < 0)
        snprintf(dir_esc, sizeof(dir_esc), "uploads");
    sbuf_add(out,
             "<!DOCTYPE html>\n<html lang='zh-CN'>\n<head>\n"
             "<meta charset='utf-8'>\n"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>\n"
             "<title>文件上传</title>\n");
    page_style(out);
    sbuf_add(out,
             "</head>\n<body>\n"
             "<header><h1>文件上传</h1><nav><a href='/'>文件列表</a><a href='/upload.html'>上传</a></nav></header>\n"
             "<div class='wrap'>\n<div class='card'>\n"
             "<label class='picker'>点击选择文件，可多选<input id='file' type='file' multiple></label>\n"
             "<div class='row'><input type='text' id='dir' value='");
    sbuf_add(out, dir_esc);
    sbuf_add(out,
             "' placeholder='保存目录，例如 uploads'><button id='btn'>开始上传</button></div>\n"
             "<div id='list'></div>\n"
             "</div>\n</div>\n");
    page_script(out);
    sbuf_add(out, "</body>\n</html>\n");
    return 0;
}

static int make_parent_dir(const char *disk)
{
    char parent[UTIL_PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", disk);
    char *slash = strrchr(parent, '\\');
    char *slash2 = strrchr(parent, '/');
    if (!slash || (slash2 && slash2 > slash))
        slash = slash2;
    if (!slash)
        return 0;
    *slash = '\0';
    return dir_make_all(parent);
}

int upload_handle_put(conn *c, const http_request *req, const app_config *cfg)
{
    char name[UTIL_PATH_MAX];
    if (http_query_get(req->query, "name", name, sizeof(name)) != 0 || name[0] == '\0')
        return http_send_error(c, 400, "缺少 name 参数");
    char rel[UTIL_PATH_MAX];
    if (path_safe(name, rel, sizeof(rel)) != 0 || strcmp(rel, ".") == 0)
        return http_send_error(c, 400, "非法的目标路径");
    if (!req->has_content_length)
        return http_send_error(c, 411, "缺少 Content-Length");
    if (cfg->max_upload > 0 && req->content_length > cfg->max_upload)
        return http_send_error(c, 413, "文件超过服务器允许的最大大小");
    char disk[UTIL_PATH_MAX];
    if (path_join(disk, sizeof(disk), cfg->root, rel) != 0)
        return http_send_error(c, 400, "路径过长");
    if (make_parent_dir(disk) != 0)
        return http_send_error(c, 500, "无法创建保存目录");
    if (req->expect_continue)
        conn_send_text(c, "HTTP/1.1 100 Continue\r\n\r\n");
    HANDLE fh = CreateFileA(disk, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return http_send_error(c, 403, "无法创建目标文件");
    char *buf = (char *)malloc(HTTP_BODY_CHUNK);
    if (!buf) {
        CloseHandle(fh);
        return http_send_error(c, 500, "内存不足");
    }
    long long left = req->content_length;
    ULONGLONG started = GetTickCount64();
    int ok = 1;
    while (left > 0) {
        size_t want = (left < (long long)HTTP_BODY_CHUNK) ? (size_t)left : (size_t)HTTP_BODY_CHUNK;
        int got = conn_read_body(c, buf, want);
        if (got <= 0) {
            ok = 0;
            break;
        }
        DWORD written = 0;
        if (!WriteFile(fh, buf, (DWORD)got, &written, NULL) || written != (DWORD)got) {
            ok = 0;
            break;
        }
        left -= got;
    }
    free(buf);
    CloseHandle(fh);
    if (!ok) {
        DeleteFileA(disk);
        return http_send_error(c, 500, "上传过程中连接中断");
    }
    double secs = (double)(GetTickCount64() - started) / 1000.0;
    double speed = secs > 0 ? (double)req->content_length / secs : 0.0;
    char name_esc[UTIL_PATH_MAX];
    if (fmt_escape_html(rel, name_esc, sizeof(name_esc)) < 0)
        snprintf(name_esc, sizeof(name_esc), "?");
    char json[UTIL_PATH_MAX + 256];
    int n = snprintf(json, sizeof(json),
                     "{\"ok\":true,\"name\":\"%s\",\"size\":%lld,\"seconds\":%.3f,\"speed\":%.0f}",
                     name_esc, req->content_length, secs, speed);
    if (n < 0 || n >= (int)sizeof(json))
        return http_send_error(c, 500, "响应构造失败");
    if (http_send_head(c, 200, "application/json; charset=utf-8", (long long)n, NULL) != 0)
        return -1;
    printf("[upload] %s (%lld bytes, %.1f KB/s)\n", rel, req->content_length, speed / 1024.0);
    fflush(stdout);
    return conn_send_all(c, json, (size_t)n);
}
