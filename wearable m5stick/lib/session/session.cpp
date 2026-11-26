#include "session.h"
#include "imu_math.h"
#include <cmath>

#ifndef NATIVE_TEST
#include <Preferences.h>
static Preferences prefs;
#endif

// ---- ConfigStore ----

// Default baselines (target angles in degrees)
static const float DEFAULT_CANT_BASELINE = 0.0f;
static const float DEFAULT_ELEVATION_BASELINE = 0.0f;
static const float DEFAULT_DRAW_BASELINE = 0.0f;

// Default tolerances (allowed deviation in degrees)
static const float DEFAULT_CANT_TOLERANCE = 5.0f;
static const float DEFAULT_ELEVATION_TOLERANCE = 10.0f;
static const float DEFAULT_DRAW_TOLERANCE = 15.0f;

// Default accel bias (g units)
static const float DEFAULT_CAL_AX_BIAS = 0.0f;
static const float DEFAULT_CAL_AY_BIAS = 0.0f;
static const float DEFAULT_CAL_AZ_BIAS = 0.0f;

ConfigStore::ConfigStore()
    : cant_baseline_(DEFAULT_CANT_BASELINE),
      elevation_baseline_(DEFAULT_ELEVATION_BASELINE),
      draw_baseline_(DEFAULT_DRAW_BASELINE),
      cant_tolerance_(DEFAULT_CANT_TOLERANCE),
      elevation_tolerance_(DEFAULT_ELEVATION_TOLERANCE),
      draw_tolerance_(DEFAULT_DRAW_TOLERANCE),
      form_check_enabled_(true),
      shot_feedback_enabled_(true),
      cal_ax_bias_(DEFAULT_CAL_AX_BIAS),
      cal_ay_bias_(DEFAULT_CAL_AY_BIAS),
      cal_az_bias_(DEFAULT_CAL_AZ_BIAS) {}

void ConfigStore::load() {
#ifndef NATIVE_TEST
    prefs.begin("archery", true);  // read-only
    cant_baseline_ = prefs.getFloat("cant_base", DEFAULT_CANT_BASELINE);
    elevation_baseline_ = prefs.getFloat("elev_base", DEFAULT_ELEVATION_BASELINE);
    draw_baseline_ = prefs.getFloat("draw_base", DEFAULT_DRAW_BASELINE);
    cant_tolerance_ = prefs.getFloat("cant_tol", DEFAULT_CANT_TOLERANCE);
    elevation_tolerance_ = prefs.getFloat("elev_tol", DEFAULT_ELEVATION_TOLERANCE);
    draw_tolerance_ = prefs.getFloat("draw_tol", DEFAULT_DRAW_TOLERANCE);
    form_check_enabled_ = prefs.getUChar("form_chk", 1) != 0;
    shot_feedback_enabled_ = prefs.getUChar("shot_fb", 1) != 0;
    cal_ax_bias_ = prefs.getFloat("cal_ax", DEFAULT_CAL_AX_BIAS);
    cal_ay_bias_ = prefs.getFloat("cal_ay", DEFAULT_CAL_AY_BIAS);
    cal_az_bias_ = prefs.getFloat("cal_az", DEFAULT_CAL_AZ_BIAS);
    prefs.end();
#endif
}

void ConfigStore::save() {
#ifndef NATIVE_TEST
    prefs.begin("archery", false);  // read-write
    prefs.putFloat("cant_base", cant_baseline_);
    prefs.putFloat("elev_base", elevation_baseline_);
    prefs.putFloat("draw_base", draw_baseline_);
    prefs.putFloat("cant_tol", cant_tolerance_);
    prefs.putFloat("elev_tol", elevation_tolerance_);
    prefs.putFloat("draw_tol", draw_tolerance_);
    prefs.putUChar("form_chk", form_check_enabled_ ? 1 : 0);
    prefs.putUChar("shot_fb", shot_feedback_enabled_ ? 1 : 0);
    prefs.putFloat("cal_ax", cal_ax_bias_);
    prefs.putFloat("cal_ay", cal_ay_bias_);
    prefs.putFloat("cal_az", cal_az_bias_);
    prefs.end();
#endif
}

// ---- SessionManager ----

SessionManager::SessionManager(ShotBuffer<64>* buffer, ConfigStore* config)
    : buffer_(buffer), config_(config), state_(SessionState::IDLE),
      shot_count_(0), start_time_ms_(0), last_packet_{} {}

void SessionManager::start(uint32_t now_ms) {
    state_ = SessionState::ACTIVE;
    shot_count_ = 0;
    start_time_ms_ = now_ms;
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

    // Compare measured angles against configurable baselines
    float cant_dev = fabsf(snapshot.roll - config_->getCantBaseline());
    float elev_dev = fabsf(snapshot.pitch - config_->getElevationBaseline());
    float draw_dev = fabsf(secondary_draw_angle - config_->getDrawBaseline());

    eval.cant_ok = (cant_dev <= config_->getCantTolerance());
    eval.elevation_ok = (elev_dev <= config_->getElevationTolerance());
    eval.draw_ok = (draw_dev <= config_->getDrawTolerance());

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
    pkt.draw_velocity = (int16_t)(snapshot.peak_angular_vel * 10.0f);
    pkt.shot_number = shot_num;
    pkt.form_flags = form_flags;
    pkt.heading = 0xFFFF;
    pkt.reserved = 0;
    return pkt;
}

FormEvaluation SessionManager::recordShot(const ShotSnapshot& snapshot, float secondary_draw_angle, uint32_t session_start_ms) {
    if (state_ != SessionState::ACTIVE) {
        return FormEvaluation{0, false, false, false};
    }

    shot_count_++;

    FormEvaluation eval = evaluateForm(snapshot, secondary_draw_angle);
    last_packet_ = buildPacket(snapshot, secondary_draw_angle, shot_count_, eval.flags);
    buffer_->push(last_packet_);

    return eval;
}
