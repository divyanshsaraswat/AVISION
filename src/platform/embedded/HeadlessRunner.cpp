#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <csignal>
#include "core/Engine.h"
#include "platform/desktop/DesktopCamera.h"
#include "platform/desktop/DesktopAudio.h" // We can reuse DesktopAudio if ALSA is available, or create console-only

// Global signal handler for graceful shutdown
std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Stopping..." << std::endl;
    keepRunning = false;
}

int main(int argc, char** argv) {
    // Register signal handler (Ctrl+C)
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        std::cout << "[Embedded] Starting Headless Mode..." << std::endl;

        // 1. Init Platform (Headless)
        // We can reuse DesktopCamera (OpenCV works fine without GUI)
        // We reuse DesktopAudio (System beeps work on many Linux distros, or just logs)
        auto camera = std::make_shared<DesktopCamera>(0); 
        auto audio = std::make_shared<DesktopAudio>();

        // 2. Init Engine
        Engine engine(camera, audio);
        if (!engine.init()) {
            std::cerr << "[Error] Failed to initialize Engine." << std::endl;
            return 1;
        }

        std::cout << "[Embedded] Engine Initialized. Running loop..." << std::endl;

        // 3. Headless Loop
        cv::Mat frame, debugFrame;
        int frameCounter = 0;

        while (keepRunning) {
            // A. Capture
            if (!camera->capture(frame)) {
                std::cerr << "[Error] Camera capture failed." << std::endl;
                break; // Or retry
            }

            // B. Process
            // Pass debugFrame, but we won't show it (save CPU)
            engine.processFrame(frame, debugFrame);

            // C. Logging (Heartbeat every 100 frames)
            frameCounter++;
            if (frameCounter % 100 == 0) {
                 std::cout << "[Status] Processed " << frameCounter << " frames. Safe." << std::endl;
            }
            
            // D. Sleep?
            // On embedded, we might want to spare CPU.
            // cv::waitKey is not needed for loop, but a small sleep helps prevents 100% CPU usage if capture is fast.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 4. Cleanup
        engine.stop();
        std::cout << "[Embedded] Shutdown complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
