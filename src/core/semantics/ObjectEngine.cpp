#include "ObjectEngine.h"
#include <iostream>

ObjectEngine::ObjectEngine() : isInitialized(false) {
}

bool ObjectEngine::init(const ModelConfig& config) {
    currentConfig = config;
    
    // Set labels based on dataset config
    if (config.dataset == "COCO") {
        classNames = {
            "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
            "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
            "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
            "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
            "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
            "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
            "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
            "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
            "hair drier", "toothbrush"
        };
    } else {
        // Default to VOC
        classNames = {
            "background", "aeroplane", "bicycle", "bird", "boat",
            "bottle", "bus", "car", "cat", "chair", "cow", "diningtable",
            "dog", "horse", "motorbike", "person", "pottedplant",
            "sheep", "sofa", "train", "tvmonitor"
        };
    }

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

// IModule Implementation
// IModule Implementation
bool ObjectEngine::init(const std::map<std::string, std::string>& params) {
    // Default config
    ModelConfig config;
    config.modelPath = "models/MobileNetSSD_deploy.caffemodel";
    config.configPath = "models/MobileNetSSD_deploy.prototxt";
    config.type = ModelType::SSD_MOBILENET;
    
    // Override from Params
    if (params.count("modelPath")) config.modelPath = params.at("modelPath");
    if (params.count("configPath")) config.configPath = params.at("configPath");
    if (params.count("threshold")) config.scoreThreshold = std::stof(params.at("threshold"));
    
    // Throttling Params
    if (params.count("eachFrame")) {
        std::string val = params.at("eachFrame");
        processEveryFrame = (val == "true" || val == "1");
    }
    if (params.count("interval")) {
        skipInterval = std::stoi(params.at("interval"));
    }
    
    return init(config);
}

void ObjectEngine::process(Context& ctx) {
    if (!isInitialized || ctx.rawFrame.empty()) return;
    
    internalFrameCount++;
    
    if (!processEveryFrame) {
        if (internalFrameCount % skipInterval != 0) {
            // Use Cache
            ctx.detections = cachedDetections;
            
            // FIX: Must still report confidence for consistency
             float avgConf = 0.0f;
             if (!ctx.detections.empty()) {
                 for (const auto& d : ctx.detections) avgConf += d.confidence;
                 avgConf /= ctx.detections.size();
             } else {
                 avgConf = 0.5f; 
             }
             ctx.moduleConfidence["ObjectModule"] = avgConf;
            return;
        }
    }

    double t_start = (double)cv::getTickCount();
    
    // Run Detection
    ctx.detections = detect(ctx.rawFrame);
    
    // Update Cache
    cachedDetections = ctx.detections;
    
    double t_end = (double)cv::getTickCount();
    double time_ms = ((t_end - t_start) / cv::getTickFrequency()) * 1000.0;
    
    // Confidence Report: Average confidence of detections, or 1.0 if ran successfully
    float avgConf = 0.0f;
    if (!ctx.detections.empty()) {
        for (const auto& d : ctx.detections) avgConf += d.confidence;
        avgConf /= ctx.detections.size();
    } else {
        avgConf = 0.5f; // Ran but found nothing (neutral)
    }
    ctx.moduleConfidence["ObjectModule"] = avgConf;
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

        for (int i = 0; i < (int)rows; ++i) {
             float* classes_scores = data + 4;
             cv::Mat scores(1, (int)classNames.size(), CV_32FC1, classes_scores);
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
    else if (currentConfig.type == ModelType::SSD_TF) {
        // --- TensorFlow SSD Logic (Multi-Head) ---
        // Expected Outputs: detection_boxes, detection_scores, detection_classes, num_detections
        // Problem: Layer names vary. Heuristic: Check shapes.
        
        std::vector<cv::Mat> outputs;
        std::vector<std::string> outNames = net.getUnconnectedOutLayersNames();
        net.forward(outputs, outNames);
        
        cv::Mat boxes, scores, classes;
        
        // Simple shape heuristic to identify tensors
        for (const auto& out : outputs) {
            // boxes: [1, N, 4] or [1, 4, N]
            if (out.dims == 3 && (out.size[2] == 4 || out.size[1] == 4)) {
                boxes = out; 
            }
            // scores/classes: [1, N]
            else if (out.dims == 2) {
                // Heuristic: Scores are float [0-1], classes are often float or int.
                // It's hard to distinguish perfectly without names. 
                // Let's assume names contain 'score' or 'box' if valid, otherwise rely on order? 
                // Reliable Fallback: Most TF exports have fixed order or names. 
                // Let's rely on standard names if present, or just assume: output[0]=boxes, [1]=classes, [2]=scores
            }
        }
        
        // Re-run forward with named lookup if possible to be safe
        // Typical TF names: "detection_boxes", "detection_scores", "detection_classes"
        // Let's try to find them by substring matching the layer names
        int boxIdx = -1, scoreIdx = -1, classIdx = -1;
        for (size_t i=0; i<outNames.size(); i++) {
            std::string name = outNames[i];
            if (name.find("box") != std::string::npos) boxIdx = (int)i;
            else if (name.find("score") != std::string::npos || name.find("conf") != std::string::npos) scoreIdx = (int)i;
            else if (name.find("class") != std::string::npos) classIdx = (int)i;
        }
        
        if (boxIdx >= 0 && scoreIdx >= 0 && classIdx >= 0) {
             boxes = outputs[boxIdx];
             scores = outputs[scoreIdx];
             classes = outputs[classIdx];
             
             // Ensure correct shape [1, N, 4] for boxes
             if (boxes.dims == 3 && boxes.size[1] == 4) { 
                 // [1, 4, N] -> [1, N, 4]
                 cv::Mat t; 
                 // Transpose logic if needed... usually TF is [1, N, 4] (ymin, xmin, ymax, xmax)
             }
             
             float* boxData = (float*)boxes.data;
             float* scoreData = (float*)scores.data;
             float* classData = (float*)classes.data;
             
             int numProposals = boxes.size[1]; // Assuming [1, N, 4]
             
             for (int i = 0; i < numProposals; i++) {
                 float score = scoreData[i];
                 if (score > currentConfig.scoreThreshold) {
                     // TF Box: [ymin, xmin, ymax, xmax]
                     float ymin = boxData[i*4 + 0];
                     float xmin = boxData[i*4 + 1];
                     float ymax = boxData[i*4 + 2];
                     float xmax = boxData[i*4 + 3];
                     
                     int left =   (int)(xmin * frame.cols);
                     int top =    (int)(ymin * frame.rows);
                     int width =  (int)((xmax - xmin) * frame.cols);
                     int height = (int)((ymax - ymin) * frame.rows);
                     
                     DetectedObject obj;
                     obj.classID = (int)classData[i];
                     obj.label = getLabel(obj.classID);
                     obj.confidence = score;
                     obj.boundingBox = cv::Rect(left, top, width, height) & cv::Rect(0, 0, frame.cols, frame.rows);
                     
                     results.push_back(obj);
                 }
             }
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
