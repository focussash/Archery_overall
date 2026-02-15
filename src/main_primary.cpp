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

static uint32_t lastDisplayTime = 0;
static uint32_t lastLoopTime = 0;
static bool splashDone = false;

// Last shot display data
static bool hasLastShot = false;
static float lastShotCant = 0;
static float lastShotElev = 0;
static float lastShotDraw = 0;
static bool lastShotCantOk = false;
static bool lastShotElevOk = false;
static bool lastShotDrawOk = false;
static uint8_t lastShotNum = 0;

void setup() {
    Serial.begin(115200);
    hal.init();
    feedback = new FeedbackEngine(&hal);
    session = new SessionManager(&shotBuffer, &configStore);

    configStore.load();

    if (!espnow_comm::initPrimary(1)) {
        Serial.println("ESP-NOW init FAILED!");
    } else {
        Serial.println("ESP-NOW init OK (listening + broadcast)");
    }

    // Boot splash
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
    hal.displayPrint("PRIMARY - MVP");

    Serial.println("=== Archery Wearable - PRIMARY ===");
    Serial.println("SP8: Integrated Shot Pipeline (MVP)");
    Serial.println("BtnA hold = start/stop session");
    Serial.printf("Thresholds: Cant=%.1f Elev=%.1f Draw=%.1f\n",
        configStore.getCantThreshold(),
        configStore.getElevationThreshold(),
        configStore.getDrawAngleThreshold());
    Serial.println("===================================");

    lastLoopTime = hal.millis();
}

// Redraw the full live UI (called after splash clears)
static void drawFullUI() {
    hal.displayClear();
}

