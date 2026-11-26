# CLAUDE.md — Binding Instructions for Claude Code

These instructions govern ALL work in this repository. They OVERRIDE default behavior. Follow them exactly.

## FIRST STEP — EVERY Session

**ALWAYS** read the `claude-memory/` folder (`DEVELOPMENT_PLAN.md`, `achievements.md`, `architecture.md`, `DECISIONS.md`, `RESUME.md`) BEFORE writing any code or making any decisions. Read the design brief `射箭视频mocap + M5Stick 训练平台 — 草稿.md` (Chinese) when a task touches scope, feedback semantics, metrics, or cross-project integration. No exceptions.

## What This Project Is

Video-based 2D motion capture for traditional thumb-draw archery: one front-facing phone camera (coronal plane) → pose estimation → shot phase detection → physics-informed metrics → concurrent / terminal feedback. Python reference implementation on the laptop first (this folder), then a port to Android as a module inside ArcheryApp. Works alongside the M5Stick IMU wearable. The design brief holds the *why*; `architecture.md` holds the *how*; `DEVELOPMENT_PLAN.md` holds the *order*.

## Development Rules

1. **ALWAYS follow the plan.** Refer to `DEVELOPMENT_PLAN.md` before writing ANY code. The plan defines implementation order and specifications. NEVER deviate without approval.

2. **NEVER modify `DEVELOPMENT_PLAN.md` without explicit user permission.** Even with auto-accept enabled, you MUST ask first. Never silently change the plan.

3. **ALWAYS sync agreed discussions into the plan.** When the user agrees to changes that deviate from or add to `DEVELOPMENT_PLAN.md`, you MUST immediately update the plan file (and its Changelog). Agreed-upon changes must NEVER remain only in conversation.

4. **Modularity is mandatory.** NEVER write monolith code. Split functionality into small, focused modules. Each file = single responsibility.

5. **Design for change.** The plan WILL change as work proceeds. Follow the rules in `architecture.md` → "Design-for-Change Rules": interfaces at swap points, registries + config instead of hard-coded choices, versioned contracts, recorded provenance, no magic numbers, capability checks instead of crashes. Equally: cheap extension points, NO speculative features — do not implement what the plan does not ask for.

6. **Decisions.** Decisions that belong to the user (metric thresholds, feedback rules and semantics, fading schedules, metric selection, UI trade-offs, cross-project changes, anything touching data privacy or cost) → add to `DECISIONS.md` → "Open" with options and a recommendation, then ask. NEVER decide them silently. Pure technical choices (libraries, algorithms, internal structure) → decide yourself and log them in `DECISIONS.md` → "Decided" with the rationale.

## Documentation Updates

After ANY code changes, update relevant documentation files (`achievements.md`, `architecture.md`, `DECISIONS.md`, `README.md`) before ending the session. Do NOT wait to be prompted.

## Progress Tracking — CRITICAL, NON-NEGOTIABLE

- **After EVERY substep** → IMMEDIATELY update `achievements.md` with what was accomplished. Do NOT wait until a full phase is complete.
- **Before EVERY new piece of work** → Re-read `DEVELOPMENT_PLAN.md` AND `achievements.md` to stay aligned with the plan and aware of current progress.
- **One step at a time.** Complete and validate one step, then STOP. Wait for user confirmation before starting the next. NEVER batch multiple steps without explicit permission. Exception spelled out in the plan: Phase 2 steps may proceed on synthetic data while the user's recording step (1.4) is still pending.
- **Validate before proceeding.** Every step has a validation tag (defined in `DEVELOPMENT_PLAN.md`):
  - `[Auto]` = Claude runs the tests and shows the FULL pytest summary (pass/fail counts, time).
  - `[User]` = additionally requires the user to act or inspect (record at the range, watch an overlay video, review a report, answer a decision). Cannot proceed until the user confirms.
- **Golden fixtures are a contract.** If a change alters golden-fixture outputs, STOP and explain why; update fixtures only with user approval, and record the reason in `achievements.md`.
- **Document everything.** Include test counts, file changes, any deviations from the plan, and any issues encountered.

## Environment & Tech Stack

