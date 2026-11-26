#pragma once
#include <cstdint>

namespace imu_math {

// Compute pitch from accelerometer only (degrees)
float computePitch(float ax, float ay, float az);

// Compute roll from accelerometer only (degrees)
float computeRoll(float ax, float ay, float az);

// Compute angular velocity magnitude from gyroscope (degrees/sec)
float computeAngularVelocity(float gx, float gy, float gz);

// Convert degrees to centidegrees (for ShotPacket_t int16 format)
int16_t degreesToCentidegrees(float deg);

// Convert centidegrees back to degrees
float centidegreesToDegrees(int16_t cdeg);

} // namespace imu_math
