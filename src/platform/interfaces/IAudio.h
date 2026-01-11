#pragma once
#include <string>

enum class AudioUrgency {
    INFO,       // Passive beep
    WARNING,    // Alert tone
    CRITICAL    // Urgent alarm
};

// INTERFACE: The "Body" (Platform) must implement this for the "Brain" (Core)
class IAudio {
public:
    virtual ~IAudio() = default;

    // Play a tone based on urgency/distance
    virtual void playTone(AudioUrgency urgency) = 0;

    // Speak a short phrase (optional for MVP)
    virtual void speak(const std::string& text) = 0;
};
