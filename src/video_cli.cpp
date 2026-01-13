#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <sys/stat.h>
#include "core/semantics/ObjectEngine.h"
#include "core/geometry/GeometryEngine.h"
#include "core/distance/DistanceEngine.h"

// Simple config loader (Duplicated from Engine.cpp for standalone simplicity, 
// or ideally we refactor ConfigLoader to a valid util)
bool loadConfig(ObjectEngine::ModelConfig& config) {
    struct stat buffer;
    if (stat("models/selected_model.json", &buffer) != 0) return false;

    FILE* fp = fopen("models/selected_model.json", "r");
    if (!fp) return false;

    char buf[1024];
    std::string jsonContent;
    while (fgets(buf, 1024, fp)) jsonContent += buf;
    fclose(fp);

    auto getString = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        size_t pos = jsonContent.find(search);
        if (pos == std::string::npos) return "";
        pos = jsonContent.find("\"", pos + search.length()); 
        if (pos == std::string::npos) return "";
        size_t end = jsonContent.find("\"", pos + 1);
        return jsonContent.substr(pos + 1, end - pos - 1);
    };

    auto getValue = [&](const std::string& key) -> std::string {
         std::string search = "\"" + key + "\":";
         size_t pos = jsonContent.find(search);
         if (pos == std::string::npos) return "";
         pos += search.length();
         while (pos < jsonContent.length() && (isspace(jsonContent[pos]))) pos++;
         size_t end = pos;
         while (end < jsonContent.length() && jsonContent[end] != ',' && jsonContent[end] != '}' && !isspace(jsonContent[end])) end++;
         return jsonContent.substr(pos, end - pos);
    };

    config.modelPath = getString("modelPath");
    if (config.modelPath.empty()) return false;
    
    config.configPath = getString("configPath");
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

    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: AVisionCLI.exe --video <path_to_video_file>" << std::endl;
        return 1;
    }

    std::string videoPath;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--video" && i + 1 < argc) {
            videoPath = argv[i + 1];
        }
    }

    if (videoPath.empty()) {
        std::cerr << "Error: No video file specified." << std::endl;
        return 1;
    }

    std::cout << "[AVisionCLI] Opening video: " << videoPath << std::endl;
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file." << std::endl;
        return 1;
    }

    // 1. Initialize Engine
    ObjectEngine engine;
    ObjectEngine::ModelConfig config;

    
    // Check if config exists, if not run setup
    struct stat buffer;
    if (stat("models/selected_model.json", &buffer) != 0) {
        std::cout << "[AVisionCLI] No model configuration found." << std::endl;
        std::cout << "[AVisionCLI] Launching setup wizard..." << std::endl;
        system("download_models.bat");
    }

    if (loadConfig(config)) {
        std::cout << "[AVisionCLI] Loaded config: " << config.modelPath << " (" << config.dataset << ")" << std::endl;
    } else {
        std::cout << "[AVisionCLI] Config load failed or skipped. Using Legacy Defaults." << std::endl;
        config.modelPath = "models/MobileNetSSD_deploy.caffemodel";
        config.configPath = "models/MobileNetSSD_deploy.prototxt";
        config.type = ObjectEngine::ModelType::SSD_MOBILENET;
    }

    if (!engine.init(config)) {
        std::cerr << "Error: Failed to initialize Object Engine." << std::endl;
        return 1;
    }

    // 2. Prepare Output
    std::ofstream outFile("result.txt");
    if (!outFile.is_open()) {
        std::cerr << "Warning: Could not create result.txt" << std::endl;
    }

    // 3. Initialize Geometry Engine
    GeometryEngine geometry;

    cv::Mat frame;
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frameIdx = 0;

    std::cout << "[AVisionCLI] Starting inference... Press ESC to stop." << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    while (cap.read(frame)) {
        // Calculate timestamp
        double timestampSec = frameIdx / (fps > 0 ? fps : 30.0);
        
        // 1. Detect Objects (on raw frame)
        std::vector<DetectedObject> objects = engine.detect(frame);

        // 2. Process Geometry (Safe zones, Obstacles)
        cv::Mat debugFrame;
        geometry.process(frame, debugFrame);
        // debugFrame now contains the geometry visualization (Safe Zone colors, Obstacle boxes)

        // 3. Check Safety / Collision Logic
        if (!geometry.isPathSafe()) {
            const auto& obstacles = geometry.getObstacles();
            if (!obstacles.empty()) {
                float maxDist = 0.0f;
                for (const auto& obs : obstacles) {
                    if (obs.relativeDistance > maxDist) maxDist = obs.relativeDistance;
                }
                
                DistanceCategory cat = DistanceEngine::estimateCategory(maxDist);
                
                std::string warningText = "";
                cv::Scalar warningColor(0, 255, 0); // Default Green (Safe-ish)

                if (cat == DistanceCategory::IMMEDIATE) {
                    warningText = "CRITICAL STOP!";
                    warningColor = cv::Scalar(0, 0, 255); // Red
                } else if (cat == DistanceCategory::NEAR) {
                    warningText = "WARNING: OBSTACLE";
                    warningColor = cv::Scalar(0, 255, 255); // Yellow
                }
                
                if (!warningText.empty()) {
                    cv::putText(debugFrame, warningText, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, warningColor, 3);
                }
            } else {
                 // Path not safe but no specific obstacle identified (e.g. rough terrain/edges)
                 cv::putText(debugFrame, "WARNING: UNSAFE PATH", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 165, 255), 2);
            }
        }

        // 4. Log & Draw Object Detections (on top of geometry viz)
        for (const auto& obj : objects) {
            // Console
            std::cout << "[" << std::fixed << std::setprecision(2) << timestampSec << "s] "
                      << "Detected: " << obj.label << " (" << int(obj.confidence * 100) << "%)" << std::endl;
            
            // File
            if (outFile.is_open()) {
                outFile << "[" << std::fixed << std::setprecision(2) << timestampSec << "s] "
                        << "Detected: " << obj.label << " (" << int(obj.confidence * 100) << "%)" << std::endl;
            }

            // Draw
            cv::rectangle(debugFrame, obj.boundingBox, cv::Scalar(0, 255, 0), 2);
            std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%";
            cv::putText(debugFrame, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 10), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        }

        cv::imshow("AVision Video CLI", debugFrame);
        if (cv::waitKey(1) == 27) break; // ESC

        frameIdx++;
    }

    if (outFile.is_open()) outFile.close();
    std::cout << "[AVisionCLI] Processing complete. Results saved to result.txt" << std::endl;
    
    return 0;
}
