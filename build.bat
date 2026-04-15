@echo off
setlocal EnableExtensions EnableDelayedExpansion

where cl >nul 2>nul
if errorlevel 1 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  set "VS_INSTALL_DIR="

  if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -property installationPath`) do (
      set "VS_INSTALL_DIR=%%i"
    )
  )

  if not "!VS_INSTALL_DIR!"=="" (
    set "DISCOVERED_VCVARS64=!VS_INSTALL_DIR!\VC\Auxiliary\Build\vcvars64.bat"
    if exist "!DISCOVERED_VCVARS64!" (
      echo Initializing MSVC environment from Visual Studio Build Tools...
      call "!DISCOVERED_VCVARS64!"
    ) else (
      echo [WARN] vswhere found Visual Studio, but vcvars64.bat was not found:
      echo !DISCOVERED_VCVARS64!
    )
  )

  where cl >nul 2>nul
  if errorlevel 1 (
    if "%VCVARS64_BAT%"=="" (
      echo [ERROR] cl not found.
      echo Install Build Tools for Visual Studio with the Desktop development with C++ workload.
      echo Optional fallback: set VCVARS64_BAT to the full path of vcvars64.bat.
      exit /b 1
    )

    if not exist "%VCVARS64_BAT%" (
      echo [ERROR] VCVARS64_BAT does not point to an existing file:
      echo %VCVARS64_BAT%
      exit /b 1
    )

    echo Initializing MSVC environment from VCVARS64_BAT...
    call "%VCVARS64_BAT%"
    if errorlevel 1 (
      echo [ERROR] Failed to initialize MSVC environment.
      exit /b 1
    )
  )

  where cl >nul 2>nul
  if errorlevel 1 (
    echo [ERROR] cl still not found after initializing the MSVC environment.
    exit /b 1
  )
)

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cmake not found. Ensure CMake is available in PATH.
  exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
  echo [ERROR] ninja not found. Ensure Ninja is available in PATH.
  exit /b 1
)

echo Configuring CMake preset: msvc-ninja-release ...
cmake --preset msvc-ninja-release
if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  exit /b 1
)

echo Building crosshair.exe ...
cmake --build --preset msvc-ninja-release
if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b 1
)

echo [OK] Build complete: build-msvc\crosshair.exe
