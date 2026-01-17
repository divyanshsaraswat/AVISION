#pragma once
#include "../kernel/IModule.h"

class EdgeSafetyModule : public IModule {
public:
    EdgeSafetyModule();
    ~EdgeSafetyModule() override = default;

    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "EdgeSafetyModule"; }

private:
    float gradientThreshold = 0.25f;
    int frameCount = 0;
    int processInterval = 1;
};
