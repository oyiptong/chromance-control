# Chromance Control — Implementation Plan (Diagnostics + OTA)

This file is the canonical, handoff-friendly plan for implementing the firmware described in `chromance_control_prompt.md`.

## 0) Current Repo Status (Observed)

- `platformio.ini` exists with a single environment: `env:featheresp32` (Arduino + Feather ESP32 board).
- `src/` is empty (no firmware entrypoint yet).
- `test/` contains only the default PlatformIO README (no unit tests yet).
- No project-local `AGENTS.md` was found under this repo root.

## 1) Goals / Non-Goals

### Goals (this milestone)
- Produce a “diagnostic firmware” build that:
  - Connects to WiFi and supports OTA updates safely (two-partition OTA; no bricking on failure).
  - Runs a diagnostic “pattern” (not a special mode) on 4 DotStar strips independently.
  - Implements the diagnostic behavior as a time-based state machine (no `delay()`), with per-strip distinct colors.
- Clean, testable architecture:
  - Core logic is Arduino-independent and unit-tested on host via `pio test -e native`.
  - Platform layer contains Arduino / DotStar / WiFi / OTA integration.
  - Compile-time configuration for strip layout (segments, orientation, pins) with `static_assert` guarantees.
- Real unit tests for mapping + state machine behavior.

### Non-goals (explicitly out of scope)
- Any “artistic” patterns or runtime pattern selection UI.
- Runtime configuration files, network APIs beyond OTA, web server, MQTT, etc.
- Testing GPIO, LED library correctness, or millisecond-level timing precision.

## 2) PlatformIO Environments (must be set up first)

Update `platformio.ini` to define at least:

1) `env:diagnostic` (ESP32 firmware)
   - `platform = espressif32`, `board = featheresp32`, `framework = arduino`
   - `lib_deps` includes DotStar library (typically `adafruit/Adafruit DotStar`)
   - OTA-capable partitions:
     - Verify the board’s default partition scheme includes `otadata`, `ota_0`, `ota_1`.
     - If not, add `partitions.csv` and set `board_build.partitions = partitions.csv`.
   - `src_filter` includes only the diagnostic entrypoint + shared modules:
     - Include: `src/main_diagnostic.cpp`, `src/core/**`, `src/platform/**`
     - Exclude other mains (future-proofing).

2) `env:native` (host unit tests)
   - `platform = native`
   - `test_framework = unity` (PlatformIO default) or another agreed framework (Unity preferred for lowest friction).
   - `test_build_project_src = true`
   - `src_filter` includes only Arduino-independent code:
     - Include: `src/core/**`
     - Exclude: `src/platform/**`, `src/main_*.cpp`

Deliverable commands:
- Firmware build: `pio run -e diagnostic`
- Tests: `pio test -e native`

## 3) WiFi/OTA Configuration Strategy (compile-time, no runtime files)

Pick one of these compile-time approaches (prefer A; implement B as fallback):

### A) Build flags from environment variables (recommended)
- In `platformio.ini`:
  - `build_flags = -D WIFI_SSID=\\\"${sysenv.WIFI_SSID}\\\" -D WIFI_PASSWORD=\\\"${sysenv.WIFI_PASSWORD}\\\"`
  - Optionally: `-D OTA_HOSTNAME=\\\"chromance-control\\\"`
- This keeps secrets out of source control and is still compile-time configuration.

### B) Local header not committed
- Create `include/wifi_secrets.h` (gitignored) with `#define WIFI_SSID "..."`, `#define WIFI_PASSWORD "..."`.
- `src/platform/wifi_config.h` includes this header and provides defaults/error messages if missing.

OTA “safe on failure” depends primarily on the partition layout + bootloader behavior; ArduinoOTA handles transfer + writing, but the partition scheme must support true A/B OTA.

## 4) Codebase Architecture & File Layout (target structure)

Create the following structure inside `src/`:

```
src/
  main_diagnostic.cpp
  core/
    layout.h
    types.h
    segment.h
    strip_layout.h
    diagnostic_strip_sm.h
    diagnostic_pattern.h
  platform/
    dotstar_leds.h
    dotstar_leds.cpp
    ota.h
    ota.cpp
```

Notes:
- Only `src/platform/**` may include Arduino headers (`Arduino.h`, `WiFi.h`, `ArduinoOTA.h`, `Adafruit_DotStar.h`).
- `src/core/**` must compile cleanly as standard C++ on host (no Arduino types/macros).
- Avoid dynamic allocations in render/update loops; allocate buffers statically or on the stack with bounded sizes.

