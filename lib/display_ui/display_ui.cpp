#include "display_ui.h"

DisplayUI::DisplayUI(HalInterface* hal)
    : hal_(hal)
    , current_screen_(ScreenID::HOME)
    , prev_screen_(ScreenID::MENU)  // Force initial full redraw
{
}

void DisplayUI::setScreen(ScreenID screen) {
    current_screen_ = screen;
}

void DisplayUI::nextScreen() {
    switch (current_screen_) {
        case ScreenID::HOME:    current_screen_ = ScreenID::LIVE; break;
        case ScreenID::LIVE:    current_screen_ = ScreenID::SESSION; break;
        case ScreenID::SESSION: current_screen_ = ScreenID::HOME; break;
        case ScreenID::MENU:    current_screen_ = ScreenID::HOME; break;
    }
}

void DisplayUI::draw(const DisplayData& data) {
    // Full redraw on screen change
    if (current_screen_ != prev_screen_) {
        hal_->displayClear();
        prev_screen_ = current_screen_;
    }

    switch (current_screen_) {
        case ScreenID::HOME:    drawHome(data); break;
        case ScreenID::LIVE:    drawLive(data); break;
        case ScreenID::SESSION: drawSession(data); break;
        case ScreenID::MENU:    break; // Menu module handles its own drawing
    }
}

// ── Status bar (top 10px, shared by all screens) ──

void DisplayUI::drawStatusBar(const DisplayData& data) {
    int w = hal_->getDisplayWidth();
    hal_->displayFillRect(0, 0, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 1);

    if (data.sec_connected) {
        hal_->displaySetTextColor(Color::GREEN);
        hal_->displayPrintf("Sec B:%d%%", data.sec_battery);
    } else {
        hal_->displaySetTextColor(Color::RED);
        hal_->displayPrint("Sec:OFF");
    }

    // Right-aligned battery
    hal_->displaySetCursor(w - 48, 1);
    hal_->displaySetTextColor(data.battery > 20 ? Color::WHITE : Color::RED);
    hal_->displayPrintf("Bat:%d%%", data.battery);
}

// ── HOME screen: overview ──

void DisplayUI::drawHome(const DisplayData& data) {
    int w = hal_->getDisplayWidth();

    drawStatusBar(data);

    // Session state (large)
    hal_->displayFillRect(0, 14, w, 30, Color::BLACK);
    hal_->displaySetTextSize(2);
    hal_->displaySetCursor(2, 16);
    if (data.session_state == SessionState::ACTIVE) {
        hal_->displaySetTextColor(Color::GREEN);
        uint32_t secs = data.elapsed_ms / 1000;
        hal_->displayPrintf("ACTIVE %lus", secs);
    } else {
        hal_->displaySetTextColor(Color::GRAY);
        hal_->displayPrint("IDLE");
    }

    // Shot count
    hal_->displayFillRect(0, 48, w, 20, Color::BLACK);
    hal_->displaySetTextSize(2);
    hal_->displaySetCursor(2, 50);
    hal_->displaySetTextColor(Color::WHITE);
    hal_->displayPrintf("Shots: %d", data.shot_count);

    // Last shot quick summary
    hal_->displayFillRect(0, 72, w, 20, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 74);
    if (data.has_last_shot) {
        bool all_ok = data.last_cant_ok && data.last_elev_ok && data.last_draw_ok;
        hal_->displaySetTextColor(all_ok ? Color::GREEN : Color::ORANGE);
        hal_->displayPrintf("#%d C:%.1f E:%.1f D:%.1f %s",
            data.last_shot_num, data.last_cant, data.last_elev, data.last_draw,
            all_ok ? "OK" : "!");
    } else {
        hal_->displaySetTextColor(Color::GRAY);
        hal_->displayPrint("No shots yet");
    }

    // Buffer info
    hal_->displayFillRect(0, 96, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 98);
    hal_->displaySetTextColor(Color::GRAY);
    hal_->displayPrintf("Buf:%d/%d", data.buffer_count, data.buffer_capacity);

    // Bottom: screen indicator
    hal_->displayFillRect(0, 125, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 127);
    hal_->displaySetTextColor(Color::DARKGRAY);
    hal_->displayPrint("[HOME]  A:next  B:menu");
}

// ── LIVE screen: real-time angles ──

