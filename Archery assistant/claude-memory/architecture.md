# Architecture — Archery Video Mocap (Python reference implementation)

Static reference for structure, interfaces, contracts and extension points. The *why* lives in the design brief; the *order* lives in `DEVELOPMENT_PLAN.md`. Update this file whenever an interface or contract changes.

Status: **planned** (no code yet, 2026-11-16). Signatures below are the intended shape; adjust during implementation and record changes here.

---

## 1. Data Flow

```
                 ┌──────────── config (TOML) + profile (TOML) ────────────┐
                 ▼                                                         ▼
video file ─► VideoSource ─► PoseBackend ─► Filter chain ─► PhaseDetector ─► Metrics ─► ShotRecord ─► Store ─► Report
   │             (frames +      (canonical      (causal or     (causal state      (registry)     (versioned
   │              exact PTS)     landmarks)      offline)       machine + offline                 JSON)
   │                                                            refinement)
   │                                                 ▲
   └─► AudioSource ─► ReleaseSources ─► EventFuser ──┘      FeedbackScheduler ─► FeedbackRules ─► FeedbackOutputs
                       (audio onset,     (+ ClockModel        (cue on/off per      (full-draw cue,      (log / sound /
                        kinematic,        per source)          shot, seeded)        terminal rules)      M5Stick stub …)
                        M5Stick log)
```

Batch mode (Phase 2) and streaming mode (Phase 3) run the **same components**. Streaming mode only admits components with `causal = True`. Offline refinement steps are separate components layered on top, never baked into the causal ones.

---

## 2. Package Layout

```
Archery assistant/
├── CLAUDE.md
├── README.md
├── 射箭视频mocap + M5Stick 训练平台 — 草稿.md     # design brief (Chinese)
├── pyproject.toml                                  # src layout; pytest pythonpath = ["src"]
├── configs/
│   ├── default.toml                                # pipeline composition + all tunables
│   └── profiles/<name>.toml                        # handedness, height, anchor type, stance
├── contracts/                                      # language-neutral; consumed by the Kotlin port too
│   ├── landmarks.json                              # canonical landmark vocabulary (versioned)
│   ├── shot_record.md                              # ShotRecord schema description (versioned)
│   └── fixtures.md                                 # golden fixture JSON format
├── docs/
│   └── recording_protocol.md                       # M0 recording protocol + checklist
├── models/                                         # pose model files (.task / .onnx) — gitignored
├── data/                                           # default data root — gitignored (see D-103)
│   └── sessions/<session_id>/...
├── src/archery_mocap/
│   ├── core/          # pure, no I/O: types, skeleton & roles, geometry, registry, clock model
│   ├── schemas/       # ShotRecord / SessionMeta / series sidecar (de)serialization, versions, migrations
│   ├── io/            # video + audio decoding (PyAV), session store, provenance cache
│   ├── pose/          # PoseBackend protocol + mediapipe_backend.py (+ rtmlib_backend.py later)
│   ├── filters/       # Filter protocol + none.py, one_euro.py, butterworth.py, gap_fill.py
│   ├── events/        # ReleaseSource protocol + audio_onset.py, kinematic.py, clap_sync.py, fuser.py
│   ├── phases/        # state_machine.py (causal), refine.py (offline)
│   ├── metrics/       # base.py (MetricSpec / Metric / MetricResult) + one file per metric
│   ├── analysis/      # noise_floor.py, perturbation.py, event_accuracy.py, backend_compare.py, conditioning.py
│   ├── feedback/      # scheduler.py, rules/, outputs/
│   ├── report/        # html.py, overlay.py, templates/
│   ├── pipeline/      # config.py, builder.py, batch.py, streaming.py
│   ├── synth/         # synthetic shots + audio with ground truth (tests and development)
│   ├── adapters/      # cross-project: m5stick.py (ShotPacket_t parsers), scores_csv.py, archeryapp.py (later)
│   ├── cli.py
│   └── __main__.py
├── tools/
│   └── label_events.py                             # manual event labeling (frame stepper)
└── tests/
    ├── unit/<package>/...
    ├── integration/
    ├── golden/                                     # fixture JSON + expected outputs + test_golden.py
    └── conftest.py
```

---

## 3. Core Data Model (`core/`, `schemas/`)

### 3.1 Canonical landmarks and semantic roles

