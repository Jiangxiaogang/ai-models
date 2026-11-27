@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if exist "%VCVARS%" goto :havevc
echo [build] vcvarsall.bat not found: %VCVARS%
exit /b 1
:havevc
call "%VCVARS%" x86 >nul
if errorlevel 1 goto :envfail
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\utf8norm.ps1
if not exist build mkdir build
del /q build\*.obj >nul 2>nul
cl /nologo /W4 /WX /analyze /wd6031 /wd4819 /utf-8 /O2 /MT /TC src\util.c src\config.c src\net.c src\http.c src\listing.c src\upload.c src\handler.c src\main.c /Fe:build\minifileserver.exe /Fo:build\ /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /OPT:REF /OPT:ICF ws2_32.lib
if errorlevel 1 goto :compilefail
echo [build] build ok: build\minifileserver.exe
exit /b 0
:envfail
echo [build] failed to init build environment
exit /b 1
:compilefail
echo [build] compile failed
exit /b 1
