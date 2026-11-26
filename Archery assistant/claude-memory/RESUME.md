# Session Resume Point

## Date
2026-11-16

## Current Status
Step 0.1 (Python environment) complete and verified. Waiting for the user's go-ahead to start Step 0.2 (repository skeleton). No project code yet.

## What Was Done This Session
- Reviewed design brief v0 against the sibling projects; wrote brief v1 (v0 kept as a backup file).
- Created `CLAUDE.md` and `claude-memory/` (`DEVELOPMENT_PLAN.md`, `architecture.md`, `DECISIONS.md`, `achievements.md`, this file).
- Added "Design for Change" and "Git: the user owns it" to `../wearable m5stick/CLAUDE.md` and `../Archery_android_app/CLAUDE.md`. Their `DEVELOPMENT_PLAN.md` files were NOT touched.
- Decided D-101 (env) and D-102 (git is the user's job).
- Step 0.1: conda env `archery-mocap` (Python 3.12.16) with numpy, scipy, mediapipe 1.2.3, av, opencv-contrib, matplotlib, jinja2, pytest. Fixed user-site leakage (see Critical Context).

## Immediate Next Steps
1. Step 0.2: repository skeleton — `pyproject.toml` (src layout, pytest `pythonpath = ["src"]`), `.gitignore`, `README.md`, package skeleton, CLI stub, smoke test. No git operations.
2. Then Step 0.3 (core types, landmark vocabulary, profiles).

## Pending User Tasks
- D-103: give the path for raw videos (user chose "another location"). Needed by Step 1.3.
- Review `DEVELOPMENT_PLAN.md` (Phases 0–3 detailed, 4–7 outline).
- Initialize git / commit / push whenever wanted (user's job, D-102).
- 2026-11-25: **monorepo migration (D-114) is in progress, and the user runs it.** Script: `E:/Random/Pet_projects/_archery_migration/migrate.sh` (`check` / `build` / `install` / `undo`); root files are in `_archery_migration/root_files/`. Claude ran only `check`, which is read-only, plus syntax and filter tests. Claude must NOT run `build` or `install`, because they make commits. `check` said both repos still have uncommitted work:
  - m5stick: SP13 changes plus untracked `claude-memory/RESUME.md`, `claude-memory/USAGE_MANUAL.md`, `lib/calibration/*`.
  - ArcheryApp: `CLAUDE.md` and `memory-claude/RESUME.md`.

  User steps: commit+push both → `check` → `build` → `install` → commit `Archery assistant/` → new GitHub repo + push → archive the old repos → fix the Android Studio VCS root → build both IDEs → delete `_archery_migration/`. If the user shows output, check it against the expected messages in the script.
  - **Progress, 2026-11-25:** the user committed and pushed both repos. `check` passed. `build` passed and was verified: 27 commits (11 + 11 + 1 root + 2 moves + 2 merges), both tips `c014124` and `8982f06` present, file trees identical, `git log --follow` reaches the first commit. `install` also passed. `Archery/` is now the repo: HEAD `e75ec4b` on `main`, no remote yet, the nested `.git` folders are backed up in `_archery_migration/work/old-git/`, and all sub-project ignore rules still apply (Claude checked with `git check-ignore`). The only untracked item is `Archery assistant/`. Remaining, all the user's: commit `Archery assistant/` → new empty GitHub repo + push → archive the old two → Android Studio VCS root → build both → delete `_archery_migration/`.
- 2026-11-25: **the bow is broken**, so Step 1.4 (M0 recording) waits until it's fixed. Not blocking: Phase 0, Steps 1.1–1.3, and Phase 2 on synthetic data (parallelism exception). Things that need real data (2.2/2.9/2.10 user reviews, M0 fixtures in 2.11, 2.12, 2.13, 3.4) wait for the recording.

## Environment Setup on Resume
- Interpreter: `E:/miniconda/envs/archery-mocap/python.exe`, ALWAYS with `-s`.

## Critical Context
- User-level site `C:\Users\Zijie Jin\AppData\Roaming\Python\Python312\site-packages` contains an unrelated Jupyter stack that leaks into any Python 3.12. Always run with `-s` / `PYTHONNOUSERSITE=1`, and install with `PYTHONNOUSERSITE=1`.
- mediapipe is 1.2.3, newer than most online examples (0.10.x). Check the API against the installed package.
- Never use the `biomotum` env. Never run git write commands.
- The M5Stick already provides the concurrent full-draw cue via IMU (`FORM_ALIGNED` on entering HOLD). The video concurrent cue (Phase 4, M3b) is conditional (D-111).
