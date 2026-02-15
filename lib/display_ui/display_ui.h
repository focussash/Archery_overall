#pragma once
#include <cstdint>
#include "hal_interface.h"
#include "session.h"

// Named RGB565 colors
namespace Color {
    constexpr uint32_t BLACK   = 0x0000;
    constexpr uint32_t WHITE   = 0xFFFF;
    constexpr uint32_t RED     = 0xF800;
    constexpr uint32_t GREEN   = 0x07E0;
    constexpr uint32_t BLUE    = 0x001F;
    constexpr uint32_t CYAN    = 0x07FF;
    constexpr uint32_t YELLOW  = 0xFFE0;
    constexpr uint32_t ORANGE  = 0xFD20;
    constexpr uint32_t GRAY    = 0x7BEF;
    constexpr uint32_t DARKGRAY = 0x4208;
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
};
