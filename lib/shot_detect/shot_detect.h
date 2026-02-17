#pragma once
#include <cstdint>

enum class ShotState : uint8_t {
    IDLE,
    DRAW_DETECTED,
    HOLD,
    SHOT_FIRED,
    RECOVERY
};

struct ShotSnapshot {
    uint32_t timestamp_ms;
    float pitch;            // bow arm elevation (degrees)
    float roll;             // bow cant (degrees)
    float draw_angle;       // draw arm angle (degrees) - filled by secondary
    float peak_angular_vel; // peak angular velocity at release (deg/s)
    float accel_magnitude;  // accel spike magnitude at release (g)
};

// Timing data for pre-shot routine cadence tracking
struct ShotTiming {
    uint32_t draw_ms;       // Time spent in draw phase (DRAW_DETECTED)
    uint32_t hold_ms;       // Time spent in hold phase (HOLD)
    uint32_t total_ms;      // Total raise-to-release time
};

struct ShotDetectorConfig {
    float accel_threshold;      // g - spike threshold for shot detection
    float draw_pitch_rate;      // deg/s - min pitch rate for draw detection
    float hold_stability;       // deg/s - max angular vel for hold state
    uint32_t debounce_ms;       // recovery period after shot
    uint32_t hold_timeout_ms;   // max hold time before resetting to idle
    uint32_t draw_timeout_ms;   // max draw time before resetting to idle
};

class ShotDetector {
public:
    ShotDetector();

    void update(float accel[3], float gyro[3], float pitch, float roll, uint32_t timestamp_ms);

    bool shotDetected() const { return shot_fired_flag_; }
    ShotState getState() const { return state_; }
    const ShotSnapshot& getLastShot() const { return last_shot_; }
    const ShotTiming& getLastTiming() const { return last_timing_; }

    void setConfig(const ShotDetectorConfig& cfg) { config_ = cfg; }
    const ShotDetectorConfig& getConfig() const { return config_; }

    void reset();

private:
    void transitionTo(ShotState new_state, uint32_t timestamp_ms);

    ShotState state_;
    bool shot_fired_flag_;
    ShotSnapshot last_shot_;
    ShotTiming last_timing_;
    ShotDetectorConfig config_;

    uint32_t state_enter_time_;
    uint32_t draw_start_time_;  // When DRAW_DETECTED began
    uint32_t hold_start_time_;  // When HOLD began
    float peak_angular_vel_;
    float peak_accel_mag_;
    float hold_pitch_;
    float hold_roll_;

    void recordTiming(uint32_t shot_time_ms);
};
