#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include <string>
#include <future>
#include <mutex>
#include <atomic>

#include "../kernel/IModule.h"

class DepthEngine : public IModule {
public:
    DepthEngine();
    
    // IModule Implementation
    bool init(const std::map<std::string, std::string>& params) override; 
    void process(Context& ctx) override;
    std::string getName() const override { return "DepthModule"; }

    // Init with specific path (legacy/manual)
    bool init(const std::string& modelPath);
    
    // Legacy direct call
    cv::Mat estimateDepth(const cv::Mat& inputFrame);

private:
    cv::dnn::Net net;
    bool isInitialized;
    
    // Config
    bool processEveryFrame = true;
    int skipInterval = 30; // Depth is heavy, default slow
    int internalFrameCount = 0;
    
    // State Cache for Throttling
    cv::Mat cachedDepthMap;
    std::string cachedAlert;
    
    // Config matches "MiDaS v2.1 Small" (Standard 256x256)
    const int inputWidth = 256;
    const int inputHeight = 256;
    
    // Preprocessing params
    const cv::Scalar mean = cv::Scalar(123.675, 116.28, 103.53);
    const cv::Scalar std = cv::Scalar(58.395, 57.12, 57.375);
    // Async State
    std::future<cv::Mat> pendingFuture;
    std::atomic<bool> isProcessing{false};
    double lastInferenceTime = 0.0;
};
