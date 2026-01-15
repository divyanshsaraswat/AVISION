#include "Engine.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <chrono>

Engine::Engine(std::shared_ptr<ICamera> cam, std::shared_ptr<IAudio> aud)
    : camera(cam), audio(aud), isRunning(false) {}

Engine::~Engine() = default;

void Engine::addModule(std::unique_ptr<IModule> module) {
    modules.push_back(std::move(module));
}

void Engine::configureModule(const std::string& moduleName, const std::map<std::string, std::string>& params) {
    moduleConfigs[moduleName] = params;
}

bool Engine::init() {
    std::cout << "[Engine] Starting Modular Pipeline..." << std::endl;
    audio->playTone(AudioUrgency::INFO); 
    
    if (camera && !camera->isOpened()) {
        std::cerr << "Camera not initialized!" << std::endl;
        return false;
    }

    // Initialize all modules
    for (auto& mod : modules) {
        std::cout << "[Engine] Initializing " << mod->getName() << "..." << std::endl;
        
        // Look for config
        std::map<std::string, std::string> params;
        if (moduleConfigs.count(mod->getName())) {
            params = moduleConfigs[mod->getName()];
        }

        if (!mod->init(params)) {
            std::cerr << "[Engine] Error: Failed to init " << mod->getName() << std::endl;
        }
    }
    
    isRunning = true;
    frameCount = 0;
    return true;
}

void Engine::stop() {
    isRunning = false;
    // Cleanup if needed
    std::cout << "Stopping A-Vision Engine." << std::endl;
}

bool Engine::processFrame(cv::Mat& frame, cv::Mat& debugOut) {
    std::vector<DetectedObject> dummy;
    return processFrame(frame, debugOut, dummy);
}

bool Engine::processFrame(cv::Mat& frame, cv::Mat& debugOut, std::vector<DetectedObject>& outDetections) {
    if (!isRunning || frame.empty()) return false;

    // 1. Setup Context
    Context ctx;
    ctx.rawFrame = frame;
    ctx.timestamp = frameCount * 0.033; // Approx
    ctx.debugOverlay = frame.clone(); // Base for drawing
    
    // 2. Run Pipeline
    for (auto& mod : modules) {
        mod->process(ctx);
    }
    
    // Export Detections
    outDetections = ctx.detections;
    
    // 3. Global Feedback (Immediate Audio)
    // The "Risk" logic is currently in the Engine for simplicity of audio access
    // Ideally this moves to a "FeedbackModule" that takes IAudio.
    
    if (!ctx.isPathSafe) {
        float maxDist = 0.0f;
        for (const auto& obs : ctx.obstacles) {
             if (obs.relativeDistance > maxDist) maxDist = obs.relativeDistance;
        }
        
        // Simple Thresholds (DistanceEngine logic duplicated here for now)
        if (maxDist > 0.85f) { // IMMEDIATE
             audio->playTone(AudioUrgency::CRITICAL);
             cv::putText(ctx.debugOverlay, "CRITICAL STOP!", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 3);
        } else if (maxDist > 0.6f) { // NEAR
             audio->playTone(AudioUrgency::WARNING);
             cv::putText(ctx.debugOverlay, "WARNING: OBSTACLE", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
        } else {
             audio->playTone(AudioUrgency::WARNING);
        }
    }

    // Visualize Depth if present
    if (!ctx.depthMap.empty()) {
        cv::Mat depthVis;
        cv::normalize(ctx.depthMap, depthVis, 0, 255, cv::NORM_MINMAX, CV_8U);
        cv::applyColorMap(depthVis, depthVis, cv::COLORMAP_MAGMA);
        
        cv::resize(depthVis, depthVis, ctx.debugOverlay.size());
        
        // Blend: 0.7 Original + 0.3 Depth
        cv::addWeighted(ctx.debugOverlay, 0.7, depthVis, 0.3, 0, ctx.debugOverlay);
        
        // Also show small inset for clarity? 
        // For now, blending is good for context.
    }

    // Draw Detections
    for (const auto& obj : ctx.detections) {
        cv::rectangle(ctx.debugOverlay, obj.boundingBox, cv::Scalar(0, 255, 0), 2);
         std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%";
        cv::putText(ctx.debugOverlay, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
    }
    
    // Draw Active Alert
    if (!ctx.activeAlert.empty()) {
        cv::putText(ctx.debugOverlay, ctx.activeAlert, cv::Point(50, 400), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 0, 255), 3);
    }
    
    debugOut = ctx.debugOverlay;
    frameCount++;
    return true;
}
