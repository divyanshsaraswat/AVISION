#pragma once

#include "ISegmentationBackend.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>

// ONNX Runtime Header
#include <onnxruntime_cxx_api.h>

namespace avision {

class MobileSAMBackend : public ISegmentationBackend {
public:
    MobileSAMBackend();
    
    cv::Mat segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed) override;
    cv::Mat segment(const cv::Mat& image, const std::vector<cv::Point>& points, const std::vector<int>& labels, const std::vector<cv::Rect>& boxes) override;
    
    std::string name() const override { return "MobileSAM (ONNX Runtime)"; }
    bool isAvailable() const;

private:
    bool modelLoaded = false;

    // ONNX Runtime Members
    std::unique_ptr<Ort::Env> env;
    
    // Encoder
    std::unique_ptr<Ort::Session> encoderSession;
    std::string encoderPath;
    
    // Decoder
    std::unique_ptr<Ort::Session> decoderSession;
    std::string decoderPath;

    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator;

    // Caching
    cv::Mat lastImage;
    std::vector<float> cachedEmbeddings;
    bool hasEmbeddings = false;

    // Internal Helpers
    void loadModels();
    void runEncoder(const cv::Mat& inputImg);
    cv::Mat runDecoder(const std::vector<cv::Point>& points, const std::vector<int>& labels, const cv::Size& originalSize, float scale);
};

} // namespace avision
