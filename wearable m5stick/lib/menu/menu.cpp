#include "menu.h"
#include "display_ui.h"  // For Color constants

Menu::Menu(HalInterface* hal, ConfigStore* config)
    : hal_(hal)
    , config_(config)
    , active_(false)
    , level_(MenuLevel::PARENT)
    , selected_(0)
    , editing_(false)
    , scroll_top_(0)
    , last_input_time_(0)
    , cal_request_(CalibrationRequest::NONE)
{
}

void Menu::enter() {
    active_ = true;
    level_ = MenuLevel::PARENT;
    selected_ = 0;
    editing_ = false;
    scroll_top_ = 0;
    cal_request_ = CalibrationRequest::NONE;
    hal_->displayClear();
    draw();
}

void Menu::exit() {
    config_->save();
    active_ = false;
}

uint8_t Menu::getItemCount() const {
    switch (level_) {
        case MenuLevel::PARENT:           return static_cast<uint8_t>(ParentItem::PARENT_COUNT);
        case MenuLevel::FEATURE_SETTINGS: return static_cast<uint8_t>(FeatureItem::FEATURE_COUNT);
        case MenuLevel::PARAM_SETTINGS:   return static_cast<uint8_t>(ParamItem::PARAM_COUNT);
        case MenuLevel::CALIBRATE:        return static_cast<uint8_t>(CalibrateItem::CALIBRATE_COUNT);
    }
    return 0;
}

const char* Menu::getTitle() const {
    switch (level_) {
        case MenuLevel::PARENT:           return "MENU";
        case MenuLevel::FEATURE_SETTINGS: return "FEATURES";
        case MenuLevel::PARAM_SETTINGS:   return "PARAMETERS";
        case MenuLevel::CALIBRATE:        return "CALIBRATE";
    }
    return "";
}

void Menu::onButtonA() {
    if (editing_) {
        // Increment current param value
        ParamItem id = static_cast<ParamItem>(selected_);
        float val = getParamValue(id) + STEP;
        if (val > getParamMax(id)) val = getParamMax(id);
        setParamValue(id, val);
        last_input_time_ = hal_->millis();
    } else {
        // Move to next menu item
        selected_ = (selected_ + 1) % getItemCount();
        if (selected_ >= scroll_top_ + VISIBLE_ITEMS) {
            scroll_top_ = selected_ - VISIBLE_ITEMS + 1;
        }
        if (selected_ < scroll_top_) {
            scroll_top_ = 0;  // Wrapped around
        }
    }
    draw();
}

void Menu::onButtonB() {
    if (level_ == MenuLevel::PARENT) {
        ParentItem id = static_cast<ParentItem>(selected_);
        if (id == ParentItem::FEATURE_SETTINGS) {
            level_ = MenuLevel::FEATURE_SETTINGS;
            selected_ = 0;
            scroll_top_ = 0;
            hal_->displayClear();
        } else if (id == ParentItem::PARAM_SETTINGS) {
            level_ = MenuLevel::PARAM_SETTINGS;
            selected_ = 0;
            scroll_top_ = 0;
            hal_->displayClear();
        } else if (id == ParentItem::CALIBRATE) {
            level_ = MenuLevel::CALIBRATE;
            selected_ = 0;
            scroll_top_ = 0;
            hal_->displayClear();
        } else if (id == ParentItem::EXIT) {
            exit();
            return;
        }
    } else if (level_ == MenuLevel::CALIBRATE) {
        CalibrateItem id = static_cast<CalibrateItem>(selected_);
        if (id == CalibrateItem::BACK) {
            level_ = MenuLevel::PARENT;
            selected_ = 2;  // Highlight "Calibrate" when returning
            scroll_top_ = 0;
            hal_->displayClear();
        } else if (id == CalibrateItem::SET_BASELINE) {
            cal_request_ = CalibrationRequest::BASELINE;
            exit();
            return;
        } else if (id == CalibrateItem::FLAT_CAL) {
            cal_request_ = CalibrationRequest::FLAT_CAL;
            exit();
            return;
        }
    } else if (level_ == MenuLevel::FEATURE_SETTINGS) {
        FeatureItem id = static_cast<FeatureItem>(selected_);
        if (id == FeatureItem::BACK) {
            level_ = MenuLevel::PARENT;
            selected_ = 0;
            scroll_top_ = 0;
            hal_->displayClear();
        } else {
            toggleFeature(id);
        }
    } else if (level_ == MenuLevel::PARAM_SETTINGS) {
        ParamItem id = static_cast<ParamItem>(selected_);
        if (id == ParamItem::BACK) {
            level_ = MenuLevel::PARENT;
            selected_ = 1;  // Highlight "Param Settings" when returning
            scroll_top_ = 0;
            hal_->displayClear();
        } else if (editing_) {
            // Decrement current value
            float val = getParamValue(id) - STEP;
            float min_val = getParamMin(id);
            if (val < min_val) val = min_val;
            setParamValue(id, val);
            last_input_time_ = hal_->millis();
        } else {
            // Start editing
            editing_ = true;
            last_input_time_ = hal_->millis();
        }
    }
    draw();
}

