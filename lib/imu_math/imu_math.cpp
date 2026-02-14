#include "imu_math.h"
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static float radToDeg(float rad) {
    return rad * (180.0f / M_PI);
}

namespace imu_math {

float computePitch(float ax, float ay, float az) {
    return radToDeg(atan2f(-ax, sqrtf(ay * ay + az * az)));
}

float computeRoll(float ax, float ay, float az) {
    return radToDeg(atan2f(ay, az));
}

float computeAngularVelocity(float gx, float gy, float gz) {
    return sqrtf(gx * gx + gy * gy + gz * gz);
}

int16_t degreesToCentidegrees(float deg) {
    float clamped = deg;
    if (clamped > 327.67f) clamped = 327.67f;
    if (clamped < -327.68f) clamped = -327.68f;
    return (int16_t)(clamped * 100.0f);
}

float centidegreesToDegrees(int16_t cdeg) {
    return (float)cdeg / 100.0f;
}

} // namespace imu_math
