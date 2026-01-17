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
        // Build for CPU Edge usually
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        
        isInitialized = !net.empty();
        if (isInitialized) {
             std::cout << "[DepthEngine] Model loaded successfully." << std::endl;
        } else {
             std::cerr << "[DepthEngine] Error: Net is empty." << std::endl;
        }
        return isInitialized;
    } catch (const cv::Exception& e) {
        std::cerr << "[DepthEngine] Exception loading model: " << e.what() << std::endl;
        return false;
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
    bool shouldRun = processEveryFrame || (internalFrameCount % skipInterval == 0);

    if (!shouldRun) {
        // Throttled: Use Cache
        if (!cachedDepthMap.empty()) {
            ctx.depthMap = cachedDepthMap.clone();
            if (!cachedAlert.empty()) ctx.activeAlert = cachedAlert;
        }
        return;
    }

    double t_start = (double)cv::getTickCount();
    
    // Run estimation
    cv::Mat result = estimateDepth(ctx.rawFrame);
    
    // Update Cache
    if (!result.empty()) {
        cachedDepthMap = result.clone();
        ctx.depthMap = result;
    }
    
    // Reset alert for this new frame analysis
    cachedAlert = ""; 

    // Check for "Close Object" (High Average Depth)
    if (!ctx.depthMap.empty()) {
        // MiDaS: High value = Close
        cv::Scalar meanDepth = cv::mean(ctx.depthMap);
        float avgDepth = (float)meanDepth[0];
        
        int cx = ctx.depthMap.cols / 4;
        int cy = ctx.depthMap.rows / 4;
        int cw = ctx.depthMap.cols / 2;
        int ch = ctx.depthMap.rows / 2;
        
        cv::Mat centerRegion = ctx.depthMap(cv::Rect(cx, cy, cw, ch));
        cv::Scalar centerMean = cv::mean(centerRegion);
        float centerAvg = (float)centerMean[0]; 
        
        if (centerAvg > 500.0f) { 
             cachedAlert = "Warning: Object Close!";
             ctx.activeAlert = cachedAlert;
        }
    }
    
    double t_end = (double)cv::getTickCount();
    double time_ms = ((t_end - t_start) / cv::getTickFrequency()) * 1000.0;
    
    if (!processEveryFrame) {
         std::cout << "[DepthModule] Inference Time: " << time_ms << " ms" << std::endl;
    }
}

cv::Mat DepthEngine::estimateDepth(const cv::Mat& inputFrame) {
    if (!isInitialized || inputFrame.empty()) return cv::Mat();

    // 1. Resize/Pre-process
    // "preprocessing": {"mean": [123.675, 116.28, 103.53], "scale": [58.395, 57.12, 57.375], "reverse_channels": true}
    // "scale" in config usually is STD. So (Pixel - Mean) / Scale.
    
    cv::Mat resized;
    cv::resize(inputFrame, resized, cv::Size(inputWidth, inputHeight));

    // Convert BGR to RGB (reverse_channels: true)
    cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);

    // Manual normalization (since blobFromImage uses single scale)
    resized.convertTo(resized, CV_32FC3);
    cv::subtract(resized, mean, resized);
    
    // Divide by Std (Multiply by 1/Std)
    std::vector<cv::Mat> channels(3);
    cv::split(resized, channels);
    
    channels[0] = channels[0] / std[0];
    channels[1] = channels[1] / std[1];
    channels[2] = channels[2] / std[2];
    
    cv::merge(channels, resized);

    // 2. Create Blob (No further mean/scale needed, swapRB already done via cvtColor)
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0, cv::Size(), cv::Scalar(), false, false);

    // 3. Inference
    net.setInput(blob);
    cv::Mat outputBlob = net.forward(); 

    // 4. Extract Output
    if (outputBlob.dims > 2) {
        int h_out = outputBlob.size[outputBlob.dims - 2];
        int w_out = outputBlob.size[outputBlob.dims - 1];
        
        cv::Mat depthMap(h_out, w_out, CV_32F, outputBlob.ptr<float>());
        return depthMap.clone(); 
    }

    return outputBlob;
}
