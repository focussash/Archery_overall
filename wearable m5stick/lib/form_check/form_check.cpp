#include "form_check.h"
#include <cmath>

FormChecker::FormChecker(const ConfigStore* config)
    : config_(config)
    , prev_state_(ShotState::IDLE)
    , aligned_(false)
    , in_hold_(false)
{}

void FormChecker::reset() {
    prev_state_ = ShotState::IDLE;
    aligned_ = false;
    in_hold_ = false;
}

bool FormChecker::checkAlignment(float pitch, float roll, float draw_angle) const {
    float cant_dev = fabsf(roll - config_->getCantBaseline());
    float elev_dev = fabsf(pitch - config_->getElevationBaseline());
    float draw_dev = fabsf(draw_angle - config_->getDrawBaseline());

    return (cant_dev <= config_->getCantTolerance()) &&
           (elev_dev <= config_->getElevationTolerance()) &&
           (draw_dev <= config_->getDrawTolerance());
}

FormCheckEvent FormChecker::update(ShotState current_state,
                                    float pitch, float roll, float draw_angle) {
    FormCheckEvent event = FormCheckEvent::NONE;

    // Detect transition into DRAW_DETECTED
    if (current_state == ShotState::DRAW_DETECTED && prev_state_ != ShotState::DRAW_DETECTED) {
        event = FormCheckEvent::DRAW_STARTED;
        aligned_ = false;
        in_hold_ = false;
    }

    // Detect transition into HOLD
    if (current_state == ShotState::HOLD && prev_state_ != ShotState::HOLD) {
        in_hold_ = true;
        bool now_aligned = checkAlignment(pitch, roll, draw_angle);
        aligned_ = now_aligned;
        if (now_aligned) {
            event = FormCheckEvent::FORM_ALIGNED;
        } else {
            event = FormCheckEvent::FORM_DEVIATED;
        }
    }

    // During HOLD: check for alignment changes
    if (current_state == ShotState::HOLD && prev_state_ == ShotState::HOLD) {
        bool now_aligned = checkAlignment(pitch, roll, draw_angle);
        if (now_aligned && !aligned_) {
            event = FormCheckEvent::FORM_ALIGNED;
        } else if (!now_aligned && aligned_) {
            event = FormCheckEvent::FORM_DEVIATED;
        }
        aligned_ = now_aligned;
    }

    // Reset tracking when leaving HOLD (shot fired or timeout)
    if (current_state != ShotState::HOLD && current_state != ShotState::DRAW_DETECTED) {
        in_hold_ = false;
    }

    prev_state_ = current_state;
    return event;
}
