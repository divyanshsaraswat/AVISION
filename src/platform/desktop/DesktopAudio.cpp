#include "DesktopAudio.h"
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

void DesktopAudio::playTone(AudioUrgency urgency) {
    // Non-blocking approach needed later (thread), but for MVP we keep it simple or minimal duration
#ifdef _WIN32
    // Launch Beep in background thread to prevent blocking main pipeline
    std::thread([urgency]() {
        switch (urgency) {
            case AudioUrgency::INFO:
                Beep(750, 100); 
                break;
            case AudioUrgency::WARNING:
                Beep(1000, 200); 
                break;
            case AudioUrgency::CRITICAL:
                Beep(2000, 300); 
                break;
        }
    }).detach();
#else
    std::cout << "[AUDIO] *BEEP* (" << (int)urgency << ")" << std::endl;
#endif
}

void DesktopAudio::speak(const std::string& text) {
    std::cout << "[AUDIO] SAY: \"" << text << "\"" << std::endl;
    // In future: Integrate text-to-speech lib
}
