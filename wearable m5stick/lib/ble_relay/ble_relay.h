#pragma once
#include <cstdint>
#include "shot_buffer.h"
#include "session.h"

// Custom 128-bit UUID base: 4152Cxxx-0001-1000-8000-00805F9B34FB
// 4152 = "AR" in ASCII, C = archery
#define BLE_SERVICE_UUID        "4152C000-0001-1000-8000-00805F9B34FB"
#define BLE_SHOT_DATA_UUID      "4152C001-0001-1000-8000-00805F9B34FB"
#define BLE_SESSION_CTRL_UUID   "4152C002-0001-1000-8000-00805F9B34FB"
#define BLE_DEVICE_STATUS_UUID  "4152C003-0001-1000-8000-00805F9B34FB"
#define BLE_CONFIG_UUID         "4152C004-0001-1000-8000-00805F9B34FB"

// Session control commands (written by phone to Session Control characteristic)
#define BLE_CMD_SESSION_START   0x01
#define BLE_CMD_SESSION_STOP    0x02

// Device status packet (5 bytes, read/notify)
struct __attribute__((packed)) BleDeviceStatus {
    uint8_t battery;        // 0-100%
    uint8_t imu_ok;         // 1 = IMU working
    uint8_t sec_connected;  // 1 = Secondary connected via ESP-NOW
    uint8_t session_state;  // 0=IDLE, 1=ACTIVE, 2=ENDED
    uint8_t shot_count;     // Current session shot count
};

// Config packet (26 bytes, read/write) — 6 floats + 2 toggle bytes
struct __attribute__((packed)) BleConfigPacket {
    float cant_baseline;
    float cant_tolerance;
    float elev_baseline;
    float elev_tolerance;
    float draw_baseline;
    float draw_tolerance;
    uint8_t form_check_enabled;     // 0 = off, 1 = on
    uint8_t shot_feedback_enabled;  // 0 = off, 1 = on
};

// Pending commands from phone (polled by main loop)
struct BleCommand {
    bool session_start;
    bool session_stop;
    bool config_updated;
};

namespace ble_relay {

// Initialize BLE GATT server. Call after ESP-NOW init.
bool init(const char* device_name = "ArcheryWearable");

// Call in main loop — handles buffer drain on new connection
void update(ShotBuffer<64>* buffer);

// Did BLE init succeed?
bool isInitialized();

// Is a BLE client connected?
bool isConnected();

// Send a shot packet as notification (20 bytes)
bool sendShot(const ShotPacket_t* packet);

// Update the device status characteristic
void updateStatus(const BleDeviceStatus& status);

// Update the config characteristic (so phone can read current values)
void updateConfig(const ConfigStore* config);

// Get and clear pending commands from phone
BleCommand getCommand();

// Apply received config to ConfigStore
void applyReceivedConfig(ConfigStore* config);

} // namespace ble_relay
