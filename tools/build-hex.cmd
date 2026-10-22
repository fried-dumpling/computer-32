@echo off
REM Companion scripts: build-asm.cmd (assembler), build-lnk.cmd (linker)
setlocal

set SCRIPT_DIR=%~dp0
set SRC=%SCRIPT_DIR%hexdump\hexdump.cpp
set OUT=%SCRIPT_DIR%build\hexdump.exe

where g++ >nul 2>nul
if errorlevel 1 (
    echo [build-hex] g++ not found in PATH. Install MinGW and add its bin directory to PATH.
    exit /b 1
)

echo [build-hex] compiling "%SRC%" -^> "%OUT%"
g++ -std=c++17 -O2 "%SRC%" -o "%OUT%"
if errorlevel 1 (
    echo [build-hex] build failed.
    exit /b 1
)

echo [build-hex] build succeeded: "%OUT%"

endlocal
