#pragma once
#include <cstdint>
#include "hal_interface.h"
#include "session.h"

// Named RGB888 colors (values > 0xFFFF go through LovyanGFX color pipeline,
// which handles panel-specific color order automatically)
namespace Color {
    constexpr uint32_t BLACK    = 0x000000;
    constexpr uint32_t WHITE    = 0xFFFFFF;
    constexpr uint32_t RED      = 0xFF0000;
    constexpr uint32_t GREEN    = 0x00FF00;
    constexpr uint32_t BLUE     = 0x0000FF;
    constexpr uint32_t CYAN     = 0x00FFFF;
    constexpr uint32_t YELLOW   = 0xFFFF00;
    constexpr uint32_t ORANGE   = 0xFF8C00;
    constexpr uint32_t GRAY     = 0x808080;
    constexpr uint32_t DARKGRAY = 0x404040;
}

enum class ScreenID : uint8_t {
    HOME,
    LIVE,
    SESSION,
    MENU    // Rendered by Menu module, not DisplayUI
};

// All data the display needs, filled by main loop
struct DisplayData {
    // Primary IMU
    float pitch;
    float roll;

    // Secondary
    bool sec_connected;
    float sec_pitch;
    float sec_roll;
    uint8_t sec_battery;

    // Session
    SessionState session_state;
    uint8_t shot_count;
    uint32_t elapsed_ms;

    // Last shot
    bool has_last_shot;
    uint8_t last_shot_num;
    float last_cant;
    float last_elev;
    float last_draw;
    bool last_cant_ok;
    bool last_elev_ok;
    bool last_draw_ok;

    // System
    int battery;
    uint8_t buffer_count;
    uint8_t buffer_capacity;

    // Status toast (temporary message, e.g. form check events)
    const char* toast_msg;      // nullptr = no toast
    uint32_t toast_color;
    uint32_t toast_expire_ms;   // millis() when toast should disappear

    // Config (for display reference)
    float base_cant;
    float base_elev;
    float base_draw;
    float tol_cant;
    float tol_elev;
    float tol_draw;
};

class DisplayUI {
public:
    DisplayUI(HalInterface* hal);

    void setScreen(ScreenID screen);
    ScreenID getScreen() const { return current_screen_; }
    void nextScreen();  // Cycle HOME -> LIVE -> SESSION -> HOME (skips MENU)

    // Draw current screen. Call at desired refresh rate (e.g., 10Hz).
    void draw(const DisplayData& data);

private:
    HalInterface* hal_;
    ScreenID current_screen_;

    // Track previous screen to force full redraw on switch
    ScreenID prev_screen_;

    void drawHome(const DisplayData& data);
    void drawLive(const DisplayData& data);
    void drawSession(const DisplayData& data);

    // Helper: draw the common status bar (top 10px)
    void drawStatusBar(const DisplayData& data);

    // Helper: draw toast overlay if active
    void drawToast(const DisplayData& data);
};
