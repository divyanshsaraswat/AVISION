#pragma once
#include <string>
#include <vector>
#include <map>

struct ModuleConfig {
    std::string name;
    bool enabled;
    std::map<std::string, std::string> params;
};

class ConfigLoader {
public:
    static std::vector<ModuleConfig> loadModules(const std::string& filePath);
};
