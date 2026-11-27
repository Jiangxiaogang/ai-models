@echo off
setlocal

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat
if not exist "%VCVARS%" set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat

call "%VCVARS%" x86 >nul

cd /d "%~dp0"

if not exist build mkdir build

cl /nologo /W4 /WX /O2 /MT /GS /D_CRT_SECURE_NO_WARNINGS ^
   /I include ^
   src\main.c src\config.c src\util.c src\net.c src\fs.c src\http.c ^
   /Fo:build\ /Fe:build\hfs.exe ^
   /link /OPT:REF /OPT:ICF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo BUILD OK
endlocal
