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

    float getCantThreshold() const { return cant_threshold_; }
    float getElevationThreshold() const { return elevation_threshold_; }
    float getDrawAngleThreshold() const { return draw_threshold_; }

    void setCantThreshold(float val) { cant_threshold_ = val; }
    void setElevationThreshold(float val) { elevation_threshold_ = val; }
    void setDrawAngleThreshold(float val) { draw_threshold_ = val; }

private:
    float cant_threshold_;
    float elevation_threshold_;
    float draw_threshold_;
};

class SessionManager {
public:
    SessionManager(ShotBuffer<64>* buffer, ConfigStore* config);

    void start(uint32_t now_ms);
    void stop(uint32_t now_ms);

    // Record a shot, evaluate form, build packet, push to buffer
    // Returns the form evaluation
    FormEvaluation recordShot(const ShotSnapshot& snapshot, float secondary_draw_angle, uint32_t session_start_ms);

    SessionState getState() const { return state_; }
    uint8_t getShotCount() const { return shot_count_; }
    uint32_t getStartTime() const { return start_time_ms_; }
    uint32_t getElapsedMs(uint32_t now_ms) const;
    const ShotPacket_t& getLastPacket() const { return last_packet_; }

    // Evaluate form against thresholds
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

    // Reference angles (first shot baseline)
    float ref_cant_;
    float ref_elevation_;
    float ref_draw_angle_;
    bool has_reference_;
};
