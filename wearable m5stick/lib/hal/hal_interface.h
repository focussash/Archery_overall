#pragma once
#include <cstdint>

struct imu_3d_t {
    float x;
    float y;
    float z;
};

struct imu_data_t {
    imu_3d_t accel;  // g units
    imu_3d_t gyro;   // degrees per second
};

class HalInterface {
public:
    virtual ~HalInterface() = default;

    // Lifecycle
    virtual void init() = 0;
    virtual void update() = 0;

    // IMU
    virtual bool readIMU(imu_data_t* data) = 0;

    // Audio
    virtual void playTone(uint16_t freq_hz, uint32_t duration_ms) = 0;
    virtual void stopTone() = 0;

    // Buttons
    virtual bool getButtonA() = 0;
    virtual bool getButtonB() = 0;
    virtual bool getButtonAHeld() = 0;
    virtual bool getButtonBHeld() = 0;

    // Power
    virtual int getBatteryLevel() = 0;

    // Display
    virtual int getDisplayWidth() = 0;
    virtual int getDisplayHeight() = 0;
    virtual void displayClear(uint32_t color = 0) = 0;
    virtual void displaySetCursor(int x, int y) = 0;
    virtual void displaySetTextSize(float size) = 0;
    virtual void displaySetTextColor(uint32_t color, uint32_t bg = 0) = 0;
    virtual void displayPrint(const char* text) = 0;
    virtual void displayPrintf(const char* fmt, ...) = 0;
    virtual void displayFillRect(int x, int y, int w, int h, uint32_t color) = 0;
    virtual void displayDrawLine(int x0, int y0, int x1, int y1, uint32_t color) = 0;

    // Magnetometer (stub for future)
    virtual float getHeading() = 0;  // Returns -1 if no magnetometer

    // Millis
    virtual uint32_t millis() = 0;
};
