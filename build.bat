@echo off
echo BUILD_BAT_ARG1: "%1" > build_debug.txt
if /I "%~1"=="core" goto :core_build

if not exist "vcpkg" (
    echo [ERROR] vcpkg folder not found. Please run setup_vcpkg.bat first.
    exit /b 1
)

echo [BOOTSTRAP] Building Configuration Tool...
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release > nul
cmake --build build --config Release --target AVisionConfig > nul

if %errorlevel% neq 0 (
    echo [ERROR] Failed to build ConfigTool. Proceeding with default build...
    goto :core_build
)

echo [BOOTSTRAP] Launching Configuration Tool...
build\Release\AVisionConfig.exe
exit /b

:core_build
echo [SETUP] Checking and downloading models...
powershell -ExecutionPolicy Bypass -File setup_models.ps1
if %errorlevel% neq 0 (
    echo [ERROR] Model setup failed.
    exit /b 1
)

echo [BUILD] Configuring project...
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [BUILD] compiling...
cmake --build build --config Release --target AVision AVisionCLI

if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo [SUCCESS] Build complete!
echo Run the app with: build\Release\AVision.exe
echo.
if "%1" neq "--step=core" pause
