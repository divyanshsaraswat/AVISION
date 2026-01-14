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
    // Note: cv::divide with scalar handles per-channel if we pass scalar correctly
    // or we multiply. 
    // CV_32FC3 element-wise multiplication
    std::vector<cv::Mat> channels(3);
    cv::split(resized, channels);
    
    channels[0] = channels[0] / std[0];
    channels[1] = channels[1] / std[1];
    channels[2] = channels[2] / std[2];
    
    cv::merge(channels, resized);

    // 2. Create Blob (No further mean/scale needed, swapRB already done via cvtColor)
    // size=192x256
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0, cv::Size(), cv::Scalar(), false, false);

    // 3. Inference
    net.setInput(blob);
    cv::Mat outputBlob = net.forward(); // shape: [1, h, w] or [1, 1, h, w]?

    // 4. Extract Output
    // Output shape typically [1, 192, 256] or similar
    // We want to return a CV_32F matrix image
    
    // Handle dimensions
    // 2D output?
    if (outputBlob.dims > 2) {
        // Assume [1, H, W] or [1, 1, H, W]
        // Get pointer to data
        // We can reshape to 2D
        // Size[2] and Size[3] usually H, W
        int h_out = outputBlob.size[outputBlob.dims - 2];
        int w_out = outputBlob.size[outputBlob.dims - 1];
        
        cv::Mat depthMap(h_out, w_out, CV_32F, outputBlob.ptr<float>());
        return depthMap.clone(); // Clone to own data
    }

    return outputBlob;
}
