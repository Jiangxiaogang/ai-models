# mini-httpd

Win32 控制台 HTTP 文件传输服务器，纯 C (C99) 编写。

## 编译

运行 `build.bat`（VS2017 x86 工具链，输出 `httpd.exe`，/W4 /WX 零警告）。

## 使用

将 `httpd.exe` 与 `httpd.conf` 放在同一目录运行即可。

配置文件 `httpd.conf`：

    port=8080
    root=files

- `port`：监听端口（1~65535，默认 8080）
- `root`：服务主目录（相对路径基于 exe 所在目录，目录不存在时自动创建）

文件不存在配置时以默认值启动。

## 功能

- 自动 index：浏览器打开任意目录路径列出文件（名称、大小、修改时间）
- GET 下载任意文件（含中文文件名， attachment 下载）
- PUT 上传：页面上传按钮/拖拽，实时显示进度条、已传字节与速度
- 上传至不存在子目录时自动创建
- 拒绝 `..` 路径穿越

## 模块

    src\util.c     URL 解码、编码转换、MIME、路径安全
    src\config.c   配置文件解析
    src\page.c     自动 index 页面与上传脚本
    src\http.c     请求解析、GET/PUT 处理
    src\server.c   监听与连接线程
    src\main.c     入口
