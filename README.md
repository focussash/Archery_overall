# Archery

A personal training system for traditional thumb-draw archery. Three sub-projects live side by side. Each has its own toolchain, docs and `CLAUDE.md`:

| Folder | What it is | Toolchain |
|---|---|---|
| `wearable m5stick/` | Firmware for the M5Stick IMU wearable: shot-phase detection, buzzer cues, BLE shot packets | PlatformIO (ESP32) |
| `Archery_android_app/` | ArcheryApp: scoring, sessions, stats. Will host the video-mocap module (`:feature:mocap`) | Android (Kotlin, Compose, Room) |
| `Archery assistant/` | Video 2D motion capture: pose → shot phases → metrics → feedback. Python reference implementation and the contracts shared with the app | Python 3.12 (conda env `archery-mocap`) |

Open each sub-project in its own IDE: Android Studio for `Archery_android_app/`, VS Code + PlatformIO for `wearable m5stick/`.

## History

This repository merges two earlier repositories and keeps their full history:

- `wearable-m5stick` (branch `main`) → `wearable m5stick/`
- `ArcheryApp` (branch `Taking-picture`) → `Archery_android_app/`

Commit hashes from the old repositories are unchanged. Commits made before the import still have that sub-project's files at the repository root. Use `git log --follow <path>` to trace a file across the move. The old repositories are archived read-only.
