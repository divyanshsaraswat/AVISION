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
     * @brief Perform segmentation with multiple prompts (points and/or box).
     * 
     * @param image Input image.
     * @param points Vector of points (prompts).
     * @param labels Vector of labels for points (1=foreground, 0=background).
     * @param box Optional bounding box prompt.
     * @return cv::Mat Binary mask.
     */
    virtual cv::Mat segment(const cv::Mat& image, const std::vector<cv::Point>& points, const std::vector<int>& labels, const std::vector<cv::Rect>& boxes = {}) {
        // Default implementation calls single-point version if applicable
        if (points.size() == 1 && boxes.empty()) {
             return segment(image, cv::Rect(), points[0]);
        }
        if (!boxes.empty() && points.empty() && boxes.size() == 1) {
             return segment(image, boxes[0]);
        }
        return cv::Mat();
    }
    
    /**
     * @brief Get the name of the backend.
     */
    virtual std::string name() const = 0;
};

} // namespace avision
