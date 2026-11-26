# Archery Wearable System — Usage Manual

## 1. Hardware Overview

### Devices

The system uses two **M5StickC** devices (Plus or Plus2 — firmware auto-detects the variant).

| Device | Placement | Role |
|--------|-----------|------|
| **Primary** | Bow-hand wrist/forearm | Main controller — shot detection, form evaluation, buzzer feedback, BLE relay, display UI |
| **Secondary** | Draw-hand elbow/upper arm | Sends draw-arm angle to Primary via ESP-NOW |

### Buttons

Each M5StickC has two physical buttons:

```
  ┌──────────────────────┐
  │                      │
  │  [A] ← Big button   │  (front face, below screen)
  │       (front)        │
  │                      │
  │  [B] ← Side button  │  (right side, smaller)
  │       (right side)   │
  │                      │
  └──────────────────────┘
```

- **Button A** (large, front face): Tap = cycle screen / navigate menu / increment value. **Hold** (2+ seconds) = start/stop session.
- **Button B** (small, right side): Tap = open menu / select menu item / toggle / decrement value.

### Wearing the Devices

1. **Primary (bow hand)**: Attach to your bow-hand wrist or forearm with screen facing up. The IMU measures your bow arm's pitch (elevation) and roll (cant angle).
2. **Secondary (draw hand)**: Attach near your draw-hand elbow or upper arm. It measures the draw arm angle and sends it wirelessly to the Primary.

Both devices should be snug enough to not shift during shooting.

---

## 2. Power On / Off

- **Power on**: Press and hold the **power button** (left side of device) for ~2 seconds.
- **Power off**: Press and hold the power button for ~6 seconds, or the device will auto-sleep after extended idle.
- On boot, the Primary shows a splash screen ("Archery Wearable PRIMARY v1.0") for 2 seconds, then enters the HOME screen.

---

## 3. Display Screens (Primary)

The Primary device has 3 main screens plus a menu overlay. Press **Button A (tap)** to cycle between them.

### HOME Screen

Shows at-a-glance status:

```
┌─────────────────────┐
│ Sec:OK B:85%  Bat:92%│  ← Status bar (secondary connection + batteries)
│                      │
│ ACTIVE 45s           │  ← Session state (ACTIVE/IDLE + elapsed time)
│                      │
│ Shots: 12            │  ← Current shot count
│                      │
│ #12 C:1.2 E:-3.0 OK │  ← Last shot summary (green=OK, orange=issue)
│                      │
│ Buf: 3/64            │  ← BLE buffer usage
│[HOME]  A:next B:menu │
└─────────────────────┘
```

### LIVE Screen

Real-time angle display (useful for setup and debugging, not during shooting):

```
┌─────────────────────┐
│ Sec:OK B:85%  Bat:92%│
│                      │
│ P: -12.3             │  ← Primary pitch (elevation), large text
│                      │
│ R:   2.1             │  ← Primary roll (cant), large text
│                      │
│ Sec P:-8.5 R:1.3     │  ← Secondary angles
│                      │
│ #12                  │  ← Session shot count (if active)
│[LIVE]  A:next B:menu │
└─────────────────────┘
```

### SESSION Screen

Detailed last-shot breakdown with form evaluation:

```
┌─────────────────────┐
│ Sec:OK B:85%  Bat:92%│
│                      │
│ Session: 120s  #12   │
│ Last shot #12:       │
│ Cant:    2.1   OK    │  ← Green if within tolerance, red if exceeded
│ Elev:  -12.5   OK    │
│ Draw:   35.2   OVER  │
│                      │
│ Base C:0 E:-10 D:32  │  ← Current baseline reference
│ Tol  C:5 E:5  D:5   │  ← Current tolerance settings
│[SESSION] A:next B:menu│
└─────────────────────┘
```

---

## 4. Session Management

### Starting a Session

**Hold Button A** for 2+ seconds. You'll hear a **rising tone sweep** (6-note ascending beep). The HOME screen shows "ACTIVE" in green.

### During a Session

- Each detected shot triggers automatic evaluation and audio feedback (see Section 7).
- Form check runs in real-time during your draw/hold phase if enabled (see Section 6).
- Shot data is buffered for BLE transmission to the companion app.

### Stopping a Session

**Hold Button A** again for 2+ seconds. You'll hear a **falling tone sweep** (6-note descending beep). The HOME screen shows "IDLE" in gray.

---

## 5. Menu System

Press **Button B (tap)** from any screen to open the menu.

### Menu Navigation

- **Button A (tap)**: Move to next item (cycles through the list)
- **Button B (tap)**: Select/enter the highlighted item
- Bottom of screen shows context-sensitive help

### Menu Hierarchy

