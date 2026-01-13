#include "Engine.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <chrono>

Engine::Engine(std::shared_ptr<ICamera> cam, std::shared_ptr<IAudio> aud)
    : camera(cam), audio(aud), isRunning(false) {}

bool Engine::init() {
    std::cout << "Starting A-Vision Engine..." << std::endl;
    audio->playTone(AudioUrgency::INFO); // Startup beep
    
    if (!camera->isOpened()) {
        std::cerr << "Camera not initialized!" << std::endl;
        return false;
    }

    // 3. Init Object Detection from Config
    ObjectEngine::ModelConfig config;
    bool configLoaded = false;
    
    // Try to load models/selected_model.json
    FILE* fp = fopen("models/selected_model.json", "r");
    if (fp) {
        char buffer[1024];
        std::string jsonContent;
        while (fgets(buffer, 1024, fp)) {
            jsonContent += buffer;
        }
        fclose(fp);
        
        // Simple manual JSON parsing (Dependency-free)
        // Helper lambda to extract string value
        auto getString = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":";
            size_t pos = jsonContent.find(search);
            if (pos == std::string::npos) return "";
            pos = jsonContent.find("\"", pos + search.length()); 
            if (pos == std::string::npos) return "";
            size_t end = jsonContent.find("\"", pos + 1);
            return jsonContent.substr(pos + 1, end - pos - 1);
        };

        // Helper lambda to extract int/float/bool
        auto getValue = [&](const std::string& key) -> std::string {
             std::string search = "\"" + key + "\":";
             size_t pos = jsonContent.find(search);
             if (pos == std::string::npos) return "";
             pos += search.length();
             while (pos < jsonContent.length() && (isspace(jsonContent[pos]))) pos++; // trim leading
             size_t end = pos;
             while (end < jsonContent.length() && jsonContent[end] != ',' && jsonContent[end] != '}' && !isspace(jsonContent[end])) end++;
             return jsonContent.substr(pos, end - pos);
        };

        std::string modelFile = getString("modelPath");
        if (!modelFile.empty()) {
            std::cout << "[Engine] Loading config from models/selected_model.json" << std::endl;
            config.modelPath = modelFile;
            std::string confPath = getString("configPath");
            if (!confPath.empty()) config.configPath = confPath;
            
            std::string typeStr = getString("type");
            if (typeStr == "SSD_MOBILENET") config.type = ObjectEngine::ModelType::SSD_MOBILENET;
            else if (typeStr == "SSD_TF") config.type = ObjectEngine::ModelType::SSD_TF;
            else if (typeStr == "YOLO_V8") config.type = ObjectEngine::ModelType::YOLO_V8;

            std::string widthStr = getValue("inputWidth");
            if (!widthStr.empty()) config.inputWidth = std::stoi(widthStr);
            
            std::string heightStr = getValue("inputHeight");
            if (!heightStr.empty()) config.inputHeight = std::stoi(heightStr);
            
            std::string scaleStr = getValue("scale");
            if (!scaleStr.empty()) config.scale = std::stof(scaleStr);
            
            std::string swapStr = getValue("swapRB");
            if (swapStr.find("true") != std::string::npos) config.swapRB = true;
            
            std::string datasetStr = getString("dataset");
            if (!datasetStr.empty()) config.dataset = datasetStr;
            
            std::string meanStr = getString("mean");
            if (!meanStr.empty()) {
                // Parse "127.5,127.5,127.5"
                std::stringstream ss(meanStr);
                std::string segment;
                std::vector<float> vals;
                while(std::getline(ss, segment, ',')) {
                    vals.push_back(std::stof(segment));
                }
                if (vals.size() >= 3) {
                     config.mean = cv::Scalar(vals[0], vals[1], vals[2]);
                }
            }
            
            configLoaded = true;
        }
    }

    if (!configLoaded) {
        std::cout << "[Engine] No config found. Fallback to legacy default." << std::endl;
        // Fallback defaults
        config.modelPath = "models/MobileNetSSD_deploy.caffemodel";
        config.configPath = "models/MobileNetSSD_deploy.prototxt";
        config.type = ObjectEngine::ModelType::SSD_MOBILENET;
    }

    if (!objectDetector.init(config)) {
        std::cerr << "Warning: Failed to load Object Detection Model (" << config.modelPath << ")." << std::endl;
        std::cerr << "Please run 'download_models.bat' to setup a model." << std::endl;
    } else {
        std::cout << "[Engine] Object Detection initialized: " << config.modelPath << std::endl;
    }
    isRunning = true;
    frameCount = 0;
    cachedObjects.clear();
    return true;
}

void Engine::stop() {
    isRunning = false;
    // Cleanup if needed
    std::cout << "Stopping A-Vision Engine." << std::endl;
}

bool Engine::processFrame(cv::Mat& frame, cv::Mat& debugOut) {
    if (!isRunning) return false;
    if (frame.empty()) return false;

    // 1. Core Processing (Geometry Phase) - ALWAYS ON
    geometry.process(frame, debugOut);

    // 2. Semantics Phase (Object Detection) - THROTTLED (Every 5 frames)
    if (frameCount % 5 == 0) {
        cachedObjects = objectDetector.detect(frame);
    }
    frameCount++;

    // 3. Logic & Decision
    // A. Geometry Safety (Priority 1)
    if (!geometry.isPathSafe()) {
        // Path blockage!
        const auto& obstacles = geometry.getObstacles();
        if (!obstacles.empty()) {
            float maxDist = 0.0f;
            for (const auto& obs : obstacles) {
                if (obs.relativeDistance > maxDist) maxDist = obs.relativeDistance;
            }
            
            DistanceCategory cat = DistanceEngine::estimateCategory(maxDist);
            
            if (cat == DistanceCategory::IMMEDIATE) {
                audio->playTone(AudioUrgency::CRITICAL);
                cv::putText(debugOut, "CRITICAL STOP!", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 3);
            } else if (cat == DistanceCategory::NEAR) {
                audio->playTone(AudioUrgency::WARNING);
                cv::putText(debugOut, "WARNING: OBSTACLE", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
            }
        } else {
             audio->playTone(AudioUrgency::WARNING);
        }
    }

    // B. Semantic Feedback (Priority 2)
    // Show detected objects
    for (const auto& obj : cachedObjects) {
        cv::rectangle(debugOut, obj.boundingBox, cv::Scalar(0, 255, 0), 2);
        std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%";
        cv::putText(debugOut, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        
        // Simple audio feedback for now (Console log)
        // In real app: Avoid spamming this. Only speak if new or central.
         // if (obj.confidence > 0.8) audio->speak(obj.label); 
    }
    return true;
}
