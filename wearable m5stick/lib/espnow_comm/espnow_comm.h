#pragma once
#include <cstdint>

// Magic number "ARCH" for packet validation
#define ESPNOW_MAGIC 0x41524348

// Timeout before considering Secondary disconnected
#define ESPNOW_TIMEOUT_MS 500

enum ESPNowMsgType : uint8_t {
    MSG_IMU_DATA = 0,
    MSG_STATUS,
    MSG_PAIR_REQUEST,   // Reserved for future unicast pairing
    MSG_PAIR_ACK        // Reserved for future unicast pairing
};

// 18 bytes packed — sent over ESP-NOW at 50Hz
struct __attribute__((packed)) ESPNowPacket {
    uint8_t type;           // ESPNowMsgType
    uint32_t magic;         // ESPNOW_MAGIC
    uint32_t timestamp_ms;  // Sender's millis()
    float pitch;            // Computed pitch (degrees)
    float roll;             // Computed roll (degrees)
    uint8_t battery;        // 0-100%
};

// Unpacked receive buffer for main loop consumption
struct ESPNowIMUData {
    float pitch;
    float roll;
    uint8_t battery;
    uint32_t timestamp_ms;      // Sender's timestamp
    uint32_t received_at_ms;    // Local millis() when received
    bool valid;                 // True after first successful receive
};

// Shot notification received from Primary (used by Secondary)
struct ESPNowShotNotify {
    uint8_t shot_number;
    uint32_t received_at_ms;
    bool pending;           // True until consumed by getShotNotify()
};

namespace espnow_comm {

// Initialize as Primary (receiver + can send shot notifies) on given WiFi channel
bool initPrimary(uint8_t channel = 1);

// Initialize as Secondary (sender + can receive shot notifies) on given WiFi channel
bool initSecondary(uint8_t channel = 1);

// Secondary: send current IMU data via broadcast
bool sendIMUData(float pitch, float roll, uint8_t battery);

// Primary: send shot notification to Secondary
bool sendShotNotify(uint8_t shot_number);

// Primary: get latest received data (lock-free copy)
ESPNowIMUData getLatestData();

// Primary: is Secondary connected (received packet within timeout)?
bool isSecondaryConnected();

// Secondary: get shot notification (returns and clears pending flag)
ESPNowShotNotify getShotNotify();

// Get total packets sent (Secondary) or received (Primary)
uint32_t getPacketCount();

} // namespace espnow_comm
