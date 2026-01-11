#pragma once
#include <string>

enum class DistanceCategory {
    FAR,
    NEAR,
    IMMEDIATE
};

class DistanceEngine {
public:
    // Convert normalized distance (0.0 to 1.0) to a category
    static DistanceCategory estimateCategory(float relativeDist);
    
    // Get text description for debug
    static std::string toString(DistanceCategory cat);
};
