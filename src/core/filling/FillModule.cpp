#include "FillModule.h"
#include "LaMaBackend.h"
#include "ClassicalFillBackend.h"
#include <iostream>
#include <algorithm>

namespace avision {

FillModule::FillModule() {
    // Priority: LaMa -> Classical
    backends.push_back(std::make_unique<LaMaBackend>());
    backends.push_back(std::make_unique<ClassicalFillBackend>());
}

cv::Mat FillModule::fill(const cv::Mat& image, const cv::Mat& mask, int padding) {
    if (image.empty() || mask.empty()) {
        std::cerr << "[FillModule] Empty input.\n";
        return image.clone();
    }

    // 1. Compute ROI & Dilate Mask
    cv::Mat grayMask;
    if (mask.channels() == 3) cv::cvtColor(mask, grayMask, cv::COLOR_BGR2GRAY);
    else grayMask = mask.clone();

    // 0. Dilate Mask (Critical for LaMa to avoid ghosting)
    // We need to cover the boundary logic so the model sees background context, not object edge.
    int dilateSize = 15; // 15px expansion
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * dilateSize + 1, 2 * dilateSize + 1), cv::Point(dilateSize, dilateSize));
    cv::Mat dilatedMask;
    cv::dilate(grayMask, dilatedMask, element);
    
    // Update grayMask to use the dilated version for processing
    grayMask = dilatedMask;

    std::vector<cv::Point> points;
    cv::findNonZero(grayMask, points);
    
    if (points.empty()) {
        std::cout << "[FillModule] No mask region found. Returning original.\n";
        return image.clone();
    }

    cv::Rect bbox = cv::boundingRect(points);
    
    // Add Padding
    // Strategy: For detailed structures, TIGHTER padding often yields SHARPER results 
    // because the model sees the texture at a higher relative frequency.
    // However, we need *some* context.
    // Let's use the user-provided padding but clamping it to ensure we don't zoom out too much.
    
    bbox.x = std::max(0, bbox.x - padding);
    bbox.y = std::max(0, bbox.y - padding);
    bbox.width = std::min(image.cols - bbox.x, bbox.width + 2 * padding);
    bbox.height = std::min(image.rows - bbox.y, bbox.height + 2 * padding);
    
    // Ensure ROI is valid
    if (bbox.width <= 0 || bbox.height <= 0) return image.clone();

    // 2. Crop
    cv::Mat cropImg = image(bbox).clone();
    cv::Mat cropMask = grayMask(bbox).clone();
    
    // 3. Resize for Model
    // LaMa is fully convolutional and handles arbitrary resolutions well.
    // To avoid blurriness, we should keep resolution high.
    // Constraint: Dimensions must be divisible by 8.
    
    int w = cropImg.cols;
    int h = cropImg.rows;
    
    // Cap max dimension to avoid OOM (e.g. 2048)
    int maxDim = 2048;
    if (w > maxDim || h > maxDim) {
        float scale = (float)maxDim / std::max(w, h);
        w = (int)(w * scale);
        h = (int)(h * scale);
    }

    // Align to 8
    w = (w / 8) * 8;
    h = (h / 8) * 8;
    if (w == 0) w = 8;
    if (h == 0) h = 8;
    
    cv::Size targetSize(w, h);

    cv::Mat inputImg, inputMask;
    cv::resize(cropImg, inputImg, targetSize, 0, 0, cv::INTER_AREA);
    
    // Nearest neighbor for mask to keep it binary sharp
    cv::resize(cropMask, inputMask, targetSize, 0, 0, cv::INTER_NEAREST);

    // 4. Inference loop
    cv::Mat inpaintedCrop;
    std::string usedBackend = "None";

    for (auto& backend : backends) {
        if (backend->isAvailable()) {
            std::cout << "[FillModule] Trying backend: " << backend->name() << "\n";
            try {
                inpaintedCrop = backend->inpaint(inputImg, inputMask);
                if (!inpaintedCrop.empty()) {
                    usedBackend = backend->name();
                    break;
                }
            } catch (const std::exception& e) {
                std::cerr << "[FillModule] Backend " << backend->name() << " failed: " << e.what() << "\n";
            }
        }
    }

    if (inpaintedCrop.empty()) {
        std::cerr << "[FillModule] All backends failed!\n";
        return image.clone();
    }

    // 5. Paste Back
    // Resize result to original ROI size
    cv::Mat finalCrop;
    cv::resize(inpaintedCrop, finalCrop, bbox.size(), 0, 0, cv::INTER_LINEAR);

    // Blend
    // Simple replacement of masked area
    // TODO: Soft blending?
    cv::Mat result = image.clone();
    
    // Iterate pixels in ROI and copy if mask > 0
    // Optimization: Use copyTo with mask
    // We need 'grayMask(bbox)' as the copy mask
    // Note: finalCrop is the WHOLE ROI (filled context + filled hole)
    // We should copy the whole ROI? No, that replaces ground truth context with generated context (which might be blurry).
    // Better: Copy ONLY the hole.
    
    finalCrop.copyTo(result(bbox), cropMask);
    
    std::cout << "[FillModule] Success using " << usedBackend << "\n";
    return result;
}

} // namespace avision
