#include "SceneUnderstandingModule.h"
#include <iostream>
#include <fstream>
#include <opencv2/imgproc.hpp>

SceneUnderstandingModule::SceneUnderstandingModule() {}

bool SceneUnderstandingModule::init(const std::map<std::string, std::string>& params) {
    std::string modelPath = "";
    std::string classesPath = "";

    if (params.find("modelPath") != params.end()) modelPath = params.at("modelPath");
    if (params.find("classesPath") != params.end()) classesPath = params.at("classesPath");
    if (params.find("interval") != params.end()) processInterval = std::stoi(params.at("interval"));

    try {
        if (!classesPath.empty()) {
            loadClasses(classesPath);
        }
        
        if (!modelPath.empty()) {
            net = cv::dnn::readNet(modelPath);
            modelLoaded = !net.empty();
            if (modelLoaded) {
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
                std::cout << "[SceneUnderstandingModule] Loaded model: " << modelPath << std::endl;
            } else {
                 std::cout << "[SceneUnderstandingModule] Failed to load model." << std::endl;
            }
        } else {
            std::cout << "[SceneUnderstandingModule] No model path provided. Passive mode." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[SceneUnderstandingModule] Error: " << e.what() << std::endl;
        modelLoaded = false;
    }

    return true;
}

void SceneUnderstandingModule::loadClasses(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;
    std::string line;
    while (std::getline(ifs, line)) {
        classes.push_back(line);
    }
}

void SceneUnderstandingModule::process(Context& ctx) {
    frameCount++;
    if (!modelLoaded || classes.empty() || frameCount % processInterval != 0 || ctx.rawFrame.empty()) return;

    // Inference
    // Torchvision models expect (pixel - mean) / std
    // Mean ~ [123, 116, 103], Std ~ [58, 57, 57]
    // Scale factor should be 1.0 / 58.0 ~= 0.017
    cv::Mat blob = cv::dnn::blobFromImage(ctx.rawFrame, 0.017, cv::Size(224, 224), cv::Scalar(123.68, 116.78, 103.94), true, false);
    
    // Inference
    net.setInput(blob);
    cv::Mat prob = net.forward();
    
    // Find best class
    double conf;
    cv::Point classIdPoint;
    cv::minMaxLoc(prob, 0, &conf, 0, &classIdPoint);
    
    int classId = classIdPoint.x;
    if (classId >= 0 && classId < classes.size()) {
        std::string scene = classes[classId];
        ctx.sceneLabel = "Scene: " + scene;
        // Optional: Keep simple log or remove entirely for production
        // std::cout << "[Scene] " << scene << " (" << conf << ")" << std::endl;
    }
}
