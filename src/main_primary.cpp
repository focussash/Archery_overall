#ifdef ROLE_PRIMARY

#include <Arduino.h>
#include <hal_m5unified.h>
#include <complementary_filter.h>
#include <shot_detect.h>
#include <feedback.h>
#include <shot_buffer.h>
#include <session.h>
#include <espnow_comm.h>

static HalM5Unified hal;
static ComplementaryFilter filter(0.98f);
static ShotDetector shotDetector;
static FeedbackEngine* feedback = nullptr;
static ShotBuffer<64> shotBuffer;
static ConfigStore configStore;
static SessionManager* session = nullptr;

static uint32_t lastPrintTime = 0;
static uint32_t lastLoopTime = 0;

void setup() {
    Serial.begin(115200);
    hal.init();
    feedback = new FeedbackEngine(&hal);
    session = new SessionManager(&shotBuffer, &configStore);

    // Load thresholds from NVS
    configStore.load();

    // Initialize ESP-NOW as Primary (receiver)
    if (!espnow_comm::initPrimary(1)) {
        Serial.println("ESP-NOW init FAILED!");
    } else {
        Serial.println("ESP-NOW init OK (listening)");
    }

    hal.displayClear();
    hal.displaySetTextSize(2);
    hal.displaySetCursor(10, 10);
    hal.displaySetTextColor(0x07E0);  // Green
    hal.displayPrint("Archery");
    hal.displaySetCursor(10, 35);
    hal.displayPrint("Wearable");
    hal.displaySetTextSize(1);
    hal.displaySetCursor(10, 60);
    hal.displaySetTextColor(0xFFFF);
    hal.displayPrint("PRIMARY - SP7");

    Serial.println("=== Archery Wearable - PRIMARY ===");
    Serial.println("SP7: ESP-NOW Two-Device Communication");
    Serial.println("BtnA hold = start/stop session");
    Serial.printf("Thresholds: Cant=%.1f Elev=%.1f Draw=%.1f\n",
        configStore.getCantThreshold(),
        configStore.getElevationThreshold(),
        configStore.getDrawAngleThreshold());
    Serial.println("===================================");

    lastLoopTime = hal.millis();
}

