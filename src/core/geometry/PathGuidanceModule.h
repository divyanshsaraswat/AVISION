#pragma once
#include "../kernel/IModule.h"

class PathGuidanceModule : public IModule {
public:
    PathGuidanceModule();
    ~PathGuidanceModule() override = default;

    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "PathGuidanceModule"; }

private:
    float turnThreshold = 0.5f;
    int frameCount = 0;
    int processInterval = 1;
};
