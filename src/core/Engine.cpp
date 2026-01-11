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
    
    std::cout << "Starting A-Vision Engine..." << std::endl;
    audio->playTone(AudioUrgency::INFO); // Startup beep

    while (isRunning) {
        auto start = std::chrono::high_resolution_clock::now();

        // 1. Capture
        if (!camera->capture(frame)) {
            std::cerr << "Failed to capture frame" << std::endl;
            break;
        }

        // 2. Core Processing (Geometry Phase)
        geometry.process(frame, debugFrame);

        // 3. Logic & Decision
        if (!geometry.isPathSafe()) {
            // Path blockage!
            // Find closest obstacle for distance
            const auto& obstacles = geometry.getObstacles();
            if (!obstacles.empty()) {
                // Get the closest one (highest relative distance value)
                float maxDist = 0.0f;
                for (const auto& obs : obstacles) {
                    if (obs.relativeDistance > maxDist) maxDist = obs.relativeDistance;
                }
                
                DistanceCategory cat = DistanceEngine::estimateCategory(maxDist);
                
                // Map category to audio
                if (cat == DistanceCategory::IMMEDIATE) {
                    audio->playTone(AudioUrgency::CRITICAL);
                    cv::putText(debugFrame, "CRITICAL STOP!", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 3);
                } else if (cat == DistanceCategory::NEAR) {
                    audio->playTone(AudioUrgency::WARNING);
                    cv::putText(debugFrame, "WARNING: OBSTACLE", cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
                }
            } else {
                 // Unsafe path but no specific object? Maybe ground gap.
                 audio->playTone(AudioUrgency::WARNING);
            }
        }

        // 4. Visualization (Desktop Shell only)
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
