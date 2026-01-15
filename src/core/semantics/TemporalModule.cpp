#include "TemporalModule.h"
#include <iostream>
#include <algorithm>
#include <cmath>

TemporalModule::TemporalModule() {}

bool TemporalModule::init(const std::map<std::string, std::string>& params) {
    if (params.find("windowSize") != params.end()) {
        windowSize = std::stoi(params.at("windowSize"));
    }
    if (params.find("decay") != params.end()) {
        decay = std::stof(params.at("decay"));
    }
    std::cout << "[TemporalModule] Init: windowSize=" << windowSize << ", decay=" << decay << std::endl;
    return true;
}

void TemporalModule::process(Context& ctx) {
    // 1. Match current detections to tracks
    std::vector<bool> matchedTrack(tracks.size(), false);
    std::vector<bool> matchedDet(ctx.detections.size(), false);

    for (size_t i = 0; i < ctx.detections.size(); ++i) {
        float bestIoU = 0.0f;
        int bestTrackIdx = -1;

        for (size_t j = 0; j < tracks.size(); ++j) {
            if (matchedTrack[j]) continue;
            if (tracks[j].label != ctx.detections[i].label) continue; // Only match same label

            // Use the last known position of the track
            cv::Rect lastPos = tracks[j].history.back();
            float iou = calculateIoU(ctx.detections[i].boundingBox, lastPos);
            
            if (iou > 0.3f && iou > bestIoU) { // Threshold 0.3 for matching
                bestIoU = iou;
                bestTrackIdx = j;
            }
        }

        if (bestTrackIdx != -1) {
            // Match found
            matchedTrack[bestTrackIdx] = true;
            matchedDet[i] = true;
            
            // Update track
            tracks[bestTrackIdx].history.push_back(ctx.detections[i].boundingBox);
            if (tracks[bestTrackIdx].history.size() > windowSize) {
                tracks[bestTrackIdx].history.pop_front();
            }
            tracks[bestTrackIdx].missingFrames = 0;

            // SMOOTHING: Replace current detection box with average
            ctx.detections[i].boundingBox = computeAverageBox(tracks[bestTrackIdx].history);
        }
    }

    // 2. Handle new tracks
    for (size_t i = 0; i < ctx.detections.size(); ++i) {
        if (!matchedDet[i]) {
            TrackedObject newTrack;
            newTrack.id = nextId++;
            newTrack.label = ctx.detections[i].label;
            newTrack.history.push_back(ctx.detections[i].boundingBox);
            tracks.push_back(newTrack);
        }
    }

    // 3. Handle lost tracks (Decay/Missing)
    // For simplicity, we just remove tracks that weren't matched this frame if they persist too long
    // But specific request was "Stabilizes... run less often". 
    // This implies we should KEEP detections alive if they are missing for a few frames?
    // Let's implement simple "keep alive" for 1-2 frames if needed, or just standard cleanup.
    // For now, let's just cleanup tracks that have missed too many frames.
    
    auto it = tracks.begin();
    while (it != tracks.end()) {
        int idx = (int)std::distance(tracks.begin(), it);
        if (idx < matchedTrack.size() && !matchedTrack[idx]) {
            it->missingFrames++;
            if (it->missingFrames > 5) { // Kill track after 5 missing frames
                it = tracks.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

float TemporalModule::calculateIoU(const cv::Rect& a, const cv::Rect& b) {
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    if (x2 < x1 || y2 < y1) return 0.0f;

    float intersection = (float)(x2 - x1) * (y2 - y1);
    float areaA = (float)a.width * a.height;
    float areaB = (float)b.width * b.height;
    
    return intersection / (areaA + areaB - intersection);
}

cv::Rect TemporalModule::computeAverageBox(const std::deque<cv::Rect>& history) {
    if (history.empty()) return cv::Rect();

    float sumX = 0, sumY = 0, sumW = 0, sumH = 0;
    for (const auto& r : history) {
        sumX += r.x;
        sumY += r.y;
        sumW += r.width;
        sumH += r.height;
    }

    float n = (float)history.size();
    return cv::Rect((int)(sumX / n), (int)(sumY / n), (int)(sumW / n), (int)(sumH / n));
}
