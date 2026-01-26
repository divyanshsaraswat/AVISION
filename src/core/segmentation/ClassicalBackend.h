#pragma once
#include "ISegmentationBackend.h"

namespace avision {

class ClassicalBackend : public ISegmentationBackend {
public:
    cv::Mat segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed = cv::Point(-1, -1)) override;
    std::string name() const override { return "Classical (GrabCut)"; }
};

} // namespace avision
