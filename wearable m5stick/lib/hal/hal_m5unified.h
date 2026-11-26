#pragma once
#ifndef NATIVE_TEST

#include "hal_interface.h"

class HalM5Unified : public HalInterface {
public:
    void init() override;
    void update() override;

    bool readIMU(imu_data_t* data) override;

    void playTone(uint16_t freq_hz, uint32_t duration_ms) override;
    void stopTone() override;

    bool getButtonA() override;
    bool getButtonB() override;
    bool getButtonAHeld() override;
    bool getButtonBHeld() override;

    int getBatteryLevel() override;

    int getDisplayWidth() override;
    int getDisplayHeight() override;
    void displayClear(uint32_t color = 0) override;
    void displaySetCursor(int x, int y) override;
    void displaySetTextSize(float size) override;
    void displaySetTextColor(uint32_t color, uint32_t bg = 0) override;
    void displayPrint(const char* text) override;
    void displayPrintf(const char* fmt, ...) override;
    void displayFillRect(int x, int y, int w, int h, uint32_t color) override;
    void displayDrawLine(int x0, int y0, int x1, int y1, uint32_t color) override;

    float getHeading() override;
    uint32_t millis() override;
};

#endif // NATIVE_TEST
