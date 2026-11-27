# MiniFileServer - HTTP 文件传输服务器

基于纯 C99 编写的 Win32 控制台 HTTP 文件传输服务器，支持目录自动索引、文件下载（断点续传）与 PUT 上传（页面实时显示进度与速度）。

## 1. 环境要求

- 编译环境: Visual Studio 2017 Professional（MSVC 19.16），路径见 `build.bat` 中的 `VCVARS`
- 运行依赖: 仅 Windows 系统自带 `KERNEL32.dll` 与 `WS2_32.dll`，CRT 静态链接，无需额外 DLL

## 2. 编译

```bat
build.bat
```

编译参数: `/W4 /WX /analyze /utf-8 /O2 /MT`（零警告、静态 CRT、代码静态分析全量通过）。
产物: `build\minifileserver.exe`（约 156 KB）。

## 3. 运行

```bat
build\minifileserver.exe config.ini
```

- 启动后访问 `http://127.0.0.1:8080/` 浏览目录
- 访问 `http://127.0.0.1:8080/upload.html` 上传文件（实时进度与速度）
- `Ctrl+C` 停止服务

## 4. 配置文件 `config.ini`

```ini
[server]
port = 8080          ; 监听端口（1-65535）
root = ./www         ; 主目录，可用相对路径或绝对路径
index = on           ; on=目录自动生成索引列表，off=关闭目录列表

[upload]
dir = uploads        ; 上传文件默认保存目录（相对主目录）
max_size = 1073741824  ; 单文件上限（字节），0 表示不限制
```

未提供配置文件时使用内置默认值（端口 8080、主目录当前目录）。

## 5. HTTP 接口

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET / HEAD | `/` | 目录自动索引页（若目录内存在 `index.html` 则优先返回它） |
| GET / HEAD | `/<文件>` | 下载文件，支持 `Range` 断点续传（返回 206 与 `Content-Range`） |
| GET / HEAD | `/upload.html` | 上传页面，含进度条、百分比、已传大小、实时速度与剩余时间 |
| PUT | `/upload?name=<相对路径>` | 上传文件，返回 JSON：`{"ok":true,"name":...,"size":...,"seconds":...,"speed":...}` |
| OPTIONS | 任意 | 返回 `Allow` 头 |

错误响应: 400 非法路径 / 403 目录索引关闭或无写权限 / 404 资源不存在 / 405 方法不支持 / 411 缺少 Content-Length / 413 超过大小上限 / 416 Range 非法。

安全约束: 所有请求路径经 `path_safe()` 归一化，`..` 越界、Windows 保留名与非法字符均被拦截，文件只能读写主目录之内。

## 6. 模块划分

| 文件 | 职责 |
| --- | --- |
| `src/util.c` | 动态字符串缓冲、字符串/URL 编解码、路径安全归一化、MIME、字节与时间格式化 |
| `src/config.c` | INI 配置解析与默认值 |
| `src/net.c` | Winsock 初始化、监听套接字、超时设置 |
| `src/http.c` | 请求行与头部解析、响应输出、Range 头处理 |
| `src/listing.c` | 目录索引页生成（按目录/名称排序，输出大小与修改时间） |
| `src/upload.c` | 上传页面生成、PUT 上传落盘与统计 |
| `src/handler.c` | 路由分发、文件传输、每连接一个线程 |
| `src/main.c` | 启动流程、配置装载、Ctrl+C 优雅退出 |

## 7. 自测脚本

`tests/upload_ui_test.js` 用 Node 模拟浏览器 DOM 与 `XMLHttpRequest`，校验上传页脚本是否真的发出 PUT 请求、进度与速度是否实时刷新：

```bat
curl.exe -s http://127.0.0.1:8080/upload.html -o up.html
node tests\upload_ui_test.js up.html http://127.0.0.1:8080 测试文件.bin
```
