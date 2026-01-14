#pragma once
#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include <string>

class DepthEngine {
public:
    DepthEngine();
    
    // Load the ONNX model
    bool init(const std::string& modelPath);
    
    // Returns a CV_32F depth map (normalized 0..1 typically, or relative inverse depth)
    // Input should be the raw BGR frame from camera/video
    cv::Mat estimateDepth(const cv::Mat& inputFrame);

private:
    cv::dnn::Net net;
    bool isInitialized;
    
    // Config matches "MiDaS v2.1 Small" (Standard 256x256)
    const int inputWidth = 256;
    const int inputHeight = 256;
    
    // Preprocessing params
    const cv::Scalar mean = cv::Scalar(123.675, 116.28, 103.53);
    const cv::Scalar std = cv::Scalar(58.395, 57.12, 57.375);
};
