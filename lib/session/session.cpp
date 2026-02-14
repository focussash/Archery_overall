#include "session.h"
#include "imu_math.h"
#include <cmath>

#ifndef NATIVE_TEST
#include <Preferences.h>
static Preferences prefs;
#endif

// ---- ConfigStore ----

static const float DEFAULT_CANT_THRESHOLD = 5.0f;       // degrees
static const float DEFAULT_ELEVATION_THRESHOLD = 10.0f;  // degrees
static const float DEFAULT_DRAW_THRESHOLD = 15.0f;       // degrees

ConfigStore::ConfigStore()
    : cant_threshold_(DEFAULT_CANT_THRESHOLD),
      elevation_threshold_(DEFAULT_ELEVATION_THRESHOLD),
      draw_threshold_(DEFAULT_DRAW_THRESHOLD) {}

void ConfigStore::load() {
#ifndef NATIVE_TEST
    prefs.begin("archery", true);  // read-only
    cant_threshold_ = prefs.getFloat("cant_thresh", DEFAULT_CANT_THRESHOLD);
    elevation_threshold_ = prefs.getFloat("elev_thresh", DEFAULT_ELEVATION_THRESHOLD);
    draw_threshold_ = prefs.getFloat("draw_thresh", DEFAULT_DRAW_THRESHOLD);
    prefs.end();
#endif
}

void ConfigStore::save() {
#ifndef NATIVE_TEST
    prefs.begin("archery", false);  // read-write
    prefs.putFloat("cant_thresh", cant_threshold_);
    prefs.putFloat("elev_thresh", elevation_threshold_);
    prefs.putFloat("draw_thresh", draw_threshold_);
    prefs.end();
#endif
}

// ---- SessionManager ----

SessionManager::SessionManager(ShotBuffer<64>* buffer, ConfigStore* config)
    : buffer_(buffer), config_(config), state_(SessionState::IDLE),
      shot_count_(0), start_time_ms_(0), last_packet_{},
      ref_cant_(0), ref_elevation_(0), ref_draw_angle_(0), has_reference_(false) {}

void SessionManager::start(uint32_t now_ms) {
    state_ = SessionState::ACTIVE;
    shot_count_ = 0;
    start_time_ms_ = now_ms;
    has_reference_ = false;
    buffer_->clear();
}

void SessionManager::stop(uint32_t now_ms) {
    state_ = SessionState::ENDED;
}

uint32_t SessionManager::getElapsedMs(uint32_t now_ms) const {
    if (state_ == SessionState::IDLE) return 0;
    return now_ms - start_time_ms_;
}

FormEvaluation SessionManager::evaluateForm(const ShotSnapshot& snapshot, float secondary_draw_angle) const {
    FormEvaluation eval;

    float cant = fabsf(snapshot.roll);
    float elevation = fabsf(snapshot.pitch);
    float draw = fabsf(secondary_draw_angle);

    // If we have a reference, evaluate relative to it
    if (has_reference_) {
        cant = fabsf(snapshot.roll - ref_cant_);
        elevation = fabsf(snapshot.pitch - ref_elevation_);
        draw = fabsf(secondary_draw_angle - ref_draw_angle_);
    }

    eval.cant_ok = (cant <= config_->getCantThreshold());
    eval.elevation_ok = (elevation <= config_->getElevationThreshold());
    eval.draw_ok = (draw <= config_->getDrawAngleThreshold());

    eval.flags = 0;
    if (eval.cant_ok) eval.flags |= FORM_FLAG_CANT_OK;
    if (eval.elevation_ok) eval.flags |= FORM_FLAG_ELEVATION_OK;
    if (eval.draw_ok) eval.flags |= FORM_FLAG_DRAW_OK;

    return eval;
}

ShotPacket_t SessionManager::buildPacket(const ShotSnapshot& snapshot, float secondary_draw_angle,
                                          uint8_t shot_num, uint8_t form_flags) const {
    ShotPacket_t pkt = {};
    pkt.timestamp_ms = snapshot.timestamp_ms - start_time_ms_;
    pkt.cant_angle = imu_math::degreesToCentidegrees(snapshot.roll);
    pkt.elevation_angle = imu_math::degreesToCentidegrees(snapshot.pitch);
    pkt.draw_angle = imu_math::degreesToCentidegrees(secondary_draw_angle);
    pkt.draw_velocity = (int16_t)(snapshot.peak_angular_vel * 10.0f);  // 0.1 deg/s units
    pkt.shot_number = shot_num;
    pkt.form_flags = form_flags;
    pkt.heading = 0xFFFF;  // No magnetometer
    pkt.reserved = 0;
    return pkt;
}

FormEvaluation SessionManager::recordShot(const ShotSnapshot& snapshot, float secondary_draw_angle, uint32_t session_start_ms) {
    if (state_ != SessionState::ACTIVE) {
        return FormEvaluation{0, false, false, false};
    }

    shot_count_++;

    // First shot becomes the reference baseline
    if (!has_reference_) {
        ref_cant_ = snapshot.roll;
        ref_elevation_ = snapshot.pitch;
        ref_draw_angle_ = secondary_draw_angle;
        has_reference_ = true;
    }

    FormEvaluation eval = evaluateForm(snapshot, secondary_draw_angle);
    last_packet_ = buildPacket(snapshot, secondary_draw_angle, shot_count_, eval.flags);
    buffer_->push(last_packet_);

    return eval;
}
