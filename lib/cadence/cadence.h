#pragma once
#include <cstdint>
#include "shot_detect.h"

// Result of cadence analysis for a single shot
struct CadenceResult {
    bool has_baseline;      // True if enough shots to compute average
    bool rushing;           // Draw+hold significantly faster than average
    bool holding_too_long;  // Hold significantly longer than average
    float draw_deviation;   // (current - avg) / avg, signed
    float hold_deviation;   // (current - avg) / avg, signed
    float total_deviation;  // (current - avg) / avg, signed
    // Running averages (for display/serial)
    uint32_t avg_draw_ms;
    uint32_t avg_hold_ms;
    uint32_t avg_total_ms;
};

class CadenceTracker {
public:
    // min_shots: minimum shots before deviation detection activates
    // deviation_threshold: fraction (e.g., 0.30 = 30%) deviation to flag
    CadenceTracker(uint8_t min_shots = 5, float deviation_threshold = 0.30f);

    // Record a shot's timing and return cadence analysis
    CadenceResult recordShot(const ShotTiming& timing);

    // Reset all history (e.g., on new session)
    void reset();

    // Getters
    uint8_t getShotCount() const { return count_; }
    bool hasBaseline() const { return count_ >= min_shots_; }

private:
    static const uint8_t MAX_HISTORY = 16;

    ShotTiming history_[MAX_HISTORY];
    uint8_t count_;         // Total shots recorded (saturates at MAX_HISTORY for avg)
    uint8_t write_idx_;     // Circular write index
    uint8_t min_shots_;
    float deviation_threshold_;

    void computeAverages(uint32_t& avg_draw, uint32_t& avg_hold, uint32_t& avg_total) const;
};