- **Landmark vocabulary** (`contracts/landmarks.json`): open, versioned list of string names (`nose`, `mouth_l`, `mouth_r`, `l_shoulder`, `r_wrist`, `r_thumb`, …). Hand and face extensions are added later with namespaced names (`r_hand.thumb_tip`, …). Adding names = additive change.
- **Backends map to the vocabulary.** Each PoseBackend declares which canonical landmarks it provides. Nothing downstream knows about MediaPipe or RTMW indices.
- **Semantic roles** (`bow_shoulder`, `bow_elbow`, `bow_wrist`, `draw_shoulder`, `draw_elbow`, `draw_wrist`, `draw_thumb`, …) are resolved through the profile's handedness. **Metrics and phase logic use roles, never left/right.**

### 3.2 Types (intended shape)

```python
@dataclass(frozen=True)
class SeriesMeta:
    schema_version: int
    backend: str                 # e.g. "mediapipe_pose"
    backend_version: str         # library + model file hash
    image_size: tuple[int, int]
    filters_applied: tuple[str, ...]
    extra: Mapping[str, Any]     # preserved unknown fields

@dataclass(frozen=True)
class KeypointSeries:
    t: np.ndarray                # (T,) seconds on the session clock (from exact PTS)
    xy: np.ndarray               # (T, K, 2) pixels; NaN where missing
    conf: np.ndarray             # (T, K) 0..1
    names: tuple[str, ...]       # K canonical landmark names
    meta: SeriesMeta
    world_xyz: np.ndarray | None = None   # optional channel (MediaPipe world landmarks); unused for now

@dataclass(frozen=True)
class Event:
    kind: str                    # open vocabulary: draw_start, full_draw, release, follow_through_end, let_down, cue_sent, ...
    t: float                     # session clock, seconds
    source: str                  # phase_detector, audio_onset, kinematic, m5stick, manual, fused, ...
    confidence: float
    data: Mapping[str, Any] = field(default_factory=dict)

@dataclass(frozen=True)
class ShotContext:               # everything a metric may read for one shot
    series: KeypointSeries       # windowed around the shot
    events: Mapping[str, Event]  # fused events by kind
    raw_events: Sequence[Event]  # all source events (for agreement analysis)
    profile: Profile
    calibration: Calibration | None   # mm/px etc., when available
```

### 3.3 Clock model

All times are on one **session clock** (video PTS, seconds). Every other source (audio track, M5Stick, phone mic later) has a `ClockModel` (offset, optional drift) that maps its time to the session clock. Offsets are estimated from matched events (clap, releases) with a robust estimator, and stored in the session metadata with their uncertainty.

---

## 4. Component Interfaces (Python `Protocol`s)

Every component kind has: a protocol in `<package>/base.py`, a registry kind, a `name`, a `version` (bumped when outputs change), `causal: bool` where relevant, and `requires` / `provides` capability declarations.

```python
class VideoSource(Protocol):
    def frames(self) -> Iterator[Frame]: ...          # Frame = (pts_seconds, image ndarray)
    info: VideoInfo                                    # fps stats, VFR flag, size, rotation, has_audio

class PoseBackend(Protocol):
    name: str; version: str
    provides: frozenset[str]                           # canonical landmark names
    def reset(self) -> None: ...
    def estimate(self, frame: Frame) -> FrameKeypoints: ...   # one frame; batch = loop + stack

class Filter(Protocol):
    name: str; version: str; causal: bool; portable: bool
    def reset(self) -> None: ...
    def step(self, t: float, xy: np.ndarray, conf: np.ndarray) -> np.ndarray: ...   # streaming
    def apply(self, series: KeypointSeries) -> KeypointSeries: ...                 # batch

class ReleaseSource(Protocol):
    name: str; version: str
    requires: frozenset[str]                           # e.g. {"audio"} or {"role:draw_wrist"}
    def detect(self, session: SessionInputs) -> list[Event]: ...

class PhaseDetector(Protocol):                         # causal state machine
    name: str; version: str; causal: bool = True; portable: bool = True
    def reset(self) -> None: ...
    def update(self, fk: FrameKeypoints, external: Sequence[Event]) -> list[Event]: ...

class Refiner(Protocol):                               # offline only
    def refine(self, series: KeypointSeries, events: list[Event]) -> list[Event]: ...

@dataclass(frozen=True)
class MetricSpec:
    name: str; version: int; unit: str
    phase_window: str                                  # e.g. "release+0..600ms", "full_draw..release"
    requires_roles: frozenset[str]; requires_events: frozenset[str]
    source: str                                        # "video" | "imu" | "video+imu"
    plane: str                                         # "coronal" | ...
    rationale: str                                     # mechanism: why this affects the shot
    optional_module: str | None = None                 # e.g. "anchor"
    portable: bool = True

class Metric(Protocol):
    spec: MetricSpec
    def compute(self, shot: ShotContext) -> MetricResult: ...   # value(s) + quality, or NotAvailable(reason)

class FeedbackScheduler(Protocol):                     # decides cue on/off per shot (seeded, logged)
    def cue_enabled(self, shot_index: int, history: Sequence[ShotRecord]) -> bool: ...

class FeedbackRule(Protocol):
    def evaluate(self, ctx: FeedbackContext) -> list[FeedbackCommand]: ...

class FeedbackOutput(Protocol):
    name: str
    latency_model: LatencyModel                        # measured or assumed, logged with every send
    def send(self, cmd: FeedbackCommand) -> DeliveryReceipt: ...  # includes estimated delivery time
```

