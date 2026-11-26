# Implementation Plan: Archery Wearable M5StickC Firmware

## Context

This is the step-by-step implementation plan for the M5StickC archery wearable firmware. The project is a fresh repo with no code — only the design doc (`claude-memory/overall_design.md`) exists.

**Goal**: Build firmware for two M5StickC devices (Primary + Secondary) that provide real-time archery form feedback via audio, with BLE data relay to an existing Android companion app ([ArcheryApp](https://github.com/focussash/ArcheryApp)).

**Environment**: PlatformIO + C++ (Arduino framework), two M5StickC units (Plus/Plus2 mix).

**Key library choice**: [M5Unified](https://github.com/m5stack/M5Unified) — auto-detects Plus vs Plus2, unified API for IMU (`M5.Imu`), display (`M5.Display`), speaker (`M5.Speaker`), buttons (`M5.BtnA/B`), power. This eliminates the need for separate HAL implementations per model.

---

## Project Structure

```
wearable m5stick/
├── platformio.ini              # 3 environments: primary, secondary, native_test
├── src/
│   ├── main_primary.cpp        # Primary device entry point
│   └── main_secondary.cpp      # Secondary device entry point
├── lib/
│   ├── hal/                    # Hardware abstraction (M5Unified wrapper)
│   │   ├── hal_interface.h     # Abstract interface
│   │   └── hal_m5unified.h/cpp # Concrete implementation
│   ├── imu_math/               # Sensor fusion, angle calculation
│   │   ├── imu_math.h/cpp
│   │   └── complementary_filter.h/cpp
│   ├── shot_detect/            # Shot detection state machine
│   │   └── shot_detect.h/cpp
│   ├── feedback/               # Beep pattern engine
│   │   └── feedback.h/cpp
│   ├── shot_buffer/            # Ring buffer + ShotPacket_t
│   │   └── shot_buffer.h/cpp
│   ├── session/                # Session state machine
│   │   └── session.h/cpp
│   ├── espnow_comm/            # ESP-NOW protocol
│   │   └── espnow_comm.h/cpp
│   ├── display_ui/             # Display screens
│   │   └── display_ui.h/cpp
│   ├── menu/                   # Menu system + threshold config
│   │   └── menu.h/cpp
│   └── ble_relay/              # NimBLE GATT service
│       └── ble_relay.h/cpp
├── test/
│   ├── test_shot_buffer/       # Native unit tests
│   ├── test_imu_math/
│   └── test_shot_detect/
└── claude-memory/
```

## Subproject Overview

| # | Subproject | Tier | Key Deliverable |
|---|-----------|------|-----------------|
| 1 | Project Scaffold + HAL + Hello World | Foundation | PlatformIO project compiles and runs on device |
| 2 | IMU Math + Sensor Fusion | Tier 1 | Pitch/roll angles displayed in real-time |
| 3 | Shot Detection State Machine | Tier 1 | Detects shots from accelerometer spikes |
| 4 | Audio Feedback Engine | Tier 1 | Distinct beep patterns for form events |
| 5 | Shot Buffer (Ring Buffer) | Tier 1 | ShotPacket_t ring buffer with native tests |
| 6 | Session Manager + Config Store | Tier 1-2 | Start/stop sessions, persist thresholds (NVS) |
| 7 | ESP-NOW Communication | Tier 1 | Two-device IMU streaming, Primary/Secondary roles |
| 8 | Integrated Shot Pipeline | Tier 1 | Full pipeline: detect → capture → buffer → beep |
| 9 | Display UI + Menu + Thresholds | Tier 2 | Config menus, live status, threshold adjustment |
| 10 | BLE GATT Relay | Tier 2 | ShotPacket_t notifications to phone |
| 11 | Pre-Shot Routine Timer | Tier 2 | Cadence detection and timing alerts |
| 12 | Real-Time Form Check + Feature Toggles | Tier 2 | Live form feedback during aiming, toggleable features, menu restructure |
| 13 | IMU Calibration + Usage Manual | Tier 3 | Guided calibration flows, user documentation |
| 14 | Final Polish | Tier 3 | Error handling, power optimization, version string |

---

## Subproject 1: Project Scaffold + HAL + Hello World

### Goal
Set up PlatformIO project, define HAL interface, implement M5Unified HAL, verify hardware works.

### Files to Create
- `platformio.ini`
- `src/main_primary.cpp`
- `src/main_secondary.cpp`
- `lib/hal/hal_interface.h`
- `lib/hal/hal_m5unified.h`
- `lib/hal/hal_m5unified.cpp`

### Substeps

**1.1** Create `platformio.ini` with three environments:
```ini
[env:primary]
platform = espressif32
board = m5stick-c        # works for both Plus and Plus2 via M5Unified auto-detect
framework = arduino
build_flags = -DROLE_PRIMARY
lib_deps =
    m5stack/M5Unified
monitor_speed = 115200

[env:secondary]
platform = espressif32
board = m5stick-c
framework = arduino
build_flags = -DROLE_SECONDARY
lib_deps =
    m5stack/M5Unified
monitor_speed = 115200

[env:native_test]
platform = native
test_framework = unity
build_flags = -DNATIVE_TEST
lib_ignore = hal, feedback, display_ui, menu, ble_relay, espnow_comm
```

**1.2** Define `hal_interface.h` — abstract interface:
- `init()` — hardware initialization
- `readIMU(imu_data_t*)` — get accel + gyro
- `playTone(freq, duration_ms)` — buzzer
- `stopTone()`
- `getButtonA()` / `getButtonB()` — button states
- `getBatteryLevel()` — percentage
- `getDisplayWidth()` / `getDisplayHeight()`
- Display draw methods (delegated to M5.Display)
- `getHeading()` → returns -1 (no magnetometer — stub for future)
- `update()` — call M5.update() per loop

**1.3** Implement `hal_m5unified.cpp` wrapping M5Unified:
- `M5.begin()` with auto-detect config
- IMU: `M5.Imu.getImuData(&data)` → returns `imu_3d_t` for accel and gyro
- Speaker: `M5.Speaker.tone(freq, duration)`
- Buttons: `M5.BtnA.wasPressed()`, etc.
- Display: `M5.Display` (TFT_eSPI-compatible)
- Power: `M5.Power.getBatteryLevel()`

**1.4** Create `main_primary.cpp`:
- Initialize HAL
- Display "Archery Wearable" + "Primary" on screen
- Loop: read IMU, print raw accel/gyro to serial every 100ms
- Show battery level on display

**1.5** Create `main_secondary.cpp`:
- Same but displays "Secondary"
- Same IMU serial output for verification

### Verification
- [ ] `pio run -e primary` compiles without errors
- [ ] Upload to device 1: display shows "Primary", serial shows IMU values that change when tilted
- [ ] `pio run -e secondary` compiles without errors
- [ ] Upload to device 2: display shows "Secondary", serial shows IMU values
- [ ] Battery percentage displays correctly

---

## Subproject 2: IMU Math + Sensor Fusion

### Goal
Convert raw IMU data into meaningful angles (pitch, roll) using a complementary filter. Pure math — testable natively.

### Files to Create
- `lib/imu_math/imu_math.h`
- `lib/imu_math/imu_math.cpp`
- `lib/imu_math/complementary_filter.h`
- `lib/imu_math/complementary_filter.cpp`
- `test/test_imu_math/test_imu_math.cpp`

### Substeps

**2.1** Implement `complementary_filter`:
- `void update(float accel[3], float gyro[3], float dt)`
- `float getPitch()` — arm elevation angle (degrees)
- `float getRoll()` — bow cant angle (degrees)
- Alpha parameter (default 0.98 gyro, 0.02 accel)
- Handles gimbal lock edge cases

**2.2** Implement `imu_math`:
- `float computePitch(float ax, float ay, float az)` — from accelerometer only (for reference)
- `float computeRoll(float ax, float ay, float az)` — from accelerometer only
- `float computeAngularVelocity(float gx, float gy, float gz)` — magnitude
- `float degreesToCentidegrees(float deg)` — for ShotPacket_t int16 format

**2.3** Write native unit tests:
- Known accel vectors → expected pitch/roll angles
- Filter convergence test (step input → settles to correct angle)
- Angular velocity magnitude calculation
- Centidegree conversion round-trip

**2.4** Integrate into `main_primary.cpp`:
- Feed IMU data through complementary filter each loop
- Display pitch and roll on screen (large font)
- Serial print: `"Pitch: XX.X Roll: XX.X"` at 10Hz

### Verification
- [ ] `pio test -e native_test` — all imu_math tests pass
- [ ] Upload to device: tilt forward → pitch increases; tilt sideways → roll increases
- [ ] Values are stable (not jittery) when device is still
- [ ] Values settle to correct angle within ~1 second after motion stops

---

## Subproject 3: Shot Detection State Machine

### Goal
Detect the moment of arrow release from the accelerometer shock on the bow-hand unit.

### Files to Create
- `lib/shot_detect/shot_detect.h`
- `lib/shot_detect/shot_detect.cpp`
- `test/test_shot_detect/test_shot_detect.cpp`

### Substeps

**3.1** Design shot detection state machine:
- States: `IDLE` → `DRAW_DETECTED` → `HOLD` → `SHOT_FIRED` → `RECOVERY` → `IDLE`
- `IDLE`: Monitoring for draw motion (smooth pitch increase)
- `DRAW_DETECTED`: Arm rising, angular velocity above threshold
- `HOLD`: Arm stable, waiting for release
- `SHOT_FIRED`: Accelerometer spike > threshold detected (release shock)
- `RECOVERY`: Debounce period (ignore aftershocks), ~500ms

**3.2** Implement `ShotDetector` class:
- `void update(float accel[3], float gyro[3], float pitch, float roll, uint32_t timestamp_ms)`
- `bool shotDetected()` — true for one frame when shot fires
- `ShotState getState()`
- Configurable: `accel_threshold`, `debounce_ms`, `hold_timeout_ms`

**3.3** Capture shot data:
- On `SHOT_FIRED`: freeze pitch, roll, angular velocity at that instant
- Store in a `ShotSnapshot` struct returned by `getLastShot()`

**3.4** Write native unit tests:
- Feed synthetic accel sequence (calm → spike → calm) → shot detected
- Debounce: spike → spike within 500ms → only one shot
- No false positive from gentle movements
- State transitions are correct

**3.5** Integrate into `main_primary.cpp`:
- Feed IMU data to ShotDetector
- On shot: print `"SHOT DETECTED! Pitch=XX Roll=XX"` to serial

### Verification
- [ ] `pio test -e native_test` — all shot_detect tests pass
- [ ] Upload: sharply tap/flick the device → "SHOT DETECTED" in serial
- [ ] Gentle movements do not trigger false shots
- [ ] No double-detections from a single tap (debounce works)
- [ ] State transitions visible in serial log

---

## Subproject 4: Audio Feedback Engine

### Goal
A beep pattern engine that plays distinct audio patterns through the M5StickC speaker/buzzer.

### Files to Create
- `lib/feedback/feedback.h`
- `lib/feedback/feedback.cpp`

### Substeps

**4.1** Define beep pattern data structure:
- Pattern = array of `{frequency_hz, duration_ms, pause_ms}` steps
- Patterns are `const` data, not runtime-generated

**4.2** Define all patterns from design doc:
- `SHOT_DETECTED` — single short beep (2000Hz, 50ms)
- `GOOD_FORM` — double beep high (2500Hz, 80ms × 2, 60ms gap)
- `CANT_EXCEEDED` — long low tone (800Hz, 400ms)
- `DRAW_INCONSISTENT` — triple fast beep (1800Hz, 40ms × 3, 30ms gap)
- `SESSION_START` — rising tone (1000→2000Hz sweep)
- `SESSION_END` — falling tone (2000→1000Hz sweep)

**4.3** Implement non-blocking pattern player:
- `void play(PatternID pattern)` — starts playing, returns immediately
- `void update()` — call each loop, advances through pattern steps using millis()
- `bool isPlaying()`
- `void stop()`
- Uses HAL `playTone()` / `stopTone()` only

**4.4** Integrate into `main_primary.cpp`:
- Button A: cycle through patterns, play each one
- Serial prints which pattern is playing

### Verification
- [ ] Upload: press button → hear each distinct pattern
- [ ] Patterns are non-blocking (display/serial continue updating while beeping)
- [ ] Each pattern is audibly distinct and matches the design doc descriptions
- [ ] No audio glitches or stuck tones

---

## Subproject 5: Shot Buffer (Ring Buffer)

### Goal
A fixed-size ring buffer storing `ShotPacket_t` structs, ready for BLE relay.

### Files to Create
- `lib/shot_buffer/shot_buffer.h`
- `lib/shot_buffer/shot_buffer.cpp`
- `test/test_shot_buffer/test_shot_buffer.cpp`

### Substeps

**5.1** Define `ShotPacket_t` (from design doc, 20 bytes packed struct) in `shot_buffer.h`

**5.2** Implement `ShotBuffer` class:
- Fixed capacity (e.g., 64 shots — configurable via template or constructor)
- `bool push(const ShotPacket_t& shot)` — add shot, overwrites oldest if full
- `bool peek(ShotPacket_t* out, uint8_t index)` — read without removing
- `bool pop(ShotPacket_t* out)` — read and remove oldest
- `uint8_t count()` — number of buffered shots
- `void clear()`
- `bool isFull()` / `bool isEmpty()`

**5.3** Write native unit tests:
- Push/pop FIFO ordering
- Wraparound behavior (push 65 into size-64 buffer → oldest lost, 64 remain)
- Empty buffer pop returns false
- Clear resets count to 0
- Peek doesn't remove
- Data integrity (push specific values, pop and verify byte-exact)

### Verification
- [ ] `pio test -e native_test` — all shot_buffer tests pass
- [ ] sizeof(ShotPacket_t) == 20 (verified in test)

---

## Subproject 6: Session Manager + Config Store

### Goal
Session state machine (idle/active/ended) and persistent threshold storage via ESP32 NVS (Non-Volatile Storage).

### Files to Create
- `lib/session/session.h`
- `lib/session/session.cpp`

### Substeps

**6.1** Implement `SessionManager`:
- States: `IDLE` → `ACTIVE` → `ENDED` → `IDLE`
- `start()` — reset shot count, record start time, play SESSION_START
- `stop()` — play SESSION_END, transition to ENDED
- `recordShot(ShotSnapshot)` — increment count, build ShotPacket_t, push to ShotBuffer
- `getState()`, `getShotCount()`, `getElapsedMs()`

**6.2** Implement `ConfigStore` (in session module):
- Uses ESP32 `Preferences` library (NVS wrapper)
- Stores 6 values: 3 baselines (target angles) + 3 tolerances (allowed deviation)
  - `cant_baseline`, `cant_tolerance`
  - `elevation_baseline`, `elevation_tolerance`
  - `draw_baseline`, `draw_tolerance`
- `load()` — reads from NVS, applies defaults if first boot
- `save()` — writes current values to NVS
- Defaults: baselines = 0.0°, tolerances = 5.0° / 10.0° / 15.0°

**6.3** Form evaluation:
- `evaluateForm(ShotSnapshot)` → returns `form_flags` bitfield
- Compares `|measured_angle - baseline| <= tolerance` for each axis
- Returns which aspects are ok/exceeded
- **No first-shot reference** — uses explicit baselines from ConfigStore

**6.4** Integrate into `main_primary.cpp`:
- Button A long press: start/stop session
- On shot detected → SessionManager.recordShot()
- Form evaluation → appropriate beep pattern
- Serial: "Session started", "Shot #3: Cant OK, Elevation EXCEEDED", etc.

### Verification
- [ ] Compile and upload
- [ ] Long press starts session (rising tone), long press again stops (falling tone)
- [ ] Shots during active session are counted (serial shows incrementing shot number)
- [ ] Thresholds survive device reboot (set a threshold, reboot, verify it's still set via serial)
- [ ] Form evaluation triggers correct beep (exceed cant → long low tone)

---

## Subproject 7: ESP-NOW Communication

### Goal
Two-device communication: Secondary streams IMU data to Primary at 50Hz via ESP-NOW.

### Files to Create
- `lib/espnow_comm/espnow_comm.h`
- `lib/espnow_comm/espnow_comm.cpp`

### Substeps

**7.1** Define ESP-NOW message types and packet format:
```cpp
enum ESPNowMsgType : uint8_t { MSG_IMU_DATA, MSG_STATUS, MSG_PAIR_REQUEST, MSG_PAIR_ACK };

// 18 bytes, packed — broadcast with magic number validation
struct __attribute__((packed)) ESPNowPacket {
    uint8_t type;           // ESPNowMsgType
    uint32_t magic;         // 0x41524348 ("ARCH") — filters foreign packets
    uint32_t timestamp_ms;
    float pitch;
    float roll;
    uint8_t battery;        // 0-100% — lets Primary display Secondary battery
};
```
- No raw accel/gyro — Primary only needs Secondary's computed angles
- Magic number prevents processing packets from other ESP-NOW systems

**7.2** Implement ESP-NOW initialization:
- `initPrimary(channel)` — `WiFi.mode(WIFI_STA)`, `esp_now_init()`, register receive callback
- `initSecondary(channel)` — same + add broadcast peer (`FF:FF:FF:FF:FF:FF`)
- Broadcast approach: no hardcoded MACs needed, works out of the box
- `MSG_PAIR_REQUEST`/`MSG_PAIR_ACK` types reserved in enum for future unicast upgrade

**7.3** Implement communication API:
- `sendIMUData(pitch, roll, battery)` — pack and broadcast ESPNowPacket
- Receive callback: validate magic + size, copy to static ESPNowIMUData buffer
- `getLatestData()` — returns latest received data
- `isSecondaryConnected()` — `(now - last_recv) < 500ms`

**7.4** Secondary main loop (`main_secondary.cpp`):
- Add ComplementaryFilter for local IMU processing
- Read IMU every loop, run filter at full speed
- Send via `espnow_comm::sendIMUData()` at 50Hz (every 20ms)
- Display: pitch/roll + TX count at 5Hz
- Serial: pitch/roll + TX count at 5Hz

**7.5** Primary receives and processes:
- `espnow_comm::initPrimary(1)` in setup
- Each loop: `getLatestData()` + `isSecondaryConnected()`
- Wire `secondaryData.pitch` as `drawAngle` into `session->recordShot()`
- Display: Secondary angles + connection status + Secondary battery
- Serial: "Pri P:XX R:XX | Sec P:XX R:XX Batt:XX%"

**7.6** Handle connection state:
- Primary tracks "last received" timestamp
- If no packet for >500ms → show "Secondary: DISCONNECTED" on display
- Auto-reconnects when Secondary resumes broadcasting

### Verification
- [ ] Upload `primary` env to device 1, `secondary` env to device 2
- [ ] Primary serial shows both devices' IMU data
- [ ] Moving Secondary device → Primary sees its angles change
- [ ] Turn off Secondary → Primary shows "DISCONNECTED" after timeout
- [ ] Turn on Secondary → reconnects automatically, data flows again
- [ ] Latency is imperceptible (sub-millisecond ESP-NOW)

---

## Subproject 8: Integrated Shot Pipeline

### Goal
Wire everything together: IMU → shot detection → angle capture (both devices) → form evaluation → beep → buffer. This is the Tier 1 MVP milestone.

### Files to Modify
- `src/main_primary.cpp` (major update)
- `src/main_secondary.cpp` (minor update)

### Substeps

**8.1** Primary main loop integration:
```
loop():
  hal.update()
  read Primary IMU → filter → pitch/roll
  receive Secondary IMU (from ESP-NOW)
  shotDetector.update(primaryIMU)
  if shotDetected:
    snapshot = {
      timestamp, primary.pitch, primary.roll,
      secondary.drawAngle, peakAngularVelocity
    }
    formFlags = session.evaluateForm(snapshot)
    packet = buildShotPacket(snapshot, formFlags)
    shotBuffer.push(packet)
    session.incrementShotCount()
    feedback.play(formFlags → pattern)
    serial.print(shot summary)
  feedback.update()
  displayUI.update()
```

**8.2** Build `ShotPacket_t` from shot data:
- Map pitch → `elevation_angle` (centidegrees)
- Map roll → `cant_angle` (centidegrees)
- Map secondary pitch → `draw_angle` (centidegrees)
- Compute `draw_velocity` from peak angular velocity
- Set `heading = 0xFFFF` (no magnetometer)
- Set `form_flags` from evaluation

**8.3** Add basic status display on Primary:
- Line 1: Session state + elapsed time
- Line 2: Shot count
- Line 3: Last shot cant / elevation
- Line 4: "Secondary: OK" or "DISCONNECTED"

**8.4** Secondary: add shot detection echo:
- When Primary detects shot, inform Secondary (optional ESP-NOW message back)
- Secondary can display "Shot!" briefly

### Verification (Tier 1 MVP Acceptance Test)
- [ ] Two devices running, communicating via ESP-NOW
- [ ] Simulate a shot (sharp shake of Primary device):
  - Beep plays immediately (correct pattern based on form)
  - Shot count increments on display
  - Serial shows full shot data (timestamp, cant, elevation, draw angle, flags)
- [ ] Multiple shots → buffer fills correctly
- [ ] Session start/stop works via button
- [ ] Form evaluation: intentionally tilt to exceed cant threshold → different beep
- [ ] No crashes over a 5-minute session with ~30 shots

---

## Subproject 9: Display UI + Menu + Threshold Configuration

### Goal
Multi-screen display UI with a menu system for threshold configuration.

### Files to Create
- `lib/display_ui/display_ui.h`
- `lib/display_ui/display_ui.cpp`
- `lib/menu/menu.h`
- `lib/menu/menu.cpp`

### Substeps

**9.1** Define display screens:
- `SCREEN_HOME` — status overview (session state, battery, connection)
- `SCREEN_LIVE` — real-time angles (pitch, roll, draw angle)
- `SCREEN_SESSION` — shot count, elapsed time, last shot summary
- `SCREEN_MENU` — threshold configuration menu

**9.2** Implement screen navigation:
- Button A short press: cycle screens (Home → Live → Session → Home)
- Button B short press: enter/exit menu (when on Home screen)
- In menu: Button A = next item, Button B = select/adjust

**9.3** Implement menu system:
- Menu items (6 configurable values + Save & Exit):
  - Cant Baseline / Cant Tolerance
  - Elev Baseline / Elev Tolerance
  - Draw Baseline / Draw Tolerance
- Adjust with button presses (A = +0.5°, B = -0.5°)
- Auto-exit editing after 2 seconds of no input
- Scrollable list (4 visible items at a time)
- Changes saved to NVS via ConfigStore on exit

**9.4** Display rendering:
- Use M5.Display (TFT_eSPI) methods
- Small font for labels, large font for values
- Color coding: green = within threshold, red = exceeded

### Verification
- [ ] Cycle through screens with button press
- [ ] Menu: adjust cant threshold from 5.0° to 3.0° → save → reboot → verify it's 3.0°
- [ ] Live screen: angles update in real-time as device is tilted
- [ ] Session screen: shows correct shot count and last shot data
- [ ] Color coding works (green/red based on thresholds)

---

## Subproject 10: BLE GATT Relay

### Goal
NimBLE GATT server on Primary that sends ShotPacket_t notifications to phone and accepts session control commands.

### Files to Create
- `lib/ble_relay/ble_relay.h`
- `lib/ble_relay/ble_relay.cpp`

### Substeps

**10.1** Add NimBLE dependency to `platformio.ini`:
```ini
lib_deps =
    m5stack/M5Unified
    h2zero/NimBLE-Arduino
```

**10.2** Define GATT service and characteristics (UUIDs):
- Service: `0000ARC0-...` (full 128-bit UUID based on custom base)
- Shot Data: notify characteristic (20 bytes)
- Session Control: write characteristic (1 byte: 0x01=start, 0x02=stop)
- Device Status: read/notify (battery, IMU status, connection)
- Threshold Config: read/write (3x float = 12 bytes)

**10.3** Implement BLE lifecycle:
- `init()` — create server, service, characteristics, start advertising
- `onConnect()` — stop advertising, set connected flag
- `onDisconnect()` — restart advertising
- `sendShot(ShotPacket_t*)` — notify if connected
- `isConnected()`

**10.4** Handle BLE + ESP-NOW coexistence:
- Both use the ESP32 radio — need to be on the same WiFi channel
- Set WiFi channel explicitly before ESP-NOW init
- BLE advertising intervals may need tuning (longer interval to reduce radio contention)

**10.5** Drain shot buffer on connect:
- When phone connects, send any buffered shots from ring buffer
- Then send new shots in real-time as they occur

**10.6** Handle session control commands:
- Phone writes 0x01 to Session Control → start session (same as button)
- Phone writes 0x02 → stop session

**10.7** Handle config sync:
- Phone writes 24 bytes (6 floats: 3 baselines + 3 tolerances) → update ConfigStore + save to NVS
- Phone can read current config from the same characteristic

**10.8** Integrate into main loop:
- After shot is buffered → `bleRelay.sendShot()` if connected
- Periodically update Device Status characteristic (battery level)

### Verification
- [ ] Install "nRF Connect" app on phone
- [ ] Device appears as "ArcheryWearable" in BLE scan
- [ ] Connect → see service and characteristics
- [ ] Simulate shots → see 20-byte notifications in nRF Connect
- [ ] Write 0x01 to Session Control → session starts on device
- [ ] Disconnect → reconnect → buffered shots drain automatically
- [ ] ESP-NOW still works while BLE is connected (both radios active)

---

## Subproject 11: Pre-Shot Routine Timer

### Goal
Detect the archer's raise-draw-release cadence and alert if timing is abnormal.

### Files to Modify
- `lib/shot_detect/shot_detect.h` (extend)
- `lib/shot_detect/shot_detect.cpp` (extend)
- `lib/feedback/feedback.h` (add patterns)

### Substeps

**11.1** Track timing between shot detection states:
- Time in `DRAW_DETECTED` → `HOLD` (draw duration)
- Time in `HOLD` → `SHOT_FIRED` (hold duration)
- Total raise-to-release time

**11.2** Maintain rolling average of timing:
- After N shots (e.g., 5), compute average cadence
- Flag if current shot's timing deviates by >30% from average

**11.3** Add feedback patterns:
- `RUSHING` — distinct pattern (fast descending beeps)
- `HOLDING_TOO_LONG` — distinct pattern (slow ascending beeps)

**11.4** Include timing data in serial output (for debugging)

### Verification
- [ ] After 5+ shots, vary timing intentionally → hear rushing/holding alerts
- [ ] Consistent timing → no extra alerts (just normal form feedback)
- [ ] Timing data visible in serial output

---

## Subproject 12: Real-Time Form Check + Feature Toggles + Menu Restructure

### Goal
Provide real-time audio feedback during the aiming/hold phase (not just after shots). Add toggleable feature settings and restructure menu into two-level hierarchy.

### Files to Create
- `lib/form_check/form_check.h`
- `lib/form_check/form_check.cpp`

### Files to Modify
- `lib/feedback/feedback.h` (add 3 new patterns)
- `lib/feedback/feedback.cpp` (pattern definitions)
- `lib/session/session.h` (add feature toggles to ConfigStore)
- `lib/session/session.cpp` (NVS load/save for toggles)
- `lib/menu/menu.h` (two-level menu structure)
- `lib/menu/menu.cpp` (rewrite navigation + rendering)
- `lib/ble_relay/ble_relay.h` (extend BleConfigPacket to 26 bytes)
- `lib/ble_relay/ble_relay.cpp` (apply/read toggles over BLE)
- `src/main_primary.cpp` (integrate FormChecker, conditional feedback)

### Substeps

**12.1** Add feature toggles to ConfigStore:
- `form_check_enabled_` (default: true) — NVS key `"form_chk"`
- `shot_feedback_enabled_` (default: true) — NVS key `"shot_fb"`

**12.2** Add 3 new beep patterns:
- `DRAW_NOTIFY` — 1500Hz, 40ms (brief "I see your draw")
- `FORM_ALIGNED` — 2200Hz double-blip (30ms + 40ms gap + 30ms) ("form is good")
- `FORM_DEVIATED` — 1000Hz, 60ms ("form drifted")

**12.3** Create FormChecker module:
- Tracks ShotDetector state transitions (prev_state_ vs current)
- Emits events: DRAW_STARTED, FORM_ALIGNED, FORM_DEVIATED
- Fires only on transitions (not every loop)
- Uses ConfigStore baselines/tolerances for alignment check
- Returns events to main loop (doesn't play sounds directly)

**12.4** Restructure menu into two-level hierarchy:
```
PARENT MENU          FEATURE SETTINGS      PARAM SETTINGS
├ Feature Settings → ├ Form Check [ON]     ├ Cant Base: 0.0°
├ Param Settings   → ├ Shot Feedback [ON]  ├ Cant Tol: 5.0°
└ Exit               └ Back                ├ Elev Base: 0.0°
                                           ├ Elev Tol: 10.0°
                                           ├ Draw Base: 0.0°
                                           ├ Draw Tol: 15.0°
                                           └ Back
```
- BtnA = next item, BtnB = select/toggle/edit
- Feature toggles: BtnB = instant toggle (no edit mode)
- NVS save on parent-level Exit only

**12.5** Integrate into main loop:
- FormChecker.update() after shotDetector.update()
- Play form check beeps if form_check_enabled
- Wrap post-shot feedback with shot_feedback_enabled check
- Reset FormChecker on shot detected and session start

**12.6** Extend BLE config packet:
- BleConfigPacket grows from 24 → 26 bytes (add 2 uint8_t for toggles)

### Verification
- [ ] Build compiles (`pio run -e primary`)
- [ ] Menu: BtnB opens parent menu with 3 items
- [ ] Feature Settings: toggle Form Check ON/OFF, toggle Shot Feedback ON/OFF
- [ ] Param Settings: same as before (6 params + Back)
- [ ] Settings persist across reboot (NVS)
- [ ] Form Check ON: draw → beep; hold with good form → double-blip; deviate → low blip
- [ ] Form Check OFF: no beeps during draw/hold
- [ ] Shot Feedback ON/OFF: controls post-shot beeps
- [ ] Serial output shows [FormCheck] events
- [ ] BLE: 26-byte config characteristic

---

## Subproject 13: IMU Calibration + Usage Manual

### Goal
Provide guided on-device calibration flows so the archer can quickly set up both devices at the range, and write a comprehensive usage manual.

### Files to Create
- `lib/calibration/calibration.h` (CalibrationManager class, CalibrationState enum)
- `lib/calibration/calibration.cpp` (calibration flows, display rendering, accel sampling)
- `claude-memory/USAGE_MANUAL.md` (user-facing documentation)

### Files to Modify
- `lib/session/session.h` (add accel bias fields to ConfigStore)
- `lib/session/session.cpp` (NVS load/save for accel bias: `cal_ax`, `cal_ay`, `cal_az`)
- `lib/imu_math/complementary_filter.h/.cpp` (accept and apply accel bias offsets)
- `lib/menu/menu.h/.cpp` (add "Calibrate" parent item → sub-menu with Set Baseline / Flat Cal / Back)
- `src/main_primary.cpp` (integrate CalibrationManager, apply bias to IMU pipeline)

### Design: Calibration Flows

**Problem**: Archer can't press a button while holding proper form without disturbing position.
**Solution**: Countdown approach — press button first, then get into position.

**Flow A — "Set Baseline" (use at the range, every session or remount)**
1. User selects "Calibrate → Set Baseline" from menu
2. Screen: `SET BASELINE` / `Press A to start`
3. User presses A
4. Screen: countdown `Get into form... 5...4...3...2...1`
5. Screen: `Capturing...` — averages Primary pitch/roll + Secondary draw angle over 2 seconds
6. Saves averaged values as cant_baseline, elev_baseline, draw_baseline in ConfigStore → NVS
7. Screen: `Saved! C:X.X° E:X.X° D:X.X°` for 2 seconds
8. Returns to menu

**Flow B — "Flat Cal" (one-time accel bias calibration)**
1. User selects "Calibrate → Flat Cal" from menu
2. Screen: `FLAT CALIBRATION` / `Place flat, press A`
3. User places both devices flat on level surface, presses A
4. Screen: `Sampling...` — averages raw accel for 2 seconds
5. Computes bias: `bias = measured - expected_gravity` per axis (expected: ax=0, ay=0, az=1.0g)
6. Saves bias to NVS (`cal_ax`, `cal_ay`, `cal_az`)
7. Screen: `Bias saved!` + shows bias values for 2 seconds
8. Returns to menu

**Menu structure with calibration:**
```
PARENT MENU
├ Feature Settings → [toggles]
├ Param Settings   → [6 values]
├ Calibrate        →
│   ├ Set Baseline   (countdown → capture form angles → save)
│   ├ Flat Cal       (place flat → capture accel bias → save)
│   └ Back
└ Save & Exit
```

### Substeps

**13.1** Add accel bias fields to ConfigStore:
- 3 new floats: `cal_ax_bias_`, `cal_ay_bias_`, `cal_az_bias_` (default 0.0)
- NVS keys: `cal_ax`, `cal_ay`, `cal_az`
- Getters/setters for each

**13.2** Apply accel bias in complementary filter:
- `setBias(float bx, float by, float bz)` method
- In `update()`: subtract bias from raw accel before computing angles
- Main loop calls `filter.setBias(...)` after ConfigStore load

**13.3** Create CalibrationManager module:
- CalibrationState enum: `IDLE`, `BASELINE_PROMPT`, `BASELINE_COUNTDOWN`, `BASELINE_CAPTURE`, `BASELINE_DONE`, `FLATCAL_PROMPT`, `FLATCAL_CAPTURE`, `FLATCAL_DONE`
- `startBaseline()` / `startFlatCal()` — enter the respective flow
- `update(now, pitch, roll, drawAngle, accel[3])` — advance state machine, sample data
- `draw(HalInterface*)` — render calibration screens (takes over display)
- `isActive()` / `isDone()` — for main loop to know when to hand back control
- Internal: accumulate samples during capture phase, compute averages, save to ConfigStore

**13.4** Add "Calibrate" sub-menu to Menu:
- New `ParentItem::CALIBRATE` between PARAM_SETTINGS and EXIT
- New `MenuLevel::CALIBRATE` with items: Set Baseline, Flat Cal, Back
- Selecting Set Baseline / Flat Cal → sets a flag that main loop reads to launch CalibrationManager

**13.5** Integrate CalibrationManager in main_primary.cpp:
- When menu signals calibration start → enter calibration mode (CalibrationManager takes over display + buttons)
- BtnA during calibration = confirm/proceed
- BtnB during calibration = cancel (return to menu)
- After calibration completes → apply new bias to filter, return to normal display

**13.6** Write Usage Manual (`claude-memory/USAGE_MANUAL.md`):
- **Hardware Overview**: M5StickC button locations (BtnA = front face, BtnB = side), wearing positions (Primary = bow wrist, Secondary = draw wrist)
- **Power On/Off**: Side button hold to power on/off
- **Screen Navigation**: BtnA tap cycles HOME → LIVE → SESSION → HOME
- **Menu System**: BtnB opens menu, BtnA = next item, BtnB = select/toggle/edit, auto-timeout on param editing
- **Calibration Procedure**: Step-by-step for both Set Baseline and Flat Cal, when to use each
- **Running a Session**: Hold BtnA to start, shoot, hold BtnA to stop
- **Understanding Beeps**: Table of all beep patterns and what they mean
- **BLE Connection**: How to connect with phone app or nRF Connect
- **Troubleshooting**: Common issues (color, connection, false triggers)

### Verification
- [ ] Build compiles (`pio run -e primary`)
- [ ] Menu shows "Calibrate" item, sub-menu has Set Baseline / Flat Cal / Back
- [ ] Set Baseline: countdown runs, angles captured, baselines saved to NVS
- [ ] Set Baseline: values survive reboot
- [ ] Set Baseline: with Secondary connected, draw baseline captures Secondary's pitch
- [ ] Flat Cal: place flat, bias computed and saved, applied on next boot
- [ ] Flat Cal: angles are more accurate after calibration (less drift when stationary)
- [ ] Cancel (BtnB) during either flow returns to menu without saving
- [ ] Usage manual is complete and accurate

---

## Subproject 14: Final Polish

### Goal
Clean up serial output, harden error handling, optimize power, and add version info.

### Files to Modify
- `src/main_primary.cpp` (serial formatting, error paths, display brightness)
- `src/main_secondary.cpp` (same)
- `lib/hal/hal_m5unified.cpp` (brightness control API)

### Substeps

**14.1** Review and clean all serial output for consistent formatting

**14.2** Add error handling for critical init failures (IMU, ESP-NOW, BLE) — display error screen instead of silent failure

**14.3** Power optimization: reduce display brightness during active session (screen is less useful while shooting), restore on session end

**14.4** Add firmware version string to boot splash (e.g., "v1.1")

### Verification
- [ ] Serial output is clean, consistent, and readable
- [ ] Unplug IMU (if possible) or simulate failure → error displayed
- [ ] Start session → display dims; stop session → display brightens
- [ ] Boot splash shows version string
- [ ] Full system runs stable for 15+ minutes without crash

---

## Key Technical Notes

### M5Unified API Reference
- IMU: `M5.Imu.getImuData(&data)` → `data.accel.x/y/z`, `data.gyro.x/y/z` (float, g and dps)
- Speaker: `M5.Speaker.tone(freq_hz, duration_ms)`, `M5.Speaker.stop()`
- Display: `M5.Display.setTextSize()`, `.setCursor()`, `.print()`, `.fillScreen()`
- Buttons: `M5.BtnA.wasPressed()`, `.wasHold()`, `.isPressed()`
- Power: `M5.Power.getBatteryLevel()` → 0-100
- Init: `M5.begin()` auto-detects Plus vs Plus2

### ESP-NOW + BLE Coexistence
- Both share the ESP32 radio — WiFi channel must be set explicitly
- Use `esp_wifi_set_channel()` to fix the channel before ESP-NOW init
- BLE advertising interval: 100ms minimum to leave radio time for ESP-NOW
- If issues arise: reduce ESP-NOW send rate from 50Hz to 25Hz

### NVS Storage Keys
- `cant_thresh` (float), `elev_thresh` (float), `draw_thresh` (float)
- `cal_ax_bias`, `cal_ay_bias`, `cal_az_bias` (float)
- `paired_mac` (6 bytes)

---

## End State

After all 14 subprojects, the system delivers:
- **Two-device wearable** with ESP-NOW communication
- **Real-time shot detection** with form evaluation
- **Audio feedback** (9+ distinct patterns including real-time form check)
- **BLE relay** to Android app (ShotPacket_t, 20 bytes per shot)
- **Menu system** with persistent threshold configuration and feature toggles
- **Cadence monitoring** with rushing/holding alerts
- **Guided calibration** (form baseline capture + accel bias correction)
- **Usage manual** for end-user reference
- **HAL architecture** ready for AtomS3R/ESP32-S3 upgrade (only HAL layer changes)
