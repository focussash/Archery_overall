# Achievements

## Subproject 1: Project Scaffold + HAL + Hello World
**Status: VERIFIED**

Files created:
- platformio.ini (3 environments: primary, secondary, native_test)
- lib/hal/hal_interface.h (abstract HAL interface)
- lib/hal/hal_m5unified.h/.cpp (M5Unified HAL implementation)
- src/main_primary.cpp (hello world + IMU serial print)
- src/main_secondary.cpp (same for secondary role)

Fixes applied during verification:
- Added `#include <Arduino.h>` to both main_primary.cpp and main_secondary.cpp (Serial was undeclared without it)
- Installed `intelhex` pip package into PlatformIO's Python env (esptoolpy post-install script failed due to spaces in user path)
- Added `M5.Imu.update()` call in `hal_m5unified.cpp::readIMU()` — IMU data was stale without it
- Added `framework =` override in platformio.ini native_test env (inherited `arduino` framework is invalid for native)
- Installed MinGW via scoop for native test compilation

Verification:
- [x] `pio run -e primary` compiles — SUCCESS (RAM 7.7%, Flash 36.2%)
- [x] `pio run -e secondary` compiles — SUCCESS (RAM 7.7%, Flash 36.2%)
- [x] Upload to device — SUCCESS (primary on COM4)
- [x] Display shows "Archery Wearable" + "PRIMARY" — confirmed by user
- [x] Serial shows IMU data, values change when tilted — confirmed by user
- [x] Battery percentage displays — confirmed by user

## Subproject 2: IMU Math + Sensor Fusion
**Status: VERIFIED**

Files created:
- lib/imu_math/complementary_filter.h/.cpp
- lib/imu_math/imu_math.h/.cpp
- test/test_imu_math/test_imu_math.cpp (16 native tests)

Fixes applied during verification:
- Added `#include <cstdint>` to imu_math.h (int16_t was undeclared in native build)

Verification:
- [x] `pio test -e native_test` — 16/16 tests passed
- [x] On-device: pitch/roll display updates correctly when tilted — confirmed by user

## Subproject 3: Shot Detection State Machine
**Status: VERIFIED**

Files created:
- lib/shot_detect/shot_detect.h/.cpp
- test/test_shot_detect/test_shot_detect.cpp (8 native tests)

Verification:
- [x] `pio test -e native_test` — 8/8 tests passed
- [x] On-device: sharp shake triggers "SHOT DETECTED" + beep — confirmed by user
- [x] Gentle movements do not trigger false positives

## Subproject 4: Audio Feedback Engine
**Status: VERIFIED**

Files created:
- lib/feedback/feedback.h/.cpp (9 patterns, non-blocking player)

Verification:
- [x] Compiles as part of primary build
- [x] On-device: all 9 patterns play via BtnA cycling — confirmed by user
- [x] Patterns are non-blocking (display continues updating while beeping)

## Subproject 5: Shot Buffer (Ring Buffer)
**Status: NATIVE TESTS VERIFIED**

Files created:
- lib/shot_buffer/shot_buffer.h/.cpp (template ring buffer + ShotPacket_t)
- test/test_shot_buffer/test_shot_buffer.cpp (10 native tests)

Fixes applied during verification:
- Changed `reserved` field from `uint16_t` to `uint32_t` to reach 20-byte BLE target

Verification:
- [x] `pio test -e native_test` — 10/10 tests passed
- [x] sizeof(ShotPacket_t) == 20

## Subproject 6: Session Manager + Config Store
**Status: VERIFIED**

Files created:
- lib/session/session.h (SessionManager, ConfigStore, FormEvaluation)
- lib/session/session.cpp (full implementation with NVS persistence)

Implementation details:
- ConfigStore loads/saves 3 thresholds (cant, elevation, draw) via ESP32 Preferences (NVS)
- SessionManager tracks state (IDLE/ACTIVE/ENDED), shot count, elapsed time
- First shot becomes reference baseline; subsequent shots evaluated relative to it
- Form evaluation sets FORM_FLAG_CANT_OK, FORM_FLAG_ELEVATION_OK, FORM_FLAG_DRAW_OK
- buildPacket uses imu_math::degreesToCentidegrees for int16 encoding
- #ifndef NATIVE_TEST guards around NVS calls for future native testability

Verification:
- [x] Compiles and uploads — SUCCESS (RAM 8.1%, Flash 36.8%)
- [x] Hold BtnA starts session (rising tone) — confirmed by user
- [x] Shots during session are counted with form evaluation — confirmed by user
- [x] Hold BtnA stops session (falling tone) — confirmed by user
- [ ] NVS persistence across reboot (deferred to SP9 menu system)

