@echo off
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat
call "%VCVARS%" x86 >nul 2>&1
if errorlevel 1 (
    echo toolchain not found
    exit /b 1
)
if not exist obj mkdir obj
cl /nologo /c /W4 /WX /O1 /MT /utf-8 /Fo:obj\ src\*.c
if errorlevel 1 exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /OPT:REF /OPT:ICF /OUT:fileserver.exe obj\*.obj
if errorlevel 1 exit /b 1
echo build ok
