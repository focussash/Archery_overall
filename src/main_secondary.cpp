#ifdef ROLE_SECONDARY

#include <hal_m5unified.h>
#include <cstdio>

static HalM5Unified hal;
static uint32_t lastPrintTime = 0;

void setup() {
    Serial.begin(115200);
    hal.init();

    hal.displayClear();
    hal.displaySetTextSize(2);
    hal.displaySetCursor(10, 10);
    hal.displaySetTextColor(0xF800);  // Red
    hal.displayPrint("Archery");
    hal.displaySetCursor(10, 35);
    hal.displayPrint("Wearable");
    hal.displaySetTextSize(1.5);
    hal.displaySetCursor(10, 65);
    hal.displaySetTextColor(0xFFFF);  // White
    hal.displayPrint("SECONDARY");

    Serial.println("=== Archery Wearable - SECONDARY ===");
}

void loop() {
    hal.update();

    uint32_t now = hal.millis();

    if (now - lastPrintTime >= 100) {
        lastPrintTime = now;

        imu_data_t imu;
        if (hal.readIMU(&imu)) {
            Serial.printf("A: %.2f %.2f %.2f | G: %.2f %.2f %.2f\n",
                imu.accel.x, imu.accel.y, imu.accel.z,
                imu.gyro.x, imu.gyro.y, imu.gyro.z);

            int batt = hal.getBatteryLevel();
            hal.displayFillRect(0, 90, hal.getDisplayWidth(), 20, 0);
            hal.displaySetTextSize(1);
            hal.displaySetCursor(10, 95);
            hal.displaySetTextColor(0xFFFF);
            hal.displayPrintf("Batt: %d%%", batt);
        }
    }
}

#endif // ROLE_SECONDARY
