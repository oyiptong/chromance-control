### 2025-12-26 — Diagnostic color constants renamed to 0-based strip indices

Status: 🔵 Decision

What was done:
- Renamed diagnostic strip color constants to use 0-based indices (`kStrip0DiagnosticColor..kStrip3DiagnosticColor`) for consistency with `kStripConfigs[0..3]`
- Verified `pio test -e native` and `pio run -e diagnostic` still succeed

Files referenced:
- src/core/layout.h

### 2025-12-26 — Segment diagnostics updated to chase LEDs then flash by order

Status: 🟢 Done

What was done:
- Updated per-segment diagnostics so each segment first “chases” a single lit LED across indices 0→13 for 3 full passes (1 second per pass), then flashes the full segment a number of times equal to its 1-based segment order, then latches ON
- Updated segment-diagnostics rendering to use the single-LED renderer during chase, while keeping previous segments latched ON and later segments OFF
- Updated host unit tests and verified `pio test -e native` and `pio run -e diagnostic`

Files referenced:
- src/core/diagnostic_strip_sm.h
- src/core/diagnostic_pattern.h
- test/test_diagnostic_pattern.cpp

### 2025-12-26 — Diagnostic colors labeled with named constants

Status: 🔵 Decision

What was done:
- Added named color constants (e.g., Red/Green/Blue/Magenta) and mapped each strip’s diagnostic color to an explicit name for readability

Files referenced:
- src/core/layout.h

### 2025-12-26 — Diagnostic program updated to global prelude + ordered segment flashes

Status: 🟢 Done

What was done:
- Implemented a multi-phase diagnostic sequence: initial all-off hold, all-on (per-strip color) for 5s, global flash for 10 cycles, then per-segment flashing with flash count equal to 1-based segment order and latch-on per segment
- Added “done” handling so strips remain fully lit after completing their segments until all strips finish, then the sequence restarts from the initial all-off hold
- Updated Unity host tests to cover segment-order flash counts, completion behavior, and high-level phase sequencing; verified `pio test -e native` and `pio run -e diagnostic` succeed

Files referenced:
- src/core/diagnostic_strip_sm.h
- src/core/diagnostic_pattern.h
- test/test_diagnostic_pattern.cpp
- test/test_main.cpp

Notes / Decisions:
- Segment flash count uses 1-based order within each strip (segment 0 flashes once, segment 1 flashes twice, etc.)

### 2025-12-26 — Diagnostic strip colors made explicit in layout config

Status: 🔵 Decision

What was done:
- Refactored the per-strip diagnostic color assignment into named constants at the top of the layout config for easy editing

Files referenced:
- src/core/layout.h

Notes / Decisions:
- Strip numbering for color constants matches the wiring table (“Strip 1” = `kStripConfigs[0]`, …)

### 2025-12-24 — DotStar strip GPIO pin mapping updated

Status: 🟢 Done

What was done:
- Set the compile-time DATA/CLOCK GPIO pins for all 4 DotStar strips to match the provided wiring table

Files referenced:
- src/core/layout.h

Notes / Decisions:
- Strip 1 uses GPIO23/GPIO22, strip 2 uses GPIO19/GPIO18, strip 3 uses GPIO17/GPIO16, strip 4 uses GPIO14/GPIO32

### 2025-12-25 — Diagnostic pattern changed to per-segment LED chase

Status: 🟢 Done

What was done:
- Replaced per-segment flashing diagnostics with a per-segment LED chase: light LED indices 0→13 at 0.5s steps, repeat 5 times, then latch the full segment ON and advance
- Updated the diagnostic renderer interface and DotStar platform implementation to support “single LED within segment” rendering
- Updated and revalidated host unit tests, and confirmed `pio run -e diagnostic` builds successfully

Files referenced:
- src/core/diagnostic_strip_sm.h
- src/core/diagnostic_pattern.h
- src/platform/dotstar_leds.h
- src/platform/dotstar_leds.cpp
- test/test_diagnostic_pattern.cpp
- test/test_main.cpp

Notes / Decisions:
- Segment/strip colors remain unchanged; only the diagnostic animation logic was updated

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

### 2026-01-03 — Milestone 0: wiring source-of-truth inputs created (templates)

Status: 🟢 Done

What was done:
- Audited firmware sources (`src/core/layout.h`, `src/core/strip_layout.h`, `src/platform/dotstar_leds.*`) for any existing segment→strip wiring order/direction tables; none exist beyond per-strip segment counts/pins.
- Added schema-valid placeholder wiring inputs to unblock mapping pipeline work:
  - `mapping/wiring.json`: 4 strips, seg IDs 1..40 exactly once, default `dir: a_to_b`
  - `mapping/wiring_bench.json`: strip1-only subset (seg 1..11), default `dir: a_to_b`
- Added `mapping/README_wiring.md` with the segment topology key and a structured validation/correction checklist.

Files touched:
- mapping/wiring.json
- mapping/wiring_bench.json
- mapping/README_wiring.md

Notes / Decisions:
- Strip segment distribution (11/12/6/11) is derived from `src/core/layout.h`; per-strip ordering and per-segment direction remain placeholders pending physical validation.
- `scripts/generate_ledmap.py` does not yet support subset wiring for bench mode; planned per `docs/architecture/wled_integration_implementation_plan.md` (Section 6.2).

