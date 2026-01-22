#pragma once

#include <string>
#include <map>
#include <chrono>
#include <sstream>
#include <iomanip>
#include "../../platform/SystemUtils.h"

struct ModuleMetric {
    double latencyMs = 0.0;
    long long memoryDelta = 0;
    float confidence = 0.0f;
};

class MetricsCollector {
public:
    struct FrameData {
        long frameId = 0;
        double totalLatencyMs = 0.0;
        bool hasHeavyActivity = false;
        std::vector<std::string> heavyModules;
        std::map<std::string, ModuleMetric> modules;
    };

    void startFrame(long frameId) {
        currentFrame = FrameData();
        currentFrame.frameId = frameId;
        frameStartTime = std::chrono::high_resolution_clock::now();
    }

    void endFrame() {
        auto end = std::chrono::high_resolution_clock::now();
        currentFrame.totalLatencyMs = std::chrono::duration<double, std::milli>(end - frameStartTime).count();
    }

    void startModule(const std::string& name) {
        moduleStartTimes[name] = std::chrono::high_resolution_clock::now();
        moduleStartMem[name] = SystemUtils::getMemoryUsage();
    }

    void endModule(const std::string& name, float confidence = -1.0f) {
        auto end = std::chrono::high_resolution_clock::now();
        long long endMem = SystemUtils::getMemoryUsage();

        if (moduleStartTimes.find(name) != moduleStartTimes.end()) {
            double latency = std::chrono::duration<double, std::milli>(end - moduleStartTimes[name]).count();
            currentFrame.modules[name].latencyMs = latency;
            currentFrame.modules[name].memoryDelta = endMem - moduleStartMem[name];
            currentFrame.modules[name].confidence = confidence;
            
            // Heuristic: If a module takes > 1.0ms, it did "real work" (not just skipping)
            // This fixes the "Measurement Illusion" where 50,000 FPS is reported during skip frames.
            if (latency > 1.0) {
                currentFrame.hasHeavyActivity = true;
                currentFrame.heavyModules.push_back(name);
            }
        }
    }

    void recordSkip(const std::string& name) {
        currentFrame.modules[name].latencyMs = 0.0;
        currentFrame.modules[name].memoryDelta = 0;
        currentFrame.modules[name].confidence = -1.0f; // Indication of skip? Or maybe add a status field.
        // For now, -1.0f confidence + 0 latency implies skip.
    }

    std::string toJson() { // Not const anymore, updates state
        // Calculate Effective FPS (Input/Output Rate)
        if (currentFrame.hasHeavyActivity) {
            auto now = std::chrono::high_resolution_clock::now();
            if (lastHeavyTime.time_since_epoch().count() > 0) {
                 double deltaMs = std::chrono::duration<double, std::milli>(now - lastHeavyTime).count();
                 if (deltaMs > 0) currentEffectiveFPS = 1000.0 / deltaMs;
            }
            lastHeavyTime = now;
        }

        std::stringstream ss;
        ss << "{";
        ss << "\"frame_id\":" << currentFrame.frameId << ",";

        // Memory Usage
        double memMB = SystemUtils::getMemoryUsage() / 1024.0 / 1024.0;
        ss << "\"sys_mem_mb\":" << std::fixed << std::setprecision(1) << memMB << ",";
        
        // Scheduler FPS (Tick Rate)
        double schedulerFPS = (1000.0 / (currentFrame.totalLatencyMs > 0 ? currentFrame.totalLatencyMs : 0.01));
        ss << "\"scheduler_fps\":" << std::fixed << std::setprecision(1) << schedulerFPS << ",";
        
        // Effective FPS (Perception Rate)
        ss << "\"effective_fps\":" << std::fixed << std::setprecision(1) << currentEffectiveFPS << ",";
        
        // Heavy Modules List
        ss << "\"heavy_modules_ran\":[";
        bool firstHeavy = true;
        for (const auto& mod : currentFrame.heavyModules) {
            if (!firstHeavy) ss << ",";
            ss << "\"" << mod << "\"";
            firstHeavy = false;
        }
        ss << "],";

        ss << "\"modules\":{"; // User requested 'modules'
        
        bool first = true;
        for (const auto& kv : currentFrame.modules) {
            if (!first) ss << ",";
            ss << "\"" << kv.first << "\":{";
            ss << "\"latency_ms\":" << std::fixed << std::setprecision(2) << kv.second.latencyMs << ",";
            ss << "\"mem_delta_b\":" << kv.second.memoryDelta << ",";
            ss << "\"confidence\":" << std::fixed << std::setprecision(2) << kv.second.confidence;
            ss << "}";
            first = false;
        }
        
        ss << "}"; // Close modules
        ss << "}";
        return ss.str();
    }

private:
    FrameData currentFrame;
    std::chrono::time_point<std::chrono::high_resolution_clock> frameStartTime;
    // Effective FPS State
    std::chrono::time_point<std::chrono::high_resolution_clock> lastHeavyTime;
    double currentEffectiveFPS = 0.0;

    std::map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> moduleStartTimes;
    std::map<std::string, long long> moduleStartMem;
};
