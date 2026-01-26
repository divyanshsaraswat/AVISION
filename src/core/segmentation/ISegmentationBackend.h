#pragma once
#include <opencv2/opencv.hpp>

namespace avision {

class ISegmentationBackend {
public:
    virtual ~ISegmentationBackend() = default;

    /**
     * @brief Perform segmentation on the input image.
     * 
     * @param image Input image (BGR).
     * @param roi Bounding box for segmentation (can be empty if seed is provided).
     * @param seed Optional seed point for interactive segmentation (-1, -1 if unused).
     * @return cv::Mat Binary mask (CV_8UC1) where 255 is foreground, 0 is background.
     */
    virtual cv::Mat segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed = cv::Point(-1, -1)) = 0;
    
    /**
     * @brief Get the name of the backend.
     */
    virtual std::string name() const = 0;
};

} // namespace avision
