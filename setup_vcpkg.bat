@echo off
echo [SETUP] Checking for git...
git --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git is not installed or not in PATH. Please install Git first.
    exit /b 1
)

if not exist "vcpkg" (
    echo [SETUP] vcpkg not found. Cloning...
    git clone https://github.com/microsoft/vcpkg.git
) else (
    echo [SETUP] vcpkg folder exists. Pulling latest...
    cd vcpkg
    git pull
    cd ..
)

echo [SETUP] Bootstrapping vcpkg...
call vcpkg\bootstrap-vcpkg.bat

echo.
echo [SUCCESS] vcpkg is installed!
echo.
echo To build the project with dependencies, run:
echo     cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
echo     cmake --build build
echo.
pause
