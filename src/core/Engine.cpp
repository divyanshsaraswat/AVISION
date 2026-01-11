#include "Engine.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <chrono>

Engine::Engine(std::shared_ptr<ICamera> cam, std::shared_ptr<IAudio> aud)
    : camera(cam), audio(aud), isRunning(false) {}

void Engine::run() {
    if (!camera->isOpened()) {
        std::cerr << "Camera not initialized!" << std::endl;
        return;
    }

    isRunning = true;
    cv::Mat frame, debugFrame;
    
    // Init Object Detection
    if (!objectDetector.init("models/MobileNetSSD_deploy.prototxt", "models/MobileNetSSD_deploy.caffemodel")) {
        std::cerr << "Warning: Failed to load MobileNet-SSD. Object detection disabled." << std::endl;
    }

    int frameCount = 0;
    std::vector<DetectedObject> cachedObjects;

    while (isRunning) {
        auto start = std::chrono::high_resolution_clock::now();

        // 1. Capture
        if (!camera->capture(frame)) {
            std::cerr << "Failed to capture frame" << std::endl;
            break;
        }

        // 2. Core Processing (Geometry Phase) - ALWAYS ON
        geometry.process(frame, debugFrame);

        // 3. Semantics Phase (Object Detection) - THROTTLED (Every 5 frames)
        if (frameCount % 5 == 0) {
            cachedObjects = objectDetector.detect(frame);
        }
        frameCount++;

        // 4. Logic & Decision
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
                    cv::putText(debugFrame, "CRITICAL STOP!", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 3);
                } else if (cat == DistanceCategory::NEAR) {
                    audio->playTone(AudioUrgency::WARNING);
                    cv::putText(debugFrame, "WARNING: OBSTACLE", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
                }
            } else {
                 audio->playTone(AudioUrgency::WARNING);
            }
        }

        // B. Semantic Feedback (Priority 2)
        // Show detected objects
        for (const auto& obj : cachedObjects) {
            cv::rectangle(debugFrame, obj.boundingBox, cv::Scalar(0, 255, 0), 2);
            std::string label = obj.label + " " + std::to_string((int)(obj.confidence * 100)) + "%";
            cv::putText(debugFrame, label, cv::Point(obj.boundingBox.x, obj.boundingBox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
            
            // Simple audio feedback for now (Console log)
            // In real app: Avoid spamming this. Only speak if new or central.
             // if (obj.confidence > 0.8) audio->speak(obj.label); 
        }

        // 5. Visualization (Desktop Shell only)
        cv::imshow("AVision Desktop Debug", debugFrame);
        if (cv::waitKey(1) == 27) { // ESC
            isRunning = false;
        }

        // Latency check
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms = end - start;
        // std::cout << "Frame time: " << ms.count() << "ms" << std::endl;
    }
}