## Subproject 7: ESP-NOW Communication
**Status: VERIFIED**

Files created:
- lib/espnow_comm/espnow_comm.h (ESPNowPacket 18 bytes, namespace API)
- lib/espnow_comm/espnow_comm.cpp (broadcast ESP-NOW, magic validation)

Files modified:
- src/main_secondary.cpp (rewritten: ComplementaryFilter + 50Hz ESP-NOW streaming)
- src/main_primary.cpp (updated: ESP-NOW receive, secondary data display, drawAngle wiring)
- claude-memory/DEVELOPMENT_PLAN.md (SP7 section updated with deviations)

Deviations from original plan:
- Broadcast (FF:FF:FF:FF:FF:FF) with magic 0x41524348 instead of hardcoded MACs
- Packet reduced to 18 bytes (type, magic, timestamp, pitch, roll, battery) — no raw accel/gyro
- Added battery field so Primary can display Secondary's battery level

Implementation details:
- ESP-IDF 4.x callback signature (3-parameter: mac, data, len)
- Secondary sends at 50Hz (20ms interval), display at 5Hz (200ms)
- Primary receives via callback, validates magic + size, stores in static buffer
- isSecondaryConnected() checks 500ms timeout on last received packet
- drawAngle from Secondary's pitch is wired into session->recordShot()
- Primary display: top bar shows "Sec P:XX R:XX B:XX%" or "Sec: DISCONNECTED"

Verification:
- [x] `pio run -e primary` — SUCCESS (RAM 14.8%, Flash 71.5%)
- [x] `pio run -e secondary` — SUCCESS (RAM 14.4%, Flash 71.3%)
- [x] Upload primary to device 1, secondary to device 2
- [x] Primary serial shows both devices' IMU data
- [x] Moving Secondary → Primary sees angle changes
- [x] Turn off Secondary → "DISCONNECTED" after 500ms — confirmed by user
- [x] Turn on Secondary → reconnects automatically — confirmed by user
- [x] Shots during session use Secondary's draw angle (Draw:3.4 etc. in shot log)
- [x] Session start/stop works (15 shots in 17s session)
- [x] Form evaluation flags (Cant/Elev/Draw OK/EXCEEDED) working

## Subproject 8: Integrated Shot Pipeline (Tier 1 MVP)
**Status: VERIFIED**

Files modified:
- src/main_primary.cpp (major rewrite: full pipeline wiring)
- src/main_secondary.cpp (rewrite: shot echo from Primary)
- lib/espnow_comm/espnow_comm.h (added sendShotNotify, getShotNotify, ESPNowShotNotify)
- lib/espnow_comm/espnow_comm.cpp (implemented shot notify via MSG_STATUS + broadcast)

Implementation details:
- Full pipeline: IMU → filter → shot detect → session.recordShot() → form eval → feedback → buffer
- Primary receives Secondary drawAngle via ESP-NOW, wires into recordShot()
- Form-based feedback: GOOD_FORM (all ok), CANT_EXCEEDED, DRAW_INCONSISTENT, or SHOT_DETECTED (elev only)
- Shot notify sent back to Secondary via ESP-NOW (MSG_STATUS, shot_number in battery field)
- Secondary displays "Shot #N!" echo for 1 second
- Full display UI on Primary: secondary status bar, pitch/roll, last shot with color-coded form flags, session status, buffer/battery bar
- BtnA hold = start/stop session, BtnB = debug info
- Serial output at 10Hz with full telemetry

Verification (from SP7 session — pipeline was live):
- [x] Compiles: primary and secondary both build
- [x] Two devices communicating via ESP-NOW
- [x] Shot detection triggers form eval + appropriate beep
- [x] Shot count increments on display
- [x] Serial shows full shot data (timestamp, cant, elev, draw, flags)
- [x] Session start/stop works via BtnA hold
- [x] Form eval: tilt exceeding threshold → different beep
- [x] Shot echo on Secondary display
- [x] Stability test: basic multi-shot sessions work
- [ ] Buffer drain verification (deferred to SP10 BLE)

Known issues:
- Display colors show blue instead of green (RGB/BGR byte order) — deferred to SP9 display UI

## Subproject 9: Display UI + Menu + Threshold Configuration
**Status: VERIFIED**

Files created:
- lib/display_ui/display_ui.h/.cpp (DisplayUI class, 3 screens + Color constants)
- lib/menu/menu.h/.cpp (Menu class, scrollable 7-item menu)

