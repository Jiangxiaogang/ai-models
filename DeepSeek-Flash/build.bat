@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%VCVARS%" goto no_vcvars
call "%VCVARS%" x86 >nul
if errorlevel 1 goto no_env

if not exist build mkdir build
cl /nologo /W4 /WX /O2 /MT /utf-8 /wd4819 /Fo:build\ /Fe:build\fileserver.exe src\*.c /link /SUBSYSTEM:CONSOLE ws2_32.lib
if errorlevel 1 goto build_fail

copy /y config.ini build\ >nul
if not exist build\www mkdir build\www
copy /y www\README.txt build\www\ >nul

for %%A in (build\fileserver.exe) do echo Size: %%~zA bytes
echo Build OK: build\fileserver.exe
exit /b 0

:no_vcvars
echo [ERROR] vcvarsall.bat not found
exit /b 1

:no_env
echo [ERROR] failed to initialize MSVC environment
exit /b 1

:build_fail
echo [ERROR] build failed
exit /b 1
