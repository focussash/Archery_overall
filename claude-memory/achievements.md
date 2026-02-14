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
