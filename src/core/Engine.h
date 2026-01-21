#include <memory>
#include <vector>
#include <map>
#include <string>
#include <opencv2/core.hpp>
#include "types/Context.h"
#include "platform/interfaces/ICamera.h"
#include "platform/interfaces/IAudio.h"
#include "kernel/IModule.h"
#include "utils/ConfigLoader.h" // For PipelineConfig definition
#include "utils/MetricsCollector.h"

// Note: Specific Engine headers removed from here to reduce coupling
// Main.cpp will include them to instantiate and pass to Engine.

class Engine {
public:
    Engine(std::shared_ptr<ICamera> cam, std::shared_ptr<IAudio> aud);
    ~Engine(); 
    
    // Lifecycle methods
    bool init();
    // Overload: Simple
    bool processFrame(cv::Mat& frame, cv::Mat& debugOut);
    // Overload: With Detections Output
    bool processFrame(cv::Mat& frame, cv::Mat& debugOut, std::vector<DetectedObject>& outDetections);
    // Overload: With Full Context Output (for CLI/Debug)
    bool processFrame(cv::Mat& frame, Context& outCtx);
    void stop();

    // Module Management
    void addModule(std::unique_ptr<IModule> module);
    void configureModule(const std::string& moduleName, const std::map<std::string, std::string>& params);
    
    // Pipeline Management
    void setPipeline(const PipelineConfig& config);

private:
    std::shared_ptr<ICamera> camera;
    std::shared_ptr<IAudio> audio;
    
    // The Pipeline
    std::vector<std::unique_ptr<IModule>> modules;
    std::vector<IModule*> executionQueue; // Sorted Pointers
    
    std::map<std::string, std::map<std::string, std::string>> moduleConfigs;
    PipelineConfig pipelineConfig;
    
    void buildExecutionGraph();

    MetricsCollector metrics;
    bool isRunning = false;
    int frameCount = 0;
};
