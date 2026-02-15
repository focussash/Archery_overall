#include "menu.h"
#include "display_ui.h"  // For Color constants

Menu::Menu(HalInterface* hal, ConfigStore* config)
    : hal_(hal)
    , config_(config)
    , active_(false)
    , selected_(0)
    , editing_(false)
    , scroll_top_(0)
    , last_input_time_(0)
{
}

void Menu::enter() {
    active_ = true;
    selected_ = 0;
    editing_ = false;
    scroll_top_ = 0;
    hal_->displayClear();
    draw();
}

void Menu::exit() {
    config_->save();
    active_ = false;
}

void Menu::onButtonA() {
    if (editing_) {
        // Increment current value
        MenuItemID id = static_cast<MenuItemID>(selected_);
        float val = getValue(id) + STEP;
        if (val > getMaxValue(id)) val = getMaxValue(id);
        setValue(id, val);
        last_input_time_ = hal_->millis();
    } else {
        // Move to next menu item
        selected_ = (selected_ + 1) % static_cast<uint8_t>(MenuItemID::ITEM_COUNT);
        // Scroll if needed
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
    MenuItemID id = static_cast<MenuItemID>(selected_);

    if (editing_) {
        // Decrement current value
        float val = getValue(id) - STEP;
        float min_val = getMinValue(id);
        if (val < min_val) val = min_val;
        setValue(id, val);
        last_input_time_ = hal_->millis();
        draw();
    } else if (id == MenuItemID::EXIT) {
        exit();
    } else {
        // Start editing selected item
        editing_ = true;
        last_input_time_ = hal_->millis();
        draw();
    }
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
    hal_->displayPrint("SETTINGS");

    // Scroll indicator
    if (static_cast<uint8_t>(MenuItemID::ITEM_COUNT) > VISIBLE_ITEMS) {
        hal_->displaySetCursor(w - 24, 3);
        hal_->displaySetTextColor(Color::GRAY, Color::DARKGRAY);
        hal_->displayPrintf("%d/%d", selected_ + 1,
            static_cast<uint8_t>(MenuItemID::ITEM_COUNT));
    }

    // Draw visible menu items
    uint8_t total = static_cast<uint8_t>(MenuItemID::ITEM_COUNT);
    for (uint8_t vi = 0; vi < VISIBLE_ITEMS && (scroll_top_ + vi) < total; vi++) {
        uint8_t i = scroll_top_ + vi;
        int y = 16 + vi * 27;
        MenuItemID id = static_cast<MenuItemID>(i);
        bool is_selected = (i == selected_);
        bool is_editing = is_selected && editing_;

        // Background highlight
        uint32_t bg = is_selected ? Color::BLUE : Color::BLACK;
        hal_->displayFillRect(0, y, w, 26, bg);

        // Label
        hal_->displaySetTextSize(1);
        hal_->displaySetCursor(4, y + 2);
        hal_->displaySetTextColor(is_selected ? Color::WHITE : Color::GRAY, bg);
        hal_->displayPrint(getLabel(id));

        // Value (not for EXIT item)
        if (id != MenuItemID::EXIT) {
            hal_->displaySetTextSize(2);
            hal_->displaySetCursor(4, y + 12);

            if (is_editing) {
                hal_->displaySetTextColor(Color::YELLOW, bg);
            } else {
                hal_->displaySetTextColor(Color::WHITE, bg);
            }
            hal_->displayPrintf("%.1f", getValue(id));

            // Unit + editing hint
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

    // Bottom help
    int y_bottom = 125;
    hal_->displayFillRect(0, y_bottom, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, y_bottom + 1);
    hal_->displaySetTextColor(Color::DARKGRAY);
    if (editing_) {
        hal_->displayPrint("[EDIT] A:+ B:- (2s:done)");
    } else {
        hal_->displayPrint("[MENU] A:next B:select");
    }
}

float Menu::getValue(MenuItemID id) const {
    switch (id) {
        case MenuItemID::CANT_BASELINE:  return config_->getCantBaseline();
        case MenuItemID::CANT_TOLERANCE: return config_->getCantTolerance();
        case MenuItemID::ELEV_BASELINE:  return config_->getElevationBaseline();
        case MenuItemID::ELEV_TOLERANCE: return config_->getElevationTolerance();
        case MenuItemID::DRAW_BASELINE:  return config_->getDrawBaseline();
        case MenuItemID::DRAW_TOLERANCE: return config_->getDrawTolerance();
        default: return 0.0f;
    }
}

void Menu::setValue(MenuItemID id, float val) {
    switch (id) {
        case MenuItemID::CANT_BASELINE:  config_->setCantBaseline(val); break;
        case MenuItemID::CANT_TOLERANCE: config_->setCantTolerance(val); break;
        case MenuItemID::ELEV_BASELINE:  config_->setElevationBaseline(val); break;
        case MenuItemID::ELEV_TOLERANCE: config_->setElevationTolerance(val); break;
        case MenuItemID::DRAW_BASELINE:  config_->setDrawBaseline(val); break;
        case MenuItemID::DRAW_TOLERANCE: config_->setDrawTolerance(val); break;
        default: break;
    }
}

const char* Menu::getLabel(MenuItemID id) const {
    switch (id) {
        case MenuItemID::CANT_BASELINE:  return "Cant Baseline";
        case MenuItemID::CANT_TOLERANCE: return "Cant Tolerance";
        case MenuItemID::ELEV_BASELINE:  return "Elev Baseline";
        case MenuItemID::ELEV_TOLERANCE: return "Elev Tolerance";
        case MenuItemID::DRAW_BASELINE:  return "Draw Baseline";
        case MenuItemID::DRAW_TOLERANCE: return "Draw Tolerance";
        case MenuItemID::EXIT:           return "> Save & Exit";
        default: return "";
    }
}

float Menu::getMinValue(MenuItemID id) const {
    switch (id) {
        // Baselines can be negative (e.g., canted bow)
        case MenuItemID::CANT_BASELINE:
        case MenuItemID::ELEV_BASELINE:
        case MenuItemID::DRAW_BASELINE:
            return -90.0f;
        // Tolerances must be positive
        default:
            return 0.5f;
    }
}

float Menu::getMaxValue(MenuItemID id) const {
    return 90.0f;
}
