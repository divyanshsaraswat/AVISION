#pragma once
#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include <vector>
#include <string>

// DetectedObject is now in Context.h

#include "../kernel/IModule.h"

class ObjectEngine : public IModule {
public:
    ObjectEngine();

    // IModule Implementation
    bool init(const std::map<std::string, std::string>& params) override; 
    void process(Context& ctx) override;
    std::string getName() const override { return "ObjectModule"; }
    
    enum class ModelType {
        SSD_MOBILENET,  // Caffe Style [1, 1, N, 7]
        SSD_TF,         // TensorFlow Style (Multi-head: boxes, scores, classes)
        YOLO_V8         // Ultralytics Style [1, 84, N]
    };

    struct ModelConfig {
        std::string modelPath;
        std::string configPath; // Optional (for Caffe)
        ModelType type = ModelType::SSD_MOBILENET;
        int inputWidth = 300;
        int inputHeight = 300;
        float scoreThreshold = 0.5f;
        float nmsThreshold = 0.45f;
        
        // Preprocessing
        float scale = 1.0f / 127.5f;
        cv::Scalar mean = cv::Scalar(127.5, 127.5, 127.5);
        bool swapRB = false;
        
        // Labels
        std::string dataset = "VOC"; // "VOC" or "COCO"
    };

    // Load model from config
    bool init(const ModelConfig& config);

    // Legacy support (optional, can be removed if not used)
    bool init(const std::string& prototxt, const std::string& model);
    
    // Run detection on a frame
    std::vector<DetectedObject> detect(const cv::Mat& frame);

    // Helper to get name from ID (COCO/VOC)
    std::string getLabel(int classId) const;

private:
    cv::dnn::Net net;
    std::vector<std::string> classNames;
    bool isInitialized;
    ModelConfig currentConfig;
    
    // Throttling
    bool processEveryFrame = true;
    int skipInterval = 5;
    int internalFrameCount = 0;
};
