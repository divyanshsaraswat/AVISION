#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace avision {

class IFillBackend {
public:
    virtual ~IFillBackend() = default;

    // Core Inpaint function
    // image: The resized crop (e.g. 512x512)
    // mask: The resized mask (single channel, 0=valid, 255=hole)
    // Returns: Inpainted image (same size)
    virtual cv::Mat inpaint(const cv::Mat& image, const cv::Mat& mask) = 0;

    virtual std::string name() const = 0;
    virtual bool isAvailable() const = 0;
};

} // namespace avision
