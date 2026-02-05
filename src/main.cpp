#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <opencv2/highgui.hpp>

#include "core/Engine.h"
#include "platform/desktop/DesktopCamera.h"
#include "platform/desktop/DesktopAudio.h"

#include "core/geometry/GeometryEngine.h"
#include "core/semantics/ObjectEngine.h"
#include "core/depth/DepthEngine.h"

// --- Headers for Module Factory ---
#include "core/utils/ConfigLoader.h"
#include "core/semantics/OCRModule.h"
#include "core/semantics/SceneUnderstandingModule.h"
// #include "core/segmentation/SegmentationModule.h" // Not IModule yet
// #include "core/filling/FillModule.h" // Not IModule yet
#include "core/semantics/TemporalModule.h"
#include "core/geometry/FreeSpaceModule.h"
#include "core/geometry/EdgeSafetyModule.h"
#include "core/geometry/PathGuidanceModule.h"

// Factory Helper
std::unique_ptr<IModule> createModule(const std::string& name) {
    if (name == "GeometryModule") return std::make_unique<GeometryEngine>();
    if (name == "ObjectModule") return std::make_unique<ObjectEngine>();
    if (name == "DepthModule") return std::make_unique<DepthEngine>();
    if (name == "OCRModule") return std::make_unique<OCRModule>();
    if (name == "SceneUnderstandingModule") return std::make_unique<SceneUnderstandingModule>();
    // if (name == "SegmentationModule") return std::make_unique<SegmentationModule>();
    // if (name == "FillModule") return std::make_unique<FillModule>();
    if (name == "TemporalModule") return std::make_unique<TemporalModule>();
    if (name == "FreeSpaceModule") return std::make_unique<FreeSpaceModule>();
    if (name == "EdgeSafetyModule") return std::make_unique<EdgeSafetyModule>();
    if (name == "PathGuidanceModule") return std::make_unique<PathGuidanceModule>();
    return nullptr;
}

int main() {
    // Branding
    std::cout << R"(
                                          ++++++++++**+*++                                          
                                         +===============++                                         
                             *******++++++****************+++++++****                               
                                *++++++++++++++++++++++++++++++++++**                                
                        ***+****+++**++++++++++++++++*************+*********                         
                       *+==+=====+++***+++++++++++****************++==+++++=*                        
                  ****##************                            **+++++*****#*                       
                  +++=====++++++*                                      ++===+****                    
              **********#*******            +++++=+=====  :            +++***********                
             *=++====+++*                   **+++++++==-:....::            *====+++++#               
        =++++*+****#****+                  ****++*+   ........:=           ++++******+++*            
        +=========+*                    +****++++**   :.......:+*              ****+++++#            
   ++++++**********                     +*+++++++**    :.....:=**               ********+++++*       
   ++++++++++                           ***++++++**      -:=+****                      *====+*       
                                        *++++++++++        *****#                      *******       
                +++++                   +++*+++++++++++++++******                  *++**             
               +****++++                   *+++++++++++++++++*                 ++*******             
                 +++++++++++                +++++++++++++++++                 ++*****                
                   **++++++++*++             ++++**++++++++++            ++++*++****                 
                    +++*******+++*                                    ++++++**#++++                  
                        +++++++++*                                    +++++++**                      
                            ++**********************************+**+**++******                       
                             ++*+++************************++++++++++*+*++                           
                               ****#*+++++++++*******++++++++++++++**##**                            
                                    ******************************+*                                 
                                    ++++++*++****************+*++++                                  

    ___ _   __________________  _  __  _________  _  _________________  _____  ___ ______________  _  __
   / _ | | / /  _/ __/  _/ __ \/ |/ / / ___/ __ \/ |/ / __/  _/ ___/ / / / _ \/ _ /_  __/  _/ __ \/ |/ /
  / __ | |/ // /_\ \_/ // /_/ /    / / /__/ /_/ /    / _/_/ // (_ / /_/ / , _/ __ |/ / _/ // /_/ /    / 
 /_/ |_|___/___/___/___/\____/_/|_/  \___/\____/_/|_/_/ /___/\___/\____/_/|_/_/ |_/_/ /___/\____/_/|_/  
)" << std::endl;
    std::cout << "        [AVision System v1.5]" << std::endl << std::endl;

    try {
        // 0. Runtime Model Setup
        // Check if config exists, if not, launch setup wizard
        struct stat buffer;
        // Check for both files to see if we need setup
        if (stat("models/selected_model.json", &buffer) != 0 && stat("models/modules.json", &buffer) != 0) {
            std::cout << "[AVision] First run detected. Launching Model Setup..." << std::endl;
            // Launch the batch script
            int result = system("download_models.bat");
            if (result != 0) {
                 // Non-fatal, might just be missing some models
            }
        }

        // 1. Init Platform Layer (The "Body")
        auto camera = std::make_shared<DesktopCamera>(0); // Default webcam
        auto audio = std::make_shared<DesktopAudio>();

        // 2. Init Core Layer (The "Brain")
        Engine engine(camera, audio);
        
        // 2b. Assemble Pipeline (Dynamic from Config)
        std::string configPath = "models/modules.json";
        struct stat configBuffer; 
        if (stat(configPath.c_str(), &configBuffer) != 0) {
             // Fallback lookup for development environment
             if (stat("../../../models/modules.json", &configBuffer) == 0) configPath = "../../../models/modules.json";
             else if (stat("../../models/modules.json", &configBuffer) == 0) configPath = "../../models/modules.json";
        }

        AppConfig config = ConfigLoader::loadAppConfig(configPath);
        std::cout << "[AVision] Loading config from: " << configPath << std::endl;
        
        if (config.modules.empty()) {
             std::cout << "[AVision] No modules found in config, loading defaults..." << std::endl;
             engine.addModule(std::make_unique<GeometryEngine>());
             engine.addModule(std::make_unique<ObjectEngine>());
             engine.addModule(std::make_unique<DepthEngine>());
        } else {
             for (const auto& modConfig : config.modules) {
                 if (modConfig.enabled) {
                     auto module = createModule(modConfig.name);
                     if (module) {
                         std::cout << "[AVision] Enabling Module: " << modConfig.name << std::endl;
                         engine.addModule(std::move(module));
                         // Configure parameters
                         engine.configureModule(modConfig.name, modConfig.params);
                     } else {
                         std::cerr << "[AVision] Warning: Unknown module " << modConfig.name << std::endl;
                     }
                 }
             }
        }

        // Apply Pipeline Graph
        engine.setPipeline(config.pipeline);

        // 3. Init Engine
        if (!engine.init()) {
            return 1;
        }

        // 4. Run Loop (Desktop owns the loop now)
        cv::Mat frame, debugFrame;
        
        std::cout << "[AVision] System Active. Press 'q' to quit." << std::endl;
        
        while (true) {
            // 4a. Capture Frame
            if (!camera->capture(frame)) {
                // If capture fails (end of file or device error), we can stop
                // But for webcam, sometimes we drop frames.
                // Assuming blocking capture for now.
                // If using video file, this means EOF.
                 break; 
            }

            // Process Frame
            // We use the overload that provides debug output for visualization
            bool running = engine.processFrame(frame, debugFrame);
            
            if (!running) break;
            if (frame.empty()) continue; 
            
            // Display Results
            // If debug frame is generated (visualization enabled), show it
            if (!debugFrame.empty()) {
                cv::imshow("AVISION Core", debugFrame);
            } else {
                cv::imshow("AVISION Core", frame);
            }
            
            int key = cv::waitKey(1);
            if (key == 'q' || key == 27) {
                engine.stop();
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[AVision] Critical Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
