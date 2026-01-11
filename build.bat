@echo off
if not exist "vcpkg" (
    echo [ERROR] vcpkg folder not found. Please run setup_vcpkg.bat first.
    exit /b 1
)

echo [BUILD] Configuring project with vcpkg toolchain...
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [BUILD] compiling...
cmake --build build --config Release

if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo [SUCCESS] Build complete!
echo Run the app with: build\Release\AVision.exe
echo.
pause
