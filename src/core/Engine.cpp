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
             ctx.activeAlert = "CRITICAL STOP!";
        } else if (maxDist > 0.6f) { // NEAR
             audio->playTone(AudioUrgency::WARNING);
             ctx.activeAlert = "WARNING: OBSTACLE";
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
    }

    // Draw Detections
    for (const auto& obj : ctx.detections) {
        cv::rectangle(ctx.debugOverlay, obj.boundingBox, cv::Scalar(0, 255, 0), 2);
         std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%";
        cv::putText(ctx.debugOverlay, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
    }
    
    // --- Centralized UI Rendering ---
    // Stack messages effectively at the bottom
    int h = ctx.debugOverlay.rows;
    int w = ctx.debugOverlay.cols;
    int bottomY = h - 40; // Start from bottom up (leaving space for status bars)
    
    auto drawMessage = [&](const std::string& msg, cv::Scalar color, float scale = 0.7) {
        if (msg.empty()) return;
        
        int thickness = 2;
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(msg, cv::FONT_HERSHEY_SIMPLEX, scale, thickness, &baseline);
        
        // Draw Background
        cv::Point textOrg(20, bottomY); // Left Aligned padding 20
        cv::rectangle(ctx.debugOverlay, textOrg + cv::Point(-5, baseline + 5), textOrg + cv::Point(textSize.width + 5, -textSize.height - 5), cv::Scalar(0,0,0), -1);
        
        // Draw Text
        cv::putText(ctx.debugOverlay, msg, textOrg, cv::FONT_HERSHEY_SIMPLEX, scale, color, thickness);
        
        // Move up
        bottomY -= (textSize.height + 15);
    };

    // 1. Navigation Command (Green) - Bottom most text
    drawMessage(ctx.guidanceCommand, cv::Scalar(0, 255, 0), 0.8);

    // 2. Scene Info (Yellow) - Above Nav
    drawMessage(ctx.sceneLabel, cv::Scalar(0, 255, 255), 0.7);

    // 3. Edge Safety Alert (Red Bar)
    if (!ctx.edgeSafetyAlert.empty()) {
        drawMessage(ctx.edgeSafetyAlert, cv::Scalar(0, 0, 255), 0.8);
    }

    // 4. Critical Alert (Red) - User requested: Small font, Bottom Right
    if (!ctx.activeAlert.empty()) {
         std::string msg = ctx.activeAlert;
         float scale = 0.6; // Small font
         int thickness = 2;
         int baseline = 0;
         cv::Size textSize = cv::getTextSize(msg, cv::FONT_HERSHEY_SIMPLEX, scale, thickness, &baseline);
         
         // Position: Bottom Right (padding 20 from right, 40 from bottom)
         cv::Point textOrg(w - textSize.width - 20, h - 40);
         
         // Background
         cv::rectangle(ctx.debugOverlay, textOrg + cv::Point(-5, baseline + 5), textOrg + cv::Point(textSize.width + 5, -textSize.height - 5), cv::Scalar(0,0,0), -1);
         
         // Text (Red)
         cv::putText(ctx.debugOverlay, msg, textOrg, cv::FONT_HERSHEY_SIMPLEX, scale, cv::Scalar(0, 0, 255), thickness);
    }
    
    debugOut = ctx.debugOverlay;
    frameCount++;
    return true;
}
