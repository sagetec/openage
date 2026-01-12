@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo      OpenAge Windows Build Script
echo ==========================================

rem [Workaround] Check for spaces in path
echo %~dp0 | find " " >nul
if %errorlevel% equ 0 (
    echo [INFO] Detected spaces in project path.
    echo [INFO] Creating/Checking junction to avoid build errors...
    
    if not exist "C:\Users\azeem\OpenAge" (
        mklink /J "C:\Users\azeem\OpenAge" "%~dp0"
        if !errorlevel! neq 0 (
            echo [ERROR] Failed to create junction. Please run as Administrator or move the project to a path without spaces.
            pause
            exit /b 1
        )
    )
    
    echo [INFO] Switching execution to junction: C:\Users\azeem\OpenAge
    echo.
    cd /d "C:\Users\azeem\OpenAge"
    set "VCPKG_ROOT=C:\Users\azeem\OpenAge\vcpkg"
)

echo [1/4] Setting up Visual Studio Environment...
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Warning: vcvars64.bat not found in standard BuildTools location.
    echo Attempting to continue using current environment...
)

echo.
echo [2/4] Installing vcpkg dependencies...
echo This step downloads and compiles Qt and other libraries. It may take 1-2 hours.
rem Force bootstrap if vcpkg is not ready
if not exist "vcpkg\vcpkg.exe" call vcpkg\bootstrap-vcpkg.bat

rem [Fix] Clean previous failed builds that might have cached the wrong path
if exist "vcpkg\buildtrees\gperf" (
    echo [INFO] Cleaning cached gperf build files...
    rmdir /s /q "vcpkg\buildtrees\gperf"
)

.\vcpkg\vcpkg.exe install dirent eigen3 fontconfig freetype harfbuzz libepoxy libogg libpng opus opusfile qtbase qtdeclarative qtmultimedia toml11 --triplet x64-windows

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Dependency installation failed!
    echo Please check the vcpkg logs. Common issues include network timeouts or toolchain errors.
    pause
    exit /b %errorlevel%
)

echo.
echo [3/4] Configuring CMake...
if not exist build mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -G "Visual Studio 17 2022" -A x64

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    pause
    exit /b %errorlevel%
)

echo.
echo [4/4] Compiling OpenAge...
cmake --build . --config RelWithDebInfo

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo           Build Successful!
echo ==========================================
echo.
echo You can run the game using:
echo   bin\RelWithDebInfo\run.exe
echo   OR
echo   python -m openage main
echo.
pause
