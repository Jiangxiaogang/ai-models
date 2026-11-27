# HTTP 文件传输服务器

Win32 控制台程序，使用 C99 和 Win32/Winsock API 实现，无需第三方库。

## 功能

- 可配置监听端口和文件主目录
- 自动目录索引和子目录浏览
- GET 文件下载
- PUT 文件上传、覆盖上传和空文件上传
- 页面选择文件上传，实时显示进度和速度
- 二进制文件及大于 4 GB 文件的 64 位长度处理
- URL 编码、HTML 转义和目录穿越拦截

## 编译

在 Windows 命令行运行：

```bat
build.bat
```

默认使用 todo.txt 指定的 VS2017 编译环境，生成 32 位 `httpserver.exe`。编译参数包含 `/W4 /WX /O1 /MT`，当前构建无警告，EXE 约 139 KB，仅导入系统 `KERNEL32.dll` 和 `WS2_32.dll`。

如编译器安装位置不同，可先指定环境变量：

```bat
set "VCVARS=D:\path\to\vcvarsall.bat"
build.bat
```

## 配置

默认读取 EXE 同目录的 `server.ini`：

```ini
port=8080
root=www
```

`port` 范围为 1 至 65535。相对 `root` 以配置文件所在目录为基准；也可使用绝对路径。默认 `www` 目录不存在时会自动创建。

也可以指定其他配置文件：

```bat
httpserver.exe path\server.ini
```

## 使用

```bat
httpserver.exe
```

浏览器访问 `http://127.0.0.1:8080/`。目录页面可直接下载文件，也可选择文件后点击 Upload，上传期间显示百分比和速度。

命令行上传下载示例：

```bat
curl.exe -T local.bin http://127.0.0.1:8080/local.bin
curl.exe http://127.0.0.1:8080/local.bin -o downloaded.bin
```

服务器监听所有网卡且未提供身份认证，请仅在可信网络中使用，不要将敏感目录设为文件主目录。

## 测试

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\integration.ps1
```

测试使用 Windows 自带的 `curl.exe`，覆盖启动、自定义端口和目录、自动索引、页面进度与速度脚本、二进制上传下载、覆盖上传、空文件、子目录、URL 编码、404 和目录穿越拦截。测试数据位于 `build\verification`。

## 文件结构

- `src\main.c`：启动、Winsock 初始化和监听循环
- `src\config.c`：配置读取和主目录解析
- `src\http.c`：HTTP 解析、目录页面及上传下载
- `build.bat`：MSVC 构建脚本
- `server.ini`：默认配置
- `tests\integration.ps1`：集成测试
