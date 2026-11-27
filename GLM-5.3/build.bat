@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /O1 /W3 /MD /D_CRT_SECURE_NO_WARNINGS /TC ^
  src\main.c src\config.c src\server.c src\http.c src\handler.c src\page.c ^
  /Fe.\server.exe /Fo.\build\ /link /SUBSYSTEM:CONSOLE kernel32.lib ws2_32.lib user32.lib
endlocal
