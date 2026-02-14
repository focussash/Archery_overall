#ifndef NATIVE_TEST

#include "hal_m5unified.h"
#include <M5Unified.h>
#include <cstdarg>

void HalM5Unified::init() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    M5.Speaker.setVolume(128);

    if (!M5.Imu.isEnabled()) {
        M5.Display.println("IMU init FAILED");
    }
}

void HalM5Unified::update() {
    M5.update();
}

bool HalM5Unified::readIMU(imu_data_t* data) {
    if (!M5.Imu.isEnabled()) return false;

    m5::imu_data_t imu;
    M5.Imu.getImuData(&imu);

    data->accel.x = imu.accel.x;
    data->accel.y = imu.accel.y;
    data->accel.z = imu.accel.z;
    data->gyro.x = imu.gyro.x;
    data->gyro.y = imu.gyro.y;
    data->gyro.z = imu.gyro.z;
    return true;
}

void HalM5Unified::playTone(uint16_t freq_hz, uint32_t duration_ms) {
    M5.Speaker.tone(freq_hz, duration_ms);
}

void HalM5Unified::stopTone() {
    M5.Speaker.stop();
}

bool HalM5Unified::getButtonA() {
    return M5.BtnA.wasPressed();
}

bool HalM5Unified::getButtonB() {
    return M5.BtnB.wasPressed();
}

bool HalM5Unified::getButtonAHeld() {
    return M5.BtnA.wasHold();
}

bool HalM5Unified::getButtonBHeld() {
    return M5.BtnB.wasHold();
}

int HalM5Unified::getBatteryLevel() {
    return M5.Power.getBatteryLevel();
}

int HalM5Unified::getDisplayWidth() {
    return M5.Display.width();
}

int HalM5Unified::getDisplayHeight() {
    return M5.Display.height();
}

void HalM5Unified::displayClear(uint32_t color) {
    M5.Display.fillScreen(color);
}

void HalM5Unified::displaySetCursor(int x, int y) {
    M5.Display.setCursor(x, y);
}

void HalM5Unified::displaySetTextSize(float size) {
    M5.Display.setTextSize(size);
}

void HalM5Unified::displaySetTextColor(uint32_t color, uint32_t bg) {
    M5.Display.setTextColor(color, bg);
}

void HalM5Unified::displayPrint(const char* text) {
    M5.Display.print(text);
}

void HalM5Unified::displayPrintf(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    M5.Display.print(buf);
}

void HalM5Unified::displayFillRect(int x, int y, int w, int h, uint32_t color) {
    M5.Display.fillRect(x, y, w, h, color);
}

void HalM5Unified::displayDrawLine(int x0, int y0, int x1, int y1, uint32_t color) {
    M5.Display.drawLine(x0, y0, x1, y1, color);
}

float HalM5Unified::getHeading() {
    return -1.0f;  // No magnetometer on M5StickC
}

uint32_t HalM5Unified::millis() {
    return ::millis();
}

#endif // NATIVE_TEST
