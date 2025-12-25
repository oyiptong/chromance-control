### 2025-12-24 — DotStar strip GPIO pin mapping updated

Status: 🟢 Done

What was done:
- Set the compile-time DATA/CLOCK GPIO pins for all 4 DotStar strips to match the provided wiring table

Files referenced:
- src/core/layout.h

Notes / Decisions:
- Strip 1 uses GPIO23/GPIO22, strip 2 uses GPIO19/GPIO18, strip 3 uses GPIO17/GPIO16, strip 4 uses GPIO14/GPIO32

### 2025-12-24 — Diagnostic firmware build fixed and verified

Status: 🟢 Done

What was done:
- Fixed `pio run -e diagnostic` compile failure caused by an incorrectly escaped `OTA_HOSTNAME` macro definition
- Updated PlatformIO config to use `build_src_filter` / `test_build_src` to avoid deprecated options
- Verified `pio run -e diagnostic` completes successfully

Files referenced:
- platformio.ini
- src/platform/ota.cpp

Notes / Decisions:
- `OTA_HOSTNAME` now defaults via `src/platform/wifi_config.h` and can be overridden via `OTA_HOSTNAME` environment variable (applied in `scripts/wifi_from_env.py`)

### 2025-12-24 — Applied DotStar GPIO pin assignment

Status: 🟢 Done

What was done:
- Updated the compile-time strip pin mapping to match the provided DATA/CLOCK GPIO assignment for all 4 strips

Files referenced:
- src/core/layout.h

Notes / Decisions:
- Strip 1 uses GPIO23/GPIO22, strip 2 uses GPIO19/GPIO18, strip 3 uses GPIO17/GPIO16, strip 4 uses GPIO14/GPIO32

### 2025-12-24 — Diagnostic firmware scaffold + native unit tests added

Status: 🧪 Test added

What was done:
- Added PlatformIO `diagnostic` (ESP32) + `native` test environments with OTA-capable partitions
- Implemented Arduino-independent core: layout constants, segment/LED mapping, per-strip diagnostic state machine, and renderer interface
- Implemented platform layer: DotStar renderer + WiFi/ArduinoOTA manager and diagnostic firmware entrypoint
- Added Unity host tests covering layout, mapping boundaries, flash count/latching, and restart clearing behavior

Files referenced:
- platformio.ini
- src/core/layout.h
- src/core/strip_layout.h
- src/core/diagnostic_strip_sm.h
- src/core/diagnostic_pattern.h
- src/platform/ota.cpp
- src/platform/dotstar_leds.cpp
- src/main_diagnostic.cpp
- test/test_main.cpp
- test/test_diagnostic_pattern.cpp
- test/test_segments.cpp
- test/test_layout.cpp

Notes / Decisions:
- WiFi credentials resolve via env vars when set, otherwise fall back to `include/wifi_secrets.h` (gitignored)
- OTA rollback validity marking uses `esp_ota_mark_app_valid_cancel_rollback()` when available
- DotStar color order is set to `DOTSTAR_BRG` and may need adjustment for the physical strips

### YYYY-MM-DD — Project initialized; implementation plan finalized

Status: 🔵 Decision

What was done:
- Finalized and approved the implementation plan for Chromance Control diagnostics + OTA
- Established task tracking and agent process constraints

Files referenced:
- AGENTS.md
- AGENT_SKILLS.md

Notes / Decisions:
- Implementation has not yet started
- All subsequent work must be logged per Task Log Update skill
