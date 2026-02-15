#pragma once
#include <cstdint>
#include "hal_interface.h"
#include "session.h"

enum class MenuItemID : uint8_t {
    CANT_BASELINE = 0,
    CANT_TOLERANCE,
    ELEV_BASELINE,
    ELEV_TOLERANCE,
    DRAW_BASELINE,
    DRAW_TOLERANCE,
    EXIT,
    ITEM_COUNT
};

class Menu {
public:
    Menu(HalInterface* hal, ConfigStore* config);

    void enter();   // Activate menu, draw initial state
    void exit();    // Save and deactivate
    bool isActive() const { return active_; }

    // Button handlers — call these from main when menu is active
    void onButtonA();       // Next item / increment value (+0.5)
    void onButtonB();       // Select item / decrement value (-0.5)

    // Call every loop to handle edit timeout
    void update(uint32_t now);

    // Render menu to display
    void draw();

private:
    HalInterface* hal_;
    ConfigStore* config_;
    bool active_;
    uint8_t selected_;     // Currently highlighted item
    bool editing_;         // True when adjusting a value
    uint8_t scroll_top_;   // Top visible item index (for scrolling)

    static constexpr float STEP = 0.5f;
    static constexpr uint32_t EDIT_TIMEOUT_MS = 2000;
    static constexpr uint8_t VISIBLE_ITEMS = 4;  // How many items fit on screen

    uint32_t last_input_time_;

    float getValue(MenuItemID id) const;
    void setValue(MenuItemID id, float val);
    const char* getLabel(MenuItemID id) const;
    float getMinValue(MenuItemID id) const;
    float getMaxValue(MenuItemID id) const;
};
