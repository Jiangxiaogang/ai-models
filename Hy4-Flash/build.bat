@echo off
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARS% (echo vcvarsall.bat not found & exit /b 1)
call %VCVARS% x64 >nul
cl /nologo /c /W4 /O1 /MT /D_CRT_SECURE_NO_WARNINGS *.c
if errorlevel 1 exit /b 1
cl /nologo /W4 /O1 /MT /Fe:httpserver.exe *.obj ws2_32.lib
del *.obj >nul
echo Build done: httpserver.exe
