#pragma once
#include "../kernel/IModule.h"

class FreeSpaceModule : public IModule {
public:
    FreeSpaceModule();
    ~FreeSpaceModule() override = default;

    bool init(const std::map<std::string, std::string>& params) override;
    void process(Context& ctx) override;
    std::string getName() const override { return "FreeSpaceModule"; }

private:
    int sectors = 3;
    float minClearance = 1.0f; // meters
};
