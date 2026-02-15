#ifdef ROLE_PRIMARY

#include <Arduino.h>
#include <hal_m5unified.h>
#include <complementary_filter.h>
#include <shot_detect.h>
#include <feedback.h>
#include <shot_buffer.h>
#include <session.h>
#include <espnow_comm.h>
#include <display_ui.h>
#include <menu.h>

static HalM5Unified hal;
static ComplementaryFilter filter(0.98f);
static ShotDetector shotDetector;
static FeedbackEngine* feedback = nullptr;
static ShotBuffer<64> shotBuffer;
static ConfigStore configStore;
static SessionManager* session = nullptr;
static DisplayUI* displayUI = nullptr;
static Menu* menu = nullptr;

static uint32_t lastDisplayTime = 0;
static uint32_t lastLoopTime = 0;
static bool splashDone = false;

// Display data shared between main loop and display module
static DisplayData dispData = {};

void setup() {
    Serial.begin(115200);
    hal.init();
    feedback = new FeedbackEngine(&hal);
    session = new SessionManager(&shotBuffer, &configStore);
    displayUI = new DisplayUI(&hal);
    menu = new Menu(&hal, &configStore);

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
    hal.displaySetTextColor(0x07E0);
    hal.displayPrint("Archery");
    hal.displaySetCursor(10, 35);
    hal.displayPrint("Wearable");
    hal.displaySetTextSize(1);
    hal.displaySetCursor(10, 60);
    hal.displaySetTextColor(0xFFFF);
    hal.displayPrint("PRIMARY v0.9");

    Serial.println("=== Archery Wearable - PRIMARY ===");
    Serial.println("SP9: Display UI + Menu System");
    Serial.println("BtnA tap=cycle screen, hold=session");
    Serial.println("BtnB tap=menu");
    Serial.printf("Baselines: Cant=%.1f Elev=%.1f Draw=%.1f\n",
        configStore.getCantBaseline(),
        configStore.getElevationBaseline(),
        configStore.getDrawBaseline());
    Serial.printf("Tolerances: Cant=%.1f Elev=%.1f Draw=%.1f\n",
        configStore.getCantTolerance(),
        configStore.getElevationTolerance(),
        configStore.getDrawTolerance());
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

    // Clear boot splash after 2 seconds
    if (!splashDone && now > 2000) {
        splashDone = true;
        hal.displayClear();
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

            // Update display data for last shot
            dispData.has_last_shot = true;
            dispData.last_shot_num = session->getShotCount();
            dispData.last_cant = snap.roll;
            dispData.last_elev = snap.pitch;
            dispData.last_draw = drawAngle;
            dispData.last_cant_ok = eval.cant_ok;
            dispData.last_elev_ok = eval.elevation_ok;
            dispData.last_draw_ok = eval.draw_ok;

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

    // ── Button handling ──

    if (menu->isActive()) {
        // Menu mode: buttons go to menu
        if (hal.getButtonA()) menu->onButtonA();
        if (hal.getButtonB()) menu->onButtonB();
        menu->update(now);

        // Menu exited? Switch display back to HOME
        if (!menu->isActive()) {
            displayUI->setScreen(ScreenID::HOME);
            hal.displayClear();
            Serial.printf("Menu closed. Base: C=%.1f E=%.1f D=%.1f | Tol: C=%.1f E=%.1f D=%.1f\n",
                configStore.getCantBaseline(), configStore.getElevationBaseline(),
                configStore.getDrawBaseline(), configStore.getCantTolerance(),
                configStore.getElevationTolerance(), configStore.getDrawTolerance());
        }
    } else {
        // Normal mode
        if (hal.getButtonA()) {
            displayUI->nextScreen();
            Serial.printf("Screen: %d\n", (int)displayUI->getScreen());
        }

        if (hal.getButtonAHeld()) {
            if (session->getState() == SessionState::ACTIVE) {
                session->stop(now);
                feedback->play(PatternID::SESSION_END);
                Serial.printf("Session STOPPED | Shots: %d | Duration: %lums | Buffer: %d\n",
                    session->getShotCount(), session->getElapsedMs(now), shotBuffer.getCount());
            } else {
                session->start(now);
                dispData.has_last_shot = false;
                feedback->play(PatternID::SESSION_START);
                Serial.println("Session STARTED");
            }
        }

        if (hal.getButtonB()) {
            menu->enter();
            displayUI->setScreen(ScreenID::MENU);
            Serial.println("Menu opened");
        }
    }

    // ── Display + serial at 10Hz ──

    if (splashDone && (now - lastDisplayTime >= 100)) {
        lastDisplayTime = now;

        // Fill display data struct
        dispData.pitch = pitch;
        dispData.roll = roll;
        dispData.sec_connected = secConnected;
        dispData.sec_pitch = secData.pitch;
        dispData.sec_roll = secData.roll;
        dispData.sec_battery = secData.battery;
        dispData.session_state = session->getState();
        dispData.shot_count = session->getShotCount();
        dispData.elapsed_ms = session->getElapsedMs(now);
        dispData.battery = hal.getBatteryLevel();
        dispData.buffer_count = shotBuffer.getCount();
        dispData.buffer_capacity = shotBuffer.capacity();
        dispData.base_cant = configStore.getCantBaseline();
        dispData.base_elev = configStore.getElevationBaseline();
        dispData.base_draw = configStore.getDrawBaseline();
        dispData.tol_cant = configStore.getCantTolerance();
        dispData.tol_elev = configStore.getElevationTolerance();
        dispData.tol_draw = configStore.getDrawTolerance();

        // Draw screen (menu draws itself)
        if (menu->isActive()) {
            // Menu redraws on button press, not every frame
        } else {
            displayUI->draw(dispData);
        }

        // Serial output
        Serial.printf("P:%.1f R:%.1f | Sec:%s",
            pitch, roll,
            secConnected ? "OK" : "---");
        if (secConnected) {
            Serial.printf(" P:%.1f R:%.1f B:%d%%",
                secData.pitch, secData.roll, secData.battery);
        }
        Serial.printf(" | Scr:%d Sess:%d #%d Buf:%d\n",
            (int)displayUI->getScreen(),
            (int)session->getState(),
            session->getShotCount(),
            shotBuffer.getCount());
    }
}

#endif // ROLE_PRIMARY
