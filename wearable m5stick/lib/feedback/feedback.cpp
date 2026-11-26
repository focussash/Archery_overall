#include "feedback.h"

// Pattern definitions
static const BeepStep PATTERN_SHOT_DETECTED[] = {
    {2000, 50, 0}
};

static const BeepStep PATTERN_GOOD_FORM[] = {
    {2500, 80, 60},
    {2500, 80, 0}
};

static const BeepStep PATTERN_CANT_EXCEEDED[] = {
    {800, 400, 0}
};

static const BeepStep PATTERN_DRAW_INCONSISTENT[] = {
    {1800, 40, 30},
    {1800, 40, 30},
    {1800, 40, 0}
};

// Rising sweep approximated as discrete steps
static const BeepStep PATTERN_SESSION_START[] = {
    {1000, 60, 10},
    {1200, 60, 10},
    {1400, 60, 10},
    {1600, 60, 10},
    {1800, 60, 10},
    {2000, 80, 0}
};

// Falling sweep
static const BeepStep PATTERN_SESSION_END[] = {
    {2000, 60, 10},
    {1800, 60, 10},
    {1600, 60, 10},
    {1400, 60, 10},
    {1200, 60, 10},
    {1000, 80, 0}
};

// Fast descending beeps (rushing)
static const BeepStep PATTERN_RUSHING[] = {
    {2200, 40, 20},
    {1800, 40, 20},
    {1400, 40, 20},
    {1000, 60, 0}
};

// Slow ascending beeps (holding too long)
static const BeepStep PATTERN_HOLDING_TOO_LONG[] = {
    {1000, 100, 80},
    {1400, 100, 80},
    {1800, 100, 80},
    {2200, 120, 0}
};

// Periodic low pulse (fatigue)
static const BeepStep PATTERN_FATIGUE_WARNING[] = {
    {600, 150, 100},
    {600, 150, 0}
};

// Brief draw detection blip
static const BeepStep PATTERN_DRAW_NOTIFY[] = {
    {1500, 40, 0}
};

// Double-blip: form is good (aligned)
static const BeepStep PATTERN_FORM_ALIGNED[] = {
    {2200, 30, 40},
    {2200, 30, 0}
};

// Low blip: form deviated
static const BeepStep PATTERN_FORM_DEVIATED[] = {
    {1000, 60, 0}
};

struct PatternDef {
    const BeepStep* steps;
    uint8_t length;
};

static const PatternDef PATTERNS[] = {
    {nullptr,                     0},  // NONE
    {PATTERN_SHOT_DETECTED,       1},
    {PATTERN_GOOD_FORM,           2},
    {PATTERN_CANT_EXCEEDED,       1},
    {PATTERN_DRAW_INCONSISTENT,   3},
    {PATTERN_SESSION_START,       6},
    {PATTERN_SESSION_END,         6},
    {PATTERN_RUSHING,             4},
    {PATTERN_HOLDING_TOO_LONG,    4},
    {PATTERN_FATIGUE_WARNING,     2},
    {PATTERN_DRAW_NOTIFY,         1},
    {PATTERN_FORM_ALIGNED,        2},
    {PATTERN_FORM_DEVIATED,       1},
};

FeedbackEngine::FeedbackEngine(HalInterface* hal)
    : hal_(hal)
    , current_pattern_(nullptr)
    , pattern_length_(0)
    , step_index_(0)
    , playing_(false)
    , in_pause_(false)
    , step_start_time_(0)
{}

void FeedbackEngine::play(PatternID pattern) {
    uint8_t idx = static_cast<uint8_t>(pattern);
    if (idx >= static_cast<uint8_t>(PatternID::COUNT)) return;

    const PatternDef& def = PATTERNS[idx];
    if (!def.steps || def.length == 0) return;

    // Stop any current playback
    hal_->stopTone();

    current_pattern_ = def.steps;
    pattern_length_ = def.length;
    step_index_ = 0;
    playing_ = true;
    in_pause_ = false;
    step_start_time_ = hal_->millis();

    // Start first tone
    hal_->playTone(current_pattern_[0].freq_hz, current_pattern_[0].duration_ms);
}

void FeedbackEngine::update() {
    if (!playing_) return;

    uint32_t now = hal_->millis();
    const BeepStep& step = current_pattern_[step_index_];

    if (!in_pause_) {
        // Currently playing a tone
        if (now - step_start_time_ >= step.duration_ms) {
            hal_->stopTone();

            if (step.pause_ms > 0) {
                // Enter pause
                in_pause_ = true;
                step_start_time_ = now;
            } else {
                // No pause, move to next step or finish
                step_index_++;
                if (step_index_ >= pattern_length_) {
                    playing_ = false;
                    return;
                }
                step_start_time_ = now;
                hal_->playTone(current_pattern_[step_index_].freq_hz,
                              current_pattern_[step_index_].duration_ms);
            }
        }
    } else {
        // Currently in a pause
        if (now - step_start_time_ >= step.pause_ms) {
            in_pause_ = false;
            step_index_++;
            if (step_index_ >= pattern_length_) {
                playing_ = false;
                return;
            }
            step_start_time_ = now;
            hal_->playTone(current_pattern_[step_index_].freq_hz,
                          current_pattern_[step_index_].duration_ms);
        }
    }
}

void FeedbackEngine::stop() {
    hal_->stopTone();
    playing_ = false;
    current_pattern_ = nullptr;
}
