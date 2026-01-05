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

### 2026-01-04 — Milestone 1 started: WLED feasibility spike scaffolded

Status: 🟡 In progress

What was done:
- Created a local WLED spike harness (`tools/wled_spike/`) including:
  - a WLED PlatformIO override env for `featheresp32`
  - a small WLED usermod that prints multi-bus start/len to Serial and overlays bus boundary markers for visual verification
  - install instructions + installer script
- Built upstream WLED with the spike env (build-only; device flashing + UI configuration still pending).

Files touched:
- tools/wled_spike/README.md
- tools/wled_spike/install_into_wled.sh
- tools/wled_spike/platformio_override.chromance_spike.ini
- tools/wled_spike/usermods/chromance_spike/library.json
- tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp
- .gitignore
- TASK_LOG.md

Notes / Decisions:
- The local WLED clone used for the spike is intentionally gitignored (`tools/wled_spike/wled`).

Proof-of-life:
- `cd tools/wled_spike/wled && pio run -e chromance_spike_featheresp32`: SUCCESS (WLED build output shows `chromance_spike` usermod linked)

### 2026-01-04 — Milestone 1 spike: improved Serial diagnostics (no missed boot prints)

Status: 🟢 Done

What was done:
- Updated the WLED spike usermod to print `ChromanceSpike:` diagnostics every ~2s (not only at boot) to avoid “no serial lines” when the monitor attaches after startup.
- Set `monitor_speed=115200` in the spike PlatformIO override.
 - Added WiFi/AP status logging (`apSSID`, `ap_ip`, `sta_ip`, `WLED_CONNECTED`) so the WebUI can be located without relying on WLED debug logs.

Files touched:
- tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp
- tools/wled_spike/platformio_override.chromance_spike.ini
- tools/wled_spike/README.md
- TASK_LOG.md

Proof-of-life:
- `cd tools/wled_spike/wled && pio run -e chromance_spike_featheresp32`: SUCCESS

### 2026-01-04 — Milestone 1 spike: seed WLED WiFi defaults from repo secrets

Status: 🟢 Done

What was done:
- Updated the spike installer to (optionally) seed WLED compile-time `CLIENT_SSID`/`CLIENT_PASS` defaults from `include/wifi_secrets.h`, so first boot can join the intended WiFi without AP provisioning.

Files touched:
- tools/wled_spike/install_into_wled.sh
- tools/wled_spike/README.md
- TASK_LOG.md

Proof-of-life:
- `./tools/wled_spike/install_into_wled.sh`: PASS (WiFi defaults applied to local WLED clone)

### 2026-01-04 — Milestone 1 spike: lift WLED WebUI SPI bus cap (2 → 4)

Status: 🟢 Done

What was done:
- Identified that WLED’s LED Preferences WebUI hard-caps “2 pin” (SPI) busses to 2, which blocks configuring Chromance’s 4 APA102/DotStar outputs.
- Updated the local spike installer to patch the WLED clone’s `wled00/data/settings_leds.htm` to allow up to 4 SPI busses (firmware already supports more via `BusManager`).

Files touched:
- tools/wled_spike/install_into_wled.sh
- tools/wled_spike/README.md
- TASK_LOG.md

Proof-of-life:
- `./tools/wled_spike/install_into_wled.sh`: PASS (patched WebUI `twopinB >= 4`)

### 2026-01-04 — Milestone 1 spike: add WLED LED Preferences config template (4× APA102)

Status: 🟢 Done

What was done:
- Added a WLED “Config template” JSON file that can be applied from `Config → LED Preferences` to provision the 4 APA102/DotStar busses (pins, lengths, contiguous start indices) without manual UI entry.

Files touched:
- tools/wled_spike/chromance_led_prefs_template.json
- tools/wled_spike/README.md
- TASK_LOG.md

Proof-of-life:
- Template created for `type=51 (APA102)` with starts `{0,154,322,406}` and total `560`.

### 2026-01-04 — Milestone 1 spike: WLED APA102 output unreliable on GPIO16/17 (board-specific)

Status: 🔴 Blocked

What was observed:
- WLED is correctly configured for 4 busses (`total_len=560`, contiguous starts) but one physical strip is intermittently/non-responsive when assigned to GPIO17 (DATA) / GPIO16 (CLK).
- The same physical strip responds when moved to another configured pin pair, suggesting a WLED/driver/pin interaction rather than mapping/generator issues (Chromance runtime firmware drives the hardware correctly).

Notes / Decisions:
- Suspected WLED limitation: only the first SPI (2-pin) bus uses HW SPI on ESP32; additional APA102 busses fall back to soft SPI, which may behave differently on some pins/boards.
- Candidate mitigations (to validate):
  - Reorder WLED busses so the problematic pin pair is bus0 (gets HW SPI), while keeping start indices custom.
  - Change the pin pair for that strip in WLED (and, if Option B proceeds, update Chromance pin assignments to match).

Proof-of-life:
- Serial (WLED spike): `busses=4 total_len=560` and `idx=322 -> bus2 local=0` while the physical strip on `17/16` did not respond reliably.

### 2026-01-04 — Milestone 1 spike: adopt GPIO33/27 for Strip 3 (WLED workaround)

Status: 🟢 Done

What was done:
- Updated the default Strip 3 (84 LED) DotStar pin assignment from GPIO17/16 to GPIO33/27 to match verified working WLED behavior on Feather ESP32.
- Updated the WLED spike README, usermod wiring notes, and the LED Preferences config template to match.

Files touched:
- src/core/layout.h
- tools/wled_spike/README.md
- tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp
- tools/wled_spike/chromance_led_prefs_template.json
- TASK_LOG.md

