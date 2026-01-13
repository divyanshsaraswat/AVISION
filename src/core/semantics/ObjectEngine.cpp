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

bool ObjectEngine::init(const ModelConfig& config) {
    currentConfig = config;
    try {
        // Auto-detect load method based on config
        if (config.type == ModelType::SSD_MOBILENET && !config.configPath.empty()) {
             // Legacy Caffe
             net = cv::dnn::readNetFromCaffe(config.configPath, config.modelPath);
        } else {
             // TFLite / ONNX (Generic)
             net = cv::dnn::readNet(config.modelPath);
        }
        
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        isInitialized = !net.empty();
        return isInitialized;
    } catch (const cv::Exception& e) {
        std::cerr << "[ObjectEngine] Error loading model: " << e.what() << std::endl;
        return false;
    }
}

// Legacy support
bool ObjectEngine::init(const std::string& prototxt, const std::string& model) {
    ModelConfig config;
    config.modelPath = model;
    config.configPath = prototxt;
    config.type = ModelType::SSD_MOBILENET;
    return init(config);
}

std::vector<DetectedObject> ObjectEngine::detect(const cv::Mat& frame) {
    std::vector<DetectedObject> results;
    if (!isInitialized || frame.empty()) return results;

    // 1. Prepare blob using config
    cv::Mat blob = cv::dnn::blobFromImage(frame, currentConfig.scale, 
                                          cv::Size(currentConfig.inputWidth, currentConfig.inputHeight), 
                                          currentConfig.mean, currentConfig.swapRB, false);
    
    net.setInput(blob);
    
    if (currentConfig.type == ModelType::SSD_MOBILENET) {
        // --- SSD Logic ---
        cv::Mat output = net.forward();
        // Output: [1, 1, N, 7]
        cv::Mat detectionMat(output.size[2], output.size[3], CV_32F, output.ptr<float>());

        for (int i = 0; i < detectionMat.rows; i++) {
            float confidence = detectionMat.at<float>(i, 2);

            if (confidence > currentConfig.scoreThreshold) {
                int classId = static_cast<int>(detectionMat.at<float>(i, 1));
                
                int xLeftBottom = static_cast<int>(detectionMat.at<float>(i, 3) * frame.cols);
                int yLeftBottom = static_cast<int>(detectionMat.at<float>(i, 4) * frame.rows);
                int xRightTop   = static_cast<int>(detectionMat.at<float>(i, 5) * frame.cols);
                int yRightTop   = static_cast<int>(detectionMat.at<float>(i, 6) * frame.rows);

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
    } 
    else if (currentConfig.type == ModelType::YOLO_V8) {
        // --- YOLOv8 Logic ---
        std::vector<cv::Mat> outputs;
        net.forward(outputs, net.getUnconnectedOutLayersNames());
        
        // YOLOv8 output is [1, 84, 8400] -> [1, 4+80, N]
        // Rows = Dimensions (cx,cy,w,h + 80 classes), Cols = Proposals
        cv::Mat output = outputs[0]; 
        
        // Transpose to [8400, 84] for easier iteration
        // output.size[1] is 84, output.size[2] is 8400
        int dimensions = output.size[1];
        int rows = output.size[2];
        
        // Need to reshape/transpose to (rows x dimensions)
        // opencv dnn output is NCHW usually, but YOLOv8 export can vary.
        // Let's assume standard ultralytics export: [1, 84, 8400]
        
        output = output.reshape(1, dimensions); 
        cv::Mat t_output = output.t(); // Now [8400, 84]

        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        float* data = (float*)t_output.data;
        
        // Scaling factors (network input to original frame)
        float x_factor = (float)frame.cols / currentConfig.inputWidth;
        float y_factor = (float)frame.rows / currentConfig.inputHeight;

        for (int i = 0; i < rows; ++i) {
             float* classes_scores = data + 4;
             cv::Mat scores(1, classNames.size(), CV_32FC1, classes_scores);
             cv::Point classIdPoint;
             double maxClassScore;
             cv::minMaxLoc(scores, 0, &maxClassScore, 0, &classIdPoint);
             
             if (maxClassScore > currentConfig.scoreThreshold) {
                 float x = data[0]; 
                 float y = data[1];
                 float w = data[2];
                 float h = data[3];
                 
                 int left = int((x - 0.5 * w) * x_factor);
                 int top = int((y - 0.5 * h) * y_factor);
                 int width = int(w * x_factor);
                 int height = int(h * y_factor);
                 
                 boxes.push_back(cv::Rect(left, top, width, height));
                 confidences.push_back((float)maxClassScore);
                 classIds.push_back(classIdPoint.x);
             }
             data += dimensions;
        }
        
        // NMS
        std::vector<int> nms_result;
        cv::dnn::NMSBoxes(boxes, confidences, currentConfig.scoreThreshold, currentConfig.nmsThreshold, nms_result);
        
        for (int idx : nms_result) {
            DetectedObject obj;
            obj.classID = classIds[idx];
            obj.label = getLabel(classIds[idx]);
            obj.confidence = confidences[idx];
            obj.boundingBox = boxes[idx] & cv::Rect(0, 0, frame.cols, frame.rows);
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
