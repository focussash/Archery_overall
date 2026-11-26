#include "complementary_filter.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static float radToDeg(float rad) {
    return rad * (180.0f / M_PI);
}

ComplementaryFilter::ComplementaryFilter(float alpha)
    : alpha_(alpha), pitch_(0.0f), roll_(0.0f), initialized_(false) {}

void ComplementaryFilter::update(float accel[3], float gyro[3], float dt) {
    // Accelerometer-based angles
    float ax = accel[0], ay = accel[1], az = accel[2];

    // Avoid division by zero / degenerate cases
    float accelMag = sqrtf(ax * ax + ay * ay + az * az);
    if (accelMag < 0.01f) return;

    float accelPitch = radToDeg(atan2f(-ax, sqrtf(ay * ay + az * az)));
    float accelRoll  = radToDeg(atan2f(ay, az));

    if (!initialized_) {
        pitch_ = accelPitch;
        roll_  = accelRoll;
        initialized_ = true;
        return;
    }

    // Gyro rates (degrees/sec) — gyro[0]=gx (roll rate), gyro[1]=gy (pitch rate)
    float gyroPitchRate = gyro[1];
    float gyroRollRate  = gyro[0];

    // Complementary filter: trust gyro short-term, accel long-term
    pitch_ = alpha_ * (pitch_ + gyroPitchRate * dt) + (1.0f - alpha_) * accelPitch;
    roll_  = alpha_ * (roll_  + gyroRollRate  * dt) + (1.0f - alpha_) * accelRoll;
}

void ComplementaryFilter::reset() {
    pitch_ = 0.0f;
    roll_ = 0.0f;
    initialized_ = false;
}
