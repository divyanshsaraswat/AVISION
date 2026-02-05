#include "OCRModule.h"
#include <iostream>
#include <opencv2/imgproc.hpp>

OCRModule::OCRModule() {}

bool OCRModule::init(const std::map<std::string, std::string>& params) {
    std::string detPath = "";
    std::string recPath = "";

    if (params.find("detectModel") != params.end()) detPath = params.at("detectModel");
    if (params.find("recogModel") != params.end()) recPath = params.at("recogModel");
    if (params.find("interval") != params.end()) processInterval = std::stoi(params.at("interval"));

    try {
        if (!detPath.empty()) {
            detector = cv::dnn::readNet(detPath);
            std::cout << "[OCRModule] Loaded Detection Model: " << detPath << std::endl;
        }
        if (!recPath.empty()) {
            try {
                recognizer = cv::dnn::readNet(recPath);
                std::cout << "[OCRModule] Loaded Recognition Model: " << recPath << std::endl;
            } catch (const std::exception& e) {
                std::cout << "[OCRModule] Warning: Could not load Recognition model (" << e.what() << "). Proceeding with Detection only." << std::endl;
            }
        }
        
        if (!detector.empty()) {
            modelsLoaded = true;
            // Set backend to OpenCV/CPU for edge compatibility
            detector.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            detector.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            if (!recognizer.empty()) {
                recognizer.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                recognizer.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            }
        } else {
            std::cout << "[OCRModule] Models not found or invalid. Module will be passive." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[OCRModule] Error loading models: " << e.what() << std::endl;
        modelsLoaded = false;
    }

    return true;
}

void OCRModule::process(Context& ctx) {
    if (!modelsLoaded || ctx.rawFrame.empty()) return;

    frameCount++;
    if (frameCount % processInterval != 0) return;

    // TODO: Implement actual inference logic for DBNet and CRNN
    // For now, this is a placeholder structure to ensure integration works.
    // Full inference requires complex pre/post processing (polygon decoding for DBNet, CTC decode for CRNN).
    // Given the "Manual Setup" constraint, we'll verify it loads first.
    
    // Placeholder visualization to show module is active
    if (!ctx.debugOverlay.empty()) {
        cv::putText(ctx.debugOverlay, "OCR Active (Standby)", cv::Point(10, 200),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 255), 1);
    }
}

std::string OCRModule::decodeText(const cv::Mat& scores) {
    return "Text"; // Placeholder
}
