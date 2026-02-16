#include "ble_relay.h"
#include <NimBLEDevice.h>

// ── Static state ──

static NimBLEServer* s_server = nullptr;
static NimBLECharacteristic* s_shot_char = nullptr;
static NimBLECharacteristic* s_session_ctrl_char = nullptr;
static NimBLECharacteristic* s_status_char = nullptr;
static NimBLECharacteristic* s_config_char = nullptr;

static volatile bool s_connected = false;
static volatile bool s_drain_pending = false;  // Trigger drain when client subscribes to shots
static bool s_initialized = false;

// Pending commands from phone
static volatile bool s_cmd_session_start = false;
static volatile bool s_cmd_session_stop = false;
static volatile bool s_cmd_config_updated = false;
static BleConfigPacket s_received_config = {};

// ── Server callbacks ──

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        s_connected = true;
        Serial.printf("BLE: Client connected (addr: %s)\n",
            connInfo.getAddress().toString().c_str());
        NimBLEDevice::startAdvertising();
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        s_connected = false;
        Serial.printf("BLE: Client disconnected (reason: %d)\n", reason);
        NimBLEDevice::startAdvertising();
    }
};

// ── Session Control write callback ──

class SessionCtrlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        const uint8_t* data = pChar->getValue().data();
        size_t len = pChar->getValue().length();
        if (len >= 1) {
            if (data[0] == BLE_CMD_SESSION_START) {
                s_cmd_session_start = true;
                Serial.println("BLE: Session START command received");
            } else if (data[0] == BLE_CMD_SESSION_STOP) {
                s_cmd_session_stop = true;
                Serial.println("BLE: Session STOP command received");
            }
        }
    }
};

// ── Config write callback ──

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        const uint8_t* data = pChar->getValue().data();
        size_t len = pChar->getValue().length();
        if (len == sizeof(BleConfigPacket)) {
            memcpy(&s_received_config, data, sizeof(BleConfigPacket));
            s_cmd_config_updated = true;
            Serial.printf("BLE: Config received C:%.1f/%.1f E:%.1f/%.1f D:%.1f/%.1f\n",
                s_received_config.cant_baseline, s_received_config.cant_tolerance,
                s_received_config.elev_baseline, s_received_config.elev_tolerance,
                s_received_config.draw_baseline, s_received_config.draw_tolerance);
        }
    }
};

// ── Shot Data subscribe callback (triggers buffer drain) ──

class ShotDataCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        if (subValue == 1 || subValue == 2) {  // notifications or indications enabled
            s_drain_pending = true;
            Serial.println("BLE: Client subscribed to shot notifications, drain pending");
        }
    }
};

// Static callback instances
static ServerCallbacks s_server_cb;
static ShotDataCallbacks s_shot_data_cb;
static SessionCtrlCallbacks s_session_ctrl_cb;
static ConfigCallbacks s_config_cb;

// ── Public API ──