Proof-of-life:
- wiring.json: PASS (4 strips, segs=1..40)
- wiring_bench.json: PASS (strips=1, segs=11)

### 2026-01-04 — Native unit tests run (post Milestone 0 wiring templates)

Status: 🟢 Done

What was done:
- Ran host/native unit tests to ensure core code still passes after adding wiring inputs and wiring README (no firmware/mapping outputs regenerated).

Files touched:
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (7 test cases)

### 2026-01-04 — Fixed generator collision (rounding semantics)

Status: 🟢 Done

What was done:
- Fixed a deterministic coordinate collision in `scripts/generate_ledmap.py` caused by Python `round()` (ties-to-even) when sampling symmetric segments near shared vertices.
- Updated the implementation plan to specify `round_half_away_from_zero` so the algorithm is reproducible across languages and does not silently reintroduce collisions.

Files touched:
- scripts/generate_ledmap.py
- docs/architecture/wled_integration_implementation_plan.md
- TASK_LOG.md

Notes / Decisions:
- This changes only raster coordinate rounding; strip/segment ordering still comes solely from `mapping/wiring.json`.

Proof-of-life:
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring.json --out-ledmap /dev/null --out-pixels /dev/null`: `leds=560 width=169 height=112 holes=18368`

### 2026-01-04 — Runtime mapping validation patterns added (physical verification helpers)

Status: 🟢 Done

What was done:
- Implemented the planned on-device mapping validation patterns as a dedicated runtime firmware:
  - `Index_Walk_Test`, `XY_Scan_Test`, `Coord_Color_Test`
- Added generator support needed for firmware mapping tables:
  - bench subset support via `isBenchSubset: true`
  - C++ header emission (`--out-header`) for `include/generated/chromance_mapping_{full,bench}.h`
- Added a `runtime` and `runtime_bench` PlatformIO env and a pre-build hook to generate headers from `mapping/wiring*.json` into `include/generated/` (ignored by git).

Files touched:
- scripts/generate_ledmap.py
- scripts/generate_mapping_headers.py
- src/main_runtime.cpp
- src/core/mapping/mapping_tables.h
- src/core/mapping/pixels_map.h
- src/core/effects/effect.h
- src/core/effects/pattern_index_walk.h
- src/core/effects/pattern_xy_scan.h
- src/core/effects/pattern_coord_color.h
- src/platform/led/led_output.h
- src/platform/led/dotstar_output.h
- src/platform/led/dotstar_output.cpp
- platformio.ini
- mapping/README_wiring.md
- docs/architecture/wled_integration_implementation_plan.md
- .gitignore
- TASK_LOG.md

Notes / Decisions:
- `include/generated/` is treated as generated output and is not hand-edited; it is produced from `mapping/wiring*.json` by the pre-build hook.

Proof-of-life:
- `pio test -e native`: PASSED (7 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-04 — Index walk serial banner + per-segment local reversal

Status: 🟢 Done

What was done:
- Made `global_to_local[]` respect per-segment direction (`dir`) by reversing strip-local indices within a segment when `dir == b_to_a` so physical LED order matches the mapping direction.
- Extended generated headers with per-LED `global_to_seg[]`, `global_to_seg_k[]`, and `global_to_dir[]` to aid physical note-taking.
- Added a throttled Serial “banner” during `Index_Walk_Test` that prints `i/seg/k/dir/strip/local/...` as the lit LED advances.

Files touched:
- scripts/generate_ledmap.py
- src/core/mapping/mapping_tables.h
- src/main_runtime.cpp
- TASK_LOG.md

Proof-of-life:
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-04 — Bench wiring comment updated after subset support

Status: 🟢 Done

What was done:
- Updated `mapping/wiring_bench.json` top-level `_comment` to reflect that subset generation is now supported when `isBenchSubset=true`.

Files touched:
- mapping/wiring_bench.json
- TASK_LOG.md

Proof-of-life:
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring_bench.json --out-ledmap /dev/null --out-pixels /dev/null`: `leds=154 width=56 height=98 holes=5334`

### 2026-01-04 — Milestone 0 complete: wiring marked verified + artifacts regenerated

Status: 🟢 Done

What was done:
- Updated `mapping/wiring.json` and `mapping/wiring_bench.json` confidence markers from PLACEHOLDER → DERIVED per on-device verification report.
- Updated `mapping/README_wiring.md` to remove “placeholder template” wording now that wiring is considered canonical.
- Regenerated mapping artifacts from the verified wiring inputs.

Files touched:
- mapping/wiring.json
- mapping/wiring_bench.json
- mapping/README_wiring.md
- mapping/ledmap.json
- mapping/pixels.json
- mapping/ledmap_bench.json
- mapping/pixels_bench.json
- TASK_LOG.md

Proof-of-life:
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring.json --out-ledmap mapping/ledmap.json --out-pixels mapping/pixels.json`: `leds=560 width=169 height=112 holes=18368`
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring_bench.json --out-ledmap mapping/ledmap_bench.json --out-pixels mapping/pixels_bench.json`: `leds=154 width=56 height=98 holes=5334`
