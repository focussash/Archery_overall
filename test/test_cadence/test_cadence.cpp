#include <unity.h>
#include "cadence.h"

void test_no_baseline_before_min_shots() {
    CadenceTracker tracker(5, 0.30f);
    ShotTiming t = {500, 1000, 1500};

    for (int i = 0; i < 4; i++) {
        CadenceResult r = tracker.recordShot(t);
        TEST_ASSERT_FALSE(r.has_baseline);
        TEST_ASSERT_FALSE(r.rushing);
        TEST_ASSERT_FALSE(r.holding_too_long);
    }
    TEST_ASSERT_EQUAL(4, tracker.getShotCount());
    TEST_ASSERT_FALSE(tracker.hasBaseline());
}

void test_baseline_after_min_shots() {
    CadenceTracker tracker(5, 0.30f);
    ShotTiming t = {500, 1000, 1500};

    // First 5 shots build baseline
    for (int i = 0; i < 5; i++) {
        tracker.recordShot(t);
    }
    TEST_ASSERT_TRUE(tracker.hasBaseline());

    // 6th shot with same timing — no deviation
    CadenceResult r = tracker.recordShot(t);
    TEST_ASSERT_TRUE(r.has_baseline);
    TEST_ASSERT_FALSE(r.rushing);
    TEST_ASSERT_FALSE(r.holding_too_long);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.total_deviation);
}

void test_rushing_detected() {
    CadenceTracker tracker(3, 0.30f);
    ShotTiming normal = {500, 1000, 1500};

    for (int i = 0; i < 3; i++) {
        tracker.recordShot(normal);
    }

    // Shot that is 50% faster than average (total 750 vs avg 1500)
    ShotTiming fast = {250, 500, 750};
    CadenceResult r = tracker.recordShot(fast);
    TEST_ASSERT_TRUE(r.has_baseline);
    TEST_ASSERT_TRUE(r.rushing);
    TEST_ASSERT_FALSE(r.holding_too_long);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.50f, r.total_deviation);
}

void test_holding_too_long_detected() {
    CadenceTracker tracker(3, 0.30f);
    ShotTiming normal = {500, 1000, 1500};

    for (int i = 0; i < 3; i++) {
        tracker.recordShot(normal);
    }

    // Shot with hold 50% longer (hold 1500 vs avg 1000)
    ShotTiming slow_hold = {500, 1500, 2000};
    CadenceResult r = tracker.recordShot(slow_hold);
    TEST_ASSERT_TRUE(r.has_baseline);
    TEST_ASSERT_FALSE(r.rushing);
    TEST_ASSERT_TRUE(r.holding_too_long);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.50f, r.hold_deviation);
}

void test_no_flag_within_threshold() {
    CadenceTracker tracker(3, 0.30f);
    ShotTiming normal = {500, 1000, 1500};

    for (int i = 0; i < 3; i++) {
        tracker.recordShot(normal);
    }

    // 20% faster — within 30% threshold
    ShotTiming slightly_fast = {400, 800, 1200};
    CadenceResult r = tracker.recordShot(slightly_fast);
    TEST_ASSERT_TRUE(r.has_baseline);
    TEST_ASSERT_FALSE(r.rushing);
    TEST_ASSERT_FALSE(r.holding_too_long);
}

void test_snap_shot_zero_timing_skipped() {
    CadenceTracker tracker(3, 0.30f);
    ShotTiming normal = {500, 1000, 1500};
    ShotTiming snap = {0, 0, 0};

    for (int i = 0; i < 3; i++) {
        tracker.recordShot(normal);
    }

    // Snap shot doesn't affect averages or count
    CadenceResult r = tracker.recordShot(snap);
    TEST_ASSERT_FALSE(r.rushing);
    TEST_ASSERT_FALSE(r.holding_too_long);
    // Count should still be 3 (snap not stored)
    TEST_ASSERT_EQUAL(3, tracker.getShotCount());
}

void test_reset_clears_history() {
    CadenceTracker tracker(3, 0.30f);
    ShotTiming t = {500, 1000, 1500};

    for (int i = 0; i < 5; i++) {
        tracker.recordShot(t);
    }
    TEST_ASSERT_TRUE(tracker.hasBaseline());

    tracker.reset();
    TEST_ASSERT_EQUAL(0, tracker.getShotCount());
    TEST_ASSERT_FALSE(tracker.hasBaseline());
}

void test_averages_correct() {
    CadenceTracker tracker(3, 0.30f);
    tracker.recordShot({400, 800, 1200});
    tracker.recordShot({500, 1000, 1500});
    tracker.recordShot({600, 1200, 1800});

    // avg: draw=500, hold=1000, total=1500
    ShotTiming same_as_avg = {500, 1000, 1500};
    CadenceResult r = tracker.recordShot(same_as_avg);
    TEST_ASSERT_EQUAL_UINT32(500, r.avg_draw_ms);
    TEST_ASSERT_EQUAL_UINT32(1000, r.avg_hold_ms);
    TEST_ASSERT_EQUAL_UINT32(1500, r.avg_total_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.total_deviation);
}

void test_circular_buffer_wraps() {
    CadenceTracker tracker(3, 0.30f);

    // Fill beyond MAX_HISTORY (16) with consistent timing
    ShotTiming t = {500, 1000, 1500};
    for (int i = 0; i < 20; i++) {
        tracker.recordShot(t);
    }

    // Should still have correct averages after wraparound
    CadenceResult r = tracker.recordShot(t);
    TEST_ASSERT_TRUE(r.has_baseline);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.total_deviation);
    TEST_ASSERT_EQUAL_UINT32(1500, r.avg_total_ms);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_no_baseline_before_min_shots);
    RUN_TEST(test_baseline_after_min_shots);
    RUN_TEST(test_rushing_detected);
    RUN_TEST(test_holding_too_long_detected);
    RUN_TEST(test_no_flag_within_threshold);
    RUN_TEST(test_snap_shot_zero_timing_skipped);
    RUN_TEST(test_reset_clears_history);
    RUN_TEST(test_averages_correct);
    RUN_TEST(test_circular_buffer_wraps);
    return UNITY_END();
}
