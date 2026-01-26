#pragma once

#include "IFillBackend.h"

namespace avision {

class ClassicalFillBackend : public IFillBackend {
public:
    cv::Mat inpaint(const cv::Mat& image, const cv::Mat& mask) override {
        cv::Mat result;
        // Radius 3 usually works well for Telea
        cv::inpaint(image, mask, result, 3, cv::INPAINT_TELEA);
        return result;
    }

    std::string name() const override { return "OpenCV Telea (Fallback)"; }
    
    // Always available if OpenCV is linked
    bool isAvailable() const override { return true; }
};

} // namespace avision
