# ESP32 LED Controller – Coding Agent Instructions

## Overview

You are writing firmware for an **Adafruit Huzzah32 Feather** (ESP32, 4MB flash, WiFi + Bluetooth) using **PlatformIO** with the **Arduino framework**.

This firmware controls **4 Adafruit DotStar LED strips** arranged as a geometric installation. The initial scope is **diagnostics + OTA only**. Visual/artistic patterns will be implemented later.

The goal is to produce **clean, testable, maintainable embedded firmware**, with real unit tests and a diagnostic program that acts as a first-class visual pattern.

---

## Hardware Layout (Compile-Time Configuration)

- Total segments: **40**
- LEDs per segment: **14**

Segments are soldered end-to-end into **4 independent strips**:

| Strip | Segments | LEDs |
|------|----------|------|
| Strip 1 | 11 | 154 |
| Strip 2 | 12 | 168 |
| Strip 3 | 6 | 84 |
| Strip 4 | 11 | 154 |

- Each strip is connected to its own GPIO pins on the ESP32
- Strip configuration must be defined **at compile time** (constants / structs / headers)
- No runtime configuration files

---

## Core Requirements

### 1. OTA Updates

- OTA updates over WiFi
- Safe on failure (no bricking)
- Non-blocking relative to LED updates
- OTA handling must run continuously in the main loop

---

### 2. Diagnostic Program (First-Class Pattern)

One firmware program must be a **diagnostic firmware**.

Diagnostics are treated as a **visual pattern**, not a special mode.

Each strip runs diagnostics **independently**.

#### Diagnostic Animation Behavior (Per Strip)

1. Turn **all LEDs OFF**
2. Iterate from **LED index 0 → end of strip**, segment by segment
3. For each segment (14 LEDs):
   - Flash the entire segment **ON and OFF exactly 5 times**
   - After flashing, leave the segment **ON**
   - Move to the next segment
4. After the final segment, restart from the beginning

#### Diagnostic Constraints

- Each strip uses a **distinct color**
- Animation must be **time-based** (no `delay()`)
- Implement as a **state machine**
- Intended purposes:
  - Verify data continuity
  - Identify broken solder joints
  - Determine strip orientation

---

### 3. Architecture Requirements

- Clean separation between:
  - Hardware abstraction (LEDs, pins)
  - Core logic (segments, strips, state machines)
  - Diagnostic pattern logic
  - OTA / networking

- Avoid dynamic memory allocation in render loops
- One strip failing must **not** affect others
- Strip orientation (normal/reversed) must be an explicit configuration option

---

## Testing Requirements (Software Tests)

Real **unit tests** are required.

### Test Environment

- Use **PlatformIO native environment**
- Run tests with `pio test`
- Tests run on host (desktop), not on ESP32

### What to Test

- Segment → LED index mapping
- Strip length correctness
- Segment boundaries
- Diagnostic state machine transitions
- Flash count correctness (exactly 5 flashes)
- Reset and restart behavior

### What Not to Test

- Hardware GPIO access
- LED libraries
- Timing precision at millisecond resolution

Core logic must be **Arduino-independent** so it can be tested natively.

---

## Compile-Time Guarantees

Use compile-time checks where possible:

- `static_assert` for segment counts
- `static_assert` for LED counts
- Fail compilation on invalid configuration

---

## PlatformIO Structure Expectations

Suggested structure:

```
src/
├── main_diagnostic.cpp        // Diagnostic firmware entry
│
├── core/
│   ├── layout.h              // Compile-time strip/segment config
│   ├── strip.h / strip.cpp
│   ├── segment.h / segment.cpp
│   ├── pattern.h
│   ├── diagnostic_pattern.h / .cpp
│
├── platform/
│   ├── leds.h / leds.cpp     // DotStar abstraction
│   ├── ota.h / ota.cpp
│

test/
├── test_layout.cpp
├── test_segments.cpp
├── test_diagnostic_pattern.cpp
```

Core logic must not depend on Arduino headers.

---

## PlatformIO Environments

- `env:diagnostic` – ESP32 diagnostic firmware
- `env:native` – host-based unit tests

Multiple firmware environments are preferred over `#ifdef`-heavy code.

---

## Scope Control

- Do **not** implement visual/artistic patterns yet
- Do **not** add runtime configuration
- Do **not** add network APIs beyond OTA

Focus only on:
- OTA
- Diagnostics
- Clean architecture
- Unit tests

---

## Deliverables

- PlatformIO project structure
- Diagnostic firmware implementation
- OTA integration
- Diagnostic state machine
- Unit tests
- Clear explanation of diagnostic behavior and extension points

---

## Engineering Priorities

1. Determinism
2. Testability
3. Maintainability
4. OTA safety
5. Clear separation of concerns

Avoid cleverness. Favor boring, explicit code.

