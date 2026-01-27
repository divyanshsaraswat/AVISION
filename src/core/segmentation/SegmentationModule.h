#pragma once
#include "ISegmentationBackend.h"
#include <memory>
#include <vector>

namespace avision {

class SegmentationModule {
public:
    SegmentationModule();
    ~SegmentationModule() = default;

    /**
     * @brief Perform segmentation attempting to use the best available backend.
     * 
     * @param image Input image.
     * @param roi ROI selection.
     * @param seed Optional seed point.
     * @return cv::Mat Binary mask (CV_8UC1).
     */
    cv::Mat segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed = cv::Point(-1, -1));

    /**
     * @brief Perform segmentation with multiple prompts (points and/or box).
     */
    cv::Mat segment(const cv::Mat& image, const std::vector<cv::Point>& points, const std::vector<int>& labels, const std::vector<cv::Rect>& boxes = {});

private:
    std::shared_ptr<ISegmentationBackend> primaryBackend;
    std::shared_ptr<ISegmentationBackend> fallbackBackend;
};

} // namespace avision
