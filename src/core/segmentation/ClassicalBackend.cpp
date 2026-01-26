#include "ClassicalBackend.h"
#include <iostream>

namespace avision {

cv::Mat ClassicalBackend::segment(const cv::Mat& img, const cv::Rect& roi, const cv::Point& seedPt) {
    if (img.empty()) return cv::Mat();

    cv::Mat binMask;
    cv::Mat mask;
    cv::Mat bgModel, fgModel;

    // SCENARIO 1: Point-based GrabCut (Smart Click)
    if (seedPt.x != -1) {
        if (seedPt.x < 0 || seedPt.x >= img.cols || seedPt.y < 0 || seedPt.y >= img.rows) {
            std::cerr << "[ClassicalBackend] Error: Seed point out of bounds\n";
            return cv::Mat();
        }

        std::cout << "[ClassicalBackend] Running Point-Initialized GrabCut...\n";

        // Initialize Mask with PROBABLE FOREGROUND
        mask.create(img.size(), CV_8UC1);
        mask.setTo(cv::Scalar(cv::GC_PR_FGD));

        // 1. Mark the seed point as SURE Foreground (Anchor)
        cv::circle(mask, seedPt, 5, cv::Scalar(cv::GC_FGD), -1);

        // 2. Mark borders as SURE Background (Constraint)
        int border = 2; // 2px border
        cv::rectangle(mask, cv::Point(0,0), cv::Point(img.cols-1, img.rows-1), cv::Scalar(cv::GC_BGD), border);

        try {
            // Increase iterations to 7 for better convergence
            cv::grabCut(img, mask, cv::Rect(), bgModel, fgModel, 7, cv::GC_INIT_WITH_MASK);
        } catch (const cv::Exception& e) {
             std::cerr << "[ClassicalBackend] GrabCut Error: " << e.what() << "\n";
             return cv::Mat();
        }
    }
    // SCENARIO 2: ROI-based GrabCut
    else {
        if (roi.area() <= 0) {
            std::cerr << "[ClassicalBackend] Error: Invalid ROI for segmentation\n";
            return cv::Mat();
        }

        std::cout << "[ClassicalBackend] Running ROI-based GrabCut... this may take a moment.\n";

        try {
            // ROI-based GrabCut (GC_INIT_WITH_RECT)
            cv::grabCut(img, mask, roi, bgModel, fgModel, 5, cv::GC_INIT_WITH_RECT); // 5 iters usually enough
        } catch (const cv::Exception& e) {
             std::cerr << "[ClassicalBackend] GrabCut Error: " << e.what() << "\n";
             return cv::Mat();
        }
    }

    // Convert result: FG + PR_FGD -> 255
    binMask = (mask == cv::GC_PR_FGD) | (mask == cv::GC_FGD);
    
    // Scale to 0-255 for display/saving
    // Operator == returns 255 for true, so we just need to ensure it's single channel 255
    // Actually the result of (mask == ...) is 255 where true.
    // So binMask is already 255/0.
    
    return binMask;
}

} // namespace avision
