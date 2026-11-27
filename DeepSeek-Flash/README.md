# HTTP 文件传输服务器

纯 C（C99）编写的 Windows 控制台 HTTP 文件服务器，支持目录浏览、GET 下载和 PUT 上传。

## 功能

- 监听端口、主目录可通过配置文件修改
- 自动目录索引：目录下有 `index.html` 时优先显示，否则生成文件列表
- GET / HEAD 下载文件，支持中文文件名
- PUT 上传文件，网页端显示进度条、已完成大小和实时速度
- 支持拖拽上传、多文件队列上传
- 上传自动创建多级目录，写盘使用临时文件，避免产生半个文件
- 拦截 `..`、绝对路径等目录穿越请求

## 构建

需要 Visual Studio 2017（或更高版本）的 C 编译器。

```
build.bat
```

产物：

- `build\fileserver.exe`：静态链接 CRT，仅依赖 `WS2_32.dll` 和 `KERNEL32.dll`
- `build\config.ini`：配置文件
- `build\www\`：默认主目录

## 运行

```
build\fileserver.exe
```

浏览器访问 `http://127.0.0.1:8080/`。

也可以指定配置文件：

```
build\fileserver.exe D:\conf\server.ini
```

按 `Ctrl+C` 停止服务。

## 配置

`config.ini`：

```ini
port=8080
root=www
index=index.html
```

| 键 | 说明 | 默认值 |
| --- | --- | --- |
| `port` | 监听端口 | `8080` |
| `root` | 主目录，相对路径以 EXE 所在目录为基准 | `www` |
| `index` | 目录默认首页文件名，留空则总是显示目录列表 | `index.html` |

以 `#` 或 `;` 开头的行为注释。

## 目录结构

```
build.bat        编译脚本
config.ini       配置文件
src\
  common.h       公共头文件与常量
  util.c/h       缓冲、编码、URL、路径、文件系统工具
  net.c/h        Socket 监听与带缓冲读写
  config.c/h     INI 配置解析
  http.c/h       HTTP 解析、下载、上传、目录索引与页面
  main.c         程序入口与监听循环
www\             默认主目录
```
