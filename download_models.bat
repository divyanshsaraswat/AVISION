@echo off
if not exist "models" mkdir "models"

echo [DOWNLOAD] Fetching MobileNet-SSD Prototxt...
curl -L -o models/MobileNetSSD_deploy.prototxt https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/MobileNetSSD_deploy.prototxt

echo [DOWNLOAD] Fetching MobileNet-SSD Weights (Caffemodel)...
curl -L -o models/MobileNetSSD_deploy.caffemodel https://github.com/chuanqi305/MobileNet-SSD/raw/master/MobileNetSSD_deploy.caffemodel

if %errorlevel% neq 0 (
    echo [ERROR] Download failed. Please check your internet connection.
    exit /b 1
)

echo.
echo [SUCCESS] Model files downloaded to 'models/' directory!
echo.
pause
