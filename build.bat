@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cmake not found. Please install CMake and ensure it is in PATH.
  exit /b 1
)

set "VCVARS64=D:\Program Files\VS\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS64%" (
  echo [ERROR] vcvars64.bat not found: %VCVARS64%
  exit /b 1
)

call "%VCVARS64%"
if errorlevel 1 (
  echo [ERROR] Failed to initialize MSVC environment.
  exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cl not found after vcvars64 initialization.
  exit /b 1
)

where nmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] nmake not found after vcvars64 initialization.
  exit /b 1
)

echo Configuring CMake (MSVC + NMake, x64)...
cmake -S . -B build-msvc --fresh -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=nmake
if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  exit /b 1
)

echo Building crosshair.exe ...
cmake --build build-msvc
if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b 1
)

echo [OK] Build complete: build-msvc\crosshair.exe
