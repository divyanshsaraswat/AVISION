#include <iostream>
#include <memory>
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

        // 3. Run
        engine.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
