#ifdef ROLE_SECONDARY

#include <Arduino.h>
#include <hal_m5unified.h>
#include <complementary_filter.h>
#include <espnow_comm.h>

static HalM5Unified hal;
static ComplementaryFilter filter(0.98f);

static uint32_t lastSendTime = 0;
static uint32_t lastDisplayTime = 0;
static uint32_t lastLoopTime = 0;

static const uint32_t SEND_INTERVAL_MS = 20;    // 50Hz ESP-NOW send rate
static const uint32_t DISPLAY_INTERVAL_MS = 200; // 5Hz display/serial update

// Shot echo display
static bool shotEchoActive = false;
static uint32_t shotEchoTime = 0;
static uint8_t shotEchoNum = 0;
static const uint32_t SHOT_ECHO_DURATION_MS = 1000;

void setup() {
    Serial.begin(115200);
    hal.init();

    if (!espnow_comm::initSecondary(1)) {
        Serial.println("ESP-NOW init FAILED!");
    } else {
        Serial.println("ESP-NOW init OK (broadcast + listen)");
    }

    hal.displayClear();
    hal.displaySetTextSize(2);
    hal.displaySetCursor(10, 10);
    hal.displaySetTextColor(0xF800);  // Red
    hal.displayPrint("Archery");
    hal.displaySetCursor(10, 35);
    hal.displayPrint("Wearable");
    hal.displaySetTextSize(1);
    hal.displaySetCursor(10, 60);
    hal.displaySetTextColor(0xFFFF);
    hal.displayPrint("SECONDARY - MVP");

    Serial.println("=== Archery Wearable - SECONDARY ===");
    Serial.println("SP8: IMU Streaming + Shot Echo");
    Serial.println("=====================================");

    lastLoopTime = hal.millis();
}

void loop() {
    hal.update();

    uint32_t now = hal.millis();
    float dt = (now - lastLoopTime) / 1000.0f;
    if (dt <= 0.0f) dt = 0.001f;
    lastLoopTime = now;

    // Read IMU and run filter every loop (full speed)
    imu_data_t imu;
    if (!hal.readIMU(&imu)) return;

    float accel[3] = {imu.accel.x, imu.accel.y, imu.accel.z};
    float gyro[3] = {imu.gyro.x, imu.gyro.y, imu.gyro.z};
    filter.update(accel, gyro, dt);

    float pitch = filter.getPitch();
    float roll = filter.getRoll();
    uint8_t battery = (uint8_t)hal.getBatteryLevel();

    // Send at 50Hz
    if (now - lastSendTime >= SEND_INTERVAL_MS) {
        lastSendTime = now;
        espnow_comm::sendIMUData(pitch, roll, battery);
    }

    // Check for shot notification from Primary
    ESPNowShotNotify notify = espnow_comm::getShotNotify();
    if (notify.pending) {
        shotEchoActive = true;
        shotEchoTime = now;
        shotEchoNum = notify.shot_number;
        Serial.printf("<<< Shot #%d echo from Primary\n", notify.shot_number);
    }

    // Clear shot echo after duration
    if (shotEchoActive && (now - shotEchoTime > SHOT_ECHO_DURATION_MS)) {
        shotEchoActive = false;
    }

    // Display + serial at 5Hz
    if (now - lastDisplayTime >= DISPLAY_INTERVAL_MS) {
        lastDisplayTime = now;

        uint32_t txCount = espnow_comm::getPacketCount();

        // Serial
        Serial.printf("P:%.1f R:%.1f | TX:%lu Batt:%d%%\n",
            pitch, roll, txCount, battery);

        // Display
        hal.displayFillRect(0, 78, hal.getDisplayWidth(), 57, 0);

        hal.displaySetTextSize(2);
        hal.displaySetCursor(5, 78);
        hal.displaySetTextColor(0x07FF);  // Cyan
        hal.displayPrintf("P:%5.1f", pitch);
        hal.displaySetCursor(5, 98);
        hal.displaySetTextColor(0xFFE0);  // Yellow
        hal.displayPrintf("R:%5.1f", roll);

        // Status bar or shot echo
        hal.displaySetTextSize(1);
        hal.displaySetCursor(5, 122);
        if (shotEchoActive) {
            hal.displaySetTextColor(0x07E0);  // Green
            hal.displayPrintf("Shot #%d!", shotEchoNum);
        } else {
            hal.displaySetTextColor(0x07E0);  // Green
            hal.displayPrintf("TX:%lu Batt:%d%%", txCount, battery);
        }
    }
}

#endif // ROLE_SECONDARY