```
MENU (Parent)
├── Features        → Toggle on/off settings
│   ├── Form Check     [ON/OFF]   — Real-time form feedback during draw
│   ├── Shot Feedback  [ON/OFF]   — Audio feedback after each shot
│   └── > Back
├── Params          → Adjust angle baselines and tolerances
│   ├── Cant Baseline   [deg]
│   ├── Cant Tolerance  [deg]
│   ├── Elev Baseline   [deg]
│   ├── Elev Tolerance  [deg]
│   ├── Draw Baseline   [deg]
│   ├── Draw Tolerance  [deg]
│   └── > Back
├── Calibrate       → Guided calibration routines
│   ├── Set Base    — Capture form angles (baseline calibration)
│   ├── Flat Cal    — Correct accelerometer bias
│   └── > Back
└── Save&Exit       → Save settings to flash and return to main screen
```

### Editing Parameters

1. Navigate to **Params** in the parent menu.
2. Highlight a parameter (e.g., "Cant Tolerance") with Button A.
3. Press **Button B** to enter edit mode (value turns yellow, bottom shows "A:+ B:-").
4. **Button A** increments by 0.5, **Button B** decrements by 0.5.
5. Stop pressing buttons for 2 seconds — edit mode auto-exits and the value is saved.

Parameter ranges:
- Baselines: -90.0 to 90.0 degrees
- Tolerances: 0.5 to 90.0 degrees

### Feature Toggles

1. Navigate to **Features** in the parent menu.
2. Highlight a feature (e.g., "Form Check").
3. Press **Button B** to toggle ON/OFF.

---

## 6. First-Time Setup

When you first power on the devices, the default baseline values are all 0 and tolerances are at defaults. You should calibrate before your first session.

### Step 1: Flat Calibration (Accelerometer Bias Correction)

This corrects any accelerometer offset so that angles read accurately. Do this once, or whenever you notice angle drift.

1. Place the Primary device **flat on a level surface** (screen facing up, device not moving).
2. Open menu: **Button B** → select **Calibrate** → select **Flat Cal**.
3. Screen shows "FLAT CAL — Place device flat on a level surface. Press A when ready."
4. Press **Button A** when the device is stable.
5. A **3-second countdown** begins (keep device still!).
6. Device captures accelerometer data for **2 seconds**.
7. Screen shows "BIAS SAVED!" with X/Y/Z bias values.
8. After 2 seconds, returns to the HOME screen automatically.

The bias values persist across reboots (saved to flash). You can verify in the serial monitor output at boot.

### Step 2: Set Baseline (Form Angle Calibration)

This captures your "ideal form" angles so the device can evaluate your shots against them.

1. **Put on both devices** (Primary on bow hand, Secondary on draw hand).
2. Wait for "Sec:OK" to appear in the status bar (Secondary connected via ESP-NOW).
3. Open menu: **Button B** → select **Calibrate** → select **Set Base**.
4. Screen shows "SET BASELINE — Hold your best form, then press A."
5. Get into your ideal shooting stance: **raise your bow, draw, and hold at anchor**.
6. While holding form, press **Button A** with your free hand (or have a partner press it).
7. A **5-second countdown** begins — use this time to settle into your best form.
8. Device captures angles for **2 seconds** — **hold perfectly still!**
9. Screen shows "SAVED!" with Cant, Elev, and Draw baseline values.
10. After 2 seconds, returns to HOME automatically.

**Tips:**
- The 5-second countdown exists specifically so you have time to get into form after pressing the button.
- If using both devices, the draw angle baseline will capture the Secondary's pitch value.
- If the Secondary is not connected, draw baseline will be 0.
- You can cancel at any time by pressing **Button B**.
- Re-run Set Baseline whenever you change your form or equipment.

### Step 3: Adjust Tolerances

After setting baselines, adjust tolerances to match your desired strictness:

1. **Button B** → **Params**.
2. Set **Cant Tolerance** (how much bow tilt deviation is acceptable). Start with 5.0 degrees.
3. Set **Elev Tolerance** (how much elevation deviation is acceptable). Start with 5.0 degrees.
4. Set **Draw Tolerance** (how much draw angle deviation is acceptable). Start with 5.0 degrees.
5. **Button A** to go to **> Back**, then **Button B** to return to parent menu.
6. Select **Save&Exit**.

---

## 7. Understanding Audio Feedback

The device uses distinct beep patterns to communicate without requiring you to look at the screen.

### Session Sounds

| Sound | Pattern | Meaning |
|-------|---------|---------|
| Rising sweep | 6 ascending tones (1000→2000 Hz) | Session started |
| Falling sweep | 6 descending tones (2000→1000 Hz) | Session ended |

### Real-Time Form Check (During Draw/Hold)

These play BEFORE you release the arrow, while holding at anchor (requires Form Check = ON):

| Sound | Pattern | Meaning |
|-------|---------|---------|
| Brief blip | Single short 1500 Hz tone (40ms) | Draw detected — you've begun drawing |
| Double blip | Two quick 2200 Hz tones (30ms each) | Form aligned — your angles match baseline |
| Low blip | Single 1000 Hz tone (60ms) | Form deviated — your form has drifted from baseline |

### Post-Shot Feedback (After Release)

These play AFTER shot detection (requires Shot Feedback = ON):

