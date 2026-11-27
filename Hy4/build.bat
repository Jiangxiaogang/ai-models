@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if exist "%VCVARS%" goto HAVE_VC
echo [ERROR] vcvarsall.bat not found
exit /b 1
:HAVE_VC
call "%VCVARS%" x86 >nul
if errorlevel 1 goto VC_FAIL
if not exist build mkdir build
echo [1/2] compiling...
cl.exe /nologo /c /W4 /WX /O1 /MT /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fo:build\ src\config.c src\util.c src\http.c src\page.c src\server.c src\main.c
if errorlevel 1 goto CL_FAIL
echo [2/2] linking...
link.exe /nologo /OUT:fileserver.exe /MACHINE:X86 /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF /RELEASE build\*.obj ws2_32.lib
if errorlevel 1 goto LINK_FAIL
echo [OK] fileserver.exe built
for %%F in (fileserver.exe) do echo [OK] size: %%~zF bytes
exit /b 0
:VC_FAIL
echo [ERROR] vcvarsall.bat failed
exit /b 1
:CL_FAIL
echo [ERROR] compile failed
exit /b 1
:LINK_FAIL
echo [ERROR] link failed
exit /b 1
