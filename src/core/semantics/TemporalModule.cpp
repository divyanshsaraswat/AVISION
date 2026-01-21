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
    if (params.find("alpha") != params.end()) {
        alpha = std::stof(params.at("alpha"));
    }
    std::cout << "[TemporalModule] Init: windowSize=" << windowSize << ", decay=" << decay << ", alpha=" << alpha << std::endl;
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

            // SMOOTHING: EMA Position & Velocity
            cv::Rect detection = ctx.detections[i].boundingBox;
            
            if (!tracks[bestTrackIdx].hasSmoothed) {
                tracks[bestTrackIdx].smoothedBox = cv::Rect2f((float)detection.x, (float)detection.y, (float)detection.width, (float)detection.height);
                tracks[bestTrackIdx].velocity = cv::Point2f(0, 0);
                tracks[bestTrackIdx].hasSmoothed = true;
            } else {
                cv::Rect2f& s = tracks[bestTrackIdx].smoothedBox;
                
                // Calculate instant velocity (Current - Previous)
                float dx = (float)detection.x - s.x;
                float dy = (float)detection.y - s.y;
                float dw = (float)detection.width - s.width;
                float dh = (float)detection.height - s.height;
                
                // Update Velocity (EMA)
                // Use a different alpha for velocity? Let's assume same or slightly slower.
                tracks[bestTrackIdx].velocity.x = alpha * dx + (1.0f - alpha) * tracks[bestTrackIdx].velocity.x;
                tracks[bestTrackIdx].velocity.y = alpha * dy + (1.0f - alpha) * tracks[bestTrackIdx].velocity.y;
                
                // Update Position (EMA)
                s.x = alpha * detection.x + (1.0f - alpha) * s.x;
                s.y = alpha * detection.y + (1.0f - alpha) * s.y;
                s.width = alpha * detection.width + (1.0f - alpha) * s.width;
                s.height = alpha * detection.height + (1.0f - alpha) * s.height;
            }
            
            // Apply smoothed box to output
            ctx.detections[i].boundingBox = tracks[bestTrackIdx].smoothedBox;
        }
    }

    // 2. Handle new tracks
    for (size_t i = 0; i < ctx.detections.size(); ++i) {
        if (!matchedDet[i]) {
            TrackedObject newTrack;
            newTrack.id = nextId++;
            newTrack.label = ctx.detections[i].label;
            // newTrack.history.push_back(ctx.detections[i].boundingBox); // Logic moved/simplified? Keep consistent with struct
            newTrack.history.push_back(ctx.detections[i].boundingBox);
            
            // Init EMA
            cv::Rect d = ctx.detections[i].boundingBox;
            newTrack.smoothedBox = cv::Rect2f((float)d.x, (float)d.y, (float)d.width, (float)d.height);
            newTrack.velocity = cv::Point2f(0, 0);
            newTrack.hasSmoothed = true;
            
            tracks.push_back(newTrack);
        }
    }

    // 3. Handle lost tracks (Decay/Missing) & PERSISTENCE
    auto it = tracks.begin();
    while (it != tracks.end()) {
        int idx = (int)std::distance(tracks.begin(), it);
        // "idx < matchedTrack.size()" check relies on index sync. matchedTrack is size of tracks BEFORE new additions?
        // Wait, 'tracks' grows in step 2. 'matchedTrack' was init at start.
        // We should only iterate up to original size? 
        // Or re-structure. BUT: 'matchedTrack' corresponds to 'tracks' at START of frame.
        // New tracks added in step 2 are NOT in 'matchedTrack' (and don't need removal).
        // Iterate only up to 'matchedTrack.size()'.
        
        if (idx >= matchedTrack.size()) break; // Stop checking new tracks

        if (!matchedTrack[idx]) {
            // Track was NOT matched this frame
            it->missingFrames++;
            
            if (it->missingFrames > 5) { // Kill track after 5 missing frames (approx 0.15s)
                it = tracks.erase(it);
                continue; // Iterator invalidated, but assigned to next
            } else {
                // PERSISTENCE: Inject "Ghost" Detection with PREDICTION
                DetectedObject ghost;
                ghost.label = it->label;
                ghost.confidence = 0.5f; 
                ghost.classID = -1;
                
                // Predict Position: Pos + Velocity * MissingFrames ?
                // Actually, just Pos + Velocity (since we step 1 frame at a time implicitly)
                // But we don't update 'smoothedBox' in missing frames (to avoid drift divergence).
                // So we project from the *last known real state*?
                // ghost = smoothedBox + velocity * missingFrames
                
                cv::Rect2f predicted = it->smoothedBox;
                predicted.x += it->velocity.x * it->missingFrames;
                predicted.y += it->velocity.y * it->missingFrames;
                // Width/Height usually don't change much, or we can use 0 velocity for size
                
                ghost.boundingBox = predicted;
                
                ctx.detections.push_back(ghost);
                
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
