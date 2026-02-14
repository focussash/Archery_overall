#include <unity.h>
#include "imu_math.h"
#include "complementary_filter.h"
#include <cmath>

void setUp(void) {}
void tearDown(void) {}

// --- imu_math tests ---

void test_pitch_flat(void) {
    // Device flat on table: accel = (0, 0, 1g) → pitch = 0
    float pitch = imu_math::computePitch(0.0f, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, pitch);
}

void test_pitch_90_forward(void) {
    // Device tilted forward 90°: accel = (1, 0, 0) → pitch = -90
    float pitch = imu_math::computePitch(1.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -90.0f, pitch);
}

void test_pitch_90_backward(void) {
    // Device tilted backward 90°: accel = (-1, 0, 0) → pitch = 90
    float pitch = imu_math::computePitch(-1.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, pitch);
}

void test_roll_flat(void) {
    // Device flat: accel = (0, 0, 1g) → roll = 0
    float roll = imu_math::computeRoll(0.0f, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, roll);
}

void test_roll_90_right(void) {
    // Device tilted right 90°: accel = (0, 1, 0) → roll = 90
    float roll = imu_math::computeRoll(0.0f, 1.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, roll);
}

void test_roll_90_left(void) {
    // Device tilted left 90°: accel = (0, -1, 0) → roll = -90
    float roll = imu_math::computeRoll(0.0f, -1.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -90.0f, roll);
}

void test_angular_velocity_zero(void) {
    float av = imu_math::computeAngularVelocity(0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, av);
}

void test_angular_velocity_single_axis(void) {
    float av = imu_math::computeAngularVelocity(100.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, av);
}

void test_angular_velocity_multi_axis(void) {
    // 3-4-5 triangle scaled: sqrt(3^2 + 4^2) = 5, then with z=0
    float av = imu_math::computeAngularVelocity(3.0f, 4.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, av);
}

void test_centidegrees_conversion(void) {
    int16_t cd = imu_math::degreesToCentidegrees(45.67f);
    TEST_ASSERT_EQUAL_INT16(4567, cd);
}

void test_centidegrees_negative(void) {
    int16_t cd = imu_math::degreesToCentidegrees(-12.34f);
    TEST_ASSERT_EQUAL_INT16(-1234, cd);
}

void test_centidegrees_roundtrip(void) {
    float original = 23.45f;
    int16_t cd = imu_math::degreesToCentidegrees(original);
    float back = imu_math::centidegreesToDegrees(cd);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, original, back);
}

void test_centidegrees_clamping(void) {
    int16_t cd = imu_math::degreesToCentidegrees(500.0f);
    TEST_ASSERT_EQUAL_INT16(32767, cd);  // clamped to max int16
}

// --- ComplementaryFilter tests ---

void test_filter_initial_from_accel(void) {
    ComplementaryFilter filter(0.98f);
    float accel[3] = {0.0f, 0.0f, 1.0f};
    float gyro[3]  = {0.0f, 0.0f, 0.0f};
    filter.update(accel, gyro, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, filter.getPitch());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, filter.getRoll());
}

void test_filter_convergence(void) {
    ComplementaryFilter filter(0.98f);

    // Start flat
    float accelFlat[3] = {0.0f, 0.0f, 1.0f};
    float gyro[3] = {0.0f, 0.0f, 0.0f};
    filter.update(accelFlat, gyro, 0.01f);

    // Now apply a tilted accel for many iterations (step input)
    // Tilted ~45° forward: accel ≈ (-0.707, 0, 0.707)
    float accelTilted[3] = {-0.707f, 0.0f, 0.707f};
    for (int i = 0; i < 500; i++) {
        filter.update(accelTilted, gyro, 0.01f);
    }

    // Should converge close to 45°
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 45.0f, filter.getPitch());
}

void test_filter_reset(void) {
    ComplementaryFilter filter(0.98f);
    float accel[3] = {-1.0f, 0.0f, 0.0f};
    float gyro[3]  = {0.0f, 0.0f, 0.0f};
    filter.update(accel, gyro, 0.01f);
    TEST_ASSERT_TRUE(fabsf(filter.getPitch()) > 10.0f);

    filter.reset();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, filter.getPitch());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, filter.getRoll());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // imu_math
    RUN_TEST(test_pitch_flat);
    RUN_TEST(test_pitch_90_forward);
    RUN_TEST(test_pitch_90_backward);
    RUN_TEST(test_roll_flat);
    RUN_TEST(test_roll_90_right);
    RUN_TEST(test_roll_90_left);
    RUN_TEST(test_angular_velocity_zero);
    RUN_TEST(test_angular_velocity_single_axis);
    RUN_TEST(test_angular_velocity_multi_axis);
    RUN_TEST(test_centidegrees_conversion);
    RUN_TEST(test_centidegrees_negative);
    RUN_TEST(test_centidegrees_roundtrip);
    RUN_TEST(test_centidegrees_clamping);

    // ComplementaryFilter
    RUN_TEST(test_filter_initial_from_accel);
    RUN_TEST(test_filter_convergence);
    RUN_TEST(test_filter_reset);

    return UNITY_END();
}
