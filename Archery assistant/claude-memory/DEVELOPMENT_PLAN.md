# Implementation Plan — Archery Video Mocap

Sequential implementation steps for the video mocap project, with a validation for every step. Design rationale: the design brief `射箭视频mocap + M5Stick 训练平台 — 草稿.md`. Structure and interfaces: `architecture.md`. Decisions: `DECISIONS.md`.

**Status Legend:** `[ ]` Pending | `[~]` In Progress | `[x]` Complete | `[-]` Dropped (kept for history, with reason)

**Validation tags:**
- **[Auto]** = Claude runs tests and shows full results; the user approves moving on.
- **[User]** = the user must also act or inspect (record, watch an overlay, review a report, decide). Cannot proceed until the user confirms.

**Workflow (same as sibling projects):**
1. Claude completes all edits for a single step without asking permission (except installs, downloads, deletions).
2. Claude runs the tests and shows the results.
3. Claude updates `achievements.md` and reports.
4. The user grants permission to proceed to the next step.

**Parallelism exception:** The user's recording step (1.4) does not block Phase 2. Phase 2 steps run on synthetic data first and are re-checked on real data once it exists.

**Brief milestone ↔ phase mapping:** M0 → Phase 1 · M1 → Phase 2 · M2 → Phase 3 · M3 → Phase 4 · M4 → Phase 5 · M5 → Phase 6 · M6 → Phase 7.

---

## How This Plan Changes

This plan is expected to change. The rules:

- **Near term is detailed, far term is an outline.** Phases 0–3 are specified step by step. Phases 4–7 are outlines; each is expanded into steps (with user approval) when its predecessor finishes, using what was learned.
- **Stable step IDs.** Steps are never renumbered. Inserted steps get a letter suffix (e.g. `2.4a`). Removed steps are marked `[-]` with a reason, not deleted.
- **Every change is approved, then synced.** No edits to this file without the user's permission (CLAUDE.md rule 2). Every agreed change is written here immediately and logged in the Changelog below.
- **Decisions gate steps, not the whole plan.** A step that needs a user decision names the `DECISIONS.md` id; other steps keep moving.

### Changelog

| Date | Change |
|---|---|
| 2026-11-16 | Plan created from design brief v1. |
| 2026-11-16 | D-101 decided (new env `archery-mocap`, Python 3.12). D-102: git handled by the user → removed git init / commit from Step 0.2. Step 0.1: opencv comes from mediapipe's `opencv-contrib-python` (installing `opencv-python` alongside it conflicts). |

---

# Phase 0: Foundation

### Step 0.1: Python Environment [User]
**Gate:** D-101 (which env + approval of package list and model download).
**Task:** Set up the Python environment the user chooses.
**Implementation:**
- New dedicated conda env `archery-mocap`, Python 3.12 (D-101); the user's `biomotum` work env stays untouched.
- Initial packages: numpy, scipy, mediapipe, av (PyAV), matplotlib, jinja2, pytest. OpenCV comes from mediapipe's `opencv-contrib-python` dependency; do not also install `opencv-python` (the two conflict).
- Record exact versions in `achievements.md`; write the interpreter path into CLAUDE.md "Commands".

**Status:** `[x]` Complete (2026-11-16)

**Test:**
- [x] `python -I -c "import numpy, scipy, mediapipe, av, cv2, matplotlib, jinja2, pytest"` succeeds, all modules resolved from inside the env
- [x] Versions recorded (CLAUDE.md, achievements.md)

---

### Step 0.2: Repository Skeleton [Auto]
**Task:** Create the project skeleton per `architecture.md` section 2.
**Files:** `pyproject.toml`, `.gitignore`, `README.md`, `src/archery_mocap/__init__.py`, `__main__.py`, `cli.py`, empty subpackages, `tests/conftest.py`, `tests/unit/test_smoke.py`, `configs/default.toml`, `configs/profiles/zijie.toml`
**Implementation:**
- src layout; pytest config with `pythonpath = ["src"]` (no editable install needed).
- `.gitignore`: `data/`, `models/`, `outputs/`, caches, video/audio extensions.
- CLI skeleton (`argparse`) with subcommands stubbed: `ingest`, `pose`, `analyze`, `report`.
- No git operations: the user initializes the repo, commits and pushes (D-102).

