#pragma once
#include <memory>
#include "../platform/interfaces/ICamera.h"
#include "../platform/interfaces/IAudio.h"
#include "geometry/GeometryEngine.h"
#include "distance/DistanceEngine.h"

class Engine {
public:
    Engine(std::shared_ptr<ICamera> cam, std::shared_ptr<IAudio> aud);
    
    // Start the main loop
    void run();

private:
    std::shared_ptr<ICamera> camera;
    std::shared_ptr<IAudio> audio;
    
    GeometryEngine geometry;
    DistanceEngine distance; // logic class, static methods
    
    bool isRunning;
};
