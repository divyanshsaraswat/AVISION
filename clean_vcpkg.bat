@echo off
echo [CLEANUP] This script will reclaim space by removing vcpkg build artifacts.
echo [WARNING] Only run this AFTER the build is 100%% complete and successful!
echo.
echo Sizes to be cleaned:
if exist "vcpkg\buildtrees" (
    echo - vcpkg\buildtrees (Intermediate build files)
)
if exist "vcpkg\downloads" (
    echo - vcpkg\downloads (Source code archives)
)
if exist "vcpkg\packages" (
    echo - vcpkg\packages (Uncompressed intermediates)
)
echo.
set /p response="Are you sure you want to delete these folders to save space? (y/n): "
if /i "%response%" neq "y" (
    echo Cleanup cancelled.
    exit /b
)

echo.
echo Removing buildtrees...
if exist "vcpkg\buildtrees" rmdir /s /q "vcpkg\buildtrees"

echo Removing downloads...
if exist "vcpkg\downloads" rmdir /s /q "vcpkg\downloads"

echo Removing packages...
if exist "vcpkg\packages" rmdir /s /q "vcpkg\packages"

echo.
echo [SUCCESS] Cleanup complete! You reclaimed significant space.
echo The installed libraries in 'vcpkg\installed' are safe and untouched.
pause