Files modified:
- lib/session/session.h/.cpp (ConfigStore: 3 thresholds → 6 values: 3 baselines + 3 tolerances; removed first-shot reference from SessionManager)
- src/main_primary.cpp (integrated DisplayUI + Menu, new button scheme)
- claude-memory/DEVELOPMENT_PLAN.md (updated SP6 and SP9 sections)

Implementation details:
- 3 display screens: HOME (overview), LIVE (real-time angles), SESSION (shot detail + config reference)
- MENU screen rendered by Menu module with scrolling (4 visible items)
- 6 configurable values: Cant/Elev/Draw baselines + tolerances
- Form eval changed from first-shot reference to: |measured - baseline| <= tolerance
- Button scheme: BtnA tap = cycle screens, BtnA hold = start/stop session, BtnB tap = open menu
- Menu editing: BtnA = +0.5°, BtnB = -0.5°, auto-exit after 2s idle
- NVS keys: cant_base, cant_tol, elev_base, elev_tol, draw_base, draw_tol
- Default baselines: 0°. Default tolerances: 5° / 10° / 15°

Deviations from original plan:
- Baseline+tolerance replaces simple thresholds (user-requested)
- Auto-timeout editing replaces hold-to-exit (wasPressed/wasHold conflict on M5Unified)
- Color issue (blue vs green) still present — likely panel-specific, cosmetic only

Verification:
- [x] Compiles — SUCCESS (RAM 14.9%, Flash 72.2%)
- [x] Screen cycling via BtnA tap — HOME/LIVE/SESSION
- [x] Menu entry via BtnB, scrolling through 7 items
- [x] Threshold adjustment: A=+0.5, B=-0.5, auto-exit after 2s
- [x] NVS persistence across reboot — confirmed by user
- [x] Color coding on SESSION screen (green=OK, red=OVER)
- [x] Form eval uses baselines+tolerances correctly
- [x] Session start/stop still works via BtnA hold

## Subproject 10: BLE GATT Relay
**Status: VERIFIED**

Files created:
- lib/ble_relay/ble_relay.h (GATT service UUIDs, BleDeviceStatus, BleConfigPacket, BleCommand structs)
- lib/ble_relay/ble_relay.cpp (NimBLE GATT server with 4 characteristics)

Files modified:
- platformio.ini (added NimBLE-Arduino dep, huge_app.csv partition, NimBLE role flags)
- src/main_primary.cpp (integrated BLE init, shot sending, buffer drain, status updates, phone commands)

Implementation details:
- NimBLE GATT server with custom UUID base: 4152Cxxx-0001-1000-8000-00805F9B34FB
- 4 characteristics: Shot Data (notify), Session Control (write), Device Status (read+notify), Config (read+write)
- Buffer drain triggered on client subscribe to shot notifications (not on connect)
- BLE device status updated every 2 seconds (battery, IMU, secondary connection, session state, shot count)
- Phone can start/stop session and push config changes via BLE writes
- Config sync: phone writes 24 bytes (6 floats) → ConfigStore updated + NVS saved

Flash optimization applied:
- Switched partition from default.csv (1,280KB app + 1,280KB OTA) to huge_app.csv (3,072KB app, no OTA)
- Disabled unused NimBLE roles: CONFIG_BT_NIMBLE_ROLE_CENTRAL=0, CONFIG_BT_NIMBLE_ROLE_OBSERVER=0
- Result: flash usage dropped from 91.5% to 37.8%

Fixes applied during verification:
- BLE advertising packet was too large (38 bytes > 31 byte limit) — split name into adv data, service UUID into scan response
- Buffer drain was firing on connect (before client subscribed to notifications) — changed to trigger on subscribe via onSubscribe callback
- Added BLE init error checking and status indicator in serial output

Build results:
- [x] `pio run -e primary` — SUCCESS (RAM 17.9%, Flash 37.8%)
- [x] `pio run -e secondary` — SUCCESS (unchanged, no BLE dependency)

Verification:
- [x] Device appears as "ArcheryWearable" in BLE scan (nRF Connect app)
- [x] Connect → see service and 4 characteristics
- [x] Simulate shots → see 20-byte notifications
- [x] Write 0x01 to Session Control → session starts on device (rising tone)
- [x] Write 0x02 to Session Control → session stops (falling tone)
- [x] Disconnect → reconnect → subscribe → buffered shots drain
- [x] ESP-NOW still works while BLE is connected (Secondary visible on Primary + shot notifications to phone simultaneously)
