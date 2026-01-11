#include <iostream>
#include <memory>
#include <opencv2/highgui.hpp> // Critical for imshow/waitKey
#include "core/Engine.h"
#include "platform/desktop/DesktopCamera.h"
#include "platform/desktop/DesktopAudio.h"

int main() {
    try {
        // 1. Init Platform Layer (The "Body")
        auto camera = std::make_shared<DesktopCamera>(0); // Default webcam
        auto audio = std::make_shared<DesktopAudio>();

        // 2. Init Core Layer (The "Brain")
        Engine engine(camera, audio);

        // 3. Init Engine
        if (!engine.init()) {
            return 1;
        }

        // 4. Run Loop (Desktop owns the loop now)
        cv::Mat frame, debugFrame;
        
        while (true) {
            // A. Platform Input
            if (!camera->capture(frame)) break;

            // B. Core Processing
            engine.processFrame(frame, debugFrame);

            // C. Platform Output (Desktop Window)
            if (!debugFrame.empty()) {
                cv::imshow("AVision Desktop Debug", debugFrame);
                if (cv::waitKey(1) == 27) break; // ESC to quit
            }
        }
        
        // 5. Cleanup
        engine.stop();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
