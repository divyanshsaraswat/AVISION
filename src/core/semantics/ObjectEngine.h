#pragma once
#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include <vector>
#include <string>

struct DetectedObject {
    std::string label;
    float confidence;
    cv::Rect boundingBox;
    int classID;
};

class ObjectEngine {
public:
    ObjectEngine();
    
    // Load model from file path
    bool init(const std::string& prototxt, const std::string& model);
    
    // Run detection on a frame
    std::vector<DetectedObject> detect(const cv::Mat& frame);

    // Helper to get name from ID (COCO/VOC)
    std::string getLabel(int classId) const;

private:
    cv::dnn::Net net;
    std::vector<std::string> classNames;
    bool isInitialized;
};
