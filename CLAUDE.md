# CLAUDE.md — Binding Instructions for Claude Code

These instructions govern ALL work in this repository. They OVERRIDE default behavior. Follow them exactly.

## FIRST STEP — EVERY Session

**ALWAYS** read the `claude-memory/` folder (`DEVELOPMENT_PLAN.md`, `achievements.md`, `overall_design.md`) BEFORE writing any code or making any decisions. No exceptions.

## Development Rules

1. **ALWAYS follow the plan.** Refer to `DEVELOPMENT_PLAN.md` before writing ANY code. The plan defines implementation order and specifications. NEVER deviate without approval.

2. **NEVER modify `DEVELOPMENT_PLAN.md` without explicit user permission.** Even with auto-accept enabled, you MUST ask first. Never silently change the plan.

3. **ALWAYS sync agreed discussions into the plan.** When the user agrees to changes that deviate from or add to `DEVELOPMENT_PLAN.md`, you MUST immediately update the plan file. Agreed-upon changes must NEVER remain only in conversation.

4. **Modularity is mandatory.** NEVER write monolith code. Split functionality into small, focused modules. Each file = single responsibility.

## Progress Tracking — CRITICAL, NON-NEGOTIABLE

- **After EVERY substep** → IMMEDIATELY update `achievements.md` with what was accomplished. Do NOT wait until a full subproject is complete.
- **Before EVERY new piece of work** → Re-read `DEVELOPMENT_PLAN.md` AND `achievements.md` to stay aligned with the plan and aware of current progress.
- **One subproject at a time.** Complete and validate one subproject, then STOP. Wait for user confirmation before starting the next.
- **Validate before proceeding.** After implementing a subproject, run tests and show results to the user. NEVER proceed until validation passes.
- **Document everything.** Include test counts, file changes, any deviations from the plan (e.g., using `cnlunar` instead of `sxtwl`), and any issues encountered.

## Environment

**Platform**: PlatformIO + C++ (Arduino framework), targeting ESP32.

**PlatformIO CLI path**: `C:/Users/Zijie Jin/.platformio/penv/Scripts/pio.exe`

**Hardware**: Two M5StickC devices (Plus / Plus2 mix). Firmware auto-detects variant via M5Unified.

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

- **Bash commands**: Run ALL freely — builds, tests, uploads, serial monitor, git, etc.
- **File edits**: Proceed freely for implementation work.
- **NEVER install, uninstall, or modify ANY packages/dependencies** (pip install, npm install, scoop install, etc.) without EXPLICITLY asking the user first. This applies to ALL package managers and ALL environments — including PlatformIO's internal Python env. If a missing module blocks a build, report it and ask the user how to proceed.

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