**Test:**
- [ ] `python -m pytest` passes (smoke test)
- [ ] `python -m archery_mocap --help` lists subcommands

---

### Step 0.3: Core Types, Landmark Vocabulary, Profiles [Auto]
**Task:** Implement the core data model (`architecture.md` section 3).
**Files:** `contracts/landmarks.json`, `src/archery_mocap/core/skeleton.py`, `core/types.py`, `core/geometry.py`, `core/profile.py`
**Implementation:**
- Canonical landmark vocabulary (v1: the union needed for MediaPipe Pose's 33 points, with canonical names).
- Semantic roles (`bow_*`, `draw_*`) resolved through profile handedness.
- `KeypointSeries` (time slicing, role lookup, NaN for missing), `FrameKeypoints`, `Event`, `ShotContext`, `SeriesMeta`.
- Geometry helpers: segment angle, line angle vs horizontal, signed displacement along an axis, normalization by shoulder width.
- Profile TOML loader (handedness, height_cm, anchor_type, stance).

**Test:**
- [ ] Right- vs left-handed profile maps `draw_wrist` to `r_wrist` / `l_wrist`
- [ ] Unknown landmark name is rejected with a clear error
- [ ] Missing landmark → NaN + low confidence, no exception
- [ ] Geometry helpers correct on hand-computed cases

---

### Step 0.4: Registry, Config, Pipeline Builder [Auto]
**Task:** Make components pluggable and config-driven (design-for-change rules 2–4, 9).
**Files:** `core/registry.py`, `pipeline/config.py`, `pipeline/builder.py`, `tests/unit/test_portable_imports.py`
**Implementation:**
- Generic registry per component kind; `@register(kind, name)`.
- Config loading: `default.toml` ← session config ← CLI overrides; stable config hash.
- Builder: instantiate components from config; check `requires` / `provides`; disable unmet components with a recorded reason; in streaming mode reject `causal = False` components.
- Portability lint: modules whose components are `portable = True` must not import scipy / cv2 / mediapipe / av.

**Test:**
- [ ] Dummy components build from config; unknown name → error listing available names
- [ ] Metric requiring a landmark the backend lacks is disabled with a reason
- [ ] Streaming build rejects a non-causal filter
- [ ] Config precedence and hash stability
- [ ] Portability lint catches a deliberate violation

---

### Step 0.5: Schemas, Serialization, Provenance Cache [Auto]
**Task:** Versioned persistence (design-for-change rules 5–6).
**Files:** `schemas/shot_record.py`, `schemas/session_meta.py`, `schemas/series_io.py`, `schemas/migrations.py`, `io/cache.py`, `contracts/shot_record.md`
**Implementation:**
- ShotRecord v1 per `architecture.md` section 5; JSON with `schema_version`; unknown fields preserved in `extra`.
- KeypointSeries on disk: `.npz` + JSON sidecar.
- Migration hook registry (`v1 → v2` functions), even if empty for now.
- Provenance: component names/versions, config hash, input fingerprints; cache reuses an artifact only on exact provenance match.

**Test:**
- [ ] Round-trip ShotRecord and KeypointSeries
- [ ] Unknown fields survive read → write
- [ ] A dummy v0 → v1 migration runs when reading an old record
- [ ] Changing a component version invalidates that stage's cache and downstream stages only

---

### Step 0.6: Synthetic Shot Generator [Auto]
**Task:** Ground-truth data so that development and tests do not wait for real recordings.
**Files:** `synth/shot.py`, `synth/audio.py`, `synth/session.py`
**Implementation:**
- Parametric 2D coronal-plane archer: raise → draw → full draw (hold duration; creep / expansion rate) → release (draw-hand velocity spike with direction) → follow-through (bow-arm drop / rise) → rest. Let-downs (draw then return without release) as an option.
- Noise model: per-landmark jitter, dropouts, confidence; frame rate and VFR timing jitter.
- Synthetic audio: string-release transients at release times, background noise, optional distractor transients (neighbouring archers), a start clap.
- Multi-shot sessions with known ground-truth events and parameters.
- Structurally realistic, not biomechanically exact; documented as such.

**Test:**
- [ ] Ground-truth events match the generated motion (e.g. velocity spike at release)
- [ ] Parameter changes produce the expected kinematic differences (creep rate → draw-wrist drift)
- [ ] Seeded generation is reproducible

---

# Phase 1: M0 — Recording Protocol & Data Ingest

### Step 1.1: Recording Protocol [User]
**Task:** Write the protocol the user follows at the range.
**Files:** `docs/recording_protocol.md` (with a one-page printable checklist)
**Implementation (content):**
- Camera: landscape, 1080p, 120 fps if available (else 60), Pro video mode with locked focus / exposure / white balance, shutter 1/500 or faster, **audio on**.
- Placement: floor marks for tripod feet and the archer's feet; record distance and height; same placement every session.
- Start of recording: calibration object of known length in the shooting plane, then one clap in view (audio–video offset).
- Shooting: numbered arrows shot in order; M5Stick session running; optionally log BLE packets on a second phone (nRF Connect); 30–50 shots including a few deliberate, safe perturbations (creep, dropped bow arm, no follow-through), noted by shot index.
- Scoring: by arrow number, so each hole maps to a shot.
- End of session: fill `session.toml` and `scores.csv`.

**Test:**
- [ ] User reviews and approves the protocol

---

### Step 1.2: Session Metadata & Scores Formats [Auto]
**Task:** Templates and validators for per-session metadata.
**Files:** `docs/templates/session.toml`, `docs/templates/scores.csv`, `schemas/session_meta.py` (validator)
**Implementation:**
- `session.toml`: date, location, distance, bow, arrows, camera model and settings, tripod distance/height, calibration object length, profile name, perturbation shots (shot index → type), notes. Versioned.
- `scores.csv`: end, shot_in_end, arrow_no, score, x_mm, y_mm, self_rating, notes (optional columns allowed). Row order = shot order.

**Test:**
- [ ] Templates parse
- [ ] Validator reports missing / malformed fields clearly

---

### Step 1.3: Video Ingest & Inspection [Auto]
**Gate:** D-103 (data root location).
**Task:** Read phone videos correctly and create a session folder.
**Files:** `io/video.py`, `io/audio.py`, `io/store.py` (session folder layout), `cli.py` (`ingest`)
**Implementation:**
- PyAV VideoSource: per-frame exact PTS (VFR-safe), fps statistics, VFR flag, resolution, rotation metadata, audio presence; optional downsampling to a target fps (to simulate 60 / 30 fps).
- AudioSource: decode the audio track to a numpy array with its own clock.
- `ingest`: validate `session.toml`, inspect the video, create `data/sessions/<id>/` with `session_meta.json`; warn on VFR, low fps, missing audio, rotation.

**Test:**
- [ ] A tiny mp4 generated in the test (synthetic frames + tone) ingests with correct frame count, timestamps and audio presence
- [ ] VFR test file is flagged; downsampling produces the expected timestamps

---

### Step 1.4: Record M0 Session [User]
**Task:** The user records the first session following the protocol, fills `session.toml` and `scores.csv`, copies the video into the data root, runs `ingest`.
**Note:** Can happen any time after 1.1–1.3; Phase 2 proceeds on synthetic data meanwhile.

**Test:**
- [ ] `ingest` succeeds on the real video; inspection results reviewed together

---

### Step 1.5: (Optional) Sports2D Quick Look [User]
**Gate:** approval to install sports2d (and its dependencies) — ideally in a separate env.
**Task:** Run Sports2D on the M0 video to eyeball 2D angles with zero code. Record impressions in `achievements.md`.

**Test:**
- [ ] User has looked at the output; notes recorded

---

# Phase 2: M1 — Offline Pipeline (Python Reference)

### Step 2.1: Pose Backend Interface + MediaPipe Adapter [User]
**Gate:** model file download approved (part of D-101).
**Task:** First pose backend behind the `PoseBackend` protocol.
**Files:** `pose/base.py`, `pose/mediapipe_backend.py`, `cli.py` (`pose`)
**Implementation:**
- MediaPipe Pose Landmarker (Tasks API), VIDEO running mode, model variant configurable (lite / full / heavy). `.task` file in `models/`; its hash recorded in provenance (the same file will ship to Android).
- Map the 33 landmarks to canonical names; visibility / presence → `conf`; world landmarks stored in the optional `world_xyz` channel.
- Determine whether VIDEO mode applies internal temporal smoothing; document it in `architecture.md`; if it blunts the release spike, add IMAGE mode (per-frame) as a config option.
- Cached output via the provenance cache.

**Test:**
- [ ] Mapping covers all 33 landmarks; frames with no person → NaN, no exception
- [ ] Cache hit on re-run, miss after changing model variant
- [ ] [User] overlay on M0 video (Step 2.2) looks right, including during release

---

### Step 2.2: Overlay Renderer [User]
**Task:** Visual QA tool used by every later step.
**Files:** `report/overlay.py`, `cli.py` (`overlay`)
**Implementation:** Render skeleton, landmark confidence, current phase and events onto the video (or a frame strip around each shot).

**Test:**
- [ ] Renders from a synthetic series without error
- [ ] [User] watches the overlay on M0 data

---

### Step 2.3: Filters [Auto]
**Task:** Pluggable filtering, explicit algorithms suitable for porting.
**Files:** `filters/base.py`, `filters/none.py`, `filters/one_euro.py` (causal, portable), `filters/butterworth.py` (zero-phase, offline), `filters/gap_fill.py`
**Implementation:**
- One Euro filter for streaming and Android; zero-phase Butterworth for offline analysis; confidence-gated gap handling (short gaps interpolated, long gaps left as NaN). All parameters in config.
- Equations documented in the module docstrings (porting reference).

**Test:**
- [ ] Causality: perturbing future samples does not change causal filter output at earlier times
- [ ] Attenuation / lag behave as expected on synthetic sinusoids and steps
- [ ] Gap handling on synthetic dropouts

---

### Step 2.4: Release Event Sources, Fuser, Clock Alignment [Auto]
**Task:** Pluggable release detection and multi-source time alignment.
**Files:** `events/base.py`, `events/audio_onset.py`, `events/kinematic.py`, `events/clap_sync.py`, `events/fuser.py`, `core/clock.py`
**Implementation:**
- `audio_onset`: band-limited energy envelope + onset detection with a refractory period → release candidates with confidence.
- `kinematic`: draw-wrist speed spike with the expected direction, only within a plausible phase context.
- `clap_sync`: detect the start clap in audio and video → audio–video offset; fallback to a per-device constant from config.
- `fuser`: associate events across sources within a window; fused time by configured priority / weights; all raw events kept.
- `ClockModel`: offset (+ optional drift) per source; robust estimation from matched event pairs.

**Test:**
- [ ] Synthetic audio: release onsets detected within 2 ms; distractor transients rejected or low-confidence
- [ ] Kinematic spike detected within 1 frame on synthetic data
- [ ] Clock offset recovered from synthetic pairs with outliers
- [ ] Fuser associates sources correctly when one source misses a shot

---

### Step 2.5: Phase Detector [Auto]
**Task:** Shot segmentation and phase events, causal-first (design-for-change rule 8).
**Files:** `phases/state_machine.py` (causal, portable), `phases/refine.py` (offline)
**Implementation:**
- State machine: IDLE → RAISE → DRAW → FULL_DRAW (hold) → RELEASE → FOLLOW_THROUGH → IDLE, plus a LET_DOWN path (draw then return without release → no shot counted).
- Full draw: draw-wrist speed below a **relative** threshold (fraction of this shot's peak draw speed) sustained for a minimum time, or a speed local minimum — works when the hold keeps expanding and speed never reaches 0.
- Release: external release events when available (fused), else kinematic spike.
- Anchor is never required (no-anchor profiles work unchanged).
- Offline refiner: snap release to the fused release event; refine full-draw onset with the full window.
- All thresholds in config.

**Test:**
- [ ] Synthetic nominal shots: event errors < 2 frames at 60 fps
- [ ] Continuous-expansion hold detects full draw
- [ ] Let-down is not counted as a shot
- [ ] Multi-shot session: correct shot count; robust to dropouts
- [ ] Same results for right- and left-handed synthetic archers

---

### Step 2.6: Metric Framework + First Metrics [Auto]
**Gate:** D-105 (confirm first-batch list and definitions).
**Task:** Metric registry and the first physics-informed metrics.
**Files:** `metrics/base.py`, one file per metric:
`follow_through_draw_hand.py`, `follow_through_bow_arm.py`, `hold_draw_hand_drift.py`, `timing.py`, `bow_arm_elevation_at_release.py`, `shoulder_line_angle.py`, `head_tilt.py`, `anchor_mouth_hand.py` (optional module `anchor`), `full_draw_to_release.py`
**Implementation:**
- `MetricSpec` filled completely, including `rationale` (mechanism) and `phase_window`.
- Lengths normalized (shoulder width, or calibrated mm/px when available) — configurable.
- `NotAvailable(reason)` when roles / events are missing.

**Test:**
- [ ] Each metric recovers known values on synthetic shots within tolerance
- [ ] Missing landmark / event → `NotAvailable` with reason
- [ ] Disabling the `anchor` module removes its metric without touching anything else
- [ ] Metric version is written into records

---

### Step 2.7: Shot Records, Session Store, Scores Join, `analyze` [Auto]
**Task:** End-to-end batch pipeline.
**Files:** `io/store.py`, `adapters/scores_csv.py`, `pipeline/batch.py`, `cli.py` (`analyze`)
**Implementation:**
- `analyze <session>`: ingest → pose → filters → events → phases → metrics → ShotRecords, reusing cached stages.
- Join scores by shot order / arrow number; count or arrow mismatches produce a visible warning, never a silent misalignment.

**Test:**
- [ ] End-to-end on a synthetic session produces one ShotRecord per shot with all fields
- [ ] Scores mismatch case is reported
- [ ] Re-run with one changed metric recomputes only metrics and records

---

### Step 2.8: Noise Floor & Sensitivity Analysis [Auto]
**Gate:** D-106 (usability criterion).
**Task:** Decide which metrics are usable — the project's main go/no-go.
**Files:** `analysis/noise_floor.py`, `analysis/perturbation.py`
**Implementation:**
- Within-shot jitter (residual during the hold after low-pass), between-shot SD, SNR; MDC95; per-metric usability flag per D-106.
- Perturbation shots (from `session.toml`) scored against normal shots.

**Test:**
- [ ] Synthetic sessions with known jitter and known between-shot variance → estimates within tolerance
- [ ] Synthetic perturbation detected when effect > noise, not detected when effect ≪ noise

---

### Step 2.9: HTML Report [User]
**Task:** Single-file offline review report.
**Files:** `report/html.py`, `report/templates/`, `cli.py` (`report`)
**Implementation:** Session summary; per-shot table (events, metrics, score); metric trends across shots; metric vs score plots for registered metrics; SNR / usability table; release-source agreement (audio / kinematic / M5Stick); perturbation results; provenance block. Static HTML with embedded images (no CDN, works offline).

**Test:**
- [ ] Renders for a synthetic session with all sections present
- [ ] [User] reviews the report on M0 data

---

### Step 2.10: Event Labeling Tool + Accuracy Evaluation [User]
**Task:** Ground truth from the user's own video.
**Files:** `tools/label_events.py`, `analysis/event_accuracy.py`
**Implementation:** Minimal frame stepper (keys to step and to mark draw_start / full_draw / release / follow_through_end), saves CSV; evaluation compares detected vs labeled events in frames.

**Test:**
- [ ] Tool works on a synthetic video; evaluation correct on synthetic labels
- [ ] [User] labels 5 M0 shots

---

### Step 2.11: Golden Fixtures v1 [Auto]
**Task:** Freeze the reference behaviour (CLAUDE.md: fixtures are a contract).
**Files:** `contracts/fixtures.md`, `tests/golden/*.json`, `tests/golden/test_golden.py`
**Implementation:** Layer-1 fixtures (keypoint series → events and metrics, with tolerances) from synthetic sessions and — with the user's approval of which ones — keypoint series from M0 (no pixels, no audio). Language-neutral JSON.

**Test:**
- [ ] Golden tests pass; an intentional change makes them fail with a readable diff

---

### Step 2.12: RTMW Comparison Backend [Auto]
**Gate:** approval to install rtmlib + onnxruntime and download RTMW weights.
**Task:** Second backend for comparison and later reanalysis.
**Files:** `pose/rtmlib_backend.py`, `analysis/backend_compare.py`
**Implementation:** RTMW whole-body via rtmlib; landmark mapping (extend vocabulary additively for hand / face points); per-landmark differences vs MediaPipe and metric SNR per backend. Feeds back into D-004 (revisit if MediaPipe is not good enough).

**Test:**
- [ ] Backend swap via config only; full pipeline runs with RTMW
- [ ] Comparison table in the report

---

### Step 2.13: M1 Acceptance [User]
**Test:**
- [ ] All M0 shots processed without errors
- [ ] Event errors < 2 frames on the 5 labeled shots
- [ ] SNR table and perturbation results reviewed; usable metric set agreed
- [ ] Audio vs kinematic release agreement reported
- [ ] User approves moving to Phase 3

---

# Phase 3: M2 — Streaming Simulation & Feedback Simulation

### Step 3.1: Streaming Runner [Auto]
**Files:** `pipeline/streaming.py`
**Implementation:** Feed frames one at a time at a simulated target fps (downsampled from the recording, with optional frame drops); builder admits only causal components; per-frame compute time measured (laptop proxy).

**Test:**
- [ ] Non-causal component rejected in streaming mode
- [ ] Streaming over a synthetic session produces phase events

---

### Step 3.2: Batch–Stream Consistency [Auto]
**Implementation:** Compare streaming vs batch events and metrics; document expected differences (offline refinement).

**Test:**
- [ ] Differences within documented tolerances on synthetic and M0 data

---

### Step 3.3: Full-Draw Detection Latency & Cue Timing [Auto]
**Implementation:** Detection latency (event time vs the time it becomes known causally) at 120 / 60 / 30 fps; simulated cue delivery using channel latency models (phone speaker, BLE → M5Stick, Bluetooth A2DP 150–300 ms).

**Test:**
- [ ] Latency table produced and added to the report → input for D-108

---

### Step 3.4: Baseline Full-Draw → Release Distribution [Auto]
**Implementation:** From no-cue M0 data, the full-draw → release interval distribution, saved as the conditioning baseline.

**Test:**
- [ ] Baseline saved with provenance; shown in the report

---

### Step 3.5: Feedback Framework (Simulated) [User]
**Gate:** D-107 (feedback rules, patterns, default fading schedule).
**Files:** `feedback/scheduler.py`, `feedback/rules/`, `feedback/outputs/`, `analysis/conditioning.py`
**Implementation:**
- Scheduler: fixed, alternating, fading (decreasing probability), random catch trials — seeded and logged per shot.
- Rules: `full_draw_cue` (fires on full_draw; semantics "keep holding", D-005); terminal rules per D-107.
- Outputs: `log` (simulation), `laptop_sound` (desk testing), stubs with contracts only for `phone_speaker` and `m5stick_buzzer`. Every send records an estimated delivery time from the output's latency model.
- Conditioning monitor: cue → release interval vs baseline (shift toward simple reaction time ~150–250 ms, variance drop, cue vs no-cue hold difference) → report alarm.

**Test:**
- [ ] Scheduler reproducible; statistics match configured probabilities
- [ ] Rules fire on synthetic events
- [ ] Conditioning monitor flags synthetic "conditioned" data and not the baseline
- [ ] [User] approves rule content and defaults

---

### Step 3.6: M5Stick Log Adapter & Cross-Source Comparison [Auto]
**Note:** Only if M0 included BLE logs; otherwise parser + tests only.
**Files:** `adapters/m5stick.py`
**Implementation:** Versioned parser for `ShotPacket_t` v1 (20 B) per `../wearable m5stick/claude-memory/overall_design.md`, from the logging format actually used; clock alignment through release events; compare IMU vs audio / video release and hold timing.

**Test:**
- [ ] Known byte vectors parse correctly
- [ ] Offset recovery on synthetic data; comparison table on real data when available

---

### Step 3.7: Golden Fixtures v2 + Export for Port [Auto]
**Implementation:** Streaming fixtures (frame-by-frame inputs → expected events and the frame at which each becomes known), exported per `contracts/fixtures.md`. These become the Kotlin port's acceptance tests.

**Test:**
- [ ] Golden tests (v1 + v2) pass; export validated by a round-trip reader

---

### Step 3.8: M2 Acceptance [User]
**Test:**
- [ ] Streaming = batch within tolerance
- [ ] Latency table reviewed; latency budget decided (D-108)
- [ ] Feedback rules decided (D-107); conditioning monitor reviewed
- [ ] User approves expanding Phase 4 into detailed steps

---

# Phase 4: M3 — Android Prototype (OUTLINE — expand before starting)

**Pre-requisites:** D-110 (permission to rewrite ArcheryApp Phase 4 in ArcheryApp's own plan); ArcheryApp core scoring stable.
**Lives in:** ArcheryApp, as `:feature:mocap` plus a shared `:core:ble` (D-003).
- 4.A Module skeleton and navigation entry inside ArcheryApp.
- 4.B CameraX + MediaPipe Tasks (Android) with the same `.task` file; on-device fps measurement.
- 4.C Kotlin port of the portable core (filters, phase machine, metrics) — acceptance = JSON golden fixtures from 3.7.
- 4.D Release trigger from the phone microphone (port of `audio_onset`).
- 4.E **M3a terminal loop:** ring buffer → post-release window analysis → phone-speaker feedback → ShotRecord linked to Session / Arrow in Room.
- 4.F **M3b video concurrent cue** — only if D-111 says so (depends on Phase 2 SNR results); otherwise the concurrent cue stays on the M5Stick.

---

# Phase 5: M4 — Post-Shot Reanalysis & Review (OUTLINE)

- Export ShotRecords from the phone; heavy-backend (RTMW) reanalysis on the laptop first; cloud later (D-112, with explicit approval of what is uploaded).
- Cross-session review: trends, metric versions, cue on/off sessions vs retention.

---

# Phase 6: M5 — M5Stick Integration (OUTLINE)

**Pre-requisites:** ArcheryApp Phase 3 BLE client; M5Stick wishlist (D-109).
- M5Stick release events as a live release source; `ShotPacket_t` stored in ShotRecords.
- Phone-computed feedback played on the M5Stick buzzer (needs the play-pattern BLE command).
- Clock sync through release events; `hold_ms` / `draw_ms` / cue flag from the packet feed the conditioning monitor.

---

# Phase 7: M6 — Second Camera (OUTLINE)

- Behind-the-archer or overhead camera for line and cant; Pose2Sim or FreeMoCap; multi-view backend still outputs canonical landmarks (+ optional 3D channel).