## 5) Compile-Time Hardware Layout (core/layout.h)

Define the installation configuration as compile-time constants:

- Global:
  - `kStripCount = 4`
  - `kTotalSegments = 40`
  - `kLedsPerSegment = 14`
- Per-strip config array (size 4), e.g.:
  - `segment_count` (11, 12, 6, 11)
  - `reversed` (bool)
  - `data_pin`, `clock_pin` (GPIO numbers; numeric constants only)
  - `color` (distinct diagnostic color per strip)

Add compile-time validation with `static_assert`:
- Segment counts sum to 40.
- All segment counts > 0.
- Derived LED counts = `segment_count * kLedsPerSegment`.
- (Optional) total LEDs = 560 for sanity.

Even though pins are “platform” concerns, keeping them in the layout struct is acceptable as long as they’re plain integers and not Arduino-dependent types.

## 6) Core Mapping Utilities (core/strip_layout.h + segment.h)

Provide Arduino-independent helpers:

- `strip_led_count(strip) = segment_count * kLedsPerSegment`
- `segment_start_led(strip, segment_index)`:
  - Normal orientation: `segment_index * kLedsPerSegment`
  - Reversed orientation: `(segment_count - 1 - segment_index) * kLedsPerSegment`
- Segment bounds checks (used by tests and state machine):
  - Segment indices `[0, segment_count)`
  - LED indices `[0, strip_led_count)`

Unit tests must cover:
- Correct start indices at boundaries for both orientations.
- No overlap between adjacent segments.

## 7) Diagnostic State Machine (core/diagnostic_strip_sm.h)

Implement “per-strip” state machine; each strip runs independently and is ticked with a time value supplied by the caller.

### Required observable behavior (per strip)
1) All LEDs OFF (initial condition)
2) Iterate segments from 0 → end (segment-by-segment)
3) For each segment:
   - Flash the entire segment ON and OFF exactly 5 times
   - After flashing, leave the segment ON
   - Move to next segment
4) After final segment, restart from the beginning (clears previous segments back to OFF)

### Define “flash count” precisely (for implementation + tests)
- One “flash” = one full ON+OFF cycle.
- For each segment: perform 5 full cycles, then transition to a final “latched ON” phase.

### Suggested states (simple, testable)
For each strip:
- `current_segment` (0..segment_count-1)
- `flash_cycle_index` (0..5)
- `phase` enum:
  - `kAllOffInit` (optional 0-duration init; ensures invariant “all off” at start)
  - `kFlashOn`
  - `kFlashOff`
  - `kLatchedOn` (segment remains ON; earlier segments remain ON; later remain OFF)
- `next_transition_ms` (or `last_transition_ms`) to make it time-based without `delay()`

### Timing constants (compile-time constants; tests should not hardcode exact numbers)
- `kFlashOnMs` (e.g., 120)
- `kFlashOffMs` (e.g., 120)
- `kLatchedHoldMs` (optional; e.g., 100) to ensure the latched state is visible before advancing

### Outputs from the state machine (Arduino-independent)
Avoid “rendering pixels” in core. Instead, expose a minimal, testable representation:
- `SegmentRenderPlan` per strip:
  - `segments_on_before` = all segments `< current_segment` are ON
  - `current_segment_on` (bool; depends on phase)
  - segments `> current_segment` are OFF
- OR a function:
  - `bool is_segment_on(segment_index)` computed from state

This makes unit tests easy and avoids allocating LED buffers in core.

### Restart behavior
After completing the last segment’s flashing+latched-on:
- Next transition resets `current_segment = 0`, clears “segments before”, and starts flashing segment 0 again.

Unit tests must explicitly validate the restart clears prior segments.

## 8) Diagnostic Pattern Orchestrator (core/diagnostic_pattern.h)

Implement a small class that owns 4 per-strip state machines and applies per-strip colors:

- `tick(now_ms)` calls each strip SM independently
- `render(renderer)` writes the desired segment states to a renderer interface

Define a renderer interface in core (pure virtual or templated) that is Arduino-free, e.g.:
- `set_segment(strip_index, segment_index, rgb, on_off)`
- Or “higher level”:
  - `set_strip_segment_range(strip_index, start_segment, end_segment_inclusive, rgb, on)`

Keep it simple; the platform layer will translate segment operations into LED writes.

## 9) Platform Layer: DotStar Implementation (platform/dotstar_leds.*)

