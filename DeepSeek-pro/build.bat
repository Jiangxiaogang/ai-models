@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 (
    echo Failed to set up VS2017 build environment
    exit /b 1
)

set CFLAGS=/nologo /W4 /WX /O1 /MT /GS- /GL /D_CRT_SECURE_NO_WARNINGS
set LFLAGS=/link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:CONSOLE

echo Compiling HTTP File Server...
cl.exe %CFLAGS% main.c config.c http.c handler.c mime.c /Fe:httpfs.exe %LFLAGS% ws2_32.lib kernel32.lib

if errorlevel 1 (
    echo Build FAILED
    exit /b 1
)

echo.
echo Build SUCCESS
echo.
dir httpfs.exe | find ".exe"
echo.
pause