void Menu::update(uint32_t now) {
    if (editing_ && (now - last_input_time_ >= EDIT_TIMEOUT_MS)) {
        editing_ = false;
        draw();
    }
}

void Menu::draw() {
    int w = hal_->getDisplayWidth();

    // Title bar
    hal_->displayFillRect(0, 0, w, 14, Color::DARKGRAY);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 3);
    hal_->displaySetTextColor(Color::WHITE, Color::DARKGRAY);
    hal_->displayPrint(getTitle());

    // Scroll indicator
    uint8_t total = getItemCount();
    if (total > VISIBLE_ITEMS) {
        hal_->displaySetCursor(w - 24, 3);
        hal_->displaySetTextColor(Color::GRAY, Color::DARKGRAY);
        hal_->displayPrintf("%d/%d", selected_ + 1, total);
    }

    switch (level_) {
        case MenuLevel::PARENT:           drawParent(); break;
        case MenuLevel::FEATURE_SETTINGS: drawFeature(); break;
        case MenuLevel::PARAM_SETTINGS:   drawParam(); break;
        case MenuLevel::CALIBRATE:        drawCalibrate(); break;
    }

    // Bottom help
    int y_bottom = 125;
    hal_->displayFillRect(0, y_bottom, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, y_bottom + 1);
    hal_->displaySetTextColor(Color::DARKGRAY);
    if (editing_) {
        hal_->displayPrint("[EDIT] A:+ B:- (2s:done)");
    } else if (level_ == MenuLevel::PARENT) {
        hal_->displayPrint("[MENU] A:next B:select");
    } else if (level_ == MenuLevel::FEATURE_SETTINGS) {
        hal_->displayPrint("[FEAT] A:next B:toggle");
    } else if (level_ == MenuLevel::CALIBRATE) {
        hal_->displayPrint("[CAL] A:next B:select");
    } else {
        hal_->displayPrint("[PARAM] A:next B:edit");
    }
}

void Menu::drawParent() {
    int w = hal_->getDisplayWidth();
    uint8_t total = static_cast<uint8_t>(ParentItem::PARENT_COUNT);

    for (uint8_t vi = 0; vi < VISIBLE_ITEMS && (scroll_top_ + vi) < total; vi++) {
        uint8_t i = scroll_top_ + vi;
        int y = 16 + vi * 27;
        bool is_selected = (i == selected_);

        uint32_t bg = is_selected ? Color::BLUE : Color::BLACK;
        hal_->displayFillRect(0, y, w, 26, bg);

        hal_->displaySetTextSize(2);
        hal_->displaySetCursor(8, y + 6);
        hal_->displaySetTextColor(is_selected ? Color::WHITE : Color::GRAY, bg);
        hal_->displayPrint(getParentLabel(static_cast<ParentItem>(i)));
    }
}