- **OS:** Windows 11 (Bash via Git Bash and PowerShell both available)
- **Language:** Python 3.12
- **Python environment:** conda env `archery-mocap` → `E:/miniconda/envs/archery-mocap/python.exe` (D-101). NEVER use the `biomotum` env (the user's work env).
- **ALWAYS run Python with `-s`** (or `PYTHONNOUSERSITE=1`). The user-level site `C:\Users\Zijie Jin\AppData\Roaming\Python\Python312\site-packages` holds an unrelated Jupyter stack that otherwise leaks into this env and can satisfy dependencies with the wrong versions. When installing (after approval), use `PYTHONNOUSERSITE=1 ... -m pip install`. The env also has `PYTHONNOUSERSITE=1` set for `conda activate`.
- **Installed (2026-11-16):** numpy 2.5.5, scipy 1.18.1, mediapipe 1.2.3 (Pose Landmarker, Tasks API; IMAGE / VIDEO / LIVE_STREAM modes), av 19.0.2 (PyAV — exact per-frame timestamps), opencv-contrib-python 5.0.0 (comes with mediapipe — do NOT also install `opencv-python`), matplotlib 3.11.2, jinja2 3.1.6, pytest 9.2.0. Later, each needing its own approval: rtmlib + onnxruntime (RTMW comparison), sports2d (quick look).
- **Config format:** TOML (read via stdlib `tomllib`).
- **No editable install required:** tests use pytest `pythonpath = ["src"]`; the CLI runs as `python -m archery_mocap`.

### Commands (run from the `Archery assistant/` folder)

```bash
PY="E:/miniconda/envs/archery-mocap/python.exe"

# Run all tests
"$PY" -s -m pytest

# Run golden-fixture regression only
"$PY" -s -m pytest tests/golden

# CLI
"$PY" -s -m archery_mocap --help
"$PY" -s -m archery_mocap ingest <video> --session <session.toml>
"$PY" -s -m archery_mocap analyze <session_dir>
```

## Cross-Project Boundaries

- Sibling projects: `../wearable m5stick` (M5Stick firmware) and `../Archery_android_app` (ArcheryApp). **NEVER edit their code or their `DEVELOPMENT_PLAN.md` from this project.** Their own CLAUDE.md rules apply to any change there, and only with the user's explicit go-ahead.
- The M5Stick BLE protocol authority is `../wearable m5stick/claude-memory/overall_design.md`. This project only *consumes* it through versioned adapters (`src/archery_mocap/adapters/`). Needed firmware changes go on the M5Stick wishlist in the design brief and `DECISIONS.md` (D-109).
- The Android part of this project will live in ArcheryApp as an independent Gradle module (D-003). Rewriting ArcheryApp's Phase 4 requires the user's permission under ArcheryApp's CLAUDE.md (D-110).

## Data & Privacy

- Raw videos and audio are personal recordings of the user. NEVER commit them, upload them, or send them to any external service. They live under the data root (D-103), which is gitignored.
- Golden fixtures may contain keypoint series derived from the user's recordings (no pixels, no audio) — only after the user approves which ones.
- Any cloud processing (Phase 5) requires explicit user approval of the service and of what gets uploaded.

## Read-Only Mode

When asked to 'diagnose', 'audit', 'review', or 'report', do NOT edit any code files. Only read files and produce a written report/document unless explicitly told to implement fixes.

## Permissions

- **Bash commands:** Run freely — tests, scripts, CLI, media inspection.
- **Git: the user owns it.** NEVER run `git init`, `add`, `commit`, `push`, or create remotes. Write the code and leave committing and uploading to the user. Read-only git (`status`, `diff`, `log`) is fine.
- **File edits:** Proceed freely for implementation work inside this folder.
- **NEVER install, uninstall, or modify ANY packages/dependencies or environments** (pip, conda, npm, etc.) without EXPLICITLY asking the user first. If a missing module blocks work, report it and ask.
- **NEVER download anything** (model files, datasets, tools) without asking first.
- **NEVER delete files or folders** without asking the user first.

## Key Files

- `射箭视频mocap + M5Stick 训练平台 — 草稿.md` — design brief v1 (Chinese): goals, hard constraints, decisions, rationale
- `claude-memory/DEVELOPMENT_PLAN.md` — sequential implementation steps with per-step validation; Changelog at top
- `claude-memory/architecture.md` — module map, interfaces, data contracts, extension recipes, design-for-change rules, porting contract
- `claude-memory/DECISIONS.md` — decided items (with rationale) and open items awaiting the user
- `claude-memory/achievements.md` — progress log: completed work, test results, issues
- `claude-memory/RESUME.md` — session resume point (written by `/leave-off`, read by `/pickup-from-last`)
- `contracts/` — language-neutral contracts shared with the future Kotlin port (landmark vocabulary, shot record schema, fixture format)
