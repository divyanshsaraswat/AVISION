#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include "IFillBackend.h"

namespace avision {

class FillModule {
public:
    FillModule();
    
    // Main fill function
    // image: Full resolution original image
    // mask: Full resolution binary mask (255 = area to fill)
    // padding: Pixels to pad around the mask ROI (context for the model)
    cv::Mat fill(const cv::Mat& image, const cv::Mat& mask, int padding = 50);

private:
    std::vector<std::unique_ptr<IFillBackend>> backends;
};

} // namespace avision
