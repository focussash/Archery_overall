#include "espnow_comm.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <cstring>

// Broadcast address — no hardcoded MACs needed
static const uint8_t BROADCAST_ADDR[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Receive buffers (written by callback, read by main loop)
static volatile ESPNowIMUData s_latest_data = {};
static volatile ESPNowShotNotify s_shot_notify = {};
static volatile uint32_t s_packet_count = 0;

// ESP-NOW receive callback (ESP-IDF 4.x signature)
static void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
    if (data_len != sizeof(ESPNowPacket)) return;

    const ESPNowPacket* pkt = reinterpret_cast<const ESPNowPacket*>(data);
    if (pkt->magic != ESPNOW_MAGIC) return;

    if (pkt->type == MSG_IMU_DATA) {
        s_latest_data.pitch = pkt->pitch;
        s_latest_data.roll = pkt->roll;
        s_latest_data.battery = pkt->battery;
        s_latest_data.timestamp_ms = pkt->timestamp_ms;
        s_latest_data.received_at_ms = millis();
        s_latest_data.valid = true;
        s_packet_count++;
    } else if (pkt->type == MSG_STATUS) {
        // Shot notification from Primary — shot_number encoded in battery field
        s_shot_notify.shot_number = pkt->battery;
        s_shot_notify.received_at_ms = millis();
        s_shot_notify.pending = true;
    }
}

// Common init: WiFi STA mode + ESP-NOW
static bool initCommon(uint8_t channel) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Fix WiFi channel for ESP-NOW (needed for BLE coexistence later)
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        return false;
    }

    return true;
}

namespace espnow_comm {

bool initPrimary(uint8_t channel) {
    if (!initCommon(channel)) return false;

    // Register receive callback
    esp_now_register_recv_cb(onDataRecv);

    // Add broadcast peer (for sending shot notifies to Secondary)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_ADDR, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        return false;
    }

    return true;
}

bool initSecondary(uint8_t channel) {
    if (!initCommon(channel)) return false;

    // Register receive callback (for shot notifies from Primary)
    esp_now_register_recv_cb(onDataRecv);

    // Add broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_ADDR, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        return false;
    }

    return true;
}

bool sendIMUData(float pitch, float roll, uint8_t battery) {
    ESPNowPacket pkt;
    pkt.type = MSG_IMU_DATA;
    pkt.magic = ESPNOW_MAGIC;
    pkt.timestamp_ms = millis();
    pkt.pitch = pitch;
    pkt.roll = roll;
    pkt.battery = battery;

    esp_err_t result = esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
    if (result == ESP_OK) {
        s_packet_count++;
        return true;
    }
    return false;
}

bool sendShotNotify(uint8_t shot_number) {
    ESPNowPacket pkt;
    pkt.type = MSG_STATUS;
    pkt.magic = ESPNOW_MAGIC;
    pkt.timestamp_ms = millis();
    pkt.pitch = 0;
    pkt.roll = 0;
    pkt.battery = shot_number;  // Encode shot number in battery field

    esp_err_t result = esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
    return (result == ESP_OK);
}

ESPNowIMUData getLatestData() {
    ESPNowIMUData copy;
    copy.pitch = s_latest_data.pitch;
    copy.roll = s_latest_data.roll;
    copy.battery = s_latest_data.battery;
    copy.timestamp_ms = s_latest_data.timestamp_ms;
    copy.received_at_ms = s_latest_data.received_at_ms;
    copy.valid = s_latest_data.valid;
    return copy;
}

bool isSecondaryConnected() {
    if (!s_latest_data.valid) return false;
    return (millis() - s_latest_data.received_at_ms) < ESPNOW_TIMEOUT_MS;
}

ESPNowShotNotify getShotNotify() {
    ESPNowShotNotify copy;
    copy.shot_number = s_shot_notify.shot_number;
    copy.received_at_ms = s_shot_notify.received_at_ms;
    copy.pending = s_shot_notify.pending;
    s_shot_notify.pending = false;  // Clear after read
    return copy;
}

uint32_t getPacketCount() {
    return s_packet_count;
}

} // namespace espnow_comm
