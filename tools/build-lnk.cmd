@echo off
REM Companion script: build-asm.cmd (assembler)
setlocal

set SCRIPT_DIR=%~dp0
set SRC=%SCRIPT_DIR%linker\main.cpp
set OUT=%SCRIPT_DIR%build\linker.exe

where g++ >nul 2>nul
if errorlevel 1 (
    echo [build-lnk] g++ not found in PATH. Install MinGW and add its bin directory to PATH.
    exit /b 1
)

echo [build-lnk] compiling "%SRC%" -^> "%OUT%"
g++ -std=c++17 -O2 "%SRC%" -o "%OUT%"
if errorlevel 1 (
    echo [build-lnk] build failed.
    exit /b 1
)

echo [build-lnk] build succeeded: "%OUT%"

endlocal
