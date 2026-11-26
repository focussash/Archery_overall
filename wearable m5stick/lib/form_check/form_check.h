#pragma once
#include <cstdint>
#include "shot_detect.h"
#include "session.h"

enum class FormCheckEvent : uint8_t {
    NONE = 0,
    DRAW_STARTED,       // Transitioned into DRAW_DETECTED
    FORM_ALIGNED,       // Entered HOLD with good form, or regained alignment
    FORM_DEVIATED       // Angles drifted outside tolerance during HOLD
};

class FormChecker {
public:
    FormChecker(const ConfigStore* config);

    // Call each loop after shotDetector.update()
    // Returns event (if any) based on state transition or alignment change
    FormCheckEvent update(ShotState current_state,
                          float pitch, float roll, float draw_angle);

    // Reset state (call on shot detected or session start)
    void reset();

private:
    const ConfigStore* config_;
    ShotState prev_state_;
    bool aligned_;          // Was form aligned last check?
    bool in_hold_;          // Currently in HOLD state?

    bool checkAlignment(float pitch, float roll, float draw_angle) const;
};
