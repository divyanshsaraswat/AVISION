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

    double t_start = (double)cv::getTickCount();

    // 1. Preprocess
    cv::Mat inputBlob = cv::dnn::blobFromImage(ctx.rawFrame, 1.0, cv::Size(224, 224), cv::Scalar(123.68, 116.78, 103.94), true, false);
    
    // 2. Inference
    net.setInput(inputBlob);
    cv::Mat prob = net.forward();
    
    double t_end = (double)cv::getTickCount();
    double time_ms = ((t_end - t_start) / cv::getTickFrequency()) * 1000.0;

    // 3. Post-process (Softmax & Label)
    double conf;
    cv::Point classIdPoint;
    cv::minMaxLoc(prob, 0, &conf, 0, &classIdPoint);
    
    int classId = classIdPoint.x;
    if (classId >= 0 && classId < classes.size()) {
        std::string scene = classes[classId];
        ctx.sceneLabel = "Scene: " + scene;
        
        // Log
        std::string log = "[Scene] " + std::to_string(time_ms).substr(0,4) + " ms, " + scene + " (" + std::to_string(conf).substr(0,4) + ")";
        ctx.moduleLogs.push_back(log);
    }
}
