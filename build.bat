@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cmake not found. Please install CMake and ensure it is in PATH.
  exit /b 1
)

where mingw32-make >nul 2>nul
if errorlevel 1 (
  echo [ERROR] mingw32-make not found. Please install MinGW-w64 and ensure mingw32-make is in PATH.
  exit /b 1
)

where gcc >nul 2>nul
if errorlevel 1 (
  echo [ERROR] gcc not found. Please install MinGW-w64 and ensure gcc is in PATH.
  exit /b 1
)

where g++ >nul 2>nul
if errorlevel 1 (
  echo [ERROR] g++ not found. Please install MinGW-w64 and ensure g++ is in PATH.
  exit /b 1
)

echo Configuring CMake...
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  exit /b 1
)

echo Building crosshair.exe ...
cmake --build build-mingw -j
if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b 1
)

echo [OK] Build complete: build-mingw\crosshair.exe