Proof-of-life:
- User report: WLED works reliably with Strip 3 wired to DATA=33 / CLK=27 (GPIO17/16 unreliable).

### 2026-01-04 — Milestone 1 spike: enable sparse 2D ledmap loading in WLED clone

Status: 🟢 Done

What was done:
- Identified that upstream WLED’s `deserializeMap()` truncates `ledmap.json` to `LED_COUNT` entries because it allocates the mapping table to `getLengthTotal()`.
- Added a reproducible installer patch that allocates the mapping table to `mapSize = width*height` when a 2D ledmap is used, so Chromance’s sparse `mapping/ledmap.json` can be fully loaded.
- Set `MAX_LEDS` build flag to `20000` for the spike build to avoid 2D size limits blocking large sparse maps.
- Updated the spike README and the LED Preferences template defaults (Strip 3 pins `33/27`, Color Order `BRG`).

Files touched:
- tools/wled_spike/install_into_wled.sh
- tools/wled_spike/platformio_override.chromance_spike.ini
- tools/wled_spike/README.md
- tools/wled_spike/chromance_led_prefs_template.json
- wled_spike_refinement_prompt.md
- TASK_LOG.md

Proof-of-life:
- `./tools/wled_spike/install_into_wled.sh`: prints “Patched WLED ledmap loader to support sparse 2D maps (mapSize=width*height).”

### 2026-01-04 — Milestone 1 spike: move Strip 2 to GPIO17/16 (WLED-friendly)

Status: 🟢 Done

What was done:
- Updated default pin assignment so the 12-segment strip (Strip 2 / bus1 / start=154 len=168) uses DATA=17 / CLK=16 instead of 19/18, per verified WLED behavior.
- Updated the WLED spike docs/template and the Chromance core pin constants to match the new wiring.

Files touched:
- src/core/layout.h
- tools/wled_spike/README.md
- tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp
- tools/wled_spike/chromance_led_prefs_template.json
- TASK_LOG.md

Proof-of-life:
- User report: moving the 12-segment strip from GPIO19/18 to GPIO17/16 made it work in WLED.

### 2026-01-04 — Milestone 1 spike: bus1 (154–322 / 168 LEDs / 12 segments) fixed by moving pins 19/18 → 17/16; regression test passed

Status: 🟢 Done

What was observed:
- Under WLED, the 168-LED strip (bus1 index range 154..322; 12 physical segments) was dark for all colors when configured on GPIO19 (DATA) / GPIO18 (CLK).
- Moving bus1 to GPIO17 (DATA) / GPIO16 (CLK) restored full output; Solid R/G/B confirmed.
- Updated runtime firmware pin mapping to match; runtime still drives all strips correctly.
- WLED 4-segment regression test now passes:
  - 0–154 = Red
  - 154–322 = Green
  - 322–406 = Blue
  - 406–560 = White

Notes / Decisions:
- Treat GPIO19/18 as a known-bad APA102 pin pair for WLED soft-SPI on Feather ESP32 in this setup.
- Prior suspicion that GPIO17/16 was unreliable was likely a misdiagnosis.

### 2026-01-04 — Milestone 1 COMPLETE — WLED contiguous indexing + working 4-bus APA102 output

Status: 🟢 Done

What was verified:
- PASS: WLED exposes a single contiguous index space with correct bus boundaries for Chromance:
  - bus0 start=0 len=154
  - bus1 start=154 len=168
  - bus2 start=322 len=84
  - bus3 start=406 len=154
- FIX: bus1 (12 segments / 168 LEDs) must use DATA=17 / CLK=16 on Feather ESP32 (GPIO19/18 fails in this setup).

Notes / Decisions:
- Known constraint (power): at higher brightness, some pixels on bus1 may lose the Blue channel (white→orange, magenta→red); mitigated by power injection and/or WLED power limiting + brightness cap.
- WLED spike patching:
  - WebUI patched to allow 4 SPI (2-pin) busses.
  - Ledmap loader patched to support Chromance sparse 2D `ledmap.json` without truncation.
  - Spike build sets `MAX_LEDS=20000` to avoid 2D size limits.

Proof-of-life:
- User serial banner (WLED spike):
  - `ChromanceSpike: busses=4 total_len=560`
  - `ChromanceSpike: idx=154 -> bus1 local=0`
  - `ChromanceSpike: idx=322 -> bus2 local=0`
  - `ChromanceSpike: idx=406 -> bus3 local=0`

### 2026-01-04 — Milestone 2 COMPLETE — mapping generator + bench subset + headers + builds

Status: 🟢 Done

What was verified:
- Mapping generator runs deterministically for full + bench wiring (no collisions, stable dimensions).
- Bench subset generation works via `mapping/wiring_bench.json` (`isBenchSubset: true`) and produces bench artifacts.
- Generated headers exist for both full + bench and are produced by the pre-build hook (and `include/generated/` is gitignored).
- PlatformIO envs build and native tests pass.

Proof-of-life:
- Full generator: `leds=560 width=169 height=112 holes=18368`
- Bench generator: `leds=154 width=56 height=98 holes=5334`
- `pio test -e native`: PASSED (7 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-04 — Milestone 0 closeout: wiring confidence markers updated to VERIFIED

Status: 🟢 Done

What was done:
- Updated per-segment `_comment` confidence markers in `mapping/wiring.json` and `mapping/wiring_bench.json` from `DERIVED` → `VERIFIED` now that all strips were physically validated.

Files touched:
- mapping/wiring.json
- mapping/wiring_bench.json
- TASK_LOG.md

Proof-of-life:
- Full generator: `leds=560 width=169 height=112 holes=18368`
- Bench generator: `leds=154 width=56 height=98 holes=5334`
- `pio test -e native`: PASSED (7 test cases)
