#include "calibration.h"
#include "display_ui.h"  // For Color:: constants
#include <cstdio>        // For snprintf

CalibrationManager::CalibrationManager(HalInterface* hal, ConfigStore* config)
    : hal_(hal), config_(config), state_(CalibrationState::IDLE),
      state_start_time_(0), sum_pitch_(0), sum_roll_(0), sum_draw_(0),
      sum_ax_(0), sum_ay_(0), sum_az_(0), sample_count_(0),
      result_cant_(0), result_elev_(0), result_draw_(0),
      result_ax_bias_(0), result_ay_bias_(0), result_az_bias_(0),
      just_finished_(false) {}

void CalibrationManager::startBaseline() {
    transitionTo(CalibrationState::BASELINE_PROMPT, 0);
}

void CalibrationManager::startFlatCal() {
    transitionTo(CalibrationState::FLATCAL_PROMPT, 0);
}

void CalibrationManager::onButtonA() {
    switch (state_) {
        case CalibrationState::BASELINE_PROMPT:
            // Will transition in update() where we have 'now'
            transitionTo(CalibrationState::BASELINE_COUNTDOWN, 0);
            break;
        case CalibrationState::FLATCAL_PROMPT:
            transitionTo(CalibrationState::FLATCAL_COUNTDOWN, 0);
            break;
        default:
            break;
    }
}

void CalibrationManager::onButtonB() {
    if (state_ != CalibrationState::IDLE) {
        state_ = CalibrationState::IDLE;
        just_finished_ = true;
    }
}

bool CalibrationManager::update(uint32_t now, float pitch, float roll,
                                 float drawAngle, float accel[3]) {
    // Fix up state_start_time_ if it was set to 0 (from onButtonA)
    if (state_start_time_ == 0 && state_ != CalibrationState::IDLE) {
        state_start_time_ = now;
    }

    // Check if we just finished (set by onButtonB cancel or DONE→IDLE)
    if (just_finished_) {
        just_finished_ = false;
        return true;
    }

    uint32_t elapsed = now - state_start_time_;

    switch (state_) {
        case CalibrationState::IDLE:
            return false;

        case CalibrationState::BASELINE_PROMPT:
        case CalibrationState::FLATCAL_PROMPT:
            // Waiting for button A press — nothing to do
            break;

        case CalibrationState::BASELINE_COUNTDOWN:
            if (elapsed >= BASELINE_COUNTDOWN_MS) {
                transitionTo(CalibrationState::BASELINE_CAPTURE, now);
            }
            break;

        case CalibrationState::BASELINE_CAPTURE:
            // Accumulate angle samples
            sum_pitch_ += pitch;
            sum_roll_ += roll;
            sum_draw_ += drawAngle;
            sample_count_++;

            if (elapsed >= CAPTURE_MS) {
                // Compute averages and save
                result_elev_ = sum_pitch_ / (float)sample_count_;
                result_cant_ = sum_roll_ / (float)sample_count_;
                result_draw_ = sum_draw_ / (float)sample_count_;

                config_->setElevationBaseline(result_elev_);
                config_->setCantBaseline(result_cant_);
                config_->setDrawBaseline(result_draw_);
                config_->save();

                transitionTo(CalibrationState::BASELINE_DONE, now);
            }
            break;

        case CalibrationState::BASELINE_DONE:
            if (elapsed >= DONE_DISPLAY_MS) {
                state_ = CalibrationState::IDLE;
                just_finished_ = true;
            }
            break;

        case CalibrationState::FLATCAL_COUNTDOWN:
            if (elapsed >= FLATCAL_COUNTDOWN_MS) {
                transitionTo(CalibrationState::FLATCAL_CAPTURE, now);
            }
            break;

        case CalibrationState::FLATCAL_CAPTURE:
            // Accumulate raw accel samples
            sum_ax_ += accel[0];
            sum_ay_ += accel[1];
            sum_az_ += accel[2];
            sample_count_++;

            if (elapsed >= CAPTURE_MS) {
                // Compute averages — expected gravity when flat: (0, 0, 1.0g)
                float avg_ax = sum_ax_ / (float)sample_count_;
                float avg_ay = sum_ay_ / (float)sample_count_;
                float avg_az = sum_az_ / (float)sample_count_;

                result_ax_bias_ = avg_ax - 0.0f;
                result_ay_bias_ = avg_ay - 0.0f;
                result_az_bias_ = avg_az - 1.0f;

                config_->setCalAxBias(result_ax_bias_);
                config_->setCalAyBias(result_ay_bias_);
                config_->setCalAzBias(result_az_bias_);
                config_->save();

                transitionTo(CalibrationState::FLATCAL_DONE, now);
            }
            break;

        case CalibrationState::FLATCAL_DONE:
            if (elapsed >= DONE_DISPLAY_MS) {
                state_ = CalibrationState::IDLE;
                just_finished_ = true;
            }
            break;
    }

    return false;
}

