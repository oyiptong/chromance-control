# Plan Feedback — Chromance Control Firmware (Diagnostics + OTA)

This document is the **authoritative, consolidated feedback** for the implementation of the Chromance Control firmware. It incorporates **all approved feedback and refinements** and supersedes any prior review notes.

The recipient of this document is the **coding agent responsible for implementation**. No external context is required.

Overall status: **APPROVED — PROCEED WITH IMPLEMENTATION**

---

## 1. Overall Assessment

The proposed implementation plan is **architecturally sound, well-scoped, and appropriate** for an ESP32-based LED installation with OTA requirements.

If implemented faithfully, the plan will produce:

- Deterministic, visually clear diagnostic firmware
- Safe, robust OTA update capability
- Real, meaningful unit-test coverage
- A clean foundation for future visual pattern work

There are **no architectural blockers**. The refinements below are intended to harden the system and eliminate known ESP32 failure modes.

---

## 2. Strengths to Preserve (Do Not Regress)

The following design decisions are correct and **must be preserved**:

### 2.1 Diagnostics as a First-Class Pattern

- Diagnostics are implemented as a **pattern**, not a special-case mode
- Behavior is defined as a **state machine**, not ad-hoc animation logic
- A “flash” is explicitly defined as one ON+OFF cycle
- Restart behavior after the final segment is deterministic and testable

This approach is correct and must not be simplified.

---

### 2.2 Arduino-Independent Core Logic

- All code under `src/core/**` must compile as standard C++
- No Arduino headers, macros, or platform types in core logic
- Platform-specific code is isolated under `src/platform/**`

This separation is essential for host-side unit testing and long-term maintainability.

---

### 2.3 Renderer Abstraction Returning Intent (Not Pixels)

Core logic must expose **render intent** (segment on/off state), not raw LED buffers.

Benefits:
- Simpler unit tests
- No duplication of LED buffers
- No platform leakage into core logic

This design choice is explicitly approved and should not be replaced with pixel-level logic in core.

---

### 2.4 Native Unit Tests via PlatformIO

- Use `env:native` for host-side unit tests
- Use `src_filter` to exclude Arduino/platform code
- Tests focus on logic, not hardware

Testing scope is correct and should not be expanded to hardware behavior.

---

## 3. Mandatory Technical Requirements

These items are **required** and must be implemented.

---

### 3.1 Explicit OTA Partition Scheme (Mandatory)

OTA requires two sufficiently large application partitions (`app0` and `app1`). Default partition schemes often allocate excessive space to SPIFFS, which is unused in this project and can silently break OTA.

**Required configuration (`platformio.ini`):**

```ini
board_build.partitions = min_spiffs.csv
```

Notes:
- `min_spiffs.csv` preserves OTA A/B slots while minimizing unused filesystem space
- `huge_app.csv` **must not be used**, as it removes OTA support unless explicitly verified otherwise

This requirement is non-negotiable.

---

### 3.2 Non-Blocking, Cooperative Main Loop

The firmware must remain cooperative to ensure OTA reliability and avoid watchdog resets.

Requirements:
- No `delay()` calls in firmware logic
- Diagnostic updates must be incremental and time-based
- `ArduinoOTA.handle()` must be serviced continuously in `loop()`

DotStar LEDs are SPI-based and `show()` calls are expected to be fast; however, unbounded loops or blocking operations must be avoided.

---

## 4. Required / Recommended Refinements

These refinements improve robustness and maintainability and should be incorporated.

---

### 4.1 Diagnostic Brightness Limit (Required)

Add an explicit compile-time brightness cap for diagnostics to prevent power issues and improve visual clarity.

Recommended constant:

```cpp
constexpr uint8_t kDiagnosticBrightness = 64;
```

This should be applied consistently across all diagnostic rendering.

---

### 4.2 Diagnostic Colors as Compile-Time Data (Required)

Each strip’s diagnostic color must be defined as part of the **compile-time strip configuration**, not hardcoded in logic.

Recommendation:
- Add `diagnostic_color` to the per-strip configuration struct
- Pass colors into the diagnostic pattern at construction

This keeps diagnostics deterministic and test-friendly.

---

### 4.3 OTA Validity Marking Discipline (Required)

New firmware must only be marked as valid **after successful completion of `setup()`**.

This preserves OTA rollback behavior in the event of early crashes.

---

### 4.4 Firmware Version Identifier (Required)

Include a compile-time firmware version string:

```cpp
constexpr char kFirmwareVersion[] = "diagnostic-0.1.0";
```

Print the version:
- At boot
- At OTA start

This is required for OTA-only iteration and debugging.

---

### 4.5 Library Version Pinning (Recommended)

Pin the Adafruit DotStar dependency using semantic versioning to ensure reproducibility while allowing safe updates.

Recommended configuration:

```ini
lib_deps =
    adafruit/Adafruit DotStar @ ^1.2.0
```

This allows patch- and minor-version updates while preventing unexpected breaking changes.

---

### 4.6 Optional Minimal Serial Escape Hatch

Optional but useful during bring-up:

- Minimal serial commands (e.g., reset diagnostics, focus a strip)
- Not a full CLI

This may be deferred if strict scope control is preferred.

---

## 5. Items Explicitly Out of Scope

The following must **not** be added at this stage:

- Runtime configuration files
- Artistic / visual pattern systems
- Network APIs beyond OTA
- Arduino dependencies in core logic
- Hardware behavior tests in unit tests

Scope discipline is intentional and must be respected.

---

## 6. Acceptance Criteria (Definition of Done)

Implementation is complete when:

- `pio run -e diagnostic` builds successfully
- Diagnostic firmware behavior matches the specification exactly:
  - Per-strip independent progress
  - Segment-by-segment flashing (5 ON+OFF cycles)
  - Latched ON behavior
  - Deterministic restart after final segment
- OTA updates succeed reliably via WiFi
- OTA handling is continuously serviced in `loop()`
- `pio test -e native` passes with meaningful coverage of:
  - Layout correctness
  - Segment mapping
  - Diagnostic state machine behavior
- Core logic remains Arduino-independent
- No dynamic allocation occurs in per-frame render/update paths

---

## 7. Final Status

**APPROVED** — proceed with implementation following this document as the authoritative gui
