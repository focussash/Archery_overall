#pragma once
#include <cstdint>
#include "shot_detect.h"
#include "shot_buffer.h"
#include "imu_math.h"

enum class SessionState : uint8_t {
    IDLE,
    ACTIVE,
    ENDED
};

// Form evaluation result
struct FormEvaluation {
    uint8_t flags;
    bool cant_ok;
    bool elevation_ok;
    bool draw_ok;
};

class ConfigStore {
public:
    ConfigStore();

    void load();
    void save();

    // Baselines — the target angles (degrees)
    float getCantBaseline() const { return cant_baseline_; }
    float getElevationBaseline() const { return elevation_baseline_; }
    float getDrawBaseline() const { return draw_baseline_; }

    void setCantBaseline(float val) { cant_baseline_ = val; }
    void setElevationBaseline(float val) { elevation_baseline_ = val; }
    void setDrawBaseline(float val) { draw_baseline_ = val; }

    // Tolerances — how much deviation is OK (degrees)
    float getCantTolerance() const { return cant_tolerance_; }
    float getElevationTolerance() const { return elevation_tolerance_; }
    float getDrawTolerance() const { return draw_tolerance_; }

    void setCantTolerance(float val) { cant_tolerance_ = val; }
    void setElevationTolerance(float val) { elevation_tolerance_ = val; }
    void setDrawTolerance(float val) { draw_tolerance_ = val; }

    // Feature toggles
    bool isFormCheckEnabled() const { return form_check_enabled_; }
    bool isShotFeedbackEnabled() const { return shot_feedback_enabled_; }
    void setFormCheckEnabled(bool val) { form_check_enabled_ = val; }
    void setShotFeedbackEnabled(bool val) { shot_feedback_enabled_ = val; }

    // Accel bias calibration (g units)
    float getCalAxBias() const { return cal_ax_bias_; }
    float getCalAyBias() const { return cal_ay_bias_; }
    float getCalAzBias() const { return cal_az_bias_; }
    void setCalAxBias(float val) { cal_ax_bias_ = val; }
    void setCalAyBias(float val) { cal_ay_bias_ = val; }
    void setCalAzBias(float val) { cal_az_bias_ = val; }

private:
    float cant_baseline_;
    float elevation_baseline_;
    float draw_baseline_;
    float cant_tolerance_;
    float elevation_tolerance_;
    float draw_tolerance_;
    bool form_check_enabled_;
    bool shot_feedback_enabled_;
    float cal_ax_bias_;
    float cal_ay_bias_;
    float cal_az_bias_;
};

class SessionManager {
public:
    SessionManager(ShotBuffer<64>* buffer, ConfigStore* config);

    void start(uint32_t now_ms);
    void stop(uint32_t now_ms);

    // Record a shot, evaluate form, build packet, push to buffer
    FormEvaluation recordShot(const ShotSnapshot& snapshot, float secondary_draw_angle, uint32_t session_start_ms);

    SessionState getState() const { return state_; }
    uint8_t getShotCount() const { return shot_count_; }
    uint32_t getStartTime() const { return start_time_ms_; }
    uint32_t getElapsedMs(uint32_t now_ms) const;
    const ShotPacket_t& getLastPacket() const { return last_packet_; }

    // Evaluate form against baselines + tolerances
    FormEvaluation evaluateForm(const ShotSnapshot& snapshot, float secondary_draw_angle) const;

    // Build a shot packet from snapshot data
    ShotPacket_t buildPacket(const ShotSnapshot& snapshot, float secondary_draw_angle,
                             uint8_t shot_num, uint8_t form_flags) const;

private:
    ShotBuffer<64>* buffer_;
    ConfigStore* config_;
    SessionState state_;
    uint8_t shot_count_;
    uint32_t start_time_ms_;
    ShotPacket_t last_packet_;
};