---

## 5. Contracts (versioned)

| Contract | Where | Consumers | Change policy |
|---|---|---|---|
| Landmark vocabulary | `contracts/landmarks.json` | all pose backends, metrics, Kotlin port | additive only; renames = new name + alias |
| KeypointSeries on disk | `.npz` arrays + JSON sidecar (`SeriesMeta`) | cache, fixtures | `schema_version`; readers tolerate unknown sidecar fields |
| ShotRecord | `contracts/shot_record.md`, `schemas/shot_record.py` | store, report, ArcheryApp later | `schema_version`; unknown fields preserved in `extra`; breaking change → v(N+1) + migration in `schemas/migrations.py` |
| SessionMeta / `session.toml` / `scores.csv` | `docs/recording_protocol.md`, `schemas/` | ingest, scores join | versioned template; validator gives clear errors |
| Golden fixtures | `contracts/fixtures.md`, `tests/golden/` | Python regression, Kotlin port acceptance | changes only with user approval (CLAUDE.md) |
| M5Stick `ShotPacket_t` | authority: `../wearable m5stick/claude-memory/overall_design.md` | `adapters/m5stick.py` | one parser per firmware packet version; never edit old parsers |

ShotRecord v1 (outline):

```
schema_version, session_id, shot_index, arrow_no
events:      { fused: {kind: {t, source, confidence}}, raw: [ ... all source events ... ] }
metrics:     { <name>: { version, value, unit, quality, n_frames } | { version, unavailable: reason } }
feedback:    { cue_enabled, cue_sent_t, cue_channel, cue_est_delivery_t, terminal: [...] }
outcome:     { score, x_mm, y_mm, self_rating }            # from scores.csv / ArcheryApp
provenance:  { code_version, config_hash, backend, backend_version, filters, component_versions }
extra:       { ... preserved unknown fields ... }
```

Metric values are always stored with the metric **version**. Comparisons across sessions only mix values with the same metric name + version (or an explicit converter).

---

## 6. Design-for-Change Rules

The user expects all three archery projects' plans to change. These rules keep change cheap:

