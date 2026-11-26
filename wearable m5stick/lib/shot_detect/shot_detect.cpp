#include "shot_detect.h"
#include <cmath>

ShotDetector::ShotDetector()
    : state_(ShotState::IDLE)
    , shot_fired_flag_(false)
    , last_shot_{}
    , last_timing_{}
    , state_enter_time_(0)
    , draw_start_time_(0)
    , hold_start_time_(0)
    , peak_angular_vel_(0.0f)
    , peak_accel_mag_(0.0f)
    , hold_pitch_(0.0f)
    , hold_roll_(0.0f)
{
    config_.accel_threshold = 6.0f;       // 6g spike for shot (bow release shock is 10-20g+)
    config_.draw_pitch_rate = 15.0f;     // deg/s pitch change for draw
    config_.hold_stability = 10.0f;      // deg/s max for "holding still"
    config_.debounce_ms = 500;
    config_.hold_timeout_ms = 10000;     // 10s max hold
    config_.draw_timeout_ms = 5000;      // 5s max draw
}

void ShotDetector::transitionTo(ShotState new_state, uint32_t timestamp_ms) {
    state_ = new_state;
    state_enter_time_ = timestamp_ms;
}

void ShotDetector::reset() {
    state_ = ShotState::IDLE;
    shot_fired_flag_ = false;
    state_enter_time_ = 0;
    draw_start_time_ = 0;
    hold_start_time_ = 0;
    peak_angular_vel_ = 0.0f;
    peak_accel_mag_ = 0.0f;
}

void ShotDetector::recordTiming(uint32_t shot_time_ms) {
    if (draw_start_time_ > 0) {
        last_timing_.total_ms = shot_time_ms - draw_start_time_;
        if (hold_start_time_ > 0) {
            last_timing_.draw_ms = hold_start_time_ - draw_start_time_;
            last_timing_.hold_ms = shot_time_ms - hold_start_time_;
        } else {
            // Shot during draw (no hold phase)
            last_timing_.draw_ms = shot_time_ms - draw_start_time_;
            last_timing_.hold_ms = 0;
        }
    } else {
        // Snap shot from IDLE (no draw/hold)
        last_timing_.draw_ms = 0;
        last_timing_.hold_ms = 0;
        last_timing_.total_ms = 0;
    }
}

void ShotDetector::update(float accel[3], float gyro[3], float pitch, float roll, uint32_t timestamp_ms) {
    // Clear one-shot flag
    shot_fired_flag_ = false;

    float accelMag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    float angularVel = sqrtf(gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2]);
    uint32_t elapsed = timestamp_ms - state_enter_time_;

    // Track peak values during draw/hold
    if (angularVel > peak_angular_vel_) peak_angular_vel_ = angularVel;
    if (accelMag > peak_accel_mag_) peak_accel_mag_ = accelMag;

    switch (state_) {
    case ShotState::IDLE:
        peak_angular_vel_ = 0.0f;
        peak_accel_mag_ = 0.0f;
        draw_start_time_ = 0;
        hold_start_time_ = 0;

        // Detect draw start: significant pitch change rate (arm rising)
        if (fabsf(gyro[1]) > config_.draw_pitch_rate) {
            draw_start_time_ = timestamp_ms;
            transitionTo(ShotState::DRAW_DETECTED, timestamp_ms);
        }

        // Also detect direct shot from idle (quick snap shot)
        if (accelMag > config_.accel_threshold) {
            hold_pitch_ = pitch;
            hold_roll_ = roll;
            last_shot_.timestamp_ms = timestamp_ms;
            last_shot_.pitch = pitch;
            last_shot_.roll = roll;
            last_shot_.draw_angle = 0.0f;
            last_shot_.peak_angular_vel = peak_angular_vel_;
            last_shot_.accel_magnitude = accelMag;
            recordTiming(timestamp_ms);
            shot_fired_flag_ = true;
            transitionTo(ShotState::RECOVERY, timestamp_ms);
        }
        break;

    case ShotState::DRAW_DETECTED:
        // Transition to HOLD when arm stabilizes
        if (angularVel < config_.hold_stability) {
            hold_pitch_ = pitch;
            hold_roll_ = roll;
            hold_start_time_ = timestamp_ms;
            transitionTo(ShotState::HOLD, timestamp_ms);
        }

        // Timeout: reset to idle
        if (elapsed > config_.draw_timeout_ms) {
            transitionTo(ShotState::IDLE, timestamp_ms);
        }

        // Shot during draw (fast release without hold)
        if (accelMag > config_.accel_threshold) {
            last_shot_.timestamp_ms = timestamp_ms;
            last_shot_.pitch = pitch;
            last_shot_.roll = roll;
            last_shot_.draw_angle = 0.0f;
            last_shot_.peak_angular_vel = peak_angular_vel_;
            last_shot_.accel_magnitude = accelMag;
            recordTiming(timestamp_ms);
            shot_fired_flag_ = true;
            transitionTo(ShotState::RECOVERY, timestamp_ms);
        }
        break;

    case ShotState::HOLD:
        // Detect shot: accelerometer spike
        if (accelMag > config_.accel_threshold) {
            last_shot_.timestamp_ms = timestamp_ms;
            last_shot_.pitch = hold_pitch_;
            last_shot_.roll = hold_roll_;
            last_shot_.draw_angle = 0.0f;
            last_shot_.peak_angular_vel = peak_angular_vel_;
            last_shot_.accel_magnitude = accelMag;
            recordTiming(timestamp_ms);
            shot_fired_flag_ = true;
            transitionTo(ShotState::RECOVERY, timestamp_ms);
        }

        // Timeout: reset to idle
        if (elapsed > config_.hold_timeout_ms) {
            transitionTo(ShotState::IDLE, timestamp_ms);
        }
        break;

    case ShotState::SHOT_FIRED:
        // Immediate transition to recovery (this state is transient)
        transitionTo(ShotState::RECOVERY, timestamp_ms);
        break;

    case ShotState::RECOVERY:
        // Wait for debounce period
        if (elapsed > config_.debounce_ms) {
            transitionTo(ShotState::IDLE, timestamp_ms);
        }
        break;
    }
}