void loop() {
    hal.update();
    feedback->update();

    uint32_t now = hal.millis();
    float dt = (now - lastLoopTime) / 1000.0f;
    if (dt <= 0.0f) dt = 0.001f;
    lastLoopTime = now;

    // Clear boot splash after 2 seconds
    if (!splashDone && now > 2000) {
        splashDone = true;
        drawFullUI();
    }

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

            // Update last shot display data
            hasLastShot = true;
            lastShotNum = session->getShotCount();
            lastShotCant = snap.roll;
            lastShotElev = snap.pitch;
            lastShotDraw = drawAngle;
            lastShotCantOk = eval.cant_ok;
            lastShotElevOk = eval.elevation_ok;
            lastShotDrawOk = eval.draw_ok;

            Serial.printf(">>> SHOT #%d | P:%.1f R:%.1f Draw:%.1f Vel:%.1f | Cant:%s Elev:%s Draw:%s\n",
                session->getShotCount(), snap.pitch, snap.roll, drawAngle, snap.peak_angular_vel,
                eval.cant_ok ? "OK" : "EXCEEDED",
                eval.elevation_ok ? "OK" : "EXCEEDED",
                eval.draw_ok ? "OK" : "EXCEEDED");

            // Play appropriate feedback pattern
            if (eval.cant_ok && eval.elevation_ok && eval.draw_ok) {
                feedback->play(PatternID::GOOD_FORM);
            } else if (!eval.cant_ok) {
                feedback->play(PatternID::CANT_EXCEEDED);
            } else if (!eval.draw_ok) {
                feedback->play(PatternID::DRAW_INCONSISTENT);
            } else {
                // Elevation only exceeded
                feedback->play(PatternID::SHOT_DETECTED);
            }

            // Notify Secondary about the shot
            espnow_comm::sendShotNotify(session->getShotCount());
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
            Serial.printf("Session STOPPED | Shots: %d | Duration: %lums | Buffer: %d\n",
                session->getShotCount(), session->getElapsedMs(now), shotBuffer.getCount());
        } else {
            session->start(now);
            hasLastShot = false;
            feedback->play(PatternID::SESSION_START);
            Serial.println("Session STARTED");
        }
    }

    // Button B = print debug info
    if (hal.getButtonB()) {
        Serial.printf("Thresholds: Cant=%.1f Elev=%.1f Draw=%.1f | Buffer: %d/%d shots\n",
            configStore.getCantThreshold(),
            configStore.getElevationThreshold(),
            configStore.getDrawAngleThreshold(),
            shotBuffer.getCount(), shotBuffer.capacity());
        Serial.printf("ESP-NOW: RX=%lu %s\n",
            espnow_comm::getPacketCount(),
            secConnected ? "CONNECTED" : "DISCONNECTED");
    }

    // Display + serial at 10Hz (only after splash clears)
    if (splashDone && (now - lastDisplayTime >= 100)) {
        lastDisplayTime = now;

        // === Serial output ===
        Serial.printf("P:%.1f R:%.1f | Sec:%s",
            pitch, roll,
            secConnected ? "OK" : "---");
        if (secConnected) {
            Serial.printf(" P:%.1f R:%.1f B:%d%%",
                secData.pitch, secData.roll, secData.battery);
        }
        Serial.printf(" | St:%d Sess:%d #%d Buf:%d\n",
            (int)shotDetector.getState(),
            (int)session->getState(),
            session->getShotCount(),
            shotBuffer.getCount());

        // === Display layout (135x240 portrait) ===
        // Row 0-9: Secondary status bar
        hal.displayFillRect(0, 0, hal.getDisplayWidth(), 10, 0);
        hal.displaySetTextSize(1);
        hal.displaySetCursor(2, 1);
        if (secConnected) {
            hal.displaySetTextColor(0x07E0);  // Green
            hal.displayPrintf("Sec P:%.0f R:%.0f B:%d%%",
                secData.pitch, secData.roll, secData.battery);
        } else {
            hal.displaySetTextColor(0xF800);  // Red
            hal.displayPrintf("Sec: DISCONNECTED");
        }

        // Row 14-30: Primary pitch
        hal.displayFillRect(0, 14, hal.getDisplayWidth(), 20, 0);
        hal.displaySetTextSize(2);
        hal.displaySetCursor(2, 16);
        hal.displaySetTextColor(0x07FF);  // Cyan
        hal.displayPrintf("P:%6.1f", pitch);

        // Row 36-52: Primary roll
        hal.displayFillRect(0, 36, hal.getDisplayWidth(), 20, 0);
        hal.displaySetTextSize(2);
        hal.displaySetCursor(2, 38);
        hal.displaySetTextColor(0xFFE0);  // Yellow
        hal.displayPrintf("R:%6.1f", roll);

        // Row 60-100: Last shot info
        hal.displayFillRect(0, 58, hal.getDisplayWidth(), 44, 0);
        hal.displaySetTextSize(1);
        if (hasLastShot) {
            // Shot header
            hal.displaySetCursor(2, 60);
            hal.displaySetTextColor(0xFFFF);
            hal.displayPrintf("Shot #%d:", lastShotNum);

            // Cant + Elevation
            hal.displaySetCursor(2, 72);
            hal.displaySetTextColor(lastShotCantOk ? 0x07E0 : 0xF800);
            hal.displayPrintf("C:%5.1f", lastShotCant);
            hal.displaySetCursor(68, 72);
            hal.displaySetTextColor(lastShotElevOk ? 0x07E0 : 0xF800);
            hal.displayPrintf("E:%5.1f", lastShotElev);

            // Draw angle
            hal.displaySetCursor(2, 84);
            hal.displaySetTextColor(lastShotDrawOk ? 0x07E0 : 0xF800);
            hal.displayPrintf("D:%5.1f", lastShotDraw);

            // Form flags summary
            hal.displaySetCursor(68, 84);
            hal.displaySetTextColor(0xFFFF);
            hal.displayPrintf(
                (lastShotCantOk && lastShotElevOk && lastShotDrawOk) ? "GOOD" : "WARN");
        } else {
            hal.displaySetCursor(2, 72);
            hal.displaySetTextColor(0x7BEF);  // Gray
            hal.displayPrintf("No shots yet");
        }

        // Row 106-122: Session status
        hal.displayFillRect(0, 106, hal.getDisplayWidth(), 20, 0);
        hal.displaySetTextSize(2);
        hal.displaySetCursor(2, 108);
        if (session->getState() == SessionState::ACTIVE) {
            hal.displaySetTextColor(0x07E0);  // Green
            uint32_t elapsed = session->getElapsedMs(now) / 1000;
            hal.displayPrintf("#%d %lus", session->getShotCount(), elapsed);
        } else {
            hal.displaySetTextColor(0xFFFF);
            hal.displayPrintf("IDLE");
        }

        // Row 128-135: Bottom bar (buffer + battery)
        hal.displayFillRect(0, 128, hal.getDisplayWidth(), 8, 0);
        hal.displaySetTextSize(1);
        hal.displaySetCursor(2, 128);
        hal.displaySetTextColor(0x7BEF);  // Gray
        hal.displayPrintf("Buf:%d/%d Bat:%d%%",
            shotBuffer.getCount(), shotBuffer.capacity(),
            hal.getBatteryLevel());
    }
}

#endif // ROLE_PRIMARY
