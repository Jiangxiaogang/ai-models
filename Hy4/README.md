# CFileServer

Win32 控制台 HTTP 文件传输服务器，纯 C 语言编写，静态链接，单文件 EXE。

## 功能

- 监听端口与主目录可配置（`server.ini` 或命令行）
- 自动 index：目录存在 `index.html` / `index.htm` 时直接返回，否则生成目录列表
- GET 下载文件，HEAD 探测，PUT 上传文件
- 上传页面内嵌 JavaScript，显示进度百分比、即时速度、平均速度与耗时
- UTF-8 路径支持，中文文件名正常显示
- 路径穿越防护，仅允许访问主目录内文件
- 多线程并发处理，静态链接 CRT，依赖系统 DLL 极少

## 编译

双击 `build.bat`，或命令行执行：

```
build.bat
```

输出 `fileserver.exe`。编译环境使用 `todo.txt` 中指定的 VS2017 `vcvarsall.bat`，参数 `/W4 /WX /O1 /MT`，保证 0 警告。

## 配置

`server.ini`：

```
port=8080
root=D:\share
```

也可命令行覆盖：

```
fileserver.exe --port 9000 --root D:\share
fileserver.exe myconfig.ini --port 9000
```

优先级：命令行参数 > 配置文件 > 默认值（端口 8080，当前目录）。

## 使用

运行后用浏览器打开 `http://127.0.0.1:8080/`：

- 点击文件名下载
- 页面底部选择文件，点击「上传到本目录」上传，实时显示进度与速度
- 也可使用标准 PUT 客户端：

```
curl -X PUT --data-binary @file.bin http://127.0.0.1:8080/file.bin
```

## 模块

| 文件 | 职责 |
| --- | --- |
| `src/common.h` | 公共结构与接口声明 |
| `src/config.c` | 配置文件与命令行参数 |
| `src/util.c` | 字符串、路径、URL、编码、MIME 工具 |
| `src/http.c` | HTTP 解析、响应、文件发送与接收 |
| `src/page.c` | 目录列表页与上传页面、PUT 处理 |
| `src/server.c` | WinSock 监听与多线程调度 |
| `src/main.c` | 入口 |

## 依赖

`KERNEL32.DLL`、`WS2_32.DLL`（均为 Windows 系统自带），CRT 静态链接，无 MSVCRT 等额外 DLL。
