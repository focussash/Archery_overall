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
#include <ble_relay.h>
#include <cadence.h>
#include <form_check.h>
#include <calibration.h>

static HalM5Unified hal;
static ComplementaryFilter filter(0.98f);
static ShotDetector shotDetector;
static FeedbackEngine* feedback = nullptr;
static ShotBuffer<64> shotBuffer;
static ConfigStore configStore;
static SessionManager* session = nullptr;
static DisplayUI* displayUI = nullptr;
static Menu* menu = nullptr;
static CadenceTracker cadenceTracker(5, 0.30f);  // 5-shot baseline, 30% deviation threshold
static FormChecker* formChecker = nullptr;
static CalibrationManager* calibration = nullptr;

static uint32_t lastDisplayTime = 0;
static uint32_t lastStatusTime = 0;
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
    formChecker = new FormChecker(&configStore);
    calibration = new CalibrationManager(&hal, &configStore);

    if (!espnow_comm::initPrimary(1)) {
        Serial.println("ESP-NOW init FAILED!");
    } else {
        Serial.println("ESP-NOW init OK (listening + broadcast)");
    }

    if (!ble_relay::init("ArcheryWearable")) {
        Serial.println("BLE init FAILED!");
    }
    ble_relay::updateConfig(&configStore);

    // Boot splash
    hal.displayClear();
    hal.displaySetTextSize(2);
    hal.displaySetCursor(10, 10);
    hal.displaySetTextColor(Color::GREEN);
    hal.displayPrint("Archery");
    hal.displaySetCursor(10, 35);
    hal.displayPrint("Wearable");
    hal.displaySetTextSize(1);
    hal.displaySetCursor(10, 60);
    hal.displaySetTextColor(Color::WHITE);
    hal.displayPrint("PRIMARY v1.0");

    Serial.println("=== Archery Wearable - PRIMARY ===");
    Serial.println("SP12: Real-Time Form Check + Feature Toggles");
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
    Serial.printf("Accel Bias: X=%.4f Y=%.4f Z=%.4f\n",
        configStore.getCalAxBias(),
        configStore.getCalAyBias(),
        configStore.getCalAzBias());
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

    float accel[3] = {
        imu.accel.x - configStore.getCalAxBias(),
        imu.accel.y - configStore.getCalAyBias(),
        imu.accel.z - configStore.getCalAzBias()
    };
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

    // Real-time form check (during draw/hold, before shot)
    if (session->getState() == SessionState::ACTIVE && configStore.isFormCheckEnabled()) {
        FormCheckEvent fcEvent = formChecker->update(shotDetector.getState(), pitch, roll, drawAngle);
        if (fcEvent != FormCheckEvent::NONE) {
            switch (fcEvent) {
                case FormCheckEvent::DRAW_STARTED:
                    feedback->play(PatternID::DRAW_NOTIFY);
                    dispData.toast_msg = ">> DRAW DETECTED";
                    dispData.toast_color = Color::CYAN;
                    dispData.toast_expire_ms = now + 1500;
                    Serial.println("[FormCheck] DRAW_STARTED");
                    break;
                case FormCheckEvent::FORM_ALIGNED:
                    feedback->play(PatternID::FORM_ALIGNED);
                    dispData.toast_msg = ">> FORM ALIGNED";
                    dispData.toast_color = Color::GREEN;
                    dispData.toast_expire_ms = now + 1500;
                    Serial.println("[FormCheck] FORM_ALIGNED");
                    break;
                case FormCheckEvent::FORM_DEVIATED:
                    feedback->play(PatternID::FORM_DEVIATED);
                    dispData.toast_msg = ">> FORM DEVIATED";
                    dispData.toast_color = Color::RED;
                    dispData.toast_expire_ms = now + 1500;
                    Serial.println("[FormCheck] FORM_DEVIATED");
                    break;
                default: break;
            }
        }
    }

    if (shotDetector.shotDetected()) {
        formChecker->reset();  // Reset form check on shot
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

            // Cadence analysis
            const ShotTiming& timing = shotDetector.getLastTiming();
            CadenceResult cadence = cadenceTracker.recordShot(timing);

            Serial.printf(">>> SHOT #%d | P:%.1f R:%.1f Draw:%.1f Vel:%.1f | Cant:%s Elev:%s Draw:%s",
                session->getShotCount(), snap.pitch, snap.roll, drawAngle, snap.peak_angular_vel,
                eval.cant_ok ? "OK" : "EXCEEDED",
                eval.elevation_ok ? "OK" : "EXCEEDED",
                eval.draw_ok ? "OK" : "EXCEEDED");
            Serial.printf(" | T:%lums D:%lums H:%lums",
                timing.total_ms, timing.draw_ms, timing.hold_ms);
            if (cadence.has_baseline) {
                Serial.printf(" Avg:%lums Dev:%.0f%%",
                    cadence.avg_total_ms, cadence.total_deviation * 100.0f);
                if (cadence.rushing) Serial.print(" RUSHING!");
                if (cadence.holding_too_long) Serial.print(" HOLD_LONG!");
            }
            Serial.println();

            // Play appropriate feedback pattern (if shot feedback enabled)
            // Also set toast to show what the beep means
            if (cadence.rushing) {
                dispData.toast_msg = "SHOT: RUSHING!";
                dispData.toast_color = Color::ORANGE;
                if (configStore.isShotFeedbackEnabled()) feedback->play(PatternID::RUSHING);
            } else if (cadence.holding_too_long) {
                dispData.toast_msg = "SHOT: HOLD TOO LONG!";
                dispData.toast_color = Color::ORANGE;
                if (configStore.isShotFeedbackEnabled()) feedback->play(PatternID::HOLDING_TOO_LONG);
            } else if (eval.cant_ok && eval.elevation_ok && eval.draw_ok) {
                dispData.toast_msg = "SHOT: GOOD FORM";
                dispData.toast_color = Color::GREEN;
                if (configStore.isShotFeedbackEnabled()) feedback->play(PatternID::GOOD_FORM);
            } else if (!eval.cant_ok) {
                dispData.toast_msg = "SHOT: CANT EXCEEDED";
                dispData.toast_color = Color::RED;
                if (configStore.isShotFeedbackEnabled()) feedback->play(PatternID::CANT_EXCEEDED);
            } else if (!eval.draw_ok) {
                dispData.toast_msg = "SHOT: DRAW OFF";
                dispData.toast_color = Color::RED;
                if (configStore.isShotFeedbackEnabled()) feedback->play(PatternID::DRAW_INCONSISTENT);
            } else {
                dispData.toast_msg = "SHOT: ELEV EXCEEDED";
                dispData.toast_color = Color::YELLOW;
                if (configStore.isShotFeedbackEnabled()) feedback->play(PatternID::SHOT_DETECTED);
            }
            dispData.toast_expire_ms = now + 2000;

            // Notify Secondary about the shot
            espnow_comm::sendShotNotify(session->getShotCount());

            // Send via BLE if connected (otherwise stays in buffer for drain on connect)
            if (ble_relay::isConnected()) {
                ble_relay::sendShot(&session->getLastPacket());
            }
        } else {
            Serial.printf(">>> SHOT (no session) P:%.1f R:%.1f Accel:%.2fg Draw:%.1f\n",
                snap.pitch, snap.roll, snap.accel_magnitude, drawAngle);
            if (configStore.isShotFeedbackEnabled()) {
                feedback->play(PatternID::SHOT_DETECTED);
            }
        }
    }

    // ── Calibration state machine (runs every loop for sample accumulation) ──

    if (calibration->isActive()) {
        bool calDone = calibration->update(now, pitch, roll, drawAngle, accel);
        if (calDone) {
            hal.displayClear();
            displayUI->setScreen(ScreenID::HOME);
            ble_relay::updateConfig(&configStore);
            Serial.printf("Calibration done. Bias: X=%.4f Y=%.4f Z=%.4f\n",
                configStore.getCalAxBias(), configStore.getCalAyBias(), configStore.getCalAzBias());
            Serial.printf("Baselines: C=%.1f E=%.1f D=%.1f\n",
                configStore.getCantBaseline(), configStore.getElevationBaseline(),
                configStore.getDrawBaseline());
        }
    }

    // ── Button handling ──

    if (calibration->isActive()) {
        // Calibration mode: buttons go to calibration
        if (hal.getButtonA()) calibration->onButtonA();
        if (hal.getButtonB()) calibration->onButtonB();
    } else if (menu->isActive()) {
        // Menu mode: buttons go to menu
        if (hal.getButtonA()) menu->onButtonA();
        if (hal.getButtonB()) menu->onButtonB();
        menu->update(now);

        // Menu exited? Check for calibration request
        if (!menu->isActive()) {
            CalibrationRequest req = menu->getCalibrationRequest();
            if (req == CalibrationRequest::BASELINE) {
                calibration->startBaseline();
                hal.displayClear();
                calibration->draw(now);
                Serial.println("Calibration: Set Baseline started");
            } else if (req == CalibrationRequest::FLAT_CAL) {
                calibration->startFlatCal();
                hal.displayClear();
                calibration->draw(now);
                Serial.println("Calibration: Flat Cal started");
            } else {
                // Normal menu exit
                displayUI->setScreen(ScreenID::HOME);
                hal.displayClear();
                ble_relay::updateConfig(&configStore);
                Serial.printf("Menu closed. Base: C=%.1f E=%.1f D=%.1f | Tol: C=%.1f E=%.1f D=%.1f\n",
                    configStore.getCantBaseline(), configStore.getElevationBaseline(),
                    configStore.getDrawBaseline(), configStore.getCantTolerance(),
                    configStore.getElevationTolerance(), configStore.getDrawTolerance());
            }
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
                cadenceTracker.reset();
                formChecker->reset();
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

    // ── BLE ──

    ble_relay::update(&shotBuffer);

    // Handle BLE commands from phone
    BleCommand bleCmd = ble_relay::getCommand();
    if (bleCmd.session_start && session->getState() != SessionState::ACTIVE) {
        session->start(now);
        cadenceTracker.reset();
        formChecker->reset();
        dispData.has_last_shot = false;
        feedback->play(PatternID::SESSION_START);
        Serial.println("Session STARTED (BLE command)");
    }
    if (bleCmd.session_stop && session->getState() == SessionState::ACTIVE) {
        session->stop(now);
        feedback->play(PatternID::SESSION_END);
        Serial.printf("Session STOPPED (BLE) | Shots: %d | Duration: %lums\n",
            session->getShotCount(), session->getElapsedMs(now));
    }
    if (bleCmd.config_updated) {
        ble_relay::applyReceivedConfig(&configStore);
        ble_relay::updateConfig(&configStore);
        Serial.println("Config updated from BLE");
    }

    // Update BLE device status every 2 seconds
    if (now - lastStatusTime >= 2000) {
        lastStatusTime = now;
        BleDeviceStatus status;
        status.battery = (uint8_t)hal.getBatteryLevel();
        status.imu_ok = 1;
        status.sec_connected = secConnected ? 1 : 0;
        status.session_state = (uint8_t)session->getState();
        status.shot_count = session->getShotCount();
        ble_relay::updateStatus(status);
    }

    // ── Display + serial at 10Hz ──

    if (splashDone && (now - lastDisplayTime >= 100)) {
        lastDisplayTime = now;

        // Expire toast
        if (dispData.toast_msg != nullptr && now >= dispData.toast_expire_ms) {
            dispData.toast_msg = nullptr;
        }

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

        // Draw screen (menu draws itself on button press)
        if (calibration->isActive()) {
            calibration->draw(now);
        } else if (menu->isActive()) {
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
        Serial.printf(" | Scr:%d Sess:%d #%d Buf:%d BLE:%s\n",
            (int)displayUI->getScreen(),
            (int)session->getState(),
            session->getShotCount(),
            shotBuffer.getCount(),
            ble_relay::isInitialized() ? (ble_relay::isConnected() ? "CONN" : "ADV") : "FAIL");
    }
}

#endif // ROLE_PRIMARY
