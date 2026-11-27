#include "page.h"

void page_head(xbuf *b, const char *urlpath, int has_parent) {
    XB(b, "<!doctype html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"utf-8\">\n");
    XB(b, "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n<title>目录索引 ");
    html_escape(b, urlpath);
    XB(b, "</title>\n<style>\n");
    XB(b, "*{box-sizing:border-box}\n");
    XB(b, "body{margin:24px;background:#f5f6f8;color:#1f2328;");
    XB(b, "font-family:\"Segoe UI\",\"Microsoft YaHei\",Arial,sans-serif}\n");
    XB(b, "h1{margin:0 0 16px;font-size:20px;font-weight:600}\n");
    XB(b, "a{color:#0969da;text-decoration:none}\n");
    XB(b, "a:hover{text-decoration:underline}\n");
    XB(b, ".panel,.table{background:#fff;border:1px solid #d8dee4;border-radius:8px}\n");
    XB(b, ".panel{max-width:920px;margin-bottom:16px;padding:16px}\n");
    XB(b, ".cap{margin-bottom:10px;font-size:13px;color:#57606a}\n");
    XB(b, "input[type=file]{font-size:14px}\n");
    XB(b, "button{margin-left:8px;padding:6px 18px;border:0;border-radius:6px;");
    XB(b, "background:#0969da;color:#fff;font-size:14px;cursor:pointer}\n");
    XB(b, "button:hover{background:#0a5fc4}\n");
    XB(b, ".bar{height:12px;margin:12px 0 8px;overflow:hidden;border-radius:6px;background:#eaeef2}\n");
    XB(b, "#fill{width:0;height:12px;border-radius:6px;background:#0969da;transition:width .1s linear}\n");
    XB(b, "#stat{font-size:13px;color:#57606a}\n");
    XB(b, "table{width:100%;max-width:920px;border-collapse:collapse}\n");
    XB(b, "th,td{padding:8px 14px;border-bottom:1px solid #eaeef2;font-size:14px;text-align:left}\n");
    XB(b, "th{background:#f6f8fa;color:#57606a}\n");
    XB(b, "tr:last-child td{border-bottom:0}\n");
    XB(b, "td.num{text-align:right;white-space:nowrap;color:#57606a}\n");
    XB(b, "td.name a{word-break:break-all}\n");
    XB(b, ".dir td.name a{font-weight:600}\n");
    XB(b, "</style>\n</head>\n<body>\n");
    XB(b, "<h1>目录索引 ");
    html_escape(b, urlpath);
    XB(b, "</h1>\n");
    XB(b, "<div class=\"panel\">\n<div class=\"cap\">上传文件到当前目录</div>\n");
    XB(b, "<input type=\"file\" id=\"f\">\n<button onclick=\"upload()\">上传</button>\n");
    XB(b, "<div class=\"bar\"><div id=\"fill\"></div></div>\n");
    XB(b, "<div id=\"stat\">请选择文件,然后点击上传</div>\n</div>\n");
    XB(b, "<table class=\"table\">\n<tr><th>名称</th><th class=\"num\">大小</th><th class=\"num\">修改时间</th></tr>\n");
    if (has_parent) {
        XB(b, "<tr class=\"dir\"><td class=\"name\"><a href=\"../\">../</a></td>");
        XB(b, "<td class=\"num\"></td><td class=\"num\"></td></tr>\n");
    }
}

void page_row(xbuf *b, const char *name, int is_dir, const char *size_text, const char *time_text) {
    XB(b, "<tr");
    if (is_dir) XB(b, " class=\"dir\"");
    XB(b, "><td class=\"name\"><a href=\"");
    url_encode(b, name);
    if (is_dir) XB(b, "/");
    XB(b, "\">");
    html_escape(b, name);
    if (is_dir) XB(b, "/");
    XB(b, "</a></td><td class=\"num\">");
    if (!is_dir) html_escape(b, size_text);
    XB(b, "</td><td class=\"num\">");
    html_escape(b, time_text);
    XB(b, "</td></tr>\n");
}

void page_tail(xbuf *b) {
    XB(b, "</table>\n<script>\n");
    XB(b, "function fmt(n){if(n<1024)return n+' B';if(n<1048576)return (n/1024).toFixed(1)+' KB';");
    XB(b, "if(n<1073741824)return (n/1048576).toFixed(1)+' MB';return (n/1073741824).toFixed(2)+' GB';}\n");
    XB(b, "function show(s){document.getElementById('stat').textContent=s;}\n");
    XB(b, "function upload(){\n");
    XB(b, "var f=document.getElementById('f').files[0];\n");
    XB(b, "if(!f){show('请选择文件');return;}\n");
    XB(b, "var x=new XMLHttpRequest();var t=Date.now();\n");
    XB(b, "x.open('PUT',encodeURIComponent(f.name),true);\n");
    XB(b, "x.upload.onprogress=function(e){\n");
    XB(b, "if(!e.lengthComputable){show('上传中...');return;}\n");
    XB(b, "var p=e.loaded/e.total*100;var el=(Date.now()-t)/1000;var sp=el>0.1?e.loaded/el:0;\n");
    XB(b, "document.getElementById('fill').style.width=p.toFixed(1)+'%';\n");
    XB(b, "show('进度 '+p.toFixed(1)+'%   '+fmt(e.loaded)+' / '+fmt(e.total)+'   速度 '+fmt(sp)+'/s');\n");
    XB(b, "};\n");
    XB(b, "x.onload=function(){\n");
    XB(b, "if(x.status>=200&&x.status<300){show('上传完成');setTimeout(function(){location.reload();},500);}\n");
    XB(b, "else{show('上传失败,HTTP '+x.status);}\n");
    XB(b, "};\n");
    XB(b, "x.onerror=function(){show('上传失败,连接错误');};\n");
    XB(b, "x.send(f);\n");
    XB(b, "}\n");
    XB(b, "</script>\n</body>\n</html>\n");
}
