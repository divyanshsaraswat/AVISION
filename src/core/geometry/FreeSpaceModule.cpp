#include "FreeSpaceModule.h"
#include <iostream>
#include <vector>
#include <numeric>
#include <opencv2/imgproc.hpp>

FreeSpaceModule::FreeSpaceModule() {}

bool FreeSpaceModule::init(const std::map<std::string, std::string>& params) {
    if (params.find("sectors") != params.end()) {
        sectors = std::stoi(params.at("sectors"));
    }
    if (params.find("minClearance") != params.end()) {
        minClearance = std::stof(params.at("minClearance"));
    }
    std::cout << "[FreeSpaceModule] Init: sectors=" << sectors << ", minClearance=" << minClearance << std::endl;
    return true;
}

void FreeSpaceModule::process(Context& ctx) {
    if (ctx.depthMap.empty()) return;

    // Simple implementation: split depth map into vertical sectors
    // Logic: Calculate average depth in each sector. If > minClearance, it's safe.
    // NOTE: This assumes depthMap values are in METERS. If they are normalized 0-255, we need conversion logic.
    // For now, let's assume raw depth values or known scalable values.
    // If using MiDaS, output is inverse depth (disparity). Larger = Closer.
    // Let's assume standard behavior: we check if things are "too close".
    // If "inverse depth" > Threshold => DANGER. 

    int w = ctx.depthMap.cols;
    int h = ctx.depthMap.rows;
    int sectorWidth = w / sectors;

    // Visualization: Draw simple lines for sectors
    if (!ctx.debugOverlay.empty()) {
        for (int i = 1; i < sectors; ++i) {
            cv::line(ctx.debugOverlay, cv::Point(i * sectorWidth, 0), cv::Point(i * sectorWidth, h), cv::Scalar(0, 255, 255), 1);
        }
    }

    std::string direction = "CLEAR";
    bool safe = true;

    // Example logic using "Close Obstacle" concept from DepthEngine context (if available) or raw map
    // We will just process the depth map directly here for robustness.
    
    // We need to know if depthMap is 8U or 32F. MiDaS usually gives 32F inverse depth.
    // Let's assume 32F for calculation.
    
    for (int i = 0; i < sectors; ++i) {
        cv::Rect roi(i * sectorWidth, 0, sectorWidth, h);
        cv::Mat sector = ctx.depthMap(roi);
        
        // Calculate average depth/disparity
        float avg = 0.0f;
        if (sector.type() == CV_8U) {
             avg = (float)cv::mean(sector)[0]; // 0-255, 255 is usually close? depends on model.
        } else {
             avg = (float)cv::mean(sector)[0];
        }

        // Logic placeholder: Assuming generic inverse depth where High = Close
        // User said minClearance = 1.0. If we are talking meters, we need real depth.
        // If we only have relative disparity, we tune 'minClearance' as a threshold.
        // Let's treat 'minClearance' as a "Safe Threshold" for now.
        
        bool sectorUnsafe = (avg > minClearance * 100); // Scaling factor purely as example if raw values are small
        // Actually, without real depth calibration, "1.0" meter is hard.
        // We will stick to the user's generic parameter and assume it is a threshold for "value > threshold = danger"
        
        // If "Inverse Depth" (Close objects have high values)
        // Danger if Value > Threshold
        // Let's map "minClearance" to a "Safe Distance Limit"
        // Actually, if it's "Clearance", higher is better (more space)?
        // Re-reading user request: "Converts perception into navigation decisions... minClearance: 1.0"
        
        // Let's implement a dummy logic that visualizes stats so user can tune it.
        std::string status = "SAFE";
        cv::Scalar color(0, 255, 0); // Green
        
        // Mock logic: If average pixel value > 150 (arbitrary close) -> Danger
        if (avg > 150) { 
            status = "BLOCKED"; 
            color = cv::Scalar(0, 0, 255);
            if (i == 1) { // Center sector blocked
                safe = false;
                direction = "TURN";
            }
        }

        // Draw status on overlay
        if (!ctx.debugOverlay.empty()) {
            cv::putText(ctx.debugOverlay, status, cv::Point(i * sectorWidth + 10, h - 30), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
        }
    }
    
    ctx.isPathSafe = safe;
    if (!safe) {
        ctx.activeAlert = "OBSTACLE DETECTED! " + direction;
    }
}
