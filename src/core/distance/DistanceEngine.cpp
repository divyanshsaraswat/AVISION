#include "DistanceEngine.h"

DistanceCategory DistanceEngine::estimateCategory(float relativeDist) {
    if (relativeDist > 0.85f) return DistanceCategory::IMMEDIATE; // Very close to bottom
    if (relativeDist > 0.6f)  return DistanceCategory::NEAR;
    return DistanceCategory::FAR;
}

std::string DistanceEngine::toString(DistanceCategory cat) {
    switch (cat) {
        case DistanceCategory::IMMEDIATE: return "IMMEDIATE";
        case DistanceCategory::NEAR:      return "NEAR";
        case DistanceCategory::FAR:       return "FAR";
        default:                          return "UNKNOWN";
    }
}
