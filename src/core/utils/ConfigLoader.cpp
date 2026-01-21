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

AppConfig ConfigLoader::loadAppConfig(const std::string& filePath) {
    AppConfig appConfig;
    appConfig.modules = loadModules(filePath); // Reuse existing for now
    
    // Naive parsing for Pipeline block
    std::ifstream file(filePath);
    if (!file.is_open()) return appConfig;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    size_t pipelinePos = json.find("\"pipeline\"");
    if (pipelinePos == std::string::npos) return appConfig;
    
    // Find Pipeline Object
    size_t pipeStart = json.find("{", pipelinePos);
    if (pipeStart == std::string::npos) return appConfig;
    
    size_t pipeEnd = pipeStart;
    int depth = 0;
    for (size_t i = pipeStart; i < json.length(); ++i) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0) {
                pipeEnd = i;
                break;
            }
        }
    }
    
    std::string pipeContent = json.substr(pipeStart, pipeEnd - pipeStart + 1);
    
    // 1. Nodes
    size_t nodesPos = pipeContent.find("\"nodes\"");
    if (nodesPos != std::string::npos) {
        size_t arrStart = pipeContent.find("[", nodesPos);
        size_t arrEnd = pipeContent.find("]", arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string content = pipeContent.substr(arrStart+1, arrEnd - arrStart - 1);
            std::stringstream ss(content);
            std::string segment;
            while(std::getline(ss, segment, ',')) {
                 std::string val = unquote(trim(segment));
                 if (!val.empty()) appConfig.pipeline.nodes.push_back(val);
            }
        }
    }
    
    // 2. Edges
    size_t edgesPos = pipeContent.find("\"edges\"");
    if (edgesPos != std::string::npos) {
        size_t arrStart = pipeContent.find("[", edgesPos);
        // Find closing bracket of the Outer array
        size_t arrEnd = arrStart;
        depth = 0;
        for (size_t i = arrStart; i < pipeContent.length(); ++i) {
             if (pipeContent[i] == '[') depth++;
             else if (pipeContent[i] == ']') {
                 depth--;
                 if (depth == 0) {
                     arrEnd = i;
                     break;
                 }
             }
        }
        
        if (arrEnd > arrStart) {
             // We have the outer array content: [ ["A","B"], ["B","C"] ]
             // Manual parse internal arrays
             size_t searchPos = arrStart + 1;
             while (searchPos < arrEnd) {
                 size_t subStart = pipeContent.find("[", searchPos);
                 if (subStart == std::string::npos || subStart >= arrEnd) break;
                 
                 size_t subEnd = pipeContent.find("]", subStart);
                 std::string pairContent = pipeContent.substr(subStart + 1, subEnd - subStart - 1);
                 
                 // Split by comma
                 size_t comma = pairContent.find(",");
                 if (comma != std::string::npos) {
                     std::string u = unquote(trim(pairContent.substr(0, comma)));
                     std::string v = unquote(trim(pairContent.substr(comma + 1)));
                     if (!u.empty() && !v.empty()) {
                         appConfig.pipeline.edges.push_back({u, v});
                     }
                 }
                 
                 searchPos = subEnd + 1;
             }
        }
    }
    
    // 3. Gates
    size_t gatesPos = pipeContent.find("\"gates\"");
    if (gatesPos != std::string::npos) {
        size_t arrStart = pipeContent.find("[", gatesPos);
        
        // Find closing bracket
        size_t arrEnd = arrStart;
        depth = 0;
        for (size_t i = arrStart; i < pipeContent.length(); ++i) {
             if (pipeContent[i] == '[') depth++;
             else if (pipeContent[i] == ']') {
                 depth--;
                 if (depth == 0) {
                     arrEnd = i;
                     break;
                 }
             }
        }
        
        if (arrEnd > arrStart) {
             size_t searchPos = arrStart + 1;
             while (searchPos < arrEnd) {
                 size_t objStart = pipeContent.find("{", searchPos);
                 if (objStart == std::string::npos || objStart >= arrEnd) break;
                 
                 size_t objEnd = pipeContent.find("}", objStart);
                 std::string gateContent = pipeContent.substr(objStart + 1, objEnd - objStart - 1);
                 
                 GateRule rule;
                 std::stringstream ss(gateContent);
                 std::string segment;
                 while(std::getline(ss, segment, ',')) {
                     size_t colon = segment.find(':');
                     if (colon != std::string::npos) {
                         std::string key = unquote(trim(segment.substr(0, colon)));
                         std::string val = unquote(trim(segment.substr(colon + 1)));
                         
                         if (key == "if_source") rule.sourceModule = val;
                         else if (key == "condition") rule.condition = val;
                         else if (key == "value") rule.threshold = std::stof(val);
                         else if (key == "then_skip") rule.targetModule = val;
                     }
                 }
                 
                 if (!rule.sourceModule.empty() && !rule.targetModule.empty()) {
                     appConfig.pipeline.gates.push_back(rule);
                 }
                 
                 searchPos = objEnd + 1;
             }
        }
    }
    
    return appConfig;
}
