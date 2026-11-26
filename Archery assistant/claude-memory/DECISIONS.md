# Decisions

Two lists. **Decided:** what was settled, by whom, why, and what would make us revisit it. **Open:** questions waiting for the user, with options, a recommendation, and the step that needs the answer.

Rules (CLAUDE.md rule 6): user-owned questions go to **Open** and wait for the user. Claude decides pure technical choices and logs them under **Decided** as "Claude (technical)". IDs are never reused. When an open item is answered, move it to Decided and keep its ID.

---

## Decided

| ID | Date | Decision | By | Rationale | Revisit if |
|---|---|---|---|---|---|
| D-001 | 2026-11-15 | One camera on a tripod beside the shooting line, facing the chest (coronal plane). Second camera on the roadmap only (Phase 7). | User | Simplest setup; most follow-through and bow-arm information is in this plane | Line or cant from video become a priority |
| D-002 | 2026-11-14 | Python reference implementation on the laptop first, then port to Android; golden fixtures define equivalence. | User (brief) | Iterate on recorded video instead of at the range; Claude can run tests itself | — |
| D-003 | 2026-11-16 | Python research code lives in `Archery assistant/`. The Android part becomes an independent Gradle module inside ArcheryApp (`:feature:mocap` + shared `:core:ble`), superseding ArcheryApp's original Phase 4. | Claude (recommended), user delegated | Phone stays on the tripod and scores are entered in the same app; the M5Stick allows only one BLE central; shots and scores in one DB. Module boundary keeps the scoring app light. | The module makes ArcheryApp builds or its process unwieldy |
| D-004 | 2026-11-16 | Pose model: MediaPipe Pose Landmarker on both Python and Android, same `.task` file. RTMW (via rtmlib) for offline comparison and post-shot reanalysis. | Claude (technical), user delegated | Same model on both ends is required for golden fixtures to carry over; MediaPipe runs on both platforms out of the box and has mouth-corner and thumb points | Step 2.8 / 2.12 show MediaPipe's SNR is insufficient → switch both ends to RTMPose (ONNX) |
| D-005 | 2026-11-16 | Concurrent cue means "full draw reached — keep holding / expanding", **not** a release signal (the opposite of a competitive clicker). Cue on/off, delivery time and release time are logged; conditioning is monitored. | User | User's traditional-archery training intent: know you are at full draw, then keep holding before release | Conditioning monitor (Step 3.5) shows the cue turning into a release trigger |
| D-006 | 2026-11-16 | Metrics are physics-informed: only metrics with a biomechanical or coaching rationale are registered; within that, as many as useful. No data-mined correlations. Follow-through first; anchor optional. | User | Avoid spurious correlations with small shot counts | — |
| D-007 | 2026-11-16 | No phone vibration as feedback (phone is on the tripod). Placeholder outputs: phone speaker, M5Stick buzzer. Bone-conduction headset or other wearables later. Outputs are pluggable. | User + facts | Phone is 2–3 m away; the M5Stick has a buzzer but no vibration motor | — |
| D-008 | 2026-11-16 | Release-event sources are pluggable: audio-track string sound (primary offline label), video kinematics, M5Stick IMU; fused per shot. | Claude (technical) | Audio onset is free, precise and needs no extra hardware; multiple sources cross-validate | — |
| D-009 | 2026-11-16 | On Android, the terminal-feedback loop (M3a) comes before a video-based concurrent cue (M3b). Until then the concurrent cue stays on the M5Stick (`FORM_ALIGNED`). | Claude, accepted with brief v1 | Terminal loop needs no causal real-time pipeline; M5Stick already cues on entering HOLD | — |
| D-010 | 2026-11-16 | Config and profile files use TOML (stdlib `tomllib`). | Claude (technical) | No extra dependency; supports comments | — |
| D-011 | 2026-11-16 | Video and audio decoding via PyAV, using exact per-frame PTS. | Claude (technical) | Phone videos are often variable frame rate; OpenCV timestamps are unreliable for VFR | — |
| D-012 | 2026-11-16 | Phase detector is a causal state machine from the start; offline refinement is a separate layer. Same core for batch and streaming. | Claude (technical) | Phase 3 becomes a runner change instead of a rewrite; the Kotlin port has a single algorithm to copy | — |
| D-101 | 2026-11-16 | New dedicated conda env `archery-mocap` (Python 3.12). Approved packages: numpy, scipy, mediapipe, av, opencv (via mediapipe's opencv-contrib-python), matplotlib, jinja2, pytest. Approved: download of the MediaPipe `.task` model from Google's official source (done in Step 2.1). | User | Keeps the `biomotum` work env untouched; mediapipe wheel support for 3.12 is solid | mediapipe / PyAV drop 3.12 support |
| D-102 | 2026-11-16 | Git and GitHub are handled entirely by the user. Claude writes code but never runs `git init` / `add` / `commit` / `push` or creates remotes; read-only git commands (`status`, `diff`, `log`) are fine. Claude still maintains `.gitignore`. | User | User's standing rule across projects: Claude writes code, the user commits and uploads | — |
| D-013 | 2026-11-16 | Design for change across all three archery projects: interfaces at swap points, registries + config, versioned contracts, provenance, no magic numbers, no speculative features (`architecture.md` section 6). | User | Plans are expected to change as work proceeds | — |
| D-114 | 2026-11-25 | One git repo (monorepo) rooted at `Archery/` holds all three projects in their current folders. The full history of `wearable-m5stick` (branch `main`) and `ArcheryApp` (branch `Taking-picture`) is kept, with commit hashes unchanged. The old GitHub repos get archived, not deleted. The migration script is `E:/Random/Pet_projects/_archery_migration/migrate.sh`: Claude wrote it and the user runs it (D-102). Files that were tracked although ignored (`.pio/build/project.checksum`, `.claude/settings.local.json`) stop being tracked. | User (Claude recommended) | The local layout is already one folder. A change to a shared contract (ShotPacket, golden fixtures, landmark vocabulary) becomes one commit. Android tests can read `Archery assistant/contracts/` directly. The `../` paths in the docs stay valid. | A sub-project needs separate access control or its own release cycle |

---

## Open — waiting for the user

| ID | Question | Options | Recommendation | Needed by |
|---|---|---|---|---|
| D-103 | Where raw videos live | User chose (b) "another location" on 2026-11-16 — **exact path still needed**. The data root is a config value, so it can change later. | — | Step 1.3 || D-104 | Lab validation session (marker mocap + synchronized phone video) | (a) yes, after Phase 2 works on real data; (b) not now | (a) if lab access is easy — one session answers absolute accuracy, open-stance foreshortening, and thumb-ring keypoint quality | Optional, any time after 2.7 |
| D-105 | First-batch metric list and definitions | Candidates in brief "指标" section: draw-hand follow-through, bow-arm follow-through, hold drift (creep / expansion), draw & hold durations, bow-arm elevation at release, shoulder-line angle, head tilt, anchor distance (optional), full-draw → release interval | Approve as listed; add / remove any | Step 2.6 |
| D-106 | Criterion for "metric usable" | (a) SNR = between-shot SD / within-shot jitter ≥ 2; (b) MDC95 smaller than the perturbation effect; (c) ICC(3,1) ≥ 0.75 | Flag with (a), report (b) and (c) alongside | Step 2.8 |
| D-107 | Feedback rule content | Which metrics drive terminal feedback; cue / pattern design; default fading schedule (fixed / alternating / fading / catch-trial rate) | Decide after seeing Phase 2 results; start with alternating + catch trials | Step 3.5 |
| D-108 | Latency budget for the concurrent cue | Set from Step 3.3 measurements | — | Step 3.8 |
| D-109 | M5Stick wishlist (to be done in the M5Stick project, under its own rules) | 1. BLE command "play pattern N"; 2. `hold_ms` + `draw_ms` in `ShotPacket_t.reserved`, cue-sent bit in `form_flags`; 3. (opt.) cue toggle + fading; 4. (opt.) raw IMU stream export for M0 | Accept 1–2 before Phase 6; 3–4 optional | Phase 6 (item 4 helps Phase 1) |
| D-110 | When to rewrite ArcheryApp's Phase 4 plan (needs your permission under ArcheryApp's CLAUDE.md) | At the start of Phase 4, or earlier | At the start of Phase 4 | Phase 4 |
| D-111 | Build a video-based concurrent cue (M3b)? | Yes / no | Decide from Phase 2: only if video gives usable hold-phase information the IMU cannot (e.g. creep) | After Step 2.13 |
| D-112 | Cloud service for post-shot reanalysis | — | Laptop first; decide when Phase 5 starts | Phase 5 |
