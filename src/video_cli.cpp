#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <sys/stat.h>
#include "core/semantics/ObjectEngine.h"
#include "core/geometry/GeometryEngine.h"
#include "core/distance/DistanceEngine.h"
#include "core/depth/DepthEngine.h"

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

    // 3. Initialize Geometry & Depth Engine
    GeometryEngine geometry;
    DepthEngine depthEngine;
    bool depthLoaded = depthEngine.init("models/midas-v2_1-small-192x256.onnx");
    if (depthLoaded) {
        std::cout << "[AVisionCLI] Depth Engine initialized successfully." << std::endl;
    } else {
         std::cerr << "[AVisionCLI] WARNING: Failed to initialize Depth Engine. Depth view will be disabled." << std::endl;
    }

    cv::Mat frame;
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frameIdx = 0;
    int detectInterval = 3;  // Run detection every 3 frames (~10 FPS)
    int depthInterval = (fps > 0) ? (int)fps : 30; // Run depth once per second (1 FPS)
    
    // Persistent caches for dual-rate processing
    std::vector<DetectedObject> cachedObjects;
    cv::Mat cachedDepthMap;
    double cachedMaxDepth = 1.0;

    std::cout << "[AVisionCLI] Rates -> Detection: Every " << detectInterval << " frames. Depth: Every " << depthInterval << " frames." << std::endl;
    std::cout << "[AVisionCLI] Starting inference... Press ESC to stop." << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    while (cap.read(frame)) {
        // Calculate timestamp
        double timestampSec = frameIdx / (fps > 0 ? fps : 30.0);
        
        // 1. Detect Objects (Fast Interval)
        if (frameIdx % detectInterval == 0) {
             cachedObjects = engine.detect(frame);
        }

        // 2. Process Geometry
        cv::Mat debugFrame;
        // Always draw frame
        geometry.process(frame, debugFrame);

        // 3. Process Depth (Slow Interval)
        if (depthLoaded && frameIdx % depthInterval == 0) {
            cachedDepthMap = depthEngine.estimateDepth(frame);
            if (!cachedDepthMap.empty()) {
                cachedMaxDepth = 0.0;
                cv::minMaxLoc(cachedDepthMap, nullptr, &cachedMaxDepth);
                if (cachedMaxDepth <= 0.0001) cachedMaxDepth = 1.0;
            }
        }
        
        // Visualization: Use Cached Depth Map for side-by-side
        if (depthLoaded && !cachedDepthMap.empty()) {
             // We need to clone or reuse the map for visualization
             cv::Mat vizDepth = cachedDepthMap.clone(); // Clone to avoid modifying cache during viz
             double minVal, maxVal;
             cv::minMaxLoc(vizDepth, &minVal, &maxVal);
             if (maxVal > minVal) {
                    vizDepth.convertTo(vizDepth, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
                    cv::applyColorMap(vizDepth, vizDepth, cv::COLORMAP_MAGMA);
                    cv::resize(vizDepth, vizDepth, cv::Size(vizDepth.cols * debugFrame.rows / vizDepth.rows, debugFrame.rows));
                    cv::hconcat(debugFrame, vizDepth, debugFrame);
             }
        }

        // 4. Check Safety / Collision Logic
        if (!geometry.isPathSafe()) {
            const auto& obstacles = geometry.getObstacles();
            if (!obstacles.empty()) {
                float maxDist = 0.0f;
                for (const auto& obs : obstacles) {
                    if (obs.relativeDistance > maxDist) maxDist = obs.relativeDistance;
                }
                
                DistanceCategory cat = DistanceEngine::estimateCategory(maxDist);
                
                if (cat == DistanceCategory::IMMEDIATE) {
                    cv::putText(debugFrame, "CRITICAL STOP!", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 3);
                } else if (cat == DistanceCategory::NEAR) {
                    cv::putText(debugFrame, "WARNING: OBSTACLE", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
                }
            } else {
                 cv::putText(debugFrame, "WARNING: UNSAFE PATH", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 165, 255), 2);
            }
        }

        // 5. Log & Draw Object Detections (Using Cached Data)
        for (auto& obj : cachedObjects) {
            std::string distanceStr = "";
            cv::Scalar color = cv::Scalar(0, 255, 0); // Green (Safe)
            
            // FUSE DEPTH (Using Cached Map)
            if (depthLoaded && !cachedDepthMap.empty()) {
                   // Clamp ROI to map size
                   cv::Rect safeBox = obj.boundingBox & cv::Rect(0, 0, cachedDepthMap.cols, cachedDepthMap.rows);
                   if (safeBox.area() > 0) {
                       cv::Scalar meanDepth = cv::mean(cachedDepthMap(safeBox));
                       
                       float relativeDepth = (float)meanDepth[0] / (float)cachedMaxDepth; 
                       
                       distanceStr = " D:" + std::to_string((int)(relativeDepth * 100)) + "%";

                       if (relativeDepth > 0.6f) {
                           color = cv::Scalar(0, 0, 255); // Red
                           distanceStr += " [CLOSE]";
                       } else if (relativeDepth > 0.4f) {
                           color = cv::Scalar(0, 255, 255); // Yellow
                       }
                   }
            }

            // Console (Only log on update frames to avoid spamming)
            if (frameIdx % detectInterval == 0) {
                std::cout << "[" << std::fixed << std::setprecision(2) << timestampSec << "s] "
                          << "Detected: " << obj.label << " (" << int(obj.confidence * 100) << "%)" 
                          << distanceStr << std::endl;
            }
            
            // Draw
            cv::rectangle(debugFrame, obj.boundingBox, color, 2);
            std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%" + distanceStr;
            cv::putText(debugFrame, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 10), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
        }

        cv::imshow("AVision Video CLI", debugFrame);
        if (cv::waitKey(1) == 27) break; // ESC

        frameIdx++;
    }

    if (outFile.is_open()) outFile.close();
    std::cout << "[AVisionCLI] Processing complete. Results saved to result.txt" << std::endl;
    
    return 0;
}