namespace ble_relay {

bool init(const char* device_name) {
    Serial.println("BLE: Starting NimBLE init...");

    if (!NimBLEDevice::init(device_name)) {
        Serial.println("BLE: NimBLEDevice::init() FAILED!");
        return false;
    }
    Serial.println("BLE: NimBLEDevice::init() OK");

    // Set transmit power
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Create server
    s_server = NimBLEDevice::createServer();
    if (!s_server) {
        Serial.println("BLE: createServer() FAILED!");
        return false;
    }
    s_server->setCallbacks(&s_server_cb);

    // Create service
    NimBLEService* service = s_server->createService(BLE_SERVICE_UUID);
    if (!service) {
        Serial.println("BLE: createService() FAILED!");
        return false;
    }

    // Shot Data characteristic (notify only, 20 bytes)
    s_shot_char = service->createCharacteristic(
        BLE_SHOT_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );
    s_shot_char->setCallbacks(&s_shot_data_cb);

    // Session Control characteristic (write, 1 byte)
    s_session_ctrl_char = service->createCharacteristic(
        BLE_SESSION_CTRL_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    s_session_ctrl_char->setCallbacks(&s_session_ctrl_cb);

    // Device Status characteristic (read + notify, 5 bytes)
    s_status_char = service->createCharacteristic(
        BLE_DEVICE_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Config characteristic (read + write, 24 bytes)
    s_config_char = service->createCharacteristic(
        BLE_CONFIG_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    s_config_char->setCallbacks(&s_config_cb);

    // Start service
    if (!service->start()) {
        Serial.println("BLE: service->start() FAILED!");
        return false;
    }
    Serial.println("BLE: Service started OK");

    // Configure advertising
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setMinInterval(0x20);   // 20ms min advertising interval
    adv->setMaxInterval(0x40);   // 40ms max advertising interval

    // Build advertising data: flags + short name (fits in 31 bytes)
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(device_name);

    // Put the 128-bit service UUID in scan response (separate 31-byte packet)
    NimBLEAdvertisementData scanRsp;
    scanRsp.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));

    adv->setAdvertisementData(advData);
    adv->setScanResponseData(scanRsp);

    Serial.printf("BLE: Adv data %d bytes, scan rsp %d bytes\n",
        advData.getPayload().size(), scanRsp.getPayload().size());

    if (!adv->start()) {
        Serial.println("BLE: advertising start() FAILED!");
        return false;
    }

    Serial.printf("BLE: Initialized as \"%s\", advertising OK\n", device_name);
    s_initialized = true;
    return true;
}

void update(ShotBuffer<64>* buffer) {
    // Drain buffered shots when client subscribes to shot notifications
    if (s_drain_pending && buffer != nullptr) {
        s_drain_pending = false;
        uint8_t count = buffer->getCount();
        if (count > 0) {
            Serial.printf("BLE: Draining %d buffered shots\n", count);
            ShotPacket_t pkt;
            while (buffer->pop(&pkt)) {
                sendShot(&pkt);
                delay(20);  // Small delay to avoid overwhelming BLE stack
            }
            Serial.println("BLE: Buffer drain complete");
        }
    }
}

bool isInitialized() {
    return s_initialized;
}

bool isConnected() {
    return s_connected;
}

bool sendShot(const ShotPacket_t* packet) {
    if (!s_connected || s_shot_char == nullptr) return false;

    s_shot_char->setValue(reinterpret_cast<const uint8_t*>(packet), sizeof(ShotPacket_t));
    s_shot_char->notify();
    return true;
}

void updateStatus(const BleDeviceStatus& status) {
    if (s_status_char == nullptr) return;

    s_status_char->setValue(reinterpret_cast<const uint8_t*>(&status), sizeof(BleDeviceStatus));
    if (s_connected) {
        s_status_char->notify();
    }
}

void updateConfig(const ConfigStore* config) {
    if (s_config_char == nullptr || config == nullptr) return;

    BleConfigPacket pkt;
    pkt.cant_baseline = config->getCantBaseline();
    pkt.cant_tolerance = config->getCantTolerance();
    pkt.elev_baseline = config->getElevationBaseline();
    pkt.elev_tolerance = config->getElevationTolerance();
    pkt.draw_baseline = config->getDrawBaseline();
    pkt.draw_tolerance = config->getDrawTolerance();

    s_config_char->setValue(reinterpret_cast<const uint8_t*>(&pkt), sizeof(BleConfigPacket));
}

BleCommand getCommand() {
    BleCommand cmd;
    cmd.session_start = s_cmd_session_start;
    cmd.session_stop = s_cmd_session_stop;
    cmd.config_updated = s_cmd_config_updated;
    s_cmd_session_start = false;
    s_cmd_session_stop = false;
    s_cmd_config_updated = false;
    return cmd;
}

void applyReceivedConfig(ConfigStore* config) {
    config->setCantBaseline(s_received_config.cant_baseline);
    config->setCantTolerance(s_received_config.cant_tolerance);
    config->setElevationBaseline(s_received_config.elev_baseline);
    config->setElevationTolerance(s_received_config.elev_tolerance);
    config->setDrawBaseline(s_received_config.draw_baseline);
    config->setDrawTolerance(s_received_config.draw_tolerance);
    config->save();
}

} // namespace ble_relay