Responsibilities:
- Own 4 `Adafruit_DotStar` instances (or pointers if the library requires runtime sizing; allocate once, not per frame).
- Provide methods used by the diagnostic pattern renderer:
  - `set_segment(strip, segment_index, rgb, on)` → sets 14 pixels for that segment
  - `clear_strip(strip)` or `clear_all()`
  - `show_strip(strip)` and/or `show_all()` (call per-strip to isolate failures)

Implementation notes:
- Allocate pixel buffers in the library once; do not allocate per frame.
- Use per-strip update + show to preserve “one strip failing must not affect others”.
- Decide + document the DotStar color order constant (`DOTSTAR_BRG`, etc.) based on physical strips; expose as compile-time config if uncertain.

## 10) Platform Layer: WiFi + OTA (platform/ota.*)

Responsibilities:
- Connect to WiFi (STA mode) using compile-time credentials.
- Initialize ArduinoOTA with:
  - Hostname, optional password
  - Start/end/progress/error callbacks (at minimum, log to Serial)
- Provide:
  - `begin()` (connect + OTA begin)
  - `handle()` (called frequently from `loop()`)

Non-blocking requirement:
- The main loop must call `ota.handle()` continuously; do not create long `delay()` windows.
- LED updates should be time-based and throttled (e.g., target ~30–120 FPS), not “busy loop show()”.

During OTA updates:
- Optionally reduce LED update rate or pause animation to maximize OTA reliability.
- Do not block for long inside OTA callbacks.

## 11) Firmware Entrypoint (main_diagnostic.cpp)

Responsibilities:
- Instantiate platform + core objects.
- `setup()`:
  - Serial init
  - LED init and clear all strips
  - WiFi connect + OTA begin
  - Create diagnostic pattern (colors from layout)
- `loop()`:
  - `ota.handle()` every iteration
  - `pattern.tick(now_ms)`
  - If it’s time to render (based on frame clock), call `pattern.render(leds)` and `leds.show_all()` (or per-strip)

Avoid `delay()`; use `millis()` for time, and a simple frame clock (e.g., render every 10–20ms).

## 12) Unit Tests (host, PlatformIO native)

Add tests under `test/` (Unity):

1) `test/test_layout.cpp`
   - `static_assert`-like runtime checks for derived counts (strip LED counts, total segments).
   - Validate the layout constants match the prompt (11/12/6/11 segments).

2) `test/test_segments.cpp`
   - Verify `segment_start_led()` correctness for normal and reversed orientation.
   - Verify segment boundaries are non-overlapping and within strip length.

3) `test/test_diagnostic_pattern.cpp`
   - Instantiate a strip SM with a known segment count.
   - Advance time in steps and assert:
     - Exactly 5 ON+OFF cycles occur for a segment.
     - After flashing, the segment is latched ON.
     - Advancing to the next segment leaves prior segments ON.
     - After final segment, restart clears earlier segments (all OFF, then segment 0 flashing).
   - Tests must validate logic, not exact millisecond values (use the SM’s constants or tick until transitions occur).

Do not test:
- DotStar library calls
- WiFi behavior
- OTA network transfer

## 13) Definition of Done (acceptance checklist)

- `pio run -e diagnostic` builds successfully.
- Firmware runs diagnostic pattern per the prompt:
  - Per-strip distinct colors, independent progress, no `delay()`.
  - Segment-by-segment with 5 flashes each, then latched ON, then advance.
  - Restarts after last segment.
- OTA:
  - Firmware can be updated via ArduinoOTA / espota workflow.
  - OTA handling is continuously serviced in `loop()`.
  - Partition scheme supports OTA safely (A/B or equivalent).
- `pio test -e native` passes with meaningful coverage of mapping + state machine.
- Core code has no Arduino includes and no dynamic allocation in per-frame loops.

## 14) Suggested Work Order (for multiple agents)

1) Agent A: PlatformIO + scaffolding
   - Update `platformio.ini` with `env:diagnostic` and `env:native`
   - Add/verify OTA partitions config
   - Add `src/` folder structure and stub files

2) Agent B: Core layout + mapping
   - Implement `core/layout.h`, `core/strip_layout.h`, `core/types.h`
   - Add compile-time checks
   - Add mapping unit tests

3) Agent C: Diagnostic state machine + tests
   - Implement `core/diagnostic_strip_sm.h` + `core/diagnostic_pattern.h`
   - Add state machine unit tests

4) Agent D: Platform integrations
   - Implement `platform/dotstar_leds.*` + `platform/ota.*`
   - Implement `main_diagnostic.cpp` wiring
   - Manual on-device sanity check (LED behavior + OTA upload)

