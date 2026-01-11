#pragma once
#include <opencv2/core.hpp>
#include <vector>

struct Obstacle {
    cv::Rect boundingBox;
    float relativeDistance; // 0.0 (far) to 1.0 (close)
    bool isDanger;
};

class GeometryEngine {
public:
    GeometryEngine();
    
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
};
