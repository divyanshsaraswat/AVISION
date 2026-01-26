#pragma once

#include "IFillBackend.h"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>

namespace avision {

class LaMaBackend : public IFillBackend {
public:
    LaMaBackend();
    
    cv::Mat inpaint(const cv::Mat& image, const cv::Mat& mask) override;
    
    std::string name() const override { return "Big-LaMa (ONNX)"; }
    bool isAvailable() const override;

private:
    void loadModel();
    cv::Mat preprocess(const cv::Mat& img, int size);

    bool modelLoaded = false;
    std::string modelPath;
    
    // ONNX Runtime
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::SessionOptions> options;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator;
};

} // namespace avision