void Menu::drawFeature() {
    int w = hal_->getDisplayWidth();
    uint8_t total = static_cast<uint8_t>(FeatureItem::FEATURE_COUNT);

    for (uint8_t vi = 0; vi < VISIBLE_ITEMS && (scroll_top_ + vi) < total; vi++) {
        uint8_t i = scroll_top_ + vi;
        int y = 16 + vi * 27;
        FeatureItem id = static_cast<FeatureItem>(i);
        bool is_selected = (i == selected_);

        uint32_t bg = is_selected ? Color::BLUE : Color::BLACK;
        hal_->displayFillRect(0, y, w, 26, bg);

        // Label
        hal_->displaySetTextSize(1);
        hal_->displaySetCursor(4, y + 2);
        hal_->displaySetTextColor(is_selected ? Color::WHITE : Color::GRAY, bg);
        hal_->displayPrint(getFeatureLabel(id));

        // Toggle value (not for Back)
        if (id != FeatureItem::BACK) {
            bool enabled = getFeatureValue(id);
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(4, y + 12);
            hal_->displaySetTextColor(enabled ? Color::GREEN : Color::RED, bg);
            hal_->displayPrint(enabled ? "ON" : "OFF");
        }
    }
}

void Menu::drawParam() {
    int w = hal_->getDisplayWidth();
    uint8_t total = static_cast<uint8_t>(ParamItem::PARAM_COUNT);

    for (uint8_t vi = 0; vi < VISIBLE_ITEMS && (scroll_top_ + vi) < total; vi++) {
        uint8_t i = scroll_top_ + vi;
        int y = 16 + vi * 27;
        ParamItem id = static_cast<ParamItem>(i);
        bool is_selected = (i == selected_);
        bool is_editing = is_selected && editing_;

        uint32_t bg = is_selected ? Color::BLUE : Color::BLACK;
        hal_->displayFillRect(0, y, w, 26, bg);

        // Label
        hal_->displaySetTextSize(1);
        hal_->displaySetCursor(4, y + 2);
        hal_->displaySetTextColor(is_selected ? Color::WHITE : Color::GRAY, bg);
        hal_->displayPrint(getParamLabel(id));

        // Value (not for Back)
        if (id != ParamItem::BACK) {
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(4, y + 12);
            if (is_editing) {
                hal_->displaySetTextColor(Color::YELLOW, bg);
            } else {
                hal_->displaySetTextColor(Color::WHITE, bg);
            }
            hal_->displayPrintf("%.1f", getParamValue(id));

            hal_->displaySetTextSize(1);
            hal_->displaySetCursor(64, y + 14);
            hal_->displaySetTextColor(Color::GRAY, bg);
            if (is_editing) {
                hal_->displayPrint("A:+ B:-");
            } else {
                hal_->displayPrint("deg");
            }
        }
    }
}

// ── Parent menu helpers ──

const char* Menu::getParentLabel(ParentItem id) const {
    switch (id) {
        case ParentItem::FEATURE_SETTINGS: return "Features";
        case ParentItem::PARAM_SETTINGS:   return "Params";
        case ParentItem::CALIBRATE:        return "Calibrate";
        case ParentItem::EXIT:             return "Save&Exit";
        default: return "";
    }
}

// ── Feature menu helpers ──

const char* Menu::getFeatureLabel(FeatureItem id) const {
    switch (id) {
        case FeatureItem::FORM_CHECK:    return "Form Check";
        case FeatureItem::SHOT_FEEDBACK: return "Shot Feedback";
        case FeatureItem::BACK:          return "> Back";
        default: return "";
    }
}

bool Menu::getFeatureValue(FeatureItem id) const {
    switch (id) {
        case FeatureItem::FORM_CHECK:    return config_->isFormCheckEnabled();
        case FeatureItem::SHOT_FEEDBACK: return config_->isShotFeedbackEnabled();
        default: return false;
    }
}

