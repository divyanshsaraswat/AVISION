#include "DepthEngine.h"
#include <opencv2/imgproc.hpp>
#include <iostream>

DepthEngine::DepthEngine() : isInitialized(false) {}

bool DepthEngine::init(const std::string& modelPath) {
    try {
        std::cout << "[DepthEngine] Loading model: " << modelPath << std::endl;
        net = cv::dnn::readNet(modelPath);
        
        // Use CUDA if available, else CPU
        /* 
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        */
        // Try OpenCL for speed boost on Intel integrated graphics
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL); 
        
        isInitialized = !net.empty();
        if (isInitialized) {
             std::cout << "[DepthEngine] Model loaded successfully. Using OpenCL if available." << std::endl;
        } else {
             std::cerr << "[DepthEngine] Error: Net is empty." << std::endl;
        }
        return isInitialized;
    } catch (const cv::Exception& e) {
        std::cerr << "[DepthEngine] Exception loading model: " << e.what() << std::endl;
        // Fallback to CPU
        try {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            isInitialized = !net.empty();
            std::cout << "[DepthEngine] Fallback to CPU." << std::endl;
            return isInitialized;
        } catch(...) { return false; }
    }
}

bool DepthEngine::init(const std::map<std::string, std::string>& params) {
    std::string modelPath = "models/midas-v2_1-small-192x256.onnx";
    
    // Check if path is provided in config
    auto it = params.find("modelPath");
    if (it != params.end()) {
        modelPath = it->second;
    }
    
    // Throttling
    if (params.count("eachFrame")) {
        std::string val = params.at("eachFrame");
        processEveryFrame = (val == "true" || val == "1");
    }
    if (params.count("interval")) {
        skipInterval = std::stoi(params.at("interval"));
    }
    
    std::cout << "[DepthEngine] Init: " << modelPath 
              << ", Interval: " << skipInterval 
              << ", Continuous: " << (processEveryFrame ? "YES" : "NO") << std::endl;
    return init(modelPath);
}


void DepthEngine::process(Context& ctx) {
    if (!isInitialized || ctx.rawFrame.empty()) return;
    
    internalFrameCount++;
    
    // 1. Check if previous async task is done
    if (isProcessing) {
        if (pendingFuture.valid() && pendingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            // Task completed!
            try {
                cv::Mat result = pendingFuture.get();
                if (!result.empty()) {
                    cachedDepthMap = result.clone();
                    // Log the time (we can't easily measure inside async without struct, 
                    // simplifying to just 'Async Update' or storing start time in member)
                    // For now, let's just mark it updated.
                    ctx.moduleLogs.push_back("[DepthModule] Updated (Async)");
                }
            } catch(...) {
                std::cerr << "[DepthModule] Async Error" << std::endl;
            }
            isProcessing = false;
        }
    }
    
    // 2. Launch new task if idle and interval met
    if (!isProcessing) {
        bool timeToRun = processEveryFrame || (internalFrameCount % skipInterval == 0);
        if (timeToRun) {
            isProcessing = true;
            
            // Clone frame for thread safety
            cv::Mat frameForThread = ctx.rawFrame.clone();
            
            // Launch Async
            pendingFuture = std::async(std::launch::async, [this, frameForThread]() {
                // Measure time inside thread if we want, but logging back is tricky.
                return estimateDepth(frameForThread);
            });
        }
    }

    // 3. Always Provide Cached Result (Non-blocking)
    if (!cachedDepthMap.empty()) {
        ctx.depthMap = cachedDepthMap.clone();
        
        // Re-run alert logic on the CACHED map every frame (cheap)
        // or just cache the alert too? Let's re-run alert on cache to make it responsive to UI
        if (!cachedDepthMap.empty()) {
            cv::Scalar meanDepth = cv::mean(cachedDepthMap);
            // ... (rest of alert logic can stay or be simplified) ...
             int cx = cachedDepthMap.cols / 4;
             int cy = cachedDepthMap.rows / 4;
             int cw = cachedDepthMap.cols / 2;
             int ch = cachedDepthMap.rows / 2;
             
             cv::Mat centerRegion = cachedDepthMap(cv::Rect(cx, cy, cw, ch));
             cv::Scalar centerMean = cv::mean(centerRegion);
             float centerAvg = (float)centerMean[0]; 
             
             if (centerAvg > 500.0f) { 
                  ctx.activeAlert = "Warning: Object Close!";
             }
        }
    }
}

cv::Mat DepthEngine::estimateDepth(const cv::Mat& inputFrame) {
    if (!isInitialized || inputFrame.empty()) return cv::Mat();

    // 1. Resize/Pre-process
    // We approximate the per-channel std [58.395, 57.12, 57.375] with average 57.63
    // This allows using the fast caching blobFromImage function instead of manual split/merge
    float scale = 1.0f / 57.63f;
    cv::Scalar meanVal(123.675, 116.28, 103.53);
    
    // SwapRB = true (BGR -> RGB)
    // crop = false
    cv::Mat blob = cv::dnn::blobFromImage(inputFrame, scale, cv::Size(inputWidth, inputHeight), meanVal, true, false);

    // 2. Inference
    net.setInput(blob);
    cv::Mat outputBlob = net.forward(); 

    // 3. Extract Output
    if (outputBlob.dims > 2) {
        int h_out = outputBlob.size[outputBlob.dims - 2];
        int w_out = outputBlob.size[outputBlob.dims - 1];
        
        cv::Mat depthMap(h_out, w_out, CV_32F, outputBlob.ptr<float>());
        return depthMap.clone(); 
    }

    return outputBlob;
}

