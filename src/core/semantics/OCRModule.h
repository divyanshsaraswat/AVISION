#pragma once
#include "../kernel/IModule.h"
#include <opencv2/dnn.hpp>

class OCRModule : public IModule {
public:
    OCRModule();
    ~OCRModule() override = default;

    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "OCRModule"; }

private:
    cv::dnn::Net detector;
    cv::dnn::Net recognizer;
    bool modelsLoaded = false;
    int frameCount = 0;
    int processInterval = 30; // Run OCR every 30 frames
    float confThreshold = 0.5f;
    
    // Helper to decode CRNN output
    std::string decodeText(const cv::Mat& scores);
};
