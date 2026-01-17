#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <sys/stat.h>
#include <filesystem>
#include "core/Engine.h"
#include "platform/desktop/DesktopAudio.h" // Reuse desktop audio or dummy
#include "core/semantics/ObjectEngine.h"
#include "core/geometry/GeometryEngine.h"
#include "core/depth/DepthEngine.h"
#include "core/utils/ConfigLoader.h"
#include "core/semantics/TemporalModule.h"
#include "core/geometry/FreeSpaceModule.h"
#include "core/geometry/EdgeSafetyModule.h"
#include "core/semantics/OCRModule.h"
#include "core/geometry/PathGuidanceModule.h"
#include "core/semantics/SceneUnderstandingModule.h"

// Note: Config loading should ideally be centralized.
// For now, we assume modules load their own defaults or logic in init()
// OR we add specific init-with-config logic to modules before adding them.

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: AVisionCLI.exe --video <path_to_video_file>" << std::endl;
        return 1;
    }

    std::string videoPath;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--video" && i + 1 < argc) {
            videoPath = argv[i + 1];
        }
    }

    if (videoPath.empty()) {
        std::cerr << "Error: No video file specified." << std::endl;
        return 1;
    }

    std::cout << "[AVisionCLI] Opening video: " << videoPath << std::endl;
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file." << std::endl;
        return 1;
    }
    
    // 1. Setup Engine (Pipeline)
    // Pass nullptr for Camera (we feed frames manually)
    // Pass DesktopAudio (so warnings work!)
    auto audio = std::make_shared<DesktopAudio>(); 
    
    Engine engine(nullptr, audio);
    
    // --- Dynamic Module Loading ---
    std::cout << "[AVisionCLI] Loading modules from models/modules.json..." << std::endl;
    auto moduleConfigs = ConfigLoader::loadModules("models/modules.json");
    
    // Fallback if config failed/empty
    if (moduleConfigs.empty()) {
        std::cerr << "[AVisionCLI] No modules found in config. Using hardcoded defaults." << std::endl;
        moduleConfigs = {
            {"GeometryModule", true, {}},
            {"ObjectModule", true, {}},
            {"DepthModule", true, {}}
        };
    }

    // Factory Logic
    for (const auto& modCfg : moduleConfigs) {
        if (!modCfg.enabled) {
            std::cout << "[AVisionCLI] Skipping " << modCfg.name << " (Disabled)" << std::endl;
            continue;
        }

        std::unique_ptr<IModule> module = nullptr;
        if (modCfg.name == "GeometryModule") module = std::make_unique<GeometryEngine>();
        else if (modCfg.name == "ObjectModule") module = std::make_unique<ObjectEngine>();
        else if (modCfg.name == "DepthModule") module = std::make_unique<DepthEngine>();
        else if (modCfg.name == "TemporalModule") module = std::make_unique<TemporalModule>();
        else if (modCfg.name == "FreeSpaceModule") module = std::make_unique<FreeSpaceModule>();
        else if (modCfg.name == "EdgeSafetyModule") module = std::make_unique<EdgeSafetyModule>();
        else if (modCfg.name == "OCRModule") module = std::make_unique<OCRModule>();
        else if (modCfg.name == "PathGuidanceModule") module = std::make_unique<PathGuidanceModule>();
        else if (modCfg.name == "SceneUnderstandingModule") module = std::make_unique<SceneUnderstandingModule>();
        
        if (module) {
            engine.configureModule(modCfg.name, modCfg.params);
            engine.addModule(std::move(module));
            std::cout << "[AVisionCLI] Added " << modCfg.name << std::endl;
        } else {
             std::cerr << "[AVisionCLI] Unknown module name: " << modCfg.name << std::endl;
        }
    }

    // 3. Init
    if (!engine.init()) {
        std::cerr << "Engine Init Failed." << std::endl;
        return 1;
    }

    cv::Mat frame, debugFrame;
    int frameIdx = 0;
    double fps = cap.get(cv::CAP_PROP_FPS);

    std::cout << "[AVisionCLI] Starting Pipeline... Press ESC to stop." << std::endl;
    
    // Determine output path relative to executable
    std::filesystem::path exePath = std::filesystem::path(argv[0]).parent_path();
    std::filesystem::path resultPath = exePath / "result.txt";
    
    std::cout << "[AVisionCLI] Writing logs to: " << std::filesystem::absolute(resultPath) << std::endl;

    std::ofstream resFile(resultPath);
    if (!resFile.is_open()) {
        std::cerr << "Warning: Could not open " << resultPath << " for writing." << std::endl;
    } else {
        resFile << "--- A-Vision Detection Log ---\n";
    }

    while (cap.read(frame)) {
        std::vector<DetectedObject> detections;
        
        // Run Pipeline
        engine.processFrame(frame, debugFrame, detections);

        // Resize debug frame if too large for screen (optional, but good for UX)
        // cv::resize(debugFrame, debugFrame, cv::Size(1280, 720)); // Example

        // Show Result
        if (!debugFrame.empty()) {
             cv::imshow("AVision Video CLI", debugFrame);
             if (cv::waitKey(1) == 27) break; // ESC
        }
        
        // Log to file
        if (resFile.is_open() && !detections.empty()) {
            double timestamp = cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
            resFile << "Time: " << timestamp << " sec | ";
            for (const auto& obj : detections) {
                resFile << obj.label << " (" << (int)(obj.confidence * 100) << "%), ";
            }
            resFile << "\n";
        }
        
        frameIdx++;
    }

    if (resFile.is_open()) resFile.close();
    std::cout << "[AVisionCLI] Results saved to result.txt" << std::endl;

    engine.stop();
    return 0;
}