void DisplayUI::drawLive(const DisplayData& data) {
    int w = hal_->getDisplayWidth();

    drawStatusBar(data);

    // Primary pitch (large)
    hal_->displayFillRect(0, 14, w, 24, Color::BLACK);
    hal_->displaySetTextSize(2);
    hal_->displaySetCursor(2, 16);
    hal_->displaySetTextColor(Color::CYAN);
    hal_->displayPrintf("P:%6.1f", data.pitch);

    // Primary roll (large)
    hal_->displayFillRect(0, 40, w, 24, Color::BLACK);
    hal_->displaySetTextSize(2);
    hal_->displaySetCursor(2, 42);
    hal_->displaySetTextColor(Color::YELLOW);
    hal_->displayPrintf("R:%6.1f", data.roll);

    // Secondary angles
    hal_->displayFillRect(0, 68, w, 20, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 70);
    if (data.sec_connected) {
        hal_->displaySetTextColor(Color::GREEN);
        hal_->displayPrintf("Sec P:%.1f R:%.1f", data.sec_pitch, data.sec_roll);
    } else {
        hal_->displaySetTextColor(Color::RED);
        hal_->displayPrint("Sec: DISCONNECTED");
    }

    // Session indicator
    hal_->displayFillRect(0, 92, w, 20, Color::BLACK);
    hal_->displaySetTextSize(2);
    hal_->displaySetCursor(2, 94);
    if (data.session_state == SessionState::ACTIVE) {
        hal_->displaySetTextColor(Color::GREEN);
        hal_->displayPrintf("#%d", data.shot_count);
    } else {
        hal_->displaySetTextColor(Color::DARKGRAY);
        hal_->displayPrint("--");
    }

    // Bottom: screen indicator
    hal_->displayFillRect(0, 125, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 127);
    hal_->displaySetTextColor(Color::DARKGRAY);
    hal_->displayPrint("[LIVE]  A:next  B:menu");
}

// ── SESSION screen: shot log & stats ──

void DisplayUI::drawSession(const DisplayData& data) {
    int w = hal_->getDisplayWidth();

    drawStatusBar(data);

    // Session header
    hal_->displayFillRect(0, 14, w, 16, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 16);
    if (data.session_state == SessionState::ACTIVE) {
        hal_->displaySetTextColor(Color::GREEN);
        uint32_t secs = data.elapsed_ms / 1000;
        hal_->displayPrintf("Session: %lus  Shots:%d", secs, data.shot_count);
    } else {
        hal_->displaySetTextColor(Color::GRAY);
        hal_->displayPrintf("No active session  #%d", data.shot_count);
    }

    // Last shot detail
    hal_->displayFillRect(0, 34, w, 60, Color::BLACK);
    if (data.has_last_shot) {
        hal_->displaySetTextSize(1);
        hal_->displaySetCursor(2, 36);
        hal_->displaySetTextColor(Color::WHITE);
        hal_->displayPrintf("Last shot #%d:", data.last_shot_num);

        // Cant
        hal_->displaySetCursor(2, 50);
        hal_->displaySetTextColor(data.last_cant_ok ? Color::GREEN : Color::RED);
        hal_->displayPrintf("Cant:  %6.1f  %s", data.last_cant,
            data.last_cant_ok ? "OK" : "OVER");

        // Elevation
        hal_->displaySetCursor(2, 62);
        hal_->displaySetTextColor(data.last_elev_ok ? Color::GREEN : Color::RED);
        hal_->displayPrintf("Elev:  %6.1f  %s", data.last_elev,
            data.last_elev_ok ? "OK" : "OVER");

        // Draw
        hal_->displaySetCursor(2, 74);
        hal_->displaySetTextColor(data.last_draw_ok ? Color::GREEN : Color::RED);
        hal_->displayPrintf("Draw:  %6.1f  %s", data.last_draw,
            data.last_draw_ok ? "OK" : "OVER");
    } else {
        hal_->displaySetTextSize(1);
        hal_->displaySetCursor(2, 50);
        hal_->displaySetTextColor(Color::GRAY);
        hal_->displayPrint("No shots recorded");
    }

    // Baselines & tolerances reference
    hal_->displayFillRect(0, 96, w, 20, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 98);
    hal_->displaySetTextColor(Color::DARKGRAY);
    hal_->displayPrintf("Base C:%.0f E:%.0f D:%.0f",
        data.base_cant, data.base_elev, data.base_draw);
    hal_->displaySetCursor(2, 108);
    hal_->displayPrintf("Tol  C:%.0f E:%.0f D:%.0f",
        data.tol_cant, data.tol_elev, data.tol_draw);

    // Bottom: screen indicator
    hal_->displayFillRect(0, 125, w, 10, Color::BLACK);
    hal_->displaySetTextSize(1);
    hal_->displaySetCursor(2, 127);
    hal_->displaySetTextColor(Color::DARKGRAY);
    hal_->displayPrint("[SESSION] A:next B:menu");
}
