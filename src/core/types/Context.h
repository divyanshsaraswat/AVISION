#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include <string>
#include <map>

// Forward declarations to avoid heavy headers here if possible
// But for Context structs, we usually need definitions.
// We will move the definitions of DetectedObject and Obstacle to here or a common types header later.
// For now, we will define them here to decouple from Engine headers.

struct DetectedObject {
    std::string label;
    float confidence;
    cv::Rect boundingBox;
    int classID;
};

struct Obstacle {
    cv::Rect boundingBox;
    float relativeDistance; // 0.0 (far) to 1.0 (close)
    bool isDanger;
};

struct Context {
    // 1. Input
    cv::Mat rawFrame;
    double timestamp; // Seconds
    
    // 2. Visualization layer (Shared canvas)
    cv::Mat debugOverlay;
    
    // 3. Shared Knowledge (Written by Modules)
    std::vector<DetectedObject> detections;
    std::vector<Obstacle> obstacles;
    cv::Mat depthMap; // CV_32F or CV_8U
    
    // 4. Flags / Signals
    bool isPathSafe = true;
    std::string activeAlert; // e.g. "STOP", "CHAIR AHEAD"
};
