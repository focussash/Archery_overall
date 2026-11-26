# Achievements — Progress Log

Completed work, test results and issues, newest phase at the bottom. Update after EVERY substep (CLAUDE.md).

---

## Planning (2026-11-14 → 2026-11-16)

- 2026-11-14 — Design brief v0 drafted in web chat (kept as `射箭视频mocap + M5Stick 训练平台 — 草稿 v0 (web).md`).
- 2026-11-16 — Brief revised to v1 against the real state of the sibling projects:
  - M5Stick (SP13) already has release detection, an IMU DRAW/HOLD state machine with a full-draw cue (`FORM_ALIGNED`), buzzer patterns, and BLE `ShotPacket_t` v1.
  - ArcheryApp (Phase 2.4.7) has Phase 3 (BLE) not started; its Phase 4 (ML Kit static pose) is superseded by this project.
  - Decisions recorded: cue semantics ("keep holding", not a clicker), physics-informed metrics, no phone vibration, pluggable release sources, Android as a module inside ArcheryApp.
- 2026-11-16 — Project scaffolding docs created: `CLAUDE.md`, `claude-memory/DEVELOPMENT_PLAN.md`, `architecture.md`, `DECISIONS.md`, `achievements.md`, `RESUME.md`. Design-for-change rules added here and to both sibling projects' CLAUDE.md.
- Decisions: D-101 (new env), D-102 (user owns all git operations; Claude never commits). D-103 is still open: the user chose "another location" for raw videos but the path isn't given yet.
- Git rule also added to both sibling CLAUDE.md files (their "run git freely" lines conflicted with the user's standing rule).
- 2026-11-25 — D-114: monorepo at `Archery/` that keeps both old repos' history. Claude wrote `E:/Random/Pet_projects/_archery_migration/migrate.sh` and the root `README.md` / `.gitignore`, for the user to run:
  - Approach: each repo is cloned to a temp folder, its files are moved into the subfolder with `git mv` in one commit, and the result is merged with `--allow-unrelated-histories`. Old commit hashes stay valid. `install` only moves `.git` folders (old ones go to a backup); working files, IDE state and PlatformIO libraries stay in place, and the path stays the same. `undo` reverses it.
  - Tested by Claude (read-only): `bash -n` passes; `check` correctly lists both repos' uncommitted work and ignores the tracked-but-ignored `.pio/build/project.checksum`; the tree-filter and empty-pattern cases were unit-tested on dummy data.
  - Not tested by Claude: `build`, `install` and `undo` (they run git write commands, D-102).
  - The user ran it on 2026-11-25. `check` OK → `build` OK: 27 commits, both original tips present with unchanged hashes, file trees identical → `install` OK, with `git status` showing only `?? "Archery assistant/"`. Claude checked the result read-only: the nested `.git` folders are gone (backed up in `work/old-git/`), and the ignore rules for `.pio`, `app/build`, `local.properties` and `.claude/settings.local.json` still apply. Still to do, by the user: commit `Archery assistant/`, push to a new GitHub repo, archive the old two.
- 2026-11-25 — Bow broken, so Step 1.4 (M0 recording) is postponed. Phase 2 proceeds on synthetic data as planned.

---

## Phase 0: Foundation

### Step 0.1: Python Environment — COMPLETE (2026-11-16)
- Created conda env `archery-mocap` (Python 3.12.16) at `E:/miniconda/envs/archery-mocap`.
- Installed: numpy 2.5.5, scipy 1.18.1, mediapipe 1.2.3, av 19.0.2, opencv-contrib-python 5.0.0.93 (pulled in by mediapipe), matplotlib 3.11.2, jinja2 3.1.6, pytest 9.2.0, plus their dependencies (python-dateutil 4.0.0, tzdata, colorama, pygments, …).
- **Issue found and fixed:** the user-level site `C:\Users\Zijie Jin\AppData\Roaming\Python\Python312\site-packages` (an unrelated Jupyter stack) leaked into the env. pip treated `python-dateutil` / `six` as already installed there, so `import matplotlib` failed in isolated mode. Fix: reinstalled with `PYTHONNOUSERSITE=1` so all dependencies live inside the env; set `PYTHONNOUSERSITE=1` as a conda env var; CLAUDE.md commands use `python -s`. The user-level folder itself was left untouched.
- Verification: in `python -I` (isolated) mode all 9 modules import and resolve from `envs/archery-mocap/Lib/site-packages`; `pip check` clean; `mediapipe.tasks.python.vision.PoseLandmarker` present with `detect_for_video` and running modes IMAGE / VIDEO / LIVE_STREAM.
- Not done yet (by design): the MediaPipe `.task` model download happens in Step 2.1.
- No code yet.
