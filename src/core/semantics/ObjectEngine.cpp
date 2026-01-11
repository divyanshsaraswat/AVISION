#include "ObjectEngine.h"
#include <iostream>

ObjectEngine::ObjectEngine() : isInitialized(false) {
    // MobileNet SSD standard classes (VOC)
    classNames = {
        "background", "aeroplane", "bicycle", "bird", "boat",
        "bottle", "bus", "car", "cat", "chair", "cow", "diningtable",
        "dog", "horse", "motorbike", "person", "pottedplant",
        "sheep", "sofa", "train", "tvmonitor"
    };
}

bool ObjectEngine::init(const std::string& prototxt, const std::string& model) {
    try {
        net = cv::dnn::readNetFromCaffe(prototxt, model);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // Ensure it runs on CPU
        isInitialized = !net.empty();
        return isInitialized;
    } catch (const cv::Exception& e) {
        std::cerr << "[ObjectEngine] Error loading model: " << e.what() << std::endl;
        return false;
    }
}

std::vector<DetectedObject> ObjectEngine::detect(const cv::Mat& frame) {
    std::vector<DetectedObject> results;
    if (!isInitialized || frame.empty()) return results;

    // 1. Prepare blob (Resize to 300x300, Scale factor 0.007843, Mean 127.5)
    // Standard MobileNet-SSD preprocessing
    cv::Mat blob = cv::dnn::blobFromImage(frame, 0.007843, cv::Size(300, 300), 
                                          cv::Scalar(127.5, 127.5, 127.5), false, false);
    
    net.setInput(blob);
    
    // 2. Forward pass
    cv::Mat output = net.forward();
    
    // 3. Parse output
    // Output shape: [1, 1, N, 7] -> (batch, class, score, x, y, w, h)
    cv::Mat detectionMat(output.size[2], output.size[3], CV_32F, output.ptr<float>());

    for (int i = 0; i < detectionMat.rows; i++) {
        float confidence = detectionMat.at<float>(i, 2);

        if (confidence > 0.5f) { // Threshold 50%
            int classId = static_cast<int>(detectionMat.at<float>(i, 1));
            
            int xLeftBottom = static_cast<int>(detectionMat.at<float>(i, 3) * frame.cols);
            int yLeftBottom = static_cast<int>(detectionMat.at<float>(i, 4) * frame.rows);
            int xRightTop   = static_cast<int>(detectionMat.at<float>(i, 5) * frame.cols);
            int yRightTop   = static_cast<int>(detectionMat.at<float>(i, 6) * frame.rows);

            // Clip to frame
            cv::Rect objectBox(xLeftBottom, yLeftBottom,
                               (xRightTop - xLeftBottom),
                               (yRightTop - yLeftBottom));
            
            objectBox = objectBox & cv::Rect(0, 0, frame.cols, frame.rows);

            DetectedObject obj;
            obj.classID = classId;
            obj.label = getLabel(classId);
            obj.confidence = confidence;
            obj.boundingBox = objectBox;
            
            results.push_back(obj);
        }
    }
    return results;
}

std::string ObjectEngine::getLabel(int classId) const {
    if (classId >= 0 && classId < classNames.size()) {
        return classNames[classId];
    }
    return "unknown";
}
