@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b 1
if not exist obj mkdir obj
cl /nologo /W4 /WX /O1 /MT /GF /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN ^
   /Foobj\ /Fehttpd.exe src\main.c src\config.c src\util.c src\page.c src\http.c src\server.c ^
   /link ws2_32.lib /OPT:REF /OPT:ICF
if errorlevel 1 exit /b 1
echo [build] ok: httpd.exe
