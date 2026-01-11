#pragma once
#include "../interfaces/IAudio.h"

class DesktopAudio : public IAudio {
public:
    void playTone(AudioUrgency urgency) override;
    void speak(const std::string& text) override;
};
