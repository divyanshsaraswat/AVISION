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
echo [1] YOLOv8 Nano (Standard - 640p)
echo     Best accuracy, standard speed (~5 FPS on CPU)
echo.
echo [2] MobileNet V1 SSD (Caffe)
echo     Legacy/Backup option
echo.
echo [3] Custom Model
echo     You provide your own .onnx/.tflite file
echo.

set /p choice="Enter your choice [1-3]: "

if "%choice%"=="1" goto setup_yolo
if "%choice%"=="2" goto setup_mobilenet_v1
if "%choice%"=="3" goto setup_custom

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
set DATASET=COCO
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
set DATASET=VOC

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
set /p MODEL_TYPE="Enter type [SSD_MOBILENET / SSD_TF / YOLO_V8]: "
set /p INPUT_SIZE="Enter input size (e.g. 300 or 640): "
set MODEL_SCALE=0.007843
set MODEL_MEAN=127.5,127.5,127.5
set SWAP_RB=false
set DATASET=VOC
if "%MODEL_TYPE%"=="YOLO_V8" (
    set MODEL_SCALE=0.00392156
    set MODEL_MEAN=0,0,0
    set SWAP_RB=true
    set DATASET=COCO
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
if "%SWAP_RB%"=="false" echo   "swapRB": false,
echo   "mean": "%MODEL_MEAN%",
echo   "dataset": "%DATASET%"
echo }
) > models/selected_model.json

echo.
echo [SETUP] Verifying/Downloading Depth Estimation Model...
set DEPTH_FILE=midas-v2_1-small-192x256.onnx
set DEPTH_URL=https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx

if exist "models\%DEPTH_FILE%" (
    echo [INFO] Depth model %DEPTH_FILE% already exists.
) else (
    echo [DOWNLOAD] Downloading Depth Model %DEPTH_FILE%...
    curl -L -o models/%DEPTH_FILE% %DEPTH_URL%
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to download Depth Model.
        echo         Depth estimation features will be disabled.
    ) else (
        echo [SUCCESS] Depth model downloaded.
    )
)

echo.
echo [SETUP] Verifying/Downloading MobileSAM Model (Fused)...
set SAM_FILE=mobile_sam.onnx
set ENCODER_URL=https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/mobile_sam_encoder.onnx
set DECODER_URL=https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/mobile_sam_decoder.onnx

set DECODER_FILE=mobile_sam_decoder.onnx

if exist "models\%ENCODER_FILE%" (
    echo [INFO] MobileSAM Encoder already exists.
) else (
    echo [DOWNLOAD] Downloading MobileSAM Encoder...
    curl -L -o models/%ENCODER_FILE% %ENCODER_URL%
)

if exist "models\%DECODER_FILE%" (
    echo [INFO] MobileSAM Decoder already exists.
) else (
    echo [DOWNLOAD] Downloading MobileSAM Decoder...
    curl -L -o models/%DECODER_FILE% %DECODER_URL%
)

set LAMA_FILE=big_lama.onnx
set LAMA_URL=https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/big-lama-standalone.onnx

if exist "models\%LAMA_FILE%" (
    echo [INFO] Big-LaMa already exists.
) else (
    echo [DOWNLOAD] Downloading Big-LaMa...
    curl -L -o models/%LAMA_FILE% %LAMA_URL%
)

echo.
echo [SUCCESS] Setup complete!
echo Selected Model: %MODEL_FILE%
echo Configuration saved to models/selected_model.json
echo.
pause
