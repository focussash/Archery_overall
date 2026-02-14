# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Important**: Always check the `claude-memory/` folder for project context, plans, and progress tracking before starting any work.

## Development Guidelines

- **Follow the plan**: Always refer to `DEVELOPMENT_PLAN.md` before writing any code. The plan defines the implementation order and specifications.
- **Plan changes require approval**: Even with auto-accept enabled, **you MUST ask the user for explicit permission** before modifying DEVELOPMENT_PLAN.md. Never silently iterate on the plan.
- **Sync discussions to plan**: Whenever the user and I discuss items that deviate from or add to the existing `DEVELOPMENT_PLAN.md`, and the user agrees with the discussed changes, I must immediately update `DEVELOPMENT_PLAN.md` to incorporate them with proper modifications. Agreed-upon plan changes should never remain only in conversation.
- **Modularity is mandatory**: Avoid monolith code at all costs. Split functionality into small, focused modules. Each file should have a single responsibility.

### Progress Tracking (CRITICAL)

- **After EVERY substep**: Update `achievements.md` immediately with what was accomplished. Do not wait until a full subproject is complete.
- **Re-read claude-memory/ after every step**: Before starting any new work, re-read `DEVELOPMENT_PLAN.md` and `achievements.md` to ensure you are still aligned with the plan and aware of current progress.
- **One subproject at a time**: Complete and validate one subproject before moving to the next. Stop after validation and wait for user confirmation before proceeding.
- **Validation before proceeding**: After implementing a subproject, run tests and show results to the user. Do not proceed until validation passes.
- **Document everything**: Include test counts, file changes, any deviations from the plan (e.g., using `cnlunar` instead of `sxtwl`), and any issues encountered.

## Environment

**Platform**: PlatformIO + C++ (Arduino framework), targeting ESP32.

**PlatformIO CLI path**: `C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe`

**Hardware**: Two M5StickC devices (Plus / Plus2 mix). The firmware auto-detects the variant via M5Unified.

**Key library**: [M5Unified](https://github.com/m5stack/M5Unified) — unified API for IMU, display, speaker, buttons, and power across M5StickC Plus and Plus2.

### Build & Test Commands

```bash
# Build primary firmware
"C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe" run -e primary

# Build secondary firmware
"C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe" run -e secondary

# Run native unit tests
"C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe" test -e native_test

# Upload to device (primary example)
"C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe" run -e primary -t upload

# Serial monitor
"C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe" device monitor -b 115200
```

### PlatformIO Environments (platformio.ini)

| Environment | Purpose | Board | Key Build Flag |
|-------------|---------|-------|----------------|
| `primary` | Primary device firmware | m5stick-c | `-DROLE_PRIMARY` |
| `secondary` | Secondary device firmware | m5stick-c | `-DROLE_SECONDARY` |
| `native_test` | Desktop unit tests (Unity) | native | `-DNATIVE_TEST` |

## Permissions

- **Bash**: You may run ALL bash commands freely (dangerously skip permissions) **UNLESS** you are deleting files/folders or installing non-PlatformIO things. Builds, tests, uploads, serial monitor, git commands, etc. are all fine without asking.
- **File edits**: Proceed freely for implementation work.

## Architecture

### Project Structure

```
wearable m5stick/
├── platformio.ini              # 3 environments: primary, secondary, native_test
├── src/
│   ├── main_primary.cpp        # Primary device entry point
│   └── main_secondary.cpp      # Secondary device entry point
├── lib/
│   ├── hal/                    # Hardware abstraction (M5Unified wrapper)
│   ├── imu_math/               # Sensor fusion, angle calculation
│   ├── shot_detect/            # Shot detection state machine
│   ├── feedback/               # Beep pattern engine
│   ├── shot_buffer/            # Ring buffer + ShotPacket_t
│   ├── session/                # Session state machine + NVS config
│   ├── espnow_comm/            # ESP-NOW protocol (future)
│   ├── display_ui/             # Display screens (future)
│   ├── menu/                   # Menu system (future)
│   └── ble_relay/              # NimBLE GATT service (future)
├── test/
│   ├── test_imu_math/          # Native unit tests
│   ├── test_shot_detect/
│   └── test_shot_buffer/
└── claude-memory/
    ├── DEVELOPMENT_PLAN.md
    ├── achievements.md
    └── overall_design.md
```
