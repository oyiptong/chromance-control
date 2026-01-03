---
name: Build Environment Hygiene
description: Prevent platform-specific dependencies from leaking into the portable core, ensuring unit tests remain runnable on the host and avoiding “Arduino.h not found” failures.
version: 1
source: AGENT_SKILLS.md
---

# Build Environment Hygiene

_Extracted from `AGENT_SKILLS.md`. Canonical governance remains in the root document._

## Skill: Build Environment Hygiene

### Purpose

Prevent platform-specific dependencies from leaking into the portable core, ensuring unit tests remain runnable on the host and avoiding “Arduino.h not found” failures.

### Rules

**Core isolation:**
- Files in `src/core/**` must **never** include Arduino/ESP/FastLED headers such as:
  - `<Arduino.h>`
  - ESP-IDF headers
  - `<FastLED.h>`
  - any `WiFi*` / `AsyncWebServer*` headers
- `src/core/**` must compile cleanly in the native test environment.

**Platform isolation:**
- Files in `src/platform/**` may include Arduino/ESP-IDF specifics and are firmware-only.

**Environment selection (no implicit fallbacks):**
- Do not rely on default PlatformIO environments.
- Always specify an environment explicitly:
  - Use `pio test -e <native-env>` for core logic and mapping verification.
  - Use `pio run -e <firmware-env>` for firmware compilation/flash.
- Use the environment names defined in `platformio.ini` (e.g., `native`, `diagnostic`, `runtime`).

**Boundary enforcement:**
- If a change causes `env:native` tests to fail due to platform includes, revert the include leakage by:
  - moving platform calls behind an interface in `src/platform/**`, OR
  - introducing a small `platform_*` abstraction boundary.

### Why this matters

The implementation plan depends on portable core code. Build hygiene ensures fast host tests remain reliable and prevents slow, fragile debug cycles on device.

---

## Enforcement

These skills are **mandatory**.

Failure to comply indicates incomplete or unreliable implementation and must be corrected before work proceeds.