1. **Interfaces at swap points.** Video source, pose backend, filter, phase detector, refiner, release source, event fuser, metric, feedback scheduler/rule/output, store, report are all behind protocols (section 4). Callers depend on protocols only.
2. **Add, don't edit.** Concrete implementations register themselves in `core/registry.py` (`@register("metric", "follow_through_draw_hand")`). The pipeline builder instantiates them from config. **Pipeline code never names a concrete implementation.** Adding a metric, backend, release source or feedback output = one new file + one config line.
3. **Config over constants.** Every threshold, window length and tunable lives in `configs/default.toml` (overridable per session / CLI). User-owned thresholds reference their `DECISIONS.md` id in a comment. No magic numbers in code.
4. **Capability negotiation, graceful degradation.** Components declare `requires` / `provides`. The builder checks them at build time: a metric whose landmarks the backend lacks is disabled **with a recorded reason**, not a crash. A shot missing an event yields `NotAvailable(reason)` for affected metrics.
5. **Version everything that persists or crosses a boundary** (section 5). Additive changes preferred; readers tolerate unknown fields; breaking changes get a new version plus a migration or adapter, and old data stays readable.
6. **Provenance + stage caching.** Each derived artifact stores component names/versions, config hash and input fingerprints. A stage is recomputed only when its provenance changes, so swapping one component re-runs only what depends on it.
7. **Semantic roles, not sides** (section 3.1): handedness and other per-person differences live in the profile.
8. **Causal-first core, one code path.** The phase detector, filters (causal ones), metrics and feedback logic are written so the same code serves batch and streaming. Offline-only improvements are separate components (`causal = False`).
9. **Portable core.** Components marked `portable = True` (causal filters, phase state machine, metrics, scheduler, rules) use only the standard library + numpy with explicit algorithms, so they can be ported to Kotlin and verified by golden fixtures. A unit test enforces that portable modules do not import scipy / cv2 / mediapipe / av.
10. **Cross-project seams are adapters.** M5Stick packets, ArcheryApp data and future wearables enter and leave only through `adapters/`, one file per external format/version.
11. **Cheap seams, no speculative features.** Create an interface where a second implementation is known or the brief anticipates change. Do NOT build plugin loading from external packages, dependency-injection frameworks, services, or features the plan does not ask for.

---

## 7. Extension Recipes

| To add … | Do this | Touch nothing else except |
|---|---|---|
| a metric | new file in `metrics/`, implement `Metric` with a full `MetricSpec` (incl. rationale), register, add unit tests with synthetic shots | `configs/default.toml` (enable + params) |
| a pose backend | new file in `pose/`, map its keypoints to canonical names (extend `landmarks.json` additively if needed), register | config `pose.backend` |
| a release source / wearable | new file in `events/` (or `adapters/` for external formats), declare `requires`, register; give it a ClockModel | config `release.sources` |
| a feedback channel (bone-conduction headset, new wearable) | new file in `feedback/outputs/` with a latency model, register | config `feedback.outputs` |
| a filter | new file in `filters/`, declare `causal` / `portable`, register | config `filters.chain` |
| a ShotRecord field | additive field + bump minor note in `contracts/shot_record.md`; breaking → new version + migration | `schemas/` |
| a second camera (Phase 7) | new VideoSource(s) + a multi-view backend that still outputs canonical landmarks (3D channel optional) | config |

---

## 8. Porting Contract (Python → Kotlin, Phase 4)

- The Python reference defines behaviour; the Kotlin port must reproduce **golden fixtures layer 1** (keypoint series → events and metrics, within tolerances) exactly as exported in `contracts/fixtures.md` format.
- Layer 2 (video → keypoints) is checked with the same MediaPipe `.task` file on both sides, within a looser tolerance.
- Only `portable = True` components are ported. Offline-only components stay in Python (laptop / cloud reanalysis).

---

## 9. Cross-Project Integration Points

| Sibling | Today | Integration (when) |
|---|---|---|
| M5Stick (`../wearable m5stick`, SP13) | `ShotPacket_t` v1 (20 B) over BLE; IMU state machine with its own full-draw cue (`FORM_ALIGNED`) | Phase 3.6: parse logged packets offline. Phase 6: live release source + buzzer output (needs wishlist items, D-109) |
| ArcheryApp (`../Archery_android_app`, Phase 2.4.7) | Sessions / ends / arrows / scores in Room; Phase 3 BLE not started; Phase 4 superseded | Phase 2: scores via `scores.csv`. Phase 4: this pipeline becomes `:feature:mocap` (+ shared `:core:ble`) inside ArcheryApp (D-003, D-110) |

---

## 10. Known Issues / To Revisit

- MediaPipe VIDEO mode may apply internal temporal smoothing that blunts the release spike; Step 2.1 must determine this and, if needed, compare against IMAGE mode.
- Phone videos are often variable frame rate; never assume constant fps. Always use PTS.
- Env is `archery-mocap` (Python 3.12, D-101). The user-level site-packages (`AppData\Roaming\Python\Python312`) leaks into any Python 3.12 unless disabled. Always run with `-s` / `PYTHONNOUSERSITE=1`.
- mediapipe is 1.2.3, a newer major version than most online examples (0.10.x). Check API details against the installed package, not tutorials.