| Sound | Pattern | Meaning |
|-------|---------|---------|
| Single short beep | 2000 Hz, 50ms | Shot detected (generic, no active session) |
| Double high beep | Two 2500 Hz tones, 80ms each | Good form — all angles within tolerance |
| Long low tone | 800 Hz, 400ms | Cant exceeded — bow tilt too far from baseline |
| Triple fast beep | Three 1800 Hz tones, 40ms each | Draw angle inconsistent |
| Fast descending | 2200→1800→1400→1000 Hz | Rushing — shot executed too quickly |
| Slow ascending | 1000→1400→1800→2200 Hz | Holding too long — aim time excessive |

### Toast Messages

In addition to audio, the screen briefly shows a colored text toast:
- **CYAN** ">> DRAW DETECTED" — draw phase started
- **GREEN** ">> FORM ALIGNED" — form looks good
- **RED** ">> FORM DEVIATED" — form drifted
- **GREEN** "SHOT: GOOD FORM" — post-shot all OK
- **RED** "SHOT: CANT EXCEEDED" / "SHOT: DRAW OFF" — post-shot issue
- **YELLOW** "SHOT: ELEV EXCEEDED" — elevation outside tolerance
- **ORANGE** "SHOT: RUSHING!" / "SHOT: HOLD TOO LONG!" — cadence issue

---

## 8. Two-Device Communication (ESP-NOW)

### How It Works

- The **Secondary** device broadcasts its IMU data via ESP-NOW at high frequency.
- The **Primary** automatically receives this data — no pairing setup needed.
- The status bar shows **"Sec:OK"** (green) when connected, **"Sec:OFF"** (red) when not.

### Setup

1. Power on both devices.
2. Secondary should show "SECONDARY" on its screen and start transmitting immediately.
3. Primary should show "Sec:OK" in the status bar within a few seconds.
4. If Secondary doesn't connect: restart both devices. They use broadcast mode with a magic number identifier, so no MAC address configuration is needed.

### What Secondary Provides

- **Draw arm pitch**: Used as the "draw angle" for form evaluation.
- **Draw arm roll**: Available for future features.
- **Battery level**: Displayed in Primary's status bar.

---

## 9. BLE Connection (Companion App)

The Primary device acts as a BLE peripheral (server) and advertises as **"ArcheryWearable"**.

### Connecting

1. Open the companion Android app (ArcheryApp).
2. Scan for BLE devices.
3. Connect to "ArcheryWearable".
4. The Primary automatically starts sending buffered shot data on connection.

### BLE Capabilities

- **Shot data**: Each shot is sent as a notification with angles, form flags, and timing.
- **Session control**: The app can remotely start/stop sessions.
- **Config sync**: The app can read and update baselines, tolerances, and feature toggles.
- **Device status**: Battery level, IMU status, Secondary connection state, session state, and shot count are updated every 2 seconds.

### Offline Operation

The device works fully standalone without BLE. Shot data is buffered in a 64-slot ring buffer and will be transmitted when a BLE client connects and subscribes to notifications.

---

## 10. Typical Range Session Workflow

1. **Before leaving**: Run **Flat Cal** if you haven't recently (one-time setup).
2. **At the range — setup**:
   a. Power on both devices.
   b. Confirm Secondary is connected ("Sec:OK" on status bar).
   c. Run **Set Baseline**: Get into your ideal form, trigger capture.
   d. Adjust tolerances in **Params** if desired.
   e. Optionally connect companion app via BLE.
3. **Shooting**:
   a. **Hold Button A** to start session (rising sweep).
   b. Shoot normally — the device evaluates each shot automatically.
   c. Listen for feedback: double-beep = good, long-low = cant issue, triple-beep = draw issue.
   d. Glance at screen between ends for details (SESSION screen).
4. **End of session**:
   a. **Hold Button A** to stop session (falling sweep).
   b. Review data on companion app, or check SESSION screen for last shot details.

---

## 11. Troubleshooting

| Problem | Solution |
|---------|----------|
| No beep on shot | Check that **Shot Feedback** is ON in Features menu. Also ensure a session is active for form evaluation beeps (basic shot-detected beep plays without session). |
| Form check not working | Ensure **Form Check** is ON in Features menu and a session is active. Set Baseline must have been run at least once. |
| Secondary not connecting | Restart both devices. Check that Secondary is powered on and showing its status screen. ESP-NOW has ~200m range outdoors. |
| Angles seem wrong | Run **Flat Cal** with device on a perfectly level surface. Check that device is worn in the correct orientation (screen up). |
| Baselines don't match my form | Re-run **Set Baseline** — hold your absolute best form during the 2s capture window. |
| Too many "EXCEEDED" alerts | Increase tolerance values in Params menu (e.g., from 5.0 to 8.0 degrees). |
| Too few alerts (missing bad form) | Decrease tolerance values (e.g., from 5.0 to 3.0 degrees). |
| Settings lost on reboot | Settings are saved to NVS flash when you select **Save&Exit** from the menu, or when calibration completes. If you change settings and the device loses power before saving, settings will revert. |
| BLE not connecting | Ensure no other device is connected to the Primary. Only one BLE client is supported at a time. Try restarting the Primary device. |
| Screen shows nothing | Wait 2 seconds for splash screen to clear. If still blank, the display may need re-initialization — restart the device. |
