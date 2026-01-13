@echo off
setlocal EnableDelayedExpansion

if not exist "models" mkdir "models"

echo.
echo ========================================================
echo       A-Vision Model Selection & Setup
echo ========================================================
echo.
echo Please select which Object Detection model to use:
echo.
echo [1] YOLOv8 Nano (ONNX) - Recommended
echo     High accuracy, modern, fast (20-30ms)
echo.
echo [2] MobileNet V2 SSD FPN-Lite (ONNX)
echo     Fastest speed, good for very old CPUs
echo.
echo [3] MobileNet V1 SSD (Caffe)
echo     Legacy/Backup option
echo.
echo [4] Custom Model
echo     You provide your own .onnx/.tflite file
echo.

set /p choice="Enter your choice [1-4]: "

if "%choice%"=="1" goto setup_yolo
if "%choice%"=="2" goto setup_mobilenet_v2
if "%choice%"=="3" goto setup_mobilenet_v1
if "%choice%"=="4" goto setup_custom

echo Invalid choice. Exiting.
exit /b 1

:setup_yolo
echo.
echo [SETUP] Selected YOLOv8 Nano.
set MODEL_FILE=yolov8n.onnx
set MODEL_URL=https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/yolov8n.onnx
set MODEL_TYPE=YOLO_V8
set INPUT_SIZE=640
set MODEL_SCALE=0.00392156
REM Mean 0,0,0
set MODEL_MEAN=0,0,0
set SWAP_RB=true
goto download_and_config

:setup_mobilenet_v2
echo.
echo [SETUP] Selected MobileNet V2 SSD (ONNX).
set MODEL_FILE=MobileNet-v2.onnx
set MODEL_URL=https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/MobileNet-v2.onnx
set MODEL_TYPE=SSD_MOBILENET
set INPUT_SIZE=300
set MODEL_SCALE=1.0
set MODEL_MEAN=0,0,0
set SWAP_RB=false
goto download_and_config

:setup_mobilenet_v1
echo.
echo [SETUP] Selected Legacy MobileNet V1 (Caffe).
set MODEL_FILE=MobileNetSSD_deploy.caffemodel
set CONFIG_FILE=MobileNetSSD_deploy.prototxt
set MODEL_URL=https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/MobileNetSSD_deploy.caffemodel
set CONFIG_URL=https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/MobileNetSSD_deploy.prototxt
set MODEL_TYPE=SSD_MOBILENET
set INPUT_SIZE=300
set MODEL_SCALE=0.007843
set MODEL_MEAN=127.5,127.5,127.5
set SWAP_RB=false

REM Download Config separately
if not exist "models\%CONFIG_FILE%" (
    echo Downloading %CONFIG_FILE%...
    curl -L -o models/%CONFIG_FILE% %CONFIG_URL%
)
goto download_and_config

:setup_custom
echo.
echo [SETUP] Custom Model.
echo Please copy your model file into the 'models' folder.
set /p MODEL_FILE="Enter the filename (e.g. my_model.onnx): "
set /p MODEL_TYPE="Enter type [SSD_MOBILENET / YOLO_V8]: "
set /p INPUT_SIZE="Enter input size (e.g. 300 or 640): "
set MODEL_SCALE=0.007843
set MODEL_MEAN=127.5,127.5,127.5
set SWAP_RB=false
if "%MODEL_TYPE%"=="YOLO_V8" (
    set MODEL_SCALE=0.00392156
    set MODEL_MEAN=0,0,0
    set SWAP_RB=true
)
goto write_config

:download_and_config
if exist "models\%MODEL_FILE%" (
    echo [INFO] %MODEL_FILE% already exists. Skipping download.
) else (
    echo [DOWNLOAD] Downloading %MODEL_FILE%...
    curl -L -o models/%MODEL_FILE% %MODEL_URL%
    if !errorlevel! neq 0 (
        echo [ERROR] Download failed.
        exit /b 1
    )
)

:write_config
echo.
echo [CONFIG] Updating selected_model.json...

(
echo {
echo   "modelPath": "models/%MODEL_FILE%",
if defined CONFIG_FILE echo   "configPath": "models/%CONFIG_FILE%",
echo   "type": "%MODEL_TYPE%",
echo   "inputWidth": %INPUT_SIZE%,
echo   "inputHeight": %INPUT_SIZE%,
echo   "scale": %MODEL_SCALE%,
if "%SWAP_RB%"=="true" echo   "swapRB": true,
if "%SWAP_RB%"=="false" echo   "swapRB": false
echo }
) > models/selected_model.json

REM Fix JSON generic mean (Hard to print array in batch, simplistic approach)
REM We will let the Engine default mean if missing, or use a simpler sed approach if needed.
REM For now, allow Engine to handle defaults if mean is missing from JSON.

echo.
echo [SUCCESS] Setup complete!
echo Selected Model: %MODEL_FILE%
echo Configuration saved to models/selected_model.json
echo.
pause
