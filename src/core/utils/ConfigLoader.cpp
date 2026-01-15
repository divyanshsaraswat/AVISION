#include "ConfigLoader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// Simple JSON parser helpers
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

static std::string unquote(const std::string& str) {
    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

std::vector<ModuleConfig> ConfigLoader::loadModules(const std::string& filePath) {
    std::vector<ModuleConfig> modules;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[ConfigLoader] Warning: Could not open " << filePath << std::endl;
        return modules; // Return empty, caller decides fallback
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    // Very naive parser for structure: {"modules": [ {Obj1}, {Obj2} ] }
    size_t modulesPos = json.find("\"modules\"");
    if (modulesPos == std::string::npos) return modules;

    size_t arrayStart = json.find("[", modulesPos);
    if (arrayStart == std::string::npos) return modules;
    
    size_t currentPos = arrayStart + 1;
    
    while (currentPos < json.length()) {
        // Find start of object
        size_t objStart = json.find("{", currentPos);
        if (objStart == std::string::npos) break; // No more objects
        
        // Find matching end of object (handling simple nesting)
        size_t objEnd = objStart;
        int depth = 0;
        bool foundEnd = false;
        
        for (size_t i = objStart; i < json.length(); ++i) {
            if (json[i] == '{') depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0) {
                    objEnd = i;
                    foundEnd = true;
                    break;
                }
            }
        }
        
        if (!foundEnd) break;
        
        std::string objContent = json.substr(objStart, objEnd - objStart + 1);
        
        // Parse Object Content
        ModuleConfig config;
        config.enabled = true; // Default
        
        // Extract Name
        size_t namePos = objContent.find("\"name\"");
        if (namePos != std::string::npos) {
            size_t valStart = objContent.find(":", namePos) + 1;
            size_t valEnd = objContent.find(",", valStart);
            size_t braceEnd = objContent.find("}", valStart); // Simple check
            if (braceEnd < valEnd) valEnd = braceEnd; // In case it's last item
            
            // Clean up extraction - this is brittle but works for simple files
            // Better: Extract string literal
            size_t quote1 = objContent.find("\"", valStart);
            size_t quote2 = objContent.find("\"", quote1 + 1);
            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                config.name = objContent.substr(quote1 + 1, quote2 - quote1 - 1);
            }
        }
        
        // Extract Enabled
        size_t enabledPos = objContent.find("\"enabled\"");
        if (enabledPos != std::string::npos) {
             size_t valStart = objContent.find(":", enabledPos) + 1;
             std::string valSub = objContent.substr(valStart, 20); // snippet
             if (valSub.find("false") != std::string::npos) config.enabled = false;
             // else true
        }
        
        // Extract Params
        size_t paramsPos = objContent.find("\"params\"");
        if (paramsPos != std::string::npos) {
            size_t openBrace = objContent.find("{", paramsPos);
            size_t closeBrace = objContent.find("}", openBrace);
            if (openBrace != std::string::npos && closeBrace != std::string::npos) {
                std::string paramsContent = objContent.substr(openBrace + 1, closeBrace - openBrace - 1);
                
                // Read comma separated "key": "value"
                std::stringstream ss(paramsContent);
                std::string segment;
                while(std::getline(ss, segment, ',')) {
                    size_t colon = segment.find(':');
                    if (colon != std::string::npos) {
                        std::string key = trim(segment.substr(0, colon));
                        std::string val = trim(segment.substr(colon + 1));
                        
                        key = unquote(key);
                        val = unquote(val);
                        
                        if (!key.empty()) config.params[key] = val;
                    }
                }
            }
        }
        
        if (!config.name.empty()) {
            modules.push_back(config);
        }
        
        currentPos = objEnd + 1;
    }
    
    return modules;
}
