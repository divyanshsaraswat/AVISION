#pragma once
#include <opencv2/core.hpp>

// INTERFACE: The "Body" (Platform) must implement this for the "Brain" (Core)
class ICamera {
public:
    virtual ~ICamera() = default;

    // Core calls this to get the next frame
    // Returns true if frame is valid
    virtual bool capture(cv::Mat& frame) = 0;

    // Core calls this to check if camera is ready
    virtual bool isOpened() const = 0;
};
