#include "cadence.h"

CadenceTracker::CadenceTracker(uint8_t min_shots, float deviation_threshold)
    : count_(0)
    , write_idx_(0)
    , min_shots_(min_shots)
    , deviation_threshold_(deviation_threshold)
{
    for (uint8_t i = 0; i < MAX_HISTORY; i++) {
        history_[i] = {};
    }
}

void CadenceTracker::reset() {
    count_ = 0;
    write_idx_ = 0;
    for (uint8_t i = 0; i < MAX_HISTORY; i++) {
        history_[i] = {};
    }
}

void CadenceTracker::computeAverages(uint32_t& avg_draw, uint32_t& avg_hold, uint32_t& avg_total) const {
    uint8_t n = (count_ < MAX_HISTORY) ? count_ : MAX_HISTORY;
    if (n == 0) {
        avg_draw = avg_hold = avg_total = 0;
        return;
    }

    uint32_t sum_draw = 0, sum_hold = 0, sum_total = 0;
    for (uint8_t i = 0; i < n; i++) {
        sum_draw += history_[i].draw_ms;
        sum_hold += history_[i].hold_ms;
        sum_total += history_[i].total_ms;
    }
    avg_draw = sum_draw / n;
    avg_hold = sum_hold / n;
    avg_total = sum_total / n;
}

CadenceResult CadenceTracker::recordShot(const ShotTiming& timing) {
    CadenceResult result = {};

    // Skip snap shots with no timing data
    if (timing.total_ms == 0) {
        result.has_baseline = hasBaseline();
        return result;
    }

    // Compute averages BEFORE adding this shot (compare against prior shots)
    if (hasBaseline()) {
        result.has_baseline = true;

        uint32_t avg_draw, avg_hold, avg_total;
        computeAverages(avg_draw, avg_hold, avg_total);
        result.avg_draw_ms = avg_draw;
        result.avg_hold_ms = avg_hold;
        result.avg_total_ms = avg_total;

        // Compute deviations (signed: negative = faster, positive = slower)
        if (avg_total > 0) {
            result.total_deviation = ((float)timing.total_ms - (float)avg_total) / (float)avg_total;
        }
        if (avg_draw > 0) {
            result.draw_deviation = ((float)timing.draw_ms - (float)avg_draw) / (float)avg_draw;
        }
        if (avg_hold > 0) {
            result.hold_deviation = ((float)timing.hold_ms - (float)avg_hold) / (float)avg_hold;
        }

        // Flag rushing: total time significantly shorter than average
        result.rushing = (result.total_deviation < -deviation_threshold_);

        // Flag holding too long: hold time significantly longer than average
        result.holding_too_long = (result.hold_deviation > deviation_threshold_);
    } else {
        result.has_baseline = false;
    }

    // Store this shot in circular buffer
    history_[write_idx_] = timing;
    write_idx_ = (write_idx_ + 1) % MAX_HISTORY;
    if (count_ < 255) count_++;

    return result;
}
