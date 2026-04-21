# Vertical Swipe Cycle displayMode — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add vertical-swipe gesture that cycles `displayMode` (Normal ↔ Pet ↔ Info) in `main.cpp`, with all tap handling in those modes moving from `justPressed` to `justReleased` classification.

**Architecture:** Single-file refactor in `src/main.cpp`. Approval and overlay menus keep their existing `justPressed` tap semantics (they're gesture-safe zones). For `DISP_NORMAL`/`PET`/`INFO`, replace the four `else if (... && tap(...))` branches and the existing clock-only `if (tpClocking && tp.justReleased)` block with one unified release-based classifier that handles vertical swipe → horizontal swipe → stationary tap, in that order.

**Tech Stack:** ESP32-S3, Arduino framework, PlatformIO. No test framework for `main.cpp`; verification is compile-clean on both boards plus on-device smoke tests by the user.

---

## Reference: spec

`docs/superpowers/specs/2026-04-21-vertical-swipe-cycle-mode-design.md`

## Reference: existing snippets

- `src/main.cpp:152-155` — existing `tap(x,y,w,h)` helper (justPressed-based, reused unchanged by approval + overlay blocks)
- `src/main.cpp:159-160` — `_tpStartX/Y/Ms` declaration
- `src/main.cpp:1120` — `_tpStart*` populated on `justPressed`
- `src/main.cpp:1124-1165` — approval + overlay tap handlers (keep unchanged)
- `src/main.cpp:1166-1207` — existing tap chain + clock-mode release block (to be replaced)

---

### Task 1: Add `tappedFrom()` helper

Companion to `tap()` that checks the **press-start** position against a rect. Used by the new classifier after a stationary-tap classification, so minor drift between press and release doesn't misroute the action.

**Files:**
- Modify: `src/main.cpp` (insert after line 160, after `_tpStartMs` declaration)

- [ ] **Step 1: Insert helper**

Open `src/main.cpp`, locate the block at lines 157-160:

```cpp
// Press-start snapshot for gesture classification (swipe). Updated on every
// justPressed; read on justReleased to compute Δx/Δy/Δt.
static int16_t  _tpStartX = 0, _tpStartY = 0;
static uint32_t _tpStartMs = 0;
```

Append immediately after line 160:

```cpp

// Rect hit-test against the press-START position. Use on justReleased after
// a gesture has been classified as a stationary tap — so a tap with minor
// finger drift still targets the region the user pressed on.
static bool tappedFrom(int x, int y, int w, int h) {
  return _tpStartX >= x && _tpStartY >= y &&
         _tpStartX <  x + w && _tpStartY <  y + h;
}
```

- [ ] **Step 2: Compile-check (1.8 env)**

Run: `pio run -e waveshare-esp32s3-touch-amoled-1-8`
Expected: `SUCCESS` (helper is unused at this point; compiler may warn. If it does, ignore — Task 2 will use it).

- [ ] **Step 3: Compile-check (1.75C env)**

Run: `pio run -e waveshare-esp32s3-touch-amoled-1-75c`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
main: add tappedFrom() press-start rect helper

Companion to tap() that tests _tpStartX/Y instead of tp.x/y. Used by
the upcoming release-based gesture classifier when routing a stationary
tap to its region action, so the release-point drift doesn't misroute.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Replace touch classifier with unified release-based version

Delete the four press-based `tap()` branches for `DISP_INFO`/`DISP_PET`/`DISP_NORMAL` (HUD + bottom), and the existing clock-only release block. Replace with one classifier that:

1. **vertical swipe** → cycle `displayMode`
2. **horizontal swipe** (clock only) → cycle species (existing logic)
3. **stationary tap** → route by `_tpStart` position

**Files:**
- Modify: `src/main.cpp:1166-1207`

- [ ] **Step 1: Delete the old branches**

In `src/main.cpp`, locate the sequence starting at line 1166:

```cpp
  } else if (displayMode == DISP_INFO && tap(W - 60, 0, 60, 70)) {
    // Top-right corner → next info page (mirrors BtnB)
    beep(2400, 30);
    infoPage = (infoPage + 1) % INFO_PAGES;
  } else if (displayMode == DISP_PET && tap(W - 60, 0, 60, 70)) {
    beep(2400, 30);
    petPage = (petPage + 1) % PET_PAGES;
    applyDisplayMode();
  } else if (displayMode == DISP_NORMAL && !tpClocking && tap(12, 20, W - 24, 110)) {
    // Tap buddy body → heart reaction (HUD only; clock mode uses swipe).
    triggerOneShot(P_HEART, 2000);
    _playfulUntil = millis() + PLAYFUL_MS;
    characterInvalidate();
    if (buddyMode) buddyInvalidate();
    beep(2400, 50);
  } else if (displayMode == DISP_NORMAL && !tpClocking && tap(0, H - 32, W, 32)) {
    // Bottom strip → scroll transcript back (mirrors BtnB short-press)
    beep(2400, 30);
    msgScroll = (msgScroll >= 30) ? 0 : msgScroll + 1;
  }

  // Clock-mode gestures on release: horizontal swipe → species, near-stationary
  // tap in the buddy region → heart. Release-based classification so the two
  // gestures don't race with press-based handlers above.
  if (tpClocking && tp.justReleased) {
    int dx = tp.x - _tpStartX;
    int dy = tp.y - _tpStartY;
    uint32_t dt = millis() - _tpStartMs;
    if (abs(dx) >= 40 && abs(dx) > abs(dy) * 2 && dt < 500) {
      beep(2400, 30);
      if (dx > 0) nextPet(); else prevPet();
      _playfulUntil = millis() + PLAYFUL_MS;
    } else if (abs(dx) < 12 && abs(dy) < 12 && dt < 800 &&
               _tpStartY < 130) {
      // Upper half = buddy region; lower half is the clock digits.
      triggerOneShot(P_HEART, 2000);
      _playfulUntil = millis() + PLAYFUL_MS;
      characterInvalidate();
      if (buddyMode) buddyInvalidate();
      beep(2400, 50);
    }
  }
```

Replace the **entire** block above (from `  } else if (displayMode == DISP_INFO && tap(...))` through the closing `}` of the `if (tpClocking && tp.justReleased)` block — including the preceding `  }` which closes the menu/settings/reset `else if`) with:

```cpp
  }
  // END of press-based approval/overlay taps. Below: release-based classifier
  // for DISP_NORMAL / DISP_PET / DISP_INFO (vertical swipe cycles mode,
  // horizontal swipe in clock mode cycles species, stationary tap routes
  // to region-specific actions). Approval and overlay menus are excluded
  // so an accidental drag can't mis-decide.

  if (tp.justReleased
      && !inPrompt && !menuOpen && !settingsOpen && !resetOpen
      && !napping && !screenOff) {
    int dx = (int)tp.x - _tpStartX;
    int dy = (int)tp.y - _tpStartY;
    uint32_t dt = millis() - _tpStartMs;

    if (abs(dy) >= 40 && abs(dy) > abs(dx) * 2 && dt < 500) {
      // Vertical swipe → cycle displayMode (up = next, down = previous).
      beep(1800, 30);
      if (dy < 0) displayMode = (displayMode + 1) % DISP_COUNT;
      else        displayMode = (displayMode + DISP_COUNT - 1) % DISP_COUNT;
      applyDisplayMode();
    }
    else if (tpClocking && abs(dx) >= 40 && abs(dx) > abs(dy) * 2 && dt < 500) {
      // Horizontal swipe in clock mode → cycle species (unchanged behaviour).
      beep(2400, 30);
      if (dx > 0) nextPet(); else prevPet();
      _playfulUntil = millis() + PLAYFUL_MS;
    }
    else if (abs(dx) < 12 && abs(dy) < 12 && dt < 800) {
      // Stationary tap → route by press-start position.
      if (displayMode == DISP_INFO && tappedFrom(W - 60, 0, 60, 70)) {
        beep(2400, 30);
        infoPage = (infoPage + 1) % INFO_PAGES;
      }
      else if (displayMode == DISP_PET && tappedFrom(W - 60, 0, 60, 70)) {
        beep(2400, 30);
        petPage = (petPage + 1) % PET_PAGES;
        applyDisplayMode();
      }
      else if (displayMode == DISP_NORMAL && !tpClocking && tappedFrom(12, 20, W - 24, 110)) {
        // Tap buddy body → heart reaction (HUD; clock mode uses the block below).
        triggerOneShot(P_HEART, 2000);
        _playfulUntil = millis() + PLAYFUL_MS;
        characterInvalidate();
        if (buddyMode) buddyInvalidate();
        beep(2400, 50);
      }
      else if (displayMode == DISP_NORMAL && !tpClocking && tappedFrom(0, H - 32, W, 32)) {
        // Bottom strip → scroll transcript back (mirrors BtnB short-press).
        beep(2400, 30);
        msgScroll = (msgScroll >= 30) ? 0 : msgScroll + 1;
      }
      else if (tpClocking && _tpStartY < 130) {
        // Clock mode upper half = buddy region (lower half is clock digits).
        triggerOneShot(P_HEART, 2000);
        _playfulUntil = millis() + PLAYFUL_MS;
        characterInvalidate();
        if (buddyMode) buddyInvalidate();
        beep(2400, 50);
      }
    }
  }
```

- [ ] **Step 2: Compile-check (1.8 env)**

Run: `pio run -e waveshare-esp32s3-touch-amoled-1-8`
Expected: `SUCCESS`.

If you get "undefined reference" for `tappedFrom`, Task 1 wasn't committed — re-do it.
If you get `abs` ambiguous overload, include `<cstdlib>` at top of main.cpp (it's already included transitively via Arduino; shouldn't happen, but noted).

- [ ] **Step 3: Compile-check (1.75C env)**

Run: `pio run -e waveshare-esp32s3-touch-amoled-1-75c`
Expected: `SUCCESS`.

- [ ] **Step 4: Visually diff the change**

Run: `git diff src/main.cpp`

Verify:
- Approval block at `if (inPrompt) { ... }` is **unchanged**
- Menu/settings/reset block is **unchanged**
- The four `else if (... && tap(...))` branches are **gone**
- The old clock-only `if (tpClocking && tp.justReleased)` block is **gone**
- One new `if (tp.justReleased && !inPrompt && ...)` block is present
- `tap()` still exists (line ~152); only the callers for mode-specific regions were removed
- `tappedFrom()` from Task 1 is now referenced

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
main: vertical-swipe cycles displayMode; unify tap classifier

All DISP_NORMAL/PET/INFO gestures now classified on justReleased:
  1. vertical swipe (|dy|>=40, 2x dx, <500ms) cycles displayMode
  2. horizontal swipe in clock mode cycles species (existing)
  3. stationary tap (<12 px, <800ms) routes by press-start position

Approval and overlay menus keep press-based taps — deliberate drag is
blocked on a decision screen. Threshold values inherited from the
already-shipped clock-mode swipe-species gesture.

Spec: docs/superpowers/specs/2026-04-21-vertical-swipe-cycle-mode-design.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Update README Controls section

Document the new swipe gesture alongside the existing tap list.

**Files:**
- Modify: `README.md:111-116` (the "Touch is supplemental" bullet list)

- [ ] **Step 1: Update the touch bullet list**

Open `README.md`. Locate lines 111-116:

```
Touch is supplemental — keys remain primary:

- **Approval screen** — tap upper half = approve, lower half = deny
- **Menu / Settings / Reset** — tap a row to select+confirm in one go
- **Info / Pet pages** — tap top-right corner to cycle pages
- **Normal HUD** — tap bottom 32 px to scroll the transcript
```

Replace with:

```
Touch is supplemental — keys remain primary:

- **Swipe up / down** (Normal / Pet / Info) — cycle mode (same as Key1)
- **Swipe left / right** (clock home screen) — cycle ASCII species
- **Approval screen** — tap upper half = approve, lower half = deny
- **Menu / Settings / Reset** — tap a row to select+confirm in one go
- **Info / Pet pages** — tap top-right corner to cycle pages
- **Normal HUD** — tap buddy = heart, bottom 32 px = scroll transcript
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "$(cat <<'EOF'
readme: document vertical-swipe and existing swipe-species gestures

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-review notes

**Spec coverage — ✓**
- Classification table → Task 2 steps (vertical/horizontal/stationary branches)
- Tap region routing table → Task 2 inner stationary-tap `if/else if` chain
- Mode cycle (Normal↔Pet↔Info, wrap, `applyDisplayMode()`) → Task 2 vertical-swipe branch
- Suppression (inPrompt/menu/settings/reset/napping/screenOff) → Task 2 outer guard
- `_tpStart*` routing over `tp.x/y` → Task 1 (`tappedFrom` helper) + Task 2 usage
- Approval / overlay keep press-based → Task 2 deletes only the mode-specific branches, not the approval/menu blocks
- Thresholds `40 / 12 / 500 / 800` → Task 2 exact values preserved
- README documentation → Task 3

**Placeholder scan — ✓** No "TBD/TODO/implement later"; all code shown complete; all commands exact.

**Type consistency — ✓** `tappedFrom` signature matches the call sites. `dx`/`dy` are `int` (widened from `int16_t - int16_t`), `dt` is `uint32_t` (matches `_tpStartMs`). `displayMode` remains `uint8_t`, arithmetic goes through `% DISP_COUNT`.

## On-device verification (user, after plan completes)

The plan produces compile-clean firmware; actual gesture behaviour has to be eyeballed on the panel.

1. Flash 1.75C: `pio run -e waveshare-esp32s3-touch-amoled-1-75c --target upload`
2. Power on, wait for boot. Verify **existing behavior unchanged**:
   - Key1 short still cycles Normal → Pet → Info
   - Horizontal swipe in clock mode still cycles species
   - Tap buddy in HUD still triggers heart
   - Tap top-right in Pet/Info still cycles sub-page
   - Tap bottom strip in HUD still scrolls transcript
3. Verify **new behavior**:
   - Swipe up in Normal → goes to Pet
   - Swipe up in Pet → goes to Info
   - Swipe up in Info → back to Normal
   - Swipe down reverses
   - Swipe up in clock mode → exits to Pet
4. Verify **blocked contexts**:
   - During approval prompt: swipe does nothing; tap upper/lower still decides
   - With menu open: swipe does nothing; tap row still selects
5. Flash 1.8: `pio run -e waveshare-esp32s3-touch-amoled-1-8 --target upload` — repeat smoke test above.
