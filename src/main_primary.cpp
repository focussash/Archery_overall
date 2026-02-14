#ifdef ROLE_PRIMARY

#include <Arduino.h>
#include <hal_m5unified.h>
#include <complementary_filter.h>
#include <shot_detect.h>
#include <feedback.h>
#include <shot_buffer.h>
#include <session.h>

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
    hal.displayPrint("PRIMARY - SP1-6 Test");

    Serial.println("=== Archery Wearable - PRIMARY ===");
    Serial.println("SP6: Session + Form Evaluation");
    Serial.println("BtnA hold = start/stop session");
    Serial.println("BtnA tap  = cycle beep patterns");
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

    imu_data_t imu;
    if (!hal.readIMU(&imu)) return;

    // Complementary filter
    float accel[3] = {imu.accel.x, imu.accel.y, imu.accel.z};
    float gyro[3] = {imu.gyro.x, imu.gyro.y, imu.gyro.z};
    filter.update(accel, gyro, dt);
    float pitch = filter.getPitch();
    float roll = filter.getRoll();

    // Shot detection
    shotDetector.update(accel, gyro, pitch, roll, now);

    if (shotDetector.shotDetected()) {
        const ShotSnapshot& snap = shotDetector.getLastShot();

        if (session->getState() == SessionState::ACTIVE) {
            // Record shot with form evaluation
            FormEvaluation eval = session->recordShot(snap, 0.0f, session->getStartTime());
            Serial.printf(">>> SHOT #%d | P:%.1f R:%.1f | Cant:%s Elev:%s Draw:%s\n",
                session->getShotCount(), snap.pitch, snap.roll,
                eval.cant_ok ? "OK" : "EXCEEDED",
                eval.elevation_ok ? "OK" : "EXCEEDED",
                eval.draw_ok ? "OK" : "EXCEEDED");

            // Play appropriate feedback
            if (eval.cant_ok && eval.elevation_ok && eval.draw_ok) {
                feedback->play(PatternID::GOOD_FORM);
            } else if (!eval.cant_ok) {
                feedback->play(PatternID::CANT_EXCEEDED);
            } else {
                feedback->play(PatternID::DRAW_INCONSISTENT);
            }
        } else {
            Serial.printf(">>> SHOT (no session) P:%.1f R:%.1f Accel:%.2fg\n",
                snap.pitch, snap.roll, snap.accel_magnitude);
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

    // Button B = print current thresholds (debug)
    if (hal.getButtonB()) {
        Serial.printf("Thresholds: Cant=%.1f Elev=%.1f Draw=%.1f | Buffer: %d shots\n",
            configStore.getCantThreshold(),
            configStore.getElevationThreshold(),
            configStore.getDrawAngleThreshold(),
            shotBuffer.getCount());
    }

    // Display update at 10Hz
    if (now - lastPrintTime >= 100) {
        lastPrintTime = now;

        // Serial: pitch/roll + state
        Serial.printf("P:%.1f R:%.1f | State:%d | Session:%d Shots:%d\n",
            pitch, roll,
            (int)shotDetector.getState(),
            (int)session->getState(),
            session->getShotCount());

        // Display
        hal.displayFillRect(0, 75, hal.getDisplayWidth(), 60, 0);

        // Pitch/Roll
        hal.displaySetTextSize(2);
        hal.displaySetCursor(5, 78);
        hal.displaySetTextColor(0x07FF);  // Cyan
        hal.displayPrintf("P:%5.1f", pitch);
        hal.displaySetCursor(5, 98);
        hal.displaySetTextColor(0xFFE0);  // Yellow
        hal.displayPrintf("R:%5.1f", roll);

        // Status bar
        hal.displayFillRect(0, 120, hal.getDisplayWidth(), 15, 0);
        hal.displaySetTextSize(1);
        hal.displaySetCursor(5, 122);
        if (session->getState() == SessionState::ACTIVE) {
            hal.displaySetTextColor(0x07E0);  // Green
            uint32_t elapsed = session->getElapsedMs(now) / 1000;
            hal.displayPrintf("REC #%d %lus Batt:%d%%",
                session->getShotCount(), elapsed, hal.getBatteryLevel());
        } else {
            hal.displaySetTextColor(0xFFFF);
            hal.displayPrintf("IDLE Batt:%d%%", hal.getBatteryLevel());
        }
    }
}

#endif // ROLE_PRIMARY
