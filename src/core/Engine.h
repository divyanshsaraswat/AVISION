#pragma once
#include <memory>
#include <vector>
#include <string>
#include <opencv2/core.hpp>
#include "../platform/interfaces/ICamera.h"
#include "../platform/interfaces/IAudio.h"
#include "geometry/GeometryEngine.h"
#include "distance/DistanceEngine.h"
#include "semantics/ObjectEngine.h"

class Engine {
public:
    Engine(std::shared_ptr<ICamera> cam, std::shared_ptr<IAudio> aud);
    
    // Lifecycle methods (Passive API)
    bool init();
    bool processFrame(cv::Mat& frame, cv::Mat& debugOut);
    void stop();

private:
    std::shared_ptr<ICamera> camera;
    std::shared_ptr<IAudio> audio;
    
    GeometryEngine geometry;
    DistanceEngine distance;
    ObjectEngine objectDetector;
    
    int frameCount = 0;
    std::vector<DetectedObject> cachedObjects;
    bool isRunning = false;
};
