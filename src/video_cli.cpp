#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <sys/stat.h>
#include "core/semantics/ObjectEngine.h"

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
    if (typeStr == "YOLO_V8") config.type = ObjectEngine::ModelType::YOLO_V8;
    else config.type = ObjectEngine::ModelType::SSD_MOBILENET;

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
    if (loadConfig(config)) {
        std::cout << "[AVisionCLI] Loaded config: " << config.modelPath << " (" << config.dataset << ")" << std::endl;
    } else {
        std::cout << "[AVisionCLI] No config found. Using Legacy Defaults." << std::endl;
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

    cv::Mat frame;
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frameIdx = 0;

    std::cout << "[AVisionCLI] Starting inference... Press ESC to stop." << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    while (cap.read(frame)) {
        // Calculate timestamp
        double timestampSec = frameIdx / (fps > 0 ? fps : 30.0);
        
        // Detect
        std::vector<DetectedObject> objects = engine.detect(frame);

        // Log & Draw
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
            cv::rectangle(frame, obj.boundingBox, cv::Scalar(0, 255, 0), 2);
            std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%";
            cv::putText(frame, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 10), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        }

        cv::imshow("AVision Video CLI", frame);
        if (cv::waitKey(1) == 27) break; // ESC

        frameIdx++;
    }

    if (outFile.is_open()) outFile.close();
    std::cout << "[AVisionCLI] Processing complete. Results saved to result.txt" << std::endl;
    
    return 0;
}
