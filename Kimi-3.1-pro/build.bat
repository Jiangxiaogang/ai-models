@echo off
setlocal
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" goto missing
call "%VCVARS%" x86
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /TC /W4 /WX /O1 /MT /GL /Gy /GF /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /Fo:build\ /Fe:httpserver.exe src\main.c src\config.c src\http.c /link /LTCG /OPT:REF /OPT:ICF ws2_32.lib
if errorlevel 1 goto failed
endlocal
exit /b 0
:failed
endlocal
exit /b 1
:missing
echo Compiler environment not found: %VCVARS%
endlocal
exit /b 1
