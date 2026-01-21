#pragma once
#include <string>
#include <vector>
#include <map>

struct ModuleConfig {
    std::string name;
    bool enabled;
    std::map<std::string, std::string> params;
};

struct GateRule {
    std::string sourceModule;
    std::string condition; // "confidence_gt", "confidence_lt"
    float threshold;
    std::string targetModule; // Module to skip
};

struct PipelineConfig {
    std::vector<std::string> nodes;
    std::vector<std::pair<std::string, std::string>> edges;
    std::vector<GateRule> gates;
};

struct AppConfig {
    std::vector<ModuleConfig> modules;
    PipelineConfig pipeline;
};

class ConfigLoader {
public:
    static std::vector<ModuleConfig> loadModules(const std::string& filePath); // Legacy
    static AppConfig loadAppConfig(const std::string& filePath);
};
