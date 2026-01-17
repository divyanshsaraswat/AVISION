#pragma once
#include <opencv2/core.hpp>
#include <vector>

// Obstacle is now in Context.h

#include "../kernel/IModule.h"

class GeometryEngine : public IModule {
public:
    GeometryEngine();

    // IModule Implementation
    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "GeometryModule"; }
    
    // Main processing function
    void process(const cv::Mat& inputFrame, cv::Mat& debugFrame);

    // Getters for world state
    bool isPathSafe() const;
    const std::vector<Obstacle>& getObstacles() const;

private:
    void detectGround(const cv::Mat& src, cv::Mat& debug);
    void detectObstacles(const cv::Mat& src, cv::Mat& debug);

    std::vector<Obstacle> currentObstacles;
    bool pathSafe;
    
    int frameCount = 0;
    int processInterval = 5; // Default check every 5 frames
};
