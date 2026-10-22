@echo off
REM Companion script: build-lnk.cmd (linker)
setlocal

set SCRIPT_DIR=%~dp0
set SRC=%SCRIPT_DIR%assembler\main.cpp
set OUT=%SCRIPT_DIR%build\assembler.exe

where g++ >nul 2>nul
if errorlevel 1 (
    echo [build-asm] g++ not found in PATH. Install MinGW and add its bin directory to PATH.
    exit /b 1
)

echo [build-asm] compiling "%SRC%" -^> "%OUT%"
g++ -std=c++17 -O2 "%SRC%" -o "%OUT%"
if errorlevel 1 (
    echo [build-asm] build failed.
    exit /b 1
)

echo [build-asm] build succeeded: "%OUT%"

endlocal
