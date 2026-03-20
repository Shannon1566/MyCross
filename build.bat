@echo off
setlocal

where g++ >nul 2>nul
if errorlevel 1 (
  echo [ERROR] g++ not found. Please install MinGW-w64 and ensure g++ is in PATH.
  exit /b 1
)

echo Building crosshair.exe ...
g++ -std=c++17 -O2 -municode -mwindows crosshair.cpp -o crosshair.exe
if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b 1
)

echo [OK] Build complete: crosshair.exe
