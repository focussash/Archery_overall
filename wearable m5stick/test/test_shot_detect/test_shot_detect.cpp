#include <unity.h>
#include "shot_detect.h"

static ShotDetector detector;

void setUp(void) {
    detector.reset();
    ShotDetectorConfig cfg;
    cfg.accel_threshold = 2.5f;
    cfg.draw_pitch_rate = 15.0f;
    cfg.hold_stability = 10.0f;
    cfg.debounce_ms = 500;
    cfg.hold_timeout_ms = 10000;
    cfg.draw_timeout_ms = 5000;
    detector.setConfig(cfg);
}

void tearDown(void) {}

static void feedCalm(uint32_t start_ms, uint32_t duration_ms) {
    float accel[3] = {0.0f, 0.0f, 1.0f};
    float gyro[3]  = {0.0f, 0.0f, 0.0f};
    for (uint32_t t = start_ms; t < start_ms + duration_ms; t += 20) {
        detector.update(accel, gyro, 0.0f, 0.0f, t);
    }
}

static void feedSpike(uint32_t timestamp_ms) {
    float accel[3] = {0.0f, 0.0f, 5.0f};  // 5g spike
    float gyro[3]  = {0.0f, 0.0f, 0.0f};
    detector.update(accel, gyro, 10.0f, 2.0f, timestamp_ms);
}

void test_initial_state_is_idle(void) {
    TEST_ASSERT_EQUAL(ShotState::IDLE, detector.getState());
    TEST_ASSERT_FALSE(detector.shotDetected());
}

void test_spike_from_idle_detects_shot(void) {
    feedCalm(0, 100);
    feedSpike(100);
    TEST_ASSERT_TRUE(detector.shotDetected());
}

void test_gentle_movement_no_false_positive(void) {
    float accel[3] = {0.1f, 0.1f, 1.0f};  // gentle
    float gyro[3]  = {5.0f, 5.0f, 5.0f};
    for (uint32_t t = 0; t < 2000; t += 20) {
        detector.update(accel, gyro, 10.0f, 2.0f, t);
        TEST_ASSERT_FALSE(detector.shotDetected());
    }
}

void test_debounce_prevents_double_detection(void) {
    feedCalm(0, 100);
    feedSpike(100);
    TEST_ASSERT_TRUE(detector.shotDetected());

    // Second spike 200ms later (within debounce)
    feedSpike(300);
    TEST_ASSERT_FALSE(detector.shotDetected());
}

void test_second_shot_after_debounce(void) {
    feedCalm(0, 100);
    feedSpike(100);
    TEST_ASSERT_TRUE(detector.shotDetected());

    // Wait past debounce
    feedCalm(120, 600);

    // Second shot should be detected
    feedSpike(720);
    TEST_ASSERT_TRUE(detector.shotDetected());
}

void test_draw_to_hold_to_shot(void) {
    // Idle
    TEST_ASSERT_EQUAL(ShotState::IDLE, detector.getState());

    // Draw: high pitch rate
    float accelCalm[3] = {0.0f, 0.0f, 1.0f};
    float gyroDrawing[3] = {0.0f, 30.0f, 0.0f};  // pitch rate > threshold
    detector.update(accelCalm, gyroDrawing, 10.0f, 0.0f, 100);
    TEST_ASSERT_EQUAL(ShotState::DRAW_DETECTED, detector.getState());

    // Hold: stabilize
    float gyroStill[3] = {1.0f, 1.0f, 1.0f};
    for (uint32_t t = 120; t < 500; t += 20) {
        detector.update(accelCalm, gyroStill, 30.0f, 1.0f, t);
    }
    TEST_ASSERT_EQUAL(ShotState::HOLD, detector.getState());

    // Shot!
    feedSpike(500);
    TEST_ASSERT_TRUE(detector.shotDetected());
}

void test_shot_snapshot_has_correct_data(void) {
    feedCalm(0, 100);

    float accel[3] = {0.0f, 0.0f, 5.0f};
    float gyro[3]  = {10.0f, 20.0f, 30.0f};
    detector.update(accel, gyro, 45.0f, -3.0f, 1000);

    TEST_ASSERT_TRUE(detector.shotDetected());
    const ShotSnapshot& snap = detector.getLastShot();
    TEST_ASSERT_EQUAL_UINT32(1000, snap.timestamp_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.0f, snap.pitch);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -3.0f, snap.roll);
}

void test_recovery_transitions_to_idle(void) {
    feedCalm(0, 100);
    feedSpike(100);
    TEST_ASSERT_EQUAL(ShotState::RECOVERY, detector.getState());

    feedCalm(120, 600);
    TEST_ASSERT_EQUAL(ShotState::IDLE, detector.getState());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_initial_state_is_idle);
    RUN_TEST(test_spike_from_idle_detects_shot);
    RUN_TEST(test_gentle_movement_no_false_positive);
    RUN_TEST(test_debounce_prevents_double_detection);
    RUN_TEST(test_second_shot_after_debounce);
    RUN_TEST(test_draw_to_hold_to_shot);
    RUN_TEST(test_shot_snapshot_has_correct_data);
    RUN_TEST(test_recovery_transitions_to_idle);

    return UNITY_END();
}
