#pragma once
#include "../kernel/IModule.h"
#include <map>
#include <deque>

// Structure to track object history for smoothing
struct TrackedObject {
    int id;
    std::string label;
    std::deque<cv::Rect> history; // Store last N positions
    cv::Rect2f smoothedBox;       // EMA Smoothed Box
    cv::Point2f velocity;         // Velocity (dx, dy) per frame
    bool hasSmoothed = false;
    int missingFrames = 0;
};

class TemporalModule : public IModule {
public:
    TemporalModule();
    ~TemporalModule() override = default;

    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "TemporalModule"; }

private:
    // Parameters
    int windowSize = 5;
    float decay = 0.85f;
    float alpha = 0.6f; // EMA factor (1.0 = no smoothing, 0.1 = heavy smoothing)
    
    // State
    std::vector<TrackedObject> tracks;
    int nextId = 1;

    // Helpers
    float calculateIoU(const cv::Rect& a, const cv::Rect& b);
    cv::Rect computeAverageBox(const std::deque<cv::Rect>& history);
};
