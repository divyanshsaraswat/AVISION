#include "PathGuidanceModule.h"
#include <iostream>
#include <cmath>
#include <opencv2/imgproc.hpp>

PathGuidanceModule::PathGuidanceModule() {}

bool PathGuidanceModule::init(const std::map<std::string, std::string>& params) {
    if (params.find("interval") != params.end()) {
        processInterval = std::stoi(params.at("interval"));
    }
    std::cout << "[PathGuidanceModule] Initialized. Interval=" << processInterval << std::endl;
    return true;
}

void PathGuidanceModule::process(Context& ctx) {
    if (ctx.depthMap.empty()) return;
    
    frameCount++;
    if (frameCount % processInterval != 0) return;

    // Simplified Vector Field Histogram (VFH)
    // We want to go straight (Attraction Force).
    // Obstacles push us away (Repulsion Force).

    int h = ctx.depthMap.rows;
    int w = ctx.depthMap.cols;
    int w3 = w / 3;

    // Region of Interest: Middle vertical band (path ahead)
    cv::Rect leftROI(0, 0, w3, h);
    cv::Rect centerROI(w3, 0, w3, h);
    cv::Rect rightROI(2 * w3, 0, w3, h);

    // Calculate average depth (inverse depth: higher is closer)
    cv::Scalar meanLeft = cv::mean(ctx.depthMap(leftROI));
    cv::Scalar meanCenter = cv::mean(ctx.depthMap(centerROI));
    cv::Scalar meanRight = cv::mean(ctx.depthMap(rightROI));

    // Assume depthMap is inverse depth (MiDaS). Higher means closer.
    // Let's create a repulsion force based on "closeness".
    double repulsionLeft = meanLeft[0];
    double repulsionCenter = meanCenter[0];
    double repulsionRight = meanRight[0];

    // Resultant force calculation (heuristic)
    // If center is blocked, look at left vs right.
    
    // Thresholds (empirical, based on normalized inverse depth)
    // Assuming depthMap is normalized 0-255 or 0-1. Let's assume 0-255 from our pipeline visualization.
    double obstacleThresh = 150.0; 

    std::string command = "Go Straight";

    if (repulsionCenter > obstacleThresh) {
        // Center blocked.
        if (repulsionLeft < repulsionRight) {
            // Left is more open (less repulsion/inverse depth)
            command = "Veer Left";
        } else {
            command = "Veer Right";
        }
        
        // If all are blocked
        if (repulsionLeft > obstacleThresh && repulsionRight > obstacleThresh) {
            command = "Stop / Rotate";
        }
    }

    // Update Context (Assuming a field for this, otherwise just visualize)
    // Since Context struct changes require recompiling *everything* that uses it,
    // and I shouldn't change Context.h unless necessary, I will just visualize for now
    // or assume there is a generic way to pass this.
    // Write to Context for centralized rendering
    ctx.guidanceCommand = "Command: " + command;
}
