#include "EdgeSafetyModule.h"
#include <iostream>
#include <opencv2/imgproc.hpp>

EdgeSafetyModule::EdgeSafetyModule() {}

bool EdgeSafetyModule::init(const std::map<std::string, std::string>& params) {
    if (params.find("gradientThreshold") != params.end()) {
        gradientThreshold = std::stof(params.at("gradientThreshold"));
    }
    if (params.find("interval") != params.end()) {
        processInterval = std::stoi(params.at("interval"));
    }
    std::cout << "[EdgeSafetyModule] Init: gradientThreshold=" << gradientThreshold << ", Interval=" << processInterval << std::endl;
    return true;
}

void EdgeSafetyModule::process(Context& ctx) {
    if (ctx.depthMap.empty()) return;

    frameCount++;
    if (frameCount % processInterval != 0) return;

    // Gradient Analysis for drop-off detection
    // We compute the gradient in the Y direction (vertical changes).
    // A sudden change in depth as we go from bottom (feet) to top (horizon) usually means a vertical surface.
    // However, for "negative obstacles" (holes, stairs down), we look for gradients on the ground plane.
    
    // Simplification:
    // 1. Focus on the bottom half of the image (ground).
    // 2. Compute Sobel Y gradient.
    // 3. High gradient values indicate "edges".
    
    int h = ctx.depthMap.rows;
    int w = ctx.depthMap.cols;
    
    // Region of Interest: Bottom 40%
    cv::Rect roi(0, (int)(h * 0.6), w, (int)(h * 0.4));
    cv::Mat ground = ctx.depthMap(roi);
    
    // Ensure 32F for gradient calculation (MiDaS outputs might be 32F or 8U inverted)
    cv::Mat gray;
    if (ground.type() != CV_8U) {
        // Normalize to 0-255 for standard gradient checks or keep float?
        // Let's keep consistent with whatever depthMap is.
        // Assuming depthMap is CV_32F (inverse depth) from DepthEngine
        ground.convertTo(gray, CV_32F);
    } else {
        ground.convertTo(gray, CV_32F);
    }
    
    cv::Mat gradY;
    cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);
    
    // Find significant edges
    cv::Mat edges;
    // Thresholding gradient.
    // Since depth is inverse, scaling matters. Let's adaptively check max values or stick to user param.
    // If gradientThreshold is 0.25 (arbitrary), we might need to scale it to the data range.
    // Let's assume the user provided threshold applies to normalized [0,1] or standard range.
    // For safety, let's normalize gradient map for visualization.
    
    cv::Mat absGradY = cv::abs(gradY);
    double minVal, maxVal;
    cv::minMaxLoc(absGradY, &minVal, &maxVal);
    
    // Detect "Drop Off"
    // If we see a strong line across the path, it's an edge.
    // Visualizing the gradient on debug overlay
    if (!ctx.debugOverlay.empty()) {
        // Convert to displayable format
        cv::Mat gradVis;
        cv::normalize(absGradY, gradVis, 0, 255, cv::NORM_MINMAX, CV_8U);
        
        // Colorize high gradients red
        cv::Mat colorGrad;
        cv::applyColorMap(gradVis, colorGrad, cv::COLORMAP_JET);
        
        // Overlay on bottom part
        // cv::addWeighted(ctx.debugOverlay(roi), 0.7, colorGrad, 0.3, 0, ctx.debugOverlay(roi));
        
        // Simpler: Just draw RED lines where gradient is very high
        cv::Mat highGradMask;
        // Using dynamic threshold relative to max found, or user param if calibrated
        // Let's use user param as absolute if data allows, else relative 50% of max
        float thresh = (float)maxVal * gradientThreshold; 
        cv::threshold(absGradY, highGradMask, thresh, 255, cv::THRESH_BINARY);
        highGradMask.convertTo(highGradMask, CV_8U);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(highGradMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        bool danger = false;
        for (const auto& cnt : contours) {
             if (cv::contourArea(cnt) > 100) { // filter noise
                 // Draw contours on the main overlay (offset by ROI y)
                 std::vector<std::vector<cv::Point>> shiftedContours;
                 shiftedContours.push_back(cnt);
                 for(auto& p : shiftedContours[0]) p.y += (int)(h * 0.6);
                 
                 cv::drawContours(ctx.debugOverlay, shiftedContours, -1, cv::Scalar(0, 0, 255), 2);
                 danger = true;
             }
        }
        
        if (danger) {
             ctx.edgeSafetyAlert = "CAUTION: STEPS / DROP-OFF";
             // ctx.activeAlert = "DROP-OFF DETECTED"; // Optional: keep system alert if needed for Audio, but UI is now separate
        }
    }
}
