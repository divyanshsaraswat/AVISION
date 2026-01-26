$ErrorActionPreference = "Stop"

$jsonFile = "models/modules.json"
$modelsDir = "build/Release/models"

# Map Filenames to URLs
$urlMap = @{
    "MobileNetSSD_deploy.caffemodel" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/MobileNetSSD_deploy.caffemodel";
    "MobileNetSSD_deploy.prototxt" = "https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/deploy.prototxt";
    "midas-v2_1-small-192x256.onnx" = "https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx";
    "yolov8n.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/yolov8n.onnx";
    "resnet18_places365.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/resnet18_places365.onnx";
    "scene_classes.txt" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/googlenet_paces365_scene_classes.txt";
    "text_detection_db.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/text_detection_db.onnx";
    "text_recognition_crnn.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/text_recognition_crnn.onnx";
    "mobile_sam_encoder.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/mobile_sam_encoder.onnx";
    "mobile_sam_decoder.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/mobile_sam_decoder.onnx";
    "big_lama.onnx" = "https://github.com/divyanshsaraswat/onnx-models/releases/download/latest/big-lama-standalone.onnx"
}

if (-not (Test-Path $modelsDir)) {
    New-Item -ItemType Directory -Force -Path $modelsDir | Out-Null
}

if (Test-Path $jsonFile) {
    Write-Host "[Setup] Reading configuration from $jsonFile..."
    $content = Get-Content $jsonFile -Raw
    $json = $content | ConvertFrom-Json

    foreach ($module in $json.modules) {
        if ($module.enabled) {
            # Check params for modelPath/configPath
            $params = $module.params
            if ($params) {
                # Setup modelPath
                if ($params.modelPath) {
                    $path = $params.modelPath
                    $filename = Split-Path $path -Leaf
                    
                    if (-not (Test-Path $path)) {
                        if ($urlMap.ContainsKey($filename)) {
                            $url = $urlMap[$filename]
                            Write-Host "[Setup] Downloading $filename..."
                            # Use curl for better reliability with GitHub redirects/HTTPS
                            $procs = Start-Process -FilePath "curl.exe" -ArgumentList "-L", "-o", $path, $url -NoNewWindow -PassThru -Wait
                            
                            if ($procs.ExitCode -ne 0) {
                                Write-Host "[Setup] Error downloading $filename" -ForegroundColor Red
                                exit 1
                            }
                        } else {
                            Write-Host "[Setup] Warning: No URL known for $filename. Please download manually." -ForegroundColor Yellow
                        }
                    } else {
                         Write-Host "[Setup] $filename exists."
                    }
                }
                
                # Setup configPath
                if ($params.configPath) {
                    $path = $params.configPath
                    $filename = Split-Path $path -Leaf
                    
                    if (-not (Test-Path $path)) {
                        if ($urlMap.ContainsKey($filename)) {
                            $url = $urlMap[$filename]
                            Write-Host "[Setup] Downloading $filename..."
                             $procs = Start-Process -FilePath "curl.exe" -ArgumentList "-L", "-o", $path, $url -NoNewWindow -PassThru -Wait
                            
                            if ($procs.ExitCode -ne 0) {
                                Write-Host "[Setup] Error downloading $filename" -ForegroundColor Red
                                exit 1
                            }
                        }
                    }
                }

                # Setup classesPath
                if ($params.classesPath) {
                    $path = $params.classesPath
                    $filename = Split-Path $path -Leaf
                    
                    if (-not (Test-Path $path)) {
                        if ($urlMap.ContainsKey($filename)) {
                            $url = $urlMap[$filename]
                            Write-Host "[Setup] Downloading $filename..."
                             $procs = Start-Process -FilePath "curl.exe" -ArgumentList "-L", "-o", $path, $url -NoNewWindow -PassThru -Wait
                            
                            if ($procs.ExitCode -ne 0) {
                                Write-Host "[Setup] Error downloading $filename" -ForegroundColor Red
                                exit 1
                            }
                        }
                    }
                }

                # Setup OCR specific params
                foreach ($key in @("detectModel", "recogModel", "encoderPath", "decoderPath")) {
                    if ($params.$key) {
                        $path = $params.$key
                        $filename = Split-Path $path -Leaf
                        if (-not (Test-Path $path)) {
                            if ($urlMap.ContainsKey($filename)) {
                                $url = $urlMap[$filename]
                                Write-Host "[Setup] Downloading $filename..."
                                $procs = Start-Process -FilePath "curl.exe" -ArgumentList "-L", "-o", $path, $url -NoNewWindow -PassThru -Wait
                                if ($procs.ExitCode -ne 0) {
                                    Write-Host "[Setup] Error downloading $filename" -ForegroundColor Red
                                    exit 1
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} else {
    Write-Host "[Setup] Warning: $jsonFile not found."
}
