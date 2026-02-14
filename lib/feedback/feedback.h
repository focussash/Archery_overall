#pragma once
#include <cstdint>
#include "hal_interface.h"

enum class PatternID : uint8_t {
    NONE = 0,
    SHOT_DETECTED,       // Single short beep
    GOOD_FORM,           // Double beep high
    CANT_EXCEEDED,       // Long low tone
    DRAW_INCONSISTENT,   // Triple fast beep
    SESSION_START,       // Rising tone sweep
    SESSION_END,         // Falling tone sweep
    RUSHING,             // Fast descending beeps
    HOLDING_TOO_LONG,    // Slow ascending beeps
    FATIGUE_WARNING,     // Periodic low pulse
    COUNT
};

struct BeepStep {
    uint16_t freq_hz;
    uint16_t duration_ms;
    uint16_t pause_ms;
};

class FeedbackEngine {
public:
    FeedbackEngine(HalInterface* hal);

    void play(PatternID pattern);
    void update();
    void stop();
    bool isPlaying() const { return playing_; }

private:
    HalInterface* hal_;
    const BeepStep* current_pattern_;
    uint8_t pattern_length_;
    uint8_t step_index_;
    bool playing_;
    bool in_pause_;
    uint32_t step_start_time_;
};
