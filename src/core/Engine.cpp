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

    // Init Object Detection
    // Note: On Android, paths will be different. We will handle assets differently in Phase 3b.
    // For now, assume relative path or passed in config.
    if (!objectDetector.init("models/MobileNetSSD_deploy.prototxt", "models/MobileNetSSD_deploy.caffemodel")) {
        std::cerr << "Warning: Failed to load MobileNet-SSD. Object detection disabled." << std::endl;
        // return true; // Use true even if strict warning, so app doesn't crash on geometry-only
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