void Menu::toggleFeature(FeatureItem id) {
    switch (id) {
        case FeatureItem::FORM_CHECK:
            config_->setFormCheckEnabled(!config_->isFormCheckEnabled());
            break;
        case FeatureItem::SHOT_FEEDBACK:
            config_->setShotFeedbackEnabled(!config_->isShotFeedbackEnabled());
            break;
        default: break;
    }
}

// ── Param menu helpers ──

const char* Menu::getParamLabel(ParamItem id) const {
    switch (id) {
        case ParamItem::CANT_BASELINE:  return "Cant Baseline";
        case ParamItem::CANT_TOLERANCE: return "Cant Tolerance";
        case ParamItem::ELEV_BASELINE:  return "Elev Baseline";
        case ParamItem::ELEV_TOLERANCE: return "Elev Tolerance";
        case ParamItem::DRAW_BASELINE:  return "Draw Baseline";
        case ParamItem::DRAW_TOLERANCE: return "Draw Tolerance";
        case ParamItem::BACK:           return "> Back";
        default: return "";
    }
}

float Menu::getParamValue(ParamItem id) const {
    switch (id) {
        case ParamItem::CANT_BASELINE:  return config_->getCantBaseline();
        case ParamItem::CANT_TOLERANCE: return config_->getCantTolerance();
        case ParamItem::ELEV_BASELINE:  return config_->getElevationBaseline();
        case ParamItem::ELEV_TOLERANCE: return config_->getElevationTolerance();
        case ParamItem::DRAW_BASELINE:  return config_->getDrawBaseline();
        case ParamItem::DRAW_TOLERANCE: return config_->getDrawTolerance();
        default: return 0.0f;
    }
}

void Menu::setParamValue(ParamItem id, float val) {
    switch (id) {
        case ParamItem::CANT_BASELINE:  config_->setCantBaseline(val); break;
        case ParamItem::CANT_TOLERANCE: config_->setCantTolerance(val); break;
        case ParamItem::ELEV_BASELINE:  config_->setElevationBaseline(val); break;
        case ParamItem::ELEV_TOLERANCE: config_->setElevationTolerance(val); break;
        case ParamItem::DRAW_BASELINE:  config_->setDrawBaseline(val); break;
        case ParamItem::DRAW_TOLERANCE: config_->setDrawTolerance(val); break;
        default: break;
    }
}

float Menu::getParamMin(ParamItem id) const {
    switch (id) {
        case ParamItem::CANT_BASELINE:
        case ParamItem::ELEV_BASELINE:
        case ParamItem::DRAW_BASELINE:
            return -90.0f;
        default:
            return 0.5f;
    }
}

float Menu::getParamMax(ParamItem id) const {
    return 90.0f;
}

// ── Calibrate menu helpers ──

const char* Menu::getCalibrateLabel(CalibrateItem id) const {
    switch (id) {
        case CalibrateItem::SET_BASELINE: return "Set Base";
        case CalibrateItem::FLAT_CAL:     return "Flat Cal";
        case CalibrateItem::BACK:         return "> Back";
        default: return "";
    }
}

void Menu::drawCalibrate() {
    int w = hal_->getDisplayWidth();
    uint8_t total = static_cast<uint8_t>(CalibrateItem::CALIBRATE_COUNT);

    for (uint8_t vi = 0; vi < VISIBLE_ITEMS && (scroll_top_ + vi) < total; vi++) {
        uint8_t i = scroll_top_ + vi;
        int y = 16 + vi * 27;
        bool is_selected = (i == selected_);

        uint32_t bg = is_selected ? Color::BLUE : Color::BLACK;
        hal_->displayFillRect(0, y, w, 26, bg);

        hal_->displaySetTextSize(2);
        hal_->displaySetCursor(8, y + 6);
        hal_->displaySetTextColor(is_selected ? Color::WHITE : Color::GRAY, bg);
        hal_->displayPrint(getCalibrateLabel(static_cast<CalibrateItem>(i)));
    }
}

CalibrationRequest Menu::getCalibrationRequest() {
    CalibrationRequest req = cal_request_;
    cal_request_ = CalibrationRequest::NONE;
    return req;
}