void loop() {
    hal.update();
    feedback->update();

    uint32_t now = hal.millis();
    float dt = (now - lastLoopTime) / 1000.0f;
    if (dt <= 0.0f) dt = 0.001f;
    lastLoopTime = now;

    // Primary IMU
    imu_data_t imu;
    if (!hal.readIMU(&imu)) return;

    float accel[3] = {imu.accel.x, imu.accel.y, imu.accel.z};
    float gyro[3] = {imu.gyro.x, imu.gyro.y, imu.gyro.z};
    filter.update(accel, gyro, dt);
    float pitch = filter.getPitch();
    float roll = filter.getRoll();

    // Secondary data (from ESP-NOW)
    ESPNowIMUData secData = espnow_comm::getLatestData();
    bool secConnected = espnow_comm::isSecondaryConnected();
    float drawAngle = secConnected ? secData.pitch : 0.0f;

    // Shot detection
    shotDetector.update(accel, gyro, pitch, roll, now);

    if (shotDetector.shotDetected()) {
        const ShotSnapshot& snap = shotDetector.getLastShot();

        if (session->getState() == SessionState::ACTIVE) {
            FormEvaluation eval = session->recordShot(snap, drawAngle, session->getStartTime());
            Serial.printf(">>> SHOT #%d | P:%.1f R:%.1f Draw:%.1f | Cant:%s Elev:%s Draw:%s\n",
                session->getShotCount(), snap.pitch, snap.roll, drawAngle,
                eval.cant_ok ? "OK" : "EXCEEDED",
                eval.elevation_ok ? "OK" : "EXCEEDED",
                eval.draw_ok ? "OK" : "EXCEEDED");

            if (eval.cant_ok && eval.elevation_ok && eval.draw_ok) {
                feedback->play(PatternID::GOOD_FORM);
            } else if (!eval.cant_ok) {
                feedback->play(PatternID::CANT_EXCEEDED);
            } else {
                feedback->play(PatternID::DRAW_INCONSISTENT);
            }
        } else {
            Serial.printf(">>> SHOT (no session) P:%.1f R:%.1f Accel:%.2fg Draw:%.1f\n",
                snap.pitch, snap.roll, snap.accel_magnitude, drawAngle);
            feedback->play(PatternID::SHOT_DETECTED);
        }
    }

    // Button A hold = start/stop session
    if (hal.getButtonAHeld()) {
        if (session->getState() == SessionState::ACTIVE) {
            session->stop(now);
            feedback->play(PatternID::SESSION_END);
            Serial.printf("Session STOPPED | Shots: %d | Duration: %lums\n",
                session->getShotCount(), session->getElapsedMs(now));
        } else {
            session->start(now);
            feedback->play(PatternID::SESSION_START);
            Serial.println("Session STARTED");
        }
    }

    // Button B = print debug info
    if (hal.getButtonB()) {
        Serial.printf("Thresholds: Cant=%.1f Elev=%.1f Draw=%.1f | Buffer: %d shots\n",
            configStore.getCantThreshold(),
            configStore.getElevationThreshold(),
            configStore.getDrawAngleThreshold(),
            shotBuffer.getCount());
        Serial.printf("ESP-NOW: RX=%lu %s\n",
            espnow_comm::getPacketCount(),
            secConnected ? "CONNECTED" : "DISCONNECTED");
    }

    // Display + serial at 10Hz
    if (now - lastPrintTime >= 100) {
        lastPrintTime = now;

        // Serial
        Serial.printf("P:%.1f R:%.1f | Sec:%s",
            pitch, roll,
            secConnected ? "OK" : "---");
        if (secConnected) {
            Serial.printf(" P:%.1f R:%.1f B:%d%%",
                secData.pitch, secData.roll, secData.battery);
        }
        Serial.printf(" | St:%d Sess:%d #%d\n",
            (int)shotDetector.getState(),
            (int)session->getState(),
            session->getShotCount());

        // Display
        hal.displayFillRect(0, 75, hal.getDisplayWidth(), 60, 0);

        // Primary pitch/roll
        hal.displaySetTextSize(2);
        hal.displaySetCursor(5, 78);
        hal.displaySetTextColor(0x07FF);  // Cyan
        hal.displayPrintf("P:%5.1f", pitch);
        hal.displaySetCursor(5, 98);
        hal.displaySetTextColor(0xFFE0);  // Yellow
        hal.displayPrintf("R:%5.1f", roll);

        // Status bar: session + secondary + battery
        hal.displayFillRect(0, 120, hal.getDisplayWidth(), 15, 0);
        hal.displaySetTextSize(1);
        hal.displaySetCursor(5, 122);

        if (session->getState() == SessionState::ACTIVE) {
            hal.displaySetTextColor(0x07E0);  // Green
            uint32_t elapsed = session->getElapsedMs(now) / 1000;
            hal.displayPrintf("REC #%d %lus", session->getShotCount(), elapsed);
        } else {
            hal.displaySetTextColor(0xFFFF);
            hal.displayPrintf("IDLE");
        }

        // Secondary connection status line
        hal.displayFillRect(0, 0, hal.getDisplayWidth(), 10, 0);
        hal.displaySetTextSize(1);
        hal.displaySetCursor(5, 1);
        if (secConnected) {
            hal.displaySetTextColor(0x07E0);  // Green
            hal.displayPrintf("Sec P:%.0f R:%.0f B:%d%%",
                secData.pitch, secData.roll, secData.battery);
        } else {
            hal.displaySetTextColor(0xF800);  // Red
            hal.displayPrintf("Sec: DISCONNECTED");
        }
    }
}

#endif // ROLE_PRIMARY
