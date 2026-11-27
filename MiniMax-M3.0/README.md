# hfs - HTTP 文件传输服务器

轻量级 Windows 控制台 HTTP 文件传输服务器，纯 C99 编写，无第三方依赖。

## 功能

- HTTP/1.1 服务，支持 keep-alive 长连接
- GET / HEAD 下载文件，支持 Range 断点续传（206 Partial Content）
- PUT 上传文件，返回带进度条、百分比、速度与字节数的 HTML 页面
- POST multipart/form-data 浏览器表单上传，返回上传结果页面
- 自动目录索引（auto_index），目录内存在 index_file 时优先返回该文件
- DELETE 删除文件
- 目录穿越防护（`..` 归一化后越界拦截）
- 可配置监听地址、端口、主目录、索引文件名、上传下载开关、上传大小上限

## 编译

```bat
build.bat
```

脚本自动调用 `vcvarsall.bat`（VS2017 x86），编译参数 `/W4 /WX /O2 /MT`，
产物在 `build\hfs.exe`。

编译环境变量（`build.bat` 顶部 `VCVARS`）：

```
VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat
```

## 运行

```bat
build\hfs.exe                       使用默认配置文件 hfs.conf
build\hfs.exe -c my.conf            指定配置文件
build\hfs.exe -p 9000               覆盖监听端口
build\hfs.exe -r D:\share          覆盖主目录
build\hfs.exe -a 127.0.0.1          覆盖监听地址
build\hfs.exe -g my.conf            生成默认配置文件后退出
build\hfs.exe -v                    控制台输出访问日志
build\hfs.exe -h                    显示帮助
```

首次运行若找不到 `hfs.conf`，程序会自动生成一份默认配置并打印提示。

## 配置

配置文件为 `key = value` 格式的 ini 风格文本，以 `#` 或 `;` 开头为注释。

| 配置项 | 默认值 | 说明 |
| --- | --- | --- |
| `listen_address` | `0.0.0.0` | 监听地址，`0.0.0.0` 表示所有网卡 |
| `listen_port` | `8080` | 监听端口，范围 1-65535 |
| `document_root` | `.` | 主目录（站点根目录） |
| `index_file` | `index.html` | 目录访问时优先返回的文件名，留空则只显示自动索引 |
| `auto_index` | `1` | 是否自动生成目录列表页 |
| `enable_upload` | `1` | 是否允许上传 |
| `enable_download` | `1` | 是否允许下载 |
| `default_charset` | `utf-8` | 文本响应字符集 |
| `max_upload_size` | `0` | 单次上传字节上限，0 表示不限制 |

布尔值接受 `1/0`、`true/false`、`yes/no`、`on/off`。

示例 `hfs.conf`：

```ini
# hfs configuration
listen_address = 0.0.0.0
listen_port = 8080
document_root = D:\share
index_file = index.html
auto_index = 1
enable_upload = 1
enable_download = 1
default_charset = utf-8
max_upload_size = 0
```

## 上传用法

命令行上传：

```bat
curl -X PUT --data-binary @file.zip http://127.0.0.1:8080/file.zip
curl -T file.zip http://127.0.0.1:8080/file.zip
```

浏览器上传：访问目录列表页，页面底部提供文件选择表单，提交后显示上传进度、
百分比、已传输字节数与总字节数、状态提示。

## 代码结构

```
include/            头文件
  hfs.h             公共类型与工具函数声明
  hfs_config.h      配置结构与配置读写
  hfs_net.h         socket 抽象
  hfs_fs.h          目录索引生成
  hfs_http.h        HTTP 服务器上下文
src/
  main.c            入口：参数解析、配置加载、监听循环
  config.c          配置解析与保存
  util.c            路径归一化、URL 编解码、MIME、日期、HTML 转义
  net.c             Winsock 封装：监听、接受、收发
  fs.c              目录扫描与自动索引 HTML 生成
  http.c            请求解析、GET/PUT/POST/DELETE、上传页面
build.bat           编译脚本
```

模块依赖单向：`main -> http -> {fs, net, util, config}`，无循环依赖。

## 实测结果

| 项目 | 结果 |
| --- | --- |
| 编译 | `/W4 /WX` 0 警告 0 错误 |
| EXE 体积 | 167 KB（参考值 1024 KB） |
| DLL 依赖 | `KERNEL32.dll`、`WS2_32.dll`（CRT 静态链接） |
| 功能测试 | 14 项全部通过：GET 文本/二进制（SHA256 一致）、目录索引、子目录索引、PUT 文本/300KB 二进制（SHA256 一致）、POST multipart、上传页面进度与速度、404、目录穿越拦截、Range 206、HEAD、DELETE |

## 说明

- 上传进度与速度由浏览器 `XMLHttpRequest` 的 `upload.onprogress` 事件实时驱动，
  服务端不参与轮询，因此上传期间页面可实时刷新进度条、百分比、速度与耗时。
- 单线程模型：同一时刻只处理一个连接，长连接通过 keep-alive 复用。
