#pragma once
#include <cstdint>
#include "hal_interface.h"
#include "session.h"

enum class CalibrationState : uint8_t {
    IDLE,
    BASELINE_PROMPT,
    BASELINE_COUNTDOWN,
    BASELINE_CAPTURE,
    BASELINE_DONE,
    FLATCAL_PROMPT,
    FLATCAL_COUNTDOWN,
    FLATCAL_CAPTURE,
    FLATCAL_DONE
};

enum class CalibrationRequest : uint8_t {
    NONE,
    BASELINE,
    FLAT_CAL
};

class CalibrationManager {
public:
    CalibrationManager(HalInterface* hal, ConfigStore* config);

    void startBaseline();
    void startFlatCal();

    void onButtonA();
    void onButtonB();   // Cancel → IDLE without saving

    // Tick state machine. Call every loop with current sensor data.
    // Returns true when calibration finishes (back to IDLE).
    bool update(uint32_t now, float pitch, float roll, float drawAngle, float accel[3]);

    void draw(uint32_t now);

    bool isActive() const { return state_ != CalibrationState::IDLE; }
    CalibrationState getState() const { return state_; }

private:
    HalInterface* hal_;
    ConfigStore* config_;
    CalibrationState state_;
    uint32_t state_start_time_;

    // Sample accumulation
    float sum_pitch_;
    float sum_roll_;
    float sum_draw_;
    float sum_ax_;
    float sum_ay_;
    float sum_az_;
    uint32_t sample_count_;

    // Results for display in DONE states
    float result_cant_;
    float result_elev_;
    float result_draw_;
    float result_ax_bias_;
    float result_ay_bias_;
    float result_az_bias_;

    bool just_finished_;  // Set true on transition to IDLE from a DONE state

    static constexpr uint32_t BASELINE_COUNTDOWN_MS = 5000;
    static constexpr uint32_t FLATCAL_COUNTDOWN_MS = 3000;
    static constexpr uint32_t CAPTURE_MS = 2000;
    static constexpr uint32_t DONE_DISPLAY_MS = 2000;

    void transitionTo(CalibrationState newState, uint32_t now);
    void resetAccumulators();
};