void CalibrationManager::draw(uint32_t now) {
    if (state_ == CalibrationState::IDLE) return;

    uint32_t elapsed = (state_start_time_ > 0) ? (now - state_start_time_) : 0;

    hal_->displayClear();

    switch (state_) {
        case CalibrationState::BASELINE_PROMPT:
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::CYAN);
            hal_->displayPrint("SET BASELINE");
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 35);
            hal_->displaySetTextColor(Color::WHITE);
            hal_->displayPrint("Hold your best form,");
            hal_->displaySetCursor(5, 48);
            hal_->displayPrint("then press A.");
            hal_->displaySetCursor(5, 68);
            hal_->displayPrint("5s countdown starts.");
            hal_->displaySetCursor(5, 88);
            hal_->displaySetTextColor(Color::GRAY);
            hal_->displayPrint("A:start  B:cancel");
            break;

        case CalibrationState::BASELINE_COUNTDOWN: {
            uint32_t remaining = 0;
            if (elapsed < BASELINE_COUNTDOWN_MS) {
                remaining = (BASELINE_COUNTDOWN_MS - elapsed + 999) / 1000;
            }
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::CYAN);
            hal_->displayPrint("GET INTO FORM");
            hal_->displaySetTextSize(4);
            hal_->displaySetCursor(50, 40);
            hal_->displaySetTextColor(Color::YELLOW);
            char buf[4];
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)remaining);
            hal_->displayPrint(buf);
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 88);
            hal_->displaySetTextColor(Color::GRAY);
            hal_->displayPrint("B:cancel");
            break;
        }

        case CalibrationState::BASELINE_CAPTURE:
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::GREEN);
            hal_->displayPrint("CAPTURING...");
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 35);
            hal_->displaySetTextColor(Color::WHITE);
            hal_->displayPrint("Hold still!");
            hal_->displaySetCursor(5, 55);
            char capBuf[32];
            snprintf(capBuf, sizeof(capBuf), "Samples: %lu", (unsigned long)sample_count_);
            hal_->displayPrint(capBuf);
            break;

        case CalibrationState::BASELINE_DONE:
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::GREEN);
            hal_->displayPrint("SAVED!");
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 35);
            hal_->displaySetTextColor(Color::WHITE);
            {
                char line[40];
                snprintf(line, sizeof(line), "Cant:  %.1f deg", result_cant_);
                hal_->displayPrint(line);
                hal_->displaySetCursor(5, 50);
                snprintf(line, sizeof(line), "Elev:  %.1f deg", result_elev_);
                hal_->displayPrint(line);
                hal_->displaySetCursor(5, 65);
                snprintf(line, sizeof(line), "Draw:  %.1f deg", result_draw_);
                hal_->displayPrint(line);
            }
            break;

        case CalibrationState::FLATCAL_PROMPT:
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::CYAN);
            hal_->displayPrint("FLAT CAL");
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 35);
            hal_->displaySetTextColor(Color::WHITE);
            hal_->displayPrint("Place device flat on");
            hal_->displaySetCursor(5, 48);
            hal_->displayPrint("a level surface.");
            hal_->displaySetCursor(5, 68);
            hal_->displayPrint("Press A when ready.");
            hal_->displaySetCursor(5, 88);
            hal_->displaySetTextColor(Color::GRAY);
            hal_->displayPrint("A:start  B:cancel");
            break;

        case CalibrationState::FLATCAL_COUNTDOWN: {
            uint32_t remaining = 0;
            if (elapsed < FLATCAL_COUNTDOWN_MS) {
                remaining = (FLATCAL_COUNTDOWN_MS - elapsed + 999) / 1000;
            }
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::CYAN);
            hal_->displayPrint("KEEP FLAT");
            hal_->displaySetTextSize(4);
            hal_->displaySetCursor(50, 40);
            hal_->displaySetTextColor(Color::YELLOW);
            char buf2[4];
            snprintf(buf2, sizeof(buf2), "%lu", (unsigned long)remaining);
            hal_->displayPrint(buf2);
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 88);
            hal_->displaySetTextColor(Color::GRAY);
            hal_->displayPrint("B:cancel");
            break;
        }

        case CalibrationState::FLATCAL_CAPTURE:
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::GREEN);
            hal_->displayPrint("SAMPLING...");
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 35);
            hal_->displaySetTextColor(Color::WHITE);
            hal_->displayPrint("Don't move!");
            hal_->displaySetCursor(5, 55);
            char fcBuf[32];
            snprintf(fcBuf, sizeof(fcBuf), "Samples: %lu", (unsigned long)sample_count_);
            hal_->displayPrint(fcBuf);
            break;

        case CalibrationState::FLATCAL_DONE:
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(5, 5);
            hal_->displaySetTextColor(Color::GREEN);
            hal_->displayPrint("BIAS SAVED!");
            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(5, 35);
            hal_->displaySetTextColor(Color::WHITE);
            {
                char line[40];
                snprintf(line, sizeof(line), "X bias: %.4f g", result_ax_bias_);
                hal_->displayPrint(line);
                hal_->displaySetCursor(5, 50);
                snprintf(line, sizeof(line), "Y bias: %.4f g", result_ay_bias_);
                hal_->displayPrint(line);
                hal_->displaySetCursor(5, 65);
                snprintf(line, sizeof(line), "Z bias: %.4f g", result_az_bias_);
                hal_->displayPrint(line);
            }
            break;

        default:
            break;
    }
}

void CalibrationManager::transitionTo(CalibrationState newState, uint32_t now) {
    state_ = newState;
    state_start_time_ = now;  // 0 means "set on next update()"
    if (newState == CalibrationState::BASELINE_CAPTURE ||
        newState == CalibrationState::FLATCAL_CAPTURE) {
        resetAccumulators();
    }
}

void CalibrationManager::resetAccumulators() {
    sum_pitch_ = 0;
    sum_roll_ = 0;
    sum_draw_ = 0;
    sum_ax_ = 0;
    sum_ay_ = 0;
    sum_az_ = 0;
    sample_count_ = 0;
}
