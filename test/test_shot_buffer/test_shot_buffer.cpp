#include <unity.h>
#include "shot_buffer.h"
#include <cstring>

static ShotBuffer<4> buf;  // Small buffer for testing wraparound

void setUp(void) {
    buf.clear();
}

void tearDown(void) {}

static ShotPacket_t makePacket(uint8_t shot_num, int16_t cant, int16_t elev) {
    ShotPacket_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.shot_number = shot_num;
    pkt.cant_angle = cant;
    pkt.elevation_angle = elev;
    pkt.heading = (int16_t)0xFFFF;
    return pkt;
}

void test_sizeof_shot_packet(void) {
    TEST_ASSERT_EQUAL_UINT32(20, sizeof(ShotPacket_t));
}

void test_empty_buffer(void) {
    TEST_ASSERT_TRUE(buf.isEmpty());
    TEST_ASSERT_FALSE(buf.isFull());
    TEST_ASSERT_EQUAL_UINT8(0, buf.getCount());
}

void test_pop_empty_returns_false(void) {
    ShotPacket_t out;
    TEST_ASSERT_FALSE(buf.pop(&out));
}

void test_push_and_pop_fifo(void) {
    buf.push(makePacket(1, 100, 200));
    buf.push(makePacket(2, 300, 400));

    TEST_ASSERT_EQUAL_UINT8(2, buf.getCount());

    ShotPacket_t out;
    TEST_ASSERT_TRUE(buf.pop(&out));
    TEST_ASSERT_EQUAL_UINT8(1, out.shot_number);
    TEST_ASSERT_EQUAL_INT16(100, out.cant_angle);

    TEST_ASSERT_TRUE(buf.pop(&out));
    TEST_ASSERT_EQUAL_UINT8(2, out.shot_number);
    TEST_ASSERT_EQUAL_INT16(300, out.cant_angle);

    TEST_ASSERT_TRUE(buf.isEmpty());
}

void test_wraparound_overwrites_oldest(void) {
    // Fill buffer (capacity=4)
    buf.push(makePacket(1, 10, 10));
    buf.push(makePacket(2, 20, 20));
    buf.push(makePacket(3, 30, 30));
    buf.push(makePacket(4, 40, 40));
    TEST_ASSERT_TRUE(buf.isFull());

    // Push one more — oldest (1) should be lost
    buf.push(makePacket(5, 50, 50));
    TEST_ASSERT_EQUAL_UINT8(4, buf.getCount());

    ShotPacket_t out;
    TEST_ASSERT_TRUE(buf.pop(&out));
    TEST_ASSERT_EQUAL_UINT8(2, out.shot_number);  // 1 was overwritten
}

void test_peek_does_not_remove(void) {
    buf.push(makePacket(1, 100, 200));
    buf.push(makePacket(2, 300, 400));

    ShotPacket_t out;
    TEST_ASSERT_TRUE(buf.peek(&out, 0));
    TEST_ASSERT_EQUAL_UINT8(1, out.shot_number);

    TEST_ASSERT_TRUE(buf.peek(&out, 1));
    TEST_ASSERT_EQUAL_UINT8(2, out.shot_number);

    TEST_ASSERT_EQUAL_UINT8(2, buf.getCount());  // Still 2
}

void test_peek_out_of_range(void) {
    buf.push(makePacket(1, 100, 200));
    ShotPacket_t out;
    TEST_ASSERT_FALSE(buf.peek(&out, 1));  // Only index 0 valid
    TEST_ASSERT_FALSE(buf.peek(&out, 5));
}

void test_clear_resets(void) {
    buf.push(makePacket(1, 100, 200));
    buf.push(makePacket(2, 300, 400));
    buf.clear();
    TEST_ASSERT_TRUE(buf.isEmpty());
    TEST_ASSERT_EQUAL_UINT8(0, buf.getCount());
}

void test_data_integrity(void) {
    ShotPacket_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.timestamp_ms = 12345678;
    pkt.cant_angle = -567;
    pkt.elevation_angle = 4523;
    pkt.draw_angle = -1234;
    pkt.draw_velocity = 9876;
    pkt.shot_number = 42;
    pkt.form_flags = FORM_FLAG_CANT_OK | FORM_FLAG_DRAW_OK;
    pkt.heading = -1;
    pkt.reserved = 0;

    buf.push(pkt);

    ShotPacket_t out;
    buf.pop(&out);

    TEST_ASSERT_EQUAL_UINT32(12345678, out.timestamp_ms);
    TEST_ASSERT_EQUAL_INT16(-567, out.cant_angle);
    TEST_ASSERT_EQUAL_INT16(4523, out.elevation_angle);
    TEST_ASSERT_EQUAL_INT16(-1234, out.draw_angle);
    TEST_ASSERT_EQUAL_INT16(9876, out.draw_velocity);
    TEST_ASSERT_EQUAL_UINT8(42, out.shot_number);
    TEST_ASSERT_EQUAL_UINT8(FORM_FLAG_CANT_OK | FORM_FLAG_DRAW_OK, out.form_flags);
    TEST_ASSERT_EQUAL_INT16(-1, out.heading);
}

void test_capacity(void) {
    TEST_ASSERT_EQUAL_UINT8(4, buf.capacity());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_sizeof_shot_packet);
    RUN_TEST(test_empty_buffer);
    RUN_TEST(test_pop_empty_returns_false);
    RUN_TEST(test_push_and_pop_fifo);
    RUN_TEST(test_wraparound_overwrites_oldest);
    RUN_TEST(test_peek_does_not_remove);
    RUN_TEST(test_peek_out_of_range);
    RUN_TEST(test_clear_resets);
    RUN_TEST(test_data_integrity);
    RUN_TEST(test_capacity);

    return UNITY_END();
}
