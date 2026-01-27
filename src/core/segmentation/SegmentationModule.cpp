#include "SegmentationModule.h"
#include "MobileSAMBackend.h"
#include "ClassicalBackend.h"
#include <iostream>

namespace avision {

SegmentationModule::SegmentationModule() {
    // Instantiate Backends
    primaryBackend = std::make_shared<MobileSAMBackend>();
    fallbackBackend = std::make_shared<ClassicalBackend>();
}

cv::Mat SegmentationModule::segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed) {
    cv::Mat result;

    // 1. Try Primary (MobileSAM)
    // We cast to MobileSAMBackend to check specific availability if needed, or rely on interface results.
    // The interface contract says it returns empty Mat if it fails/can't run.
    
    // Check if primary is actually available (e.g. model loaded)
    // We could add `isAvailable()` to interface, but for now we know the concrete type here or just try segment.
    // Let's assume segment() returns empty if it fails.
    
    result = primaryBackend->segment(image, roi, seed);
    
    if (!result.empty()) {
        std::cout << "[SegmentationModule] Used Primary Backend: " << primaryBackend->name() << "\n";
        return result;
    }

    // 2. Fallback (Classical)
    std::cout << "[SegmentationModule] Primary backend unavailable or failed. Switching to Fallback: " << fallbackBackend->name() << "\n";
    result = fallbackBackend->segment(image, roi, seed);

    return result;
}

cv::Mat SegmentationModule::segment(const cv::Mat& image, const std::vector<cv::Point>& points, const std::vector<int>& labels, const cv::Rect& box) {
    cv::Mat result;

    // 1. Try Primary (MobileSAM)
    result = primaryBackend->segment(image, points, labels, box);
    
    if (!result.empty()) {
        std::cout << "[SegmentationModule] Used Primary Backend: " << primaryBackend->name() << "\n";
        return result;
    }

    // 2. Fallback (Classical)
    std::cout << "[SegmentationModule] Primary backend unavailable or failed. Switching to Fallback: " << fallbackBackend->name() << "\n";
    result = fallbackBackend->segment(image, points, labels, box);

    return result;
}

} // namespace avision
