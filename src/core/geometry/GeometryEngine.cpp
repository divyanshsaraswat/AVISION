#include "GeometryEngine.h"
#include <opencv2/imgproc.hpp>

GeometryEngine::GeometryEngine() : pathSafe(true), frameCount(0), processInterval(5) {}

bool GeometryEngine::init(const std::map<std::string, std::string>& params) {
    if (params.find("interval") != params.end()) {
        processInterval = std::stoi(params.at("interval"));
    }
    return true;
}

void GeometryEngine::process(Context& ctx) {
    frameCount++;
    if (frameCount % processInterval != 0) return;

    if (ctx.rawFrame.empty()) return;
    
    double t_start = (double)cv::getTickCount();
    
    // Use the overlay from context
    process(ctx.rawFrame, ctx.debugOverlay);
    
    // Copy results to Context
    ctx.isPathSafe = isPathSafe();
    ctx.obstacles = getObstacles();
    
    double t_end = (double)cv::getTickCount();
    double time_ms = ((t_end - t_start) / cv::getTickFrequency()) * 1000.0;
    
    // Log
    std::string safeStr = ctx.isPathSafe ? "Safe" : "UNSAFE";
    std::string obsStr = std::to_string(ctx.obstacles.size()) + " obs";
    // [Geometry] 12.5 ms, Path: Safe, 5 obs
    ctx.moduleLogs.push_back("[Geometry] " + std::to_string(time_ms).substr(0,4) + " ms, Path: " + safeStr + ", " + obsStr);
}

void GeometryEngine::process(const cv::Mat& inputFrame, cv::Mat& debugFrame) {
    if (inputFrame.empty()) return;

    // Reset state
    currentObstacles.clear();
    pathSafe = true;

    // Clone for debug visualization
    debugFrame = inputFrame.clone();

    // 1. Preprocessing (Blur to reduce noise)
    cv::Mat gray, blurred;
    cv::cvtColor(inputFrame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

    // 2. Run detections
    detectGround(blurred, debugFrame);
    detectObstacles(blurred, debugFrame);
}

void GeometryEngine::detectGround(const cv::Mat& src, cv::Mat& debug) {
    // MVP: Simple heuristic - bottom center should be edge-free for "safe walking"
    // In real version: use Hough Lines / Segmentation
    
    int h = src.rows;
    int w = src.cols;
    
    // Check ROI at bottom center
    cv::Rect safeZone(w/4, h - h/3, w/2, h/3);
    cv::Mat roi = src(safeZone);
    
    cv::Mat edges;
    cv::Canny(roi, edges, 50, 150);
    
    int edgePixels = cv::countNonZero(edges);
    
    // If too many edges in the "walking path", it's not safe
    if (edgePixels > (roi.total() * 0.05)) { // > 5% edges
        pathSafe = false;
        cv::rectangle(debug, safeZone, cv::Scalar(0, 0, 255), 2); // Red Warning
    } else {
        pathSafe = true;
        cv::rectangle(debug, safeZone, cv::Scalar(0, 255, 0), 2); // Green Safe
    }
}

void GeometryEngine::detectObstacles(const cv::Mat& src, cv::Mat& debug) {
    // MVP: Simple Contour detection for obstacles
    cv::Mat bin;
    cv::threshold(src, bin, 100, 255, cv::THRESH_BINARY_INV); // Dark objects?
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bin, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    for (const auto& cnt : contours) {
        double area = cv::contourArea(cnt);
        if (area < 1000) continue; // Ignore small noise
        
        cv::Rect box = cv::boundingRect(cnt);
        
        // Filter: Ignore things that are too high up (ceiling lights etc)
        if (box.y + box.height < src.rows / 2) continue;
        
        Obstacle obs;
        obs.boundingBox = box;
        obs.isDanger = true; 
        
        // MVP Distance: simple Y-axis mapping (lower = closer)
        float bottomY = (float)(box.y + box.height);
        obs.relativeDistance = bottomY / (float)src.rows; 
        
        currentObstacles.push_back(obs);
        
        // Draw
        cv::rectangle(debug, box, cv::Scalar(255, 0, 0), 2); // Blue
    }
}

bool GeometryEngine::isPathSafe() const { return pathSafe; }
const std::vector<Obstacle>& GeometryEngine::getObstacles() const { return currentObstacles; }
