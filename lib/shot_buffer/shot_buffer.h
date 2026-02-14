#pragma once
#include <cstdint>
#include <cstring>

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;     // Millis since session start
    int16_t  cant_angle;       // Roll in 0.01 degree units
    int16_t  elevation_angle;  // Pitch in 0.01 degree units
    int16_t  draw_angle;       // Draw arm angle in 0.01 degree units
    int16_t  draw_velocity;    // Peak angular velocity
    uint8_t  shot_number;      // Shot index within session
    uint8_t  form_flags;       // Bitfield: cant_ok, elevation_ok, draw_ok, fatigue_flag
    int16_t  heading;          // Compass heading in 0.01 degrees (0xFFFF = no mag)
    uint16_t reserved;         // Future use
} ShotPacket_t;

// Form flag bits
#define FORM_FLAG_CANT_OK       (1 << 0)
#define FORM_FLAG_ELEVATION_OK  (1 << 1)
#define FORM_FLAG_DRAW_OK       (1 << 2)
#define FORM_FLAG_FATIGUE       (1 << 3)

template<uint8_t CAPACITY = 64>
class ShotBuffer {
public:
    ShotBuffer() : head_(0), tail_(0), count_(0) {}

    bool push(const ShotPacket_t& shot) {
        buffer_[head_] = shot;
        head_ = (head_ + 1) % CAPACITY;
        if (count_ < CAPACITY) {
            count_++;
        } else {
            // Overwrite oldest: advance tail
            tail_ = (tail_ + 1) % CAPACITY;
        }
        return true;
    }

    bool pop(ShotPacket_t* out) {
        if (count_ == 0) return false;
        *out = buffer_[tail_];
        tail_ = (tail_ + 1) % CAPACITY;
        count_--;
        return true;
    }

    bool peek(ShotPacket_t* out, uint8_t index) const {
        if (index >= count_) return false;
        uint8_t actual = (tail_ + index) % CAPACITY;
        *out = buffer_[actual];
        return true;
    }

    uint8_t getCount() const { return count_; }
    bool isEmpty() const { return count_ == 0; }
    bool isFull() const { return count_ >= CAPACITY; }
    uint8_t capacity() const { return CAPACITY; }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

private:
    ShotPacket_t buffer_[CAPACITY];
    uint8_t head_;
    uint8_t tail_;
    uint8_t count_;
};
