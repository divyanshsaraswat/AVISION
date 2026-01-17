#pragma once
#include "../kernel/IModule.h"
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>

class SceneUnderstandingModule : public IModule {
public:
    SceneUnderstandingModule();
    ~SceneUnderstandingModule() override = default;

    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "SceneUnderstandingModule"; }

private:
    cv::dnn::Net net;
    std::vector<std::string> classes;
    bool modelLoaded = false;
    int processInterval = 100; // Run very infrequently (every 100 frames)
    int frameCount = 0;
    
    void loadClasses(const std::string& path);
};
