# Overall Design: Archery Wearable System

## System Concept

A two-device wearable system for **asiatic traditional archery** (thumb draw, laminated wood bow, no sights) that provides **real-time sensing, audio feedback, and BLE data relay** to improve consistency. An existing **Android companion app** ([ArcheryApp](https://github.com/focussash/ArcheryApp)) handles session management, scoring, statistics, history, and data export — the M5StickC devices focus exclusively on what must happen on the body: sensing, immediate feedback, and wireless data transmission.

**Core principle**: All technology goes on the human — the bow stays completely unmodified.

---

## Hardware

### Device: M5StickC (Plus or Plus2)

Each unit provides:
- **ESP32 microcontroller** (WiFi + Bluetooth + ESP-NOW)
- **6-axis IMU** (accelerometer + gyroscope) — no magnetometer
- **Small TFT display** (useful for setup/config, not usable during shooting)
- **Built-in buzzer** (audio feedback)
- **Physical button(s)** (user input)
- **LiPo battery** (built-in)
- **Grove port** (for future sensor expansion, e.g., magnetometer — see [Magnetometer section](#future-feature-magnetometer--compass-referenced-aiming))

### Two-Device Layout

| Device | Placement | Role |
|--------|-----------|------|
| **Bow-hand unit** | Wrist or forearm of bow hand | Measures bow arm pitch, cant angle, aiming elevation |
| **Draw-hand unit** | Near elbow or upper arm of draw hand | Measures draw arm angle, anchor point consistency |

Both devices communicate wirelessly. One device acts as the **primary** (coordinates timing, aggregates data, drives feedback); the other acts as **secondary**.

---

## Sensor Capabilities (6-Axis IMU)

With accelerometer + gyroscope only (no magnetometer), the system can measure:

- **Pitch** (arm elevation angle) — aiming height
- **Roll** (bow cant angle) — bow tilt consistency
- **Angular velocity** — speed and smoothness of movements
- **Acceleration spikes** — shot release detection
- **Relative angle changes** — draw length consistency (via draw-arm angle delta)

**Cannot measure** (no magnetometer): absolute compass heading, cardinal direction of aim. The 6-axis IMU is sufficient for all core features (Tier 1-3). For compass-referenced aiming, see [Future Feature: Magnetometer / Compass-Referenced Aiming](#future-feature-magnetometer--compass-referenced-aiming).

---

## Features

### Tier 1 — Core (MVP)

- **Shot detection**: Detect the moment of release via accelerometer spike on bow-hand unit
- **Bow cant monitoring**: Measure roll angle at the moment of release; beep if cant exceeds a user-set threshold
- **Aiming elevation tracking**: Record pitch angle at release for each shot
- **Draw arm angle at release**: Capture draw-hand arm angle to assess anchor consistency
- **Audio feedback per shot**: Immediate beep pattern after release indicating good/bad form
- **Shot data buffering**: Store per-shot data (timestamp, cant, elevation, draw angle) in a local ring buffer for BLE relay to the Android app

### Tier 2 — Training Aids & App Integration

- **Threshold configuration**: Adjustable cant/angle thresholds via button-driven menu on the display
- **Shot counting**: Track arrow count per session (useful for training volume tracking)
- **Pre-shot routine timer**: Detect raise-draw-release cadence; alert if the archer is rushing or holding too long
- **BLE data relay to Android app**: Per-shot packet transmission over BLE GATT (see [BLE section](#ble-device-to-phone))

### Tier 3 — Advanced On-Device

- **Fatigue monitoring**: Detect degrading form over a session (increasing cant variance, dropping draw angle) — real-time detection on-device, detailed analysis in the app
- **Draw weight estimation**: Use draw-arm acceleration profile to estimate relative draw effort
- **Calibration routines**: Guided IMU calibration procedure (gravity reference, rest position, axis mapping)

### Tier 4 — Future / Hardware Upgrade Required

- **Voice commands**: Hands-free control ("start session", "read stats", "set cant threshold 5 degrees") via **xiaozhi-esp32** — requires ESP32-S3 hardware (see Upgrade Path below)
- **Voice feedback**: Spoken results instead of beep codes
- **Video sync**: Timestamp correlation with phone video for form review
- **Magnetometer / compass-referenced aiming**: Absolute heading at release for aim consistency — see [dedicated section](#future-feature-magnetometer--compass-referenced-aiming)

### Features Delegated to Android Companion App

The following features are handled by [ArcheryApp](https://github.com/focussash/ArcheryApp), not on the M5StickC:

| Feature | Reason |
|---------|--------|
| Consistency scoring | App has full session history for meaningful comparison |
| Session statistics & charts | App already implements statistics, calendar, and chart views |
| Session comparison | App stores all sessions in Room DB with full query capability |
| Data export (CSV/JSON) | App already supports data export |
| Multi-archer profiles | App has user/profile management |
| Shot log persistence | App uses Room database; device only needs a ring buffer for BLE relay |
| Phone companion app | Already exists — [ArcheryApp](https://github.com/focussash/ArcheryApp) |

---

## Communication Architecture

### ESP-NOW (Device-to-Device)

- **Protocol**: ESP-NOW — peer-to-peer, low-latency, no WiFi router needed
- **Why ESP-NOW**: Sub-millisecond latency, works outdoors with no infrastructure, low power
- **Topology**: Primary ↔ Secondary (direct pair)
- **Data flow**:
  - Secondary sends IMU readings to Primary at a fixed rate (e.g., 50-100 Hz)
  - Primary fuses both IMU streams, detects shots, computes metrics, drives feedback
- **Pairing**: Hardcoded MAC addresses or one-button pairing at startup

### BLE (Device-to-Phone)

The M5StickC firmware is the **authority** on the BLE protocol design. The Android app's Phase 3 BLE implementation will adapt to conform to this protocol — not the other way around.

**GATT Service Layout**:

| Service/Characteristic | UUID (TBD) | Direction | Description |
|------------------------|------------|-----------|-------------|
| Archery Service | `0xARC0` | — | Primary service |
| Shot Data | `0xARC1` | Device → App | Per-shot data notification (see packet format) |
| Session Control | `0xARC2` | App → Device | Commands: start/stop session, request sync |
| Device Status | `0xARC3` | Device → App | Battery level, IMU status, connection state |
| Threshold Config | `0xARC4` | Bidirectional | Read/write cant and angle thresholds |

**Shot Data Packet Format** (~20 bytes binary struct):

```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;     // Millis since session start
    int16_t  cant_angle;       // Roll in 0.01 degree units
    int16_t  elevation_angle;  // Pitch in 0.01 degree units
    int16_t  draw_angle;       // Draw arm angle in 0.01 degree units
    int16_t  draw_velocity;    // Peak angular velocity
    uint8_t  shot_number;      // Shot index within session
    uint8_t  form_flags;       // Bitfield: cant_ok, elevation_ok, draw_ok, fatigue_flag
    int16_t  heading;          // Compass heading in 0.01 degrees (0xFFFF if no magnetometer)
    uint16_t reserved;         // Future use
} ShotPacket_t;               // 20 bytes
```

**Connection Lifecycle**:
- Device advertises when powered on (low-duty cycle to save power)
- App initiates connection; device accepts and stops advertising
- Shot data sent as BLE notifications (no polling)
- If connection drops, device buffers shots in ring buffer and retransmits on reconnect
- Session start/stop can be triggered from either device button or app command

### Data Flow Diagram

```
                    ┌─────────────────────────┐
                    │   Android Companion App │
                    │  (ArcheryApp)           │
                    │  - Session management   │
                    │  - Scoring & statistics  │
                    │  - History & export      │
                    └────────────┬────────────┘
                                 │ BLE (GATT)
                                 │
[Draw-hand unit]  --ESP-NOW-->  [Bow-hand unit (Primary)]
   IMU data                      IMU data (local)
                                 Shot detection
                                 Angle computation
                                 Real-time form feedback
                                 Buzzer feedback
                                 Shot ring buffer
                                 BLE data relay
                                 Display (config only)
```

---

## Companion Android App Integration

### App Overview

**Repository**: [ArcheryApp](https://github.com/focussash/ArcheryApp)
**Tech stack**: Kotlin, Jetpack Compose, Room database, Hilt DI, MVVM architecture
**Current capabilities** (Phases 1-2, implemented):
- Session management with ends and individual arrows
- Scoring: OpenCV-based target photo scoring + manual score entry
- Calendar view with session history
- Statistics and charts (score trends, arrow groupings)
- Data export

**Phase 3** (M5StickC BLE integration): Planned but not yet built. The BLE specs in the app's Phase 3 plan are placeholders — the protocol defined in this document is authoritative, and the app will be updated to match before implementation.

### Key Data Models (App Side)

| Model | Description | M5StickC Relevance |
|-------|-------------|-------------------|
| `Session` | A shooting session with metadata | Created when device starts session or app starts session |
| `End` | A group of arrows (typically 3-6) | Could be signaled by device (pause detection) or app |
| `Arrow` | Individual arrow with score + form data | Maps 1:1 to a `ShotPacket_t` from the device |
| `AngleReading` (planned) | Per-shot IMU angles and form metrics | Directly populated from `ShotPacket_t` fields |

### Integration Points

| Integration | Direction | Description |
|-------------|-----------|-------------|
| Shot event | Device → App | Each shot triggers a BLE notification with `ShotPacket_t` |
| Session lifecycle | Bidirectional | Start/stop from device button or app command via `Session Control` characteristic |
| Threshold sync | App → Device | App can push updated thresholds to device |
| Form + score correlation | App only | App correlates device form data with arrow scores for analysis |

### What M5StickC Firmware Does NOT Need

- Persistent storage (no flash logging — ring buffer only)
- Statistics computation (app handles all analysis)
- Data export (app handles CSV/JSON export)
- User profiles (app manages profiles)
- Score tracking (app handles scoring via camera or manual entry)

### What M5StickC Firmware DOES Need

- BLE GATT service with characteristics defined above
- Binary packet serialization (`ShotPacket_t`)
- Command reception and parsing (session control, threshold updates)
- Ring buffer for shot data (survives brief BLE disconnections)
- Session state machine (started/stopped, shot counting)

**Note**: M5StickC firmware is developed first. The BLE protocol defined in this document is authoritative. The app's Phase 3 implementation plan will be updated to conform to this protocol before any app-side BLE work begins.

---

## Feedback Mechanisms

Since the archer **cannot look at a screen while shooting**, feedback is primarily audio:

### Audio (Buzzer/Beep Patterns)

| Pattern | Meaning |
|---------|---------|
| Single short beep | Shot detected |
| Double beep (high) | Good form (within thresholds) |
| Long low tone | Cant exceeded threshold |
| Triple fast beep | Draw angle inconsistent |
| Rising tone | Session started |
| Falling tone | Session ended |

Beep patterns are configurable. All patterns should be distinct and learnable.

### Display (Non-Shooting Use)

- **Pre-session**: Configuration menus, threshold adjustment
- **Post-session**: Quick summary on device; detailed stats and history available in the Android app
- **Between ends**: Quick glance at last-end stats on device; full end analysis in the app

### Haptic (Future)

- Vibration motor via Grove port — possible future addition for silent feedback

---

## Input

### Current: Button-Based

- M5StickC has 1-2 physical buttons
- Menu navigation: short press / long press / double press
- Sufficient for: start/stop session, cycle display pages, adjust thresholds
- Limitation: slow for complex configuration

### Future: Voice Commands

- Requires hardware upgrade to **ESP32-S3** (see Upgrade Path)
- Integration with **xiaozhi-esp32** for on-device voice recognition
- Enables fully hands-free operation during shooting

---

## Hardware Abstraction Layer (HAL) Strategy

To support the current M5StickC and a future upgrade path, all code is structured with a clean hardware abstraction boundary.

### Architecture

```
┌─────────────────────────────────────┐
│         Feature / Application       │
│  (shot detection, ESP-NOW protocol, │
│   angle math, BLE relay, logging)   │
├─────────────────────────────────────┤
│        Hardware Abstraction Layer   │
│  (HAL interface)                    │
├──────────┬──────────┬───────────────┤
│ M5StickC │ M5StickC │   AtomS3R    │
│  Plus    │  Plus2   │  (future)    │
└──────────┴──────────┴───────────────┘
```

### HAL Responsibilities

The HAL encapsulates all hardware-specific details:

- **IMU initialization** and raw data reading (different IMU chips across models)
- **Pin assignments** (button GPIOs, I2C pins, etc.)
- **Buzzer/speaker API** (tone generation — some models have buzzer, others have speaker with I2S)
- **Display initialization** (different TFT drivers/resolutions)
- **Button mapping** (number of buttons, GPIO numbers)
- **Power management** (battery reading, sleep modes)
- **BLE peripheral stack** (NimBLE initialization, GATT service registration, advertising)
- **Magnetometer reading** (abstracted `getHeading()` — returns valid heading or sentinel if no magnetometer present)

### What Stays Hardware-Agnostic

All of these are written once and never change across hardware:

- IMU math (angle calculation, filtering, sensor fusion)
- ESP-NOW communication protocol and message formats
- Shot detection algorithm
- Beep pattern definitions (expressed as frequency + duration sequences)
- Shot buffer data structures (ring buffer)
- BLE shot data packet format and transmission logic
- Magnetometer fusion algorithm (when magnetometer available)
- Menu state machine
- Session management

---

## Upgrade Path: M5StickC → ESP32-S3

### Why Upgrade Eventually

- **xiaozhi-esp32** (voice AI framework) requires **ESP32-S3** — it does **NOT** support the original ESP32-PICO used in M5StickC Plus/Plus2
- ESP32-S3 has a built-in USB-OTG and vector instructions useful for audio processing
- More GPIO, more RAM for buffering
- Some S3 boards include 9-axis IMU (with magnetometer) — see [Magnetometer section](#future-feature-magnetometer--compass-referenced-aiming)

### What This Means in Practice

| Concern | Impact |
|---------|--------|
| Core features (Tier 1-2) | **No blocker** — all work on current M5StickC, including BLE relay |
| Advanced on-device (Tier 3) | **No blocker** — works on current M5StickC |
| Voice commands (Tier 4) | **Requires ESP32-S3 hardware** |
| Magnetometer (Tier 4) | External Grove module works on current M5StickC; onboard magnetometer with AtomS3R or CoreS3 |
| Code migration | Only HAL layer changes; all feature code is portable |
| Timeline | Upgrade when voice features become a priority, not before |

### Candidate Upgrade Hardware

- **M5Stack AtomS3R + Atomic Echo Base** (recommended): ESP32-S3, BMI270+BMM150 (9-axis with magnetometer), official xiaozhi support, mic+speaker via Echo Base, very small form factor. Checks every box: voice commands, compass heading, and all core features.
- **M5Stack CoreS3**: Same 9-axis sensors (BMI270+BMM150) + xiaozhi support, but much larger (54x54mm) — less wearable-friendly.
- **M5StickS3**: Right form factor but only 6-axis (no magnetometer), no official xiaozhi support.
- **Note**: CoreS3 SE is a cost-down version with **no IMU/magnetometer** — not suitable.

### Migration Process

1. Write a new HAL implementation for the target ESP32-S3 board
2. Update pin definitions, IMU driver, display driver
3. All feature code compiles and runs without changes
4. Add xiaozhi-esp32 integration as a new module on top
5. Add magnetometer HAL if target has 9-axis IMU (AtomS3R, CoreS3)

---

## Future Feature: Magnetometer / Compass-Referenced Aiming

### Motivation

In asiatic traditional archery — with no sights on the bow — the archer's **body orientation IS the aiming system**. A compass heading at the moment of release tells you:

- **Where you were actually aiming** (absolute direction)
- **How consistent your aim direction is** across shots (heading spread)
- **Wind reference**: correlate heading with wind direction for drift compensation
- **Target bearing**: compare aim heading to known target direction

This is uniquely valuable for sightless archery — it turns the archer's body into a compass-referenced aiming platform.

### Hardware Options

| Option | Works With | Magnetometer | Cost | Notes |
|--------|-----------|-------------|------|-------|
| **External QMC5883L** (Grove I2C) | Current M5StickC | QMC5883L | ~$3 | Plug into Grove port, immediate use |
| **External BMM150** (Grove I2C) | Current M5StickC | BMM150 | ~$5 | Higher quality, same interface |
| **AtomS3R** (recommended upgrade) | — | BMM150 onboard | ~$15 | No external module needed, plus voice support |
| **CoreS3** | — | BMM150 onboard | ~$40 | Also has onboard magnetometer, but too large for wrist wear |

**Recommended path**: If staying on M5StickC, use a Grove QMC5883L module (~$3, plug-and-play). If upgrading, the AtomS3R gives you the magnetometer for free alongside voice capability and ESP32-S3.

### HAL Impact

- New `getCompassHeading()` method in HAL interface (returns heading in degrees, or sentinel value if no magnetometer)
- Magnetometer calibration routine (hard-iron and soft-iron compensation — important near metal bow components)
- Upgrade from 6-axis complementary/Madgwick filter to 9-axis AHRS when magnetometer is available
- HAL auto-detects magnetometer presence and enables/disables heading features accordingly

### Data Flow

```
Magnetometer → Sensor fusion (9-axis AHRS) → Heading at release
    → ShotPacket_t.heading field → BLE → Android app
    → App: polar plot (heading vs score), heading consistency, wind correlation
```

### Priority

**Tier 4** — Core form feedback (cant, elevation, draw angle) works perfectly without compass heading. Magnetometer adds a new dimension of analysis but requires either extra hardware or a board upgrade, plus calibration for accuracy near metal. Implement after core features are stable.

---

## Development Language & Framework

- **Language**: C++ (Arduino framework) or MicroPython — TBD based on xiaozhi-esp32 compatibility and performance needs
- **Build system**: PlatformIO (recommended for ESP32 Arduino development)
- **Libraries**: ESP-NOW (built into ESP-IDF), IMU driver library (model-specific), display library (TFT_eSPI or similar), NimBLE (BLE peripheral stack), magnetometer driver (QMC5883L or BMM150, when applicable)

---

## Open Questions (To Be Decided During Implementation)

- Exact IMU sample rate and filtering approach (complementary filter vs. Madgwick)
- Shot detection algorithm details (threshold, debounce, confirmation window)
- Which device is primary by default (bow-hand is proposed, but could be configurable)
- Button interaction scheme (exact press patterns for each action)
- Power budget and expected battery life per session
- BLE GATT service/characteristic UUID assignment (custom 128-bit UUIDs vs. short UUIDs)
- BLE shot data packet format: binary struct (proposed above) vs. CBOR encoding
- Timestamp synchronization between M5StickC and Android app (device millis vs. app wall-clock)
- Continuous IMU streaming vs. per-shot packet transmission (tradeoff: bandwidth vs. analysis granularity)
- Magnetometer calibration approach (hard-iron / soft-iron compensation near metal bow components)
- Single vs. dual BLE device connections from the app (can one phone connect to both M5StickC units, or only Primary?)
