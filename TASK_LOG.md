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

### 2026-01-05 — Milestone 3: portable effects core scaffolding (scheduler/params/signals/registry)

Status: 🟡 In progress

What was done:
- Added portable core scaffolding for Milestone 3:
  - `EffectParams` baseline parameter model
  - `Signals`/`ModulationInputs` structs + `ModulationProvider` stub (`NullModulationProvider`)
  - `FrameScheduler` with runtime `target_fps` (0 = uncapped) and deterministic rounding
  - `EffectRegistry` for effect lookup/selection by id
- Updated core validation patterns to accept a unified `EffectFrame` (time in, pixels out) while remaining Arduino-free.
- Added native unit tests covering the scheduler rounding behavior and registry invariants.

Files touched:
- src/core/effects/effect.h
- src/core/effects/pattern_index_walk.h
- src/core/effects/pattern_xy_scan.h
- src/core/effects/pattern_coord_color.h
- src/core/effects/effect_params.h
- src/core/effects/signals.h
- src/core/effects/modulation_provider.h
- src/core/effects/frame_scheduler.h
- src/core/effects/effect_registry.h
- test/test_main.cpp
- test/test_frame_scheduler.cpp
- test/test_effect_registry.cpp
- src/main_runtime.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (11 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-05 — Milestone 3 COMPLETE — scheduler + params/signals + validation patterns remain portable

Status: 🟢 Done

What was verified:
- `core/` remains Arduino-free and compiles under host tests.
- Scheduler/registry behaviors are covered by deterministic unit tests.

Proof-of-life:
- `pio test -e native`: PASSED (11 test cases)

### 2026-01-05 — Milestone 3: runtime brightness control (+/-) with persistence

Status: 🟢 Done

What was done:
- Added serial `+`/`-` controls to change brightness in 10% increments (clamped 0..100).
- Persisted brightness percent across reboots using ESP32 NVS (`Preferences`).
- Included `brightness_pct` in the periodic runtime UART stats line for easier log capture.
- Refactored brightness logic into portable `core/` and added unit tests covering clamp/quantize/step/percent->255 plus persistence semantics via a fake key-value store.

Files touched:
- src/core/brightness.h
- src/core/settings/kv_store.h
- src/core/settings/brightness_setting.h
- src/platform/settings.h
- src/platform/settings.cpp
- src/main_runtime.cpp
- test/test_brightness.cpp
- test/test_brightness_setting.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (28 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-05 — Native test coverage: add unit tests for remaining `core/` modules

Status: 🧪 Test added

What was done:
- Added native unit tests for `PixelsMap` scan-order/coord bounds, `MappingTables` invariants, validation pattern behaviors, and `NullModulationProvider`.

Files touched:
- test/test_pixels_map.cpp
- test/test_mapping_tables.cpp
- test/test_effect_patterns.cpp
- test/test_modulation_provider.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (28 test cases)

### 2026-01-05 — Runtime: add compile-time brightness ceiling + soft brightness scaling

Status: 🟢 Done

What was done:
- Added a compile-time hardware brightness ceiling (`src/core/brightness_config.h`) starting at 50%.
- Stored brightness remains a persisted “soft” 0..100% value; `+`/`-` still step soft brightness by 10%, but effective output is scaled by the ceiling (e.g., ceiling=30% ⇒ each step is 3% of hardware max).
- Serial output now shows `soft_brightness_pct`, `hw_ceiling_pct`, and `effective_brightness_pct`.

Files touched:
- src/core/brightness_config.h
- src/core/brightness.h
- src/main_runtime.cpp
- test/test_brightness.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (29 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-05 — Milestone 4: output layer refactor (active strips, per-strip API, perf)

Status: 🟡 In progress

What was done:
- Refactored `DotstarOutput` to:
  - compute active strip lengths from `MappingTables` (bench builds only allocate/drive the strips used by the mapping)
  - set DotStar global brightness to 255 (effect output scaling owns brightness)
  - add an optional per-strip output API (`ILedOutput::show_strips`) for future non-global-buffer paths

Files touched:
- src/platform/led/led_output.h
- src/platform/led/dotstar_output.h
- src/platform/led/dotstar_output.cpp
- TASK_LOG.md

Proof-of-life:
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: add patterns 4/5 (Rainbow_Pulse, Two_Dots)

Status: 🟢 Done

What was done:
- Added two additional runtime-selectable patterns:
  - `4=Rainbow_Pulse`: full display, rainbow color steps with fade-in (0.7s), hold (2.0s), fade-out (0.7s).
  - `5=Two_Dots`: two moving dots with randomly chosen colors that change after a full traversal.
- Added native unit tests for both patterns.

Files touched:
- src/core/effects/pattern_rainbow_pulse.h
- src/core/effects/pattern_two_dots.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (31 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-05 — Runtime: mode 5 “Two_Dots” upgraded to “Two_Comets” (head+tail)

Status: 🟢 Done

What was done:
- Updated mode 5 from single-pixel dots to two comet trails:
  - comet head length randomized per traversal (`3..5` LEDs at full brightness)
  - linear fading tail of equal length
  - colors still randomize after each full traversal

Files touched:
- src/core/effects/pattern_two_dots.h
- test/test_effect_patterns.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (35 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-07 — Chromance map: generate both front/back view images

Status: 🟢 Done

What was done:
- Updated the chromance map renderer to output both unmirrored “back view” and x-mirrored “front view” images.
- Regenerated the map outputs for quick reference during mapping/topology debugging.

Files touched:
- scripts/render_chromance_map.py
- chromance_map_front.png
- chromance_map_back.png
- chromance_map_front.svg
- chromance_map_back.svg
- TASK_LOG.md

Proof-of-life:
- `python3 scripts/render_chromance_map.py --ledmap mapping/ledmap.json --out-svg chromance_map.svg --out-png chromance_map.png --out-svg-front chromance_map_front.svg --out-png-front chromance_map_front.png --out-svg-back chromance_map_back.svg --out-png-back chromance_map_back.png`

### 2026-01-07 — Fix Mode 1 vertex “toward” direction (use physical local-in-segment)

Status: 🟢 Done

What was done:
- Fixed `Index_Walk_Test` vertex diagnostic so “walk toward vertex” always uses the physical LED index within a segment (`global_to_local % 14`) rather than `global_to_seg_k` (which is in global-index iteration order and can invert direction for `b_to_a` segments).
- Updated native unit test to validate endpoint selection using the corrected per-LED canonical `ab_k`.

Files touched:
- src/core/effects/pattern_index_walk.h
- test/test_effect_patterns.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (48 test cases)

### 2026-01-07 — PlatformIO: add OTA upload environments for runtime

Status: 🟢 Done

What was done:
- Added explicit OTA upload environments so `runtime` and `runtime_bench` can be uploaded via ArduinoOTA without changing the default serial upload configuration.

Files touched:
- platformio.ini
- TASK_LOG.md

Proof-of-life:
- Use: `pio run -e runtime_ota -t upload` (or `pio run -e runtime_bench_ota -t upload`)

### 2026-01-07 — Runtime: Mode 1 step-hold (`s`/`S`) to freeze advancement

Status: 🟢 Done

What was done:
- Added Mode 1 (`Index_Walk_Test`) single-step hold controls:
  - `s`: step forward once and hold (no time-based advancement until another `s`/`S`)
  - `S`: step backward once and hold
- Step-hold applies to both the index/topology scans and the vertex diagnostic fill progression.
- Added native unit tests to ensure step-hold freezes output until the next step and does not auto-advance with time.
- Updated vertex diagnostic “pause mode” so when a vertex is selected manually it continues looping the toward-vertex walk animation (only vertex selection is paused), and in vertex mode `s`/`S` cycles vertices without stopping the animation.

Files touched:
- src/core/effects/pattern_index_walk.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (48 test cases)

### 2026-01-07 — Runtime: Mode 1 vertex diagnostic (incident segments + fill toward vertex)

Status: 🟢 Done

What was done:
- Extended Mode 1 (`Index_Walk_Test`) with a topology-driven vertex diagnostic scan (`VERTEX_TOWARD`) that highlights a single vertex at a time by lighting all incident segments and filling LEDs toward the vertex, enabling practical vertex discovery + segment direction debugging.
- Added generator-emitted topology tables to the mapping header (`vertex_id` + per-segment endpoint vertex IDs) and exposed them via `MappingTables` so core effects can be topology-aware without Arduino/WiFi dependencies.
- Updated runtime controls/logging: `n/N` step vertex when in vertex mode (otherwise `n` cycles scan modes), and serial output prints the active `V# (vx,vy)` plus incident `segIds`.
- Added native unit tests covering vertex fill direction correctness and stepping behavior.

Files touched:
- scripts/generate_ledmap.py
- src/core/mapping/mapping_tables.h
- src/core/effects/pattern_index_walk.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring.json ...` → `leds=560 width=169 height=112 holes=18368`
- `pio test -e native`: PASSED (45 test cases)

### 2026-01-07 — Mode 7 “Breathing” plan v2 clarifications

Status: 🔵 Decision

What was done:
- Updated the Mode 7 breathing implementation plan to explicitly default to boundary-local path disjointness (first K steps) rather than full global edge-disjointness, and to clarify center tie-break + arrival semantics.
- Strengthened the exhale wavefront accounting plan to be layer-based so wavefront counts cannot skip segments under low FPS / large `dt`.
- Updated the plan-improvement prompt to match the v2 defaults (K=2, range 1–3) and required test/determinism coverage.

Files touched:
- breath_pattern_improvement_implementation_plan.md
- breath_pattern_improvement_implementation_plan_improvement_prompt.md
- TASK_LOG.md

### 2026-01-06 — Mode 7: breathing improvement report + implementation plan

Status: 🟢 Done

What was done:
- Wrote a comprehensive report + implementation plan to upgrade Mode 7 from geometry/time-driven behavior to a topology-driven, event-driven breath loop.
- Included concrete completion conditions (inhale dots reach center; exhale wavefront counts; pause beat-count completion), required topology data model, generator touchpoints, and a native-test checklist.
- Added relevant excerpts of current Mode 7 code to highlight what changes (time-driven phase selection, geometric spoke assignment, nearest-LED dots, continuous wave field).

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `381`

### 2026-01-07 — Runtime: Mode 1 scan-mode cycling (topology LTR/UTD, RTL/DTU, INDEX)

Status: 🟢 Done

What was done:
- Extended Mode 1 (`Index_Walk_Test`) with 3 scan modes toggled by `n`: `LTR/UTD` (per-segment scan using pixel topology coords), `RTL/DTU`, and the existing global `INDEX` walk.
- Added `ESC` handling in Mode 1 to return to `INDEX` behavior.
- Updated serial banner logging to include the current scan mode name and the active `seg` ID.
- Added native unit tests covering scan-mode cycling and per-segment axis ordering for topology scans.

Files touched:
- src/core/effects/pattern_index_walk.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (44 test cases)

### 2026-01-07 — Decision: generator-based topology tables (vertex IDs) for Mode 7 + future patterns

Status: 🔵 Decision

What was done:
- Updated the Mode 7 breathing improvement plan to commit to a generator-based topology data model (stable `vertex_id` + per-segment endpoint vertex IDs) emitted into mapping headers and exposed via `MappingTables`.
- Documented how to derive a canonical per-LED segment coordinate (`ab_k`) using existing `global_to_seg_k` and `global_to_dir`, enabling endpoint-directional effects without guessing wiring direction.
- Added a comprehensive implementation plan for a new Mode 1 diagnostic scan mode (“Vertex incident segments”) that lights all segments attached to a vertex and fills LEDs toward the vertex, using the generated topology tables; includes serial logging and native-test plan.

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `553`

### 2026-01-07 — Mode 7 plan v2.2: center-lane uniqueness + round-robin fallback

Status: 🟢 Done

What was done:
- Refined the Mode 7 INHALE distinctness rules: removed boundary-local disjointness and replaced it with a center-lane policy that prefers unique center-incident segments per inhale when possible.
- Added explicit lane assignment at inhale start (per-dot preferred center-adjacent vertex `u`) and a persistent round-robin offset to rotate unavoidable center-lane reuse across inhales.
- Updated routing description to “route to `u`, then append `u→center`”, plus a clear fallback strategy (try alternate lanes; relax only center-lane uniqueness; never relax per-dot segment-simple).
- Updated tests/determinism notes to cover center-lane uniqueness and rotation.

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `594`

### 2026-01-07 — Mode 7 plan v2.4: safety guardrails + unambiguous config

Status: 🟢 Done

What was done:
- Refined the Mode 7 plan to v2.4 with final guardrails and removed remaining ambiguity: generalized INHALE start selection to an arbitrary `num_dots` using a deterministic farthest-from-center pool, made final center-lane segment semantics explicit, and locked down center-lane round-robin offset advancement rules.
- Added PAUSE fail-safe (`max_pause_duration_ms`) that injects deterministic virtual beats if no beat events arrive, ensuring pauses cannot hang indefinitely.
- Locked EXHALE segment distance to `seg_dist = min(dist[va], dist[vb])` and normalized tail language to “perceptual brightness LUT”.

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `646`

### 2026-01-07 — Mode 7 plan v2.5: INHALE-only center-lane stepping (`s`/`S`)

Status: 🟢 Done

What was done:
- Refined the Mode 7 plan to v2.5 by adding an INHALE-only manual lane-iteration control (`s`/`S`) that adjusts `center_lane_rr_offset` with wrap-around and reinitializes INHALE (start selection + lane assignment + routing regenerated) without using `n/N` (phase stepping).
- Clarified offset advancement semantics to include `s/S` in INHALE manual mode while keeping `n/N` reserved for phase navigation; updated the files-to-change checklist and test plan accordingly.

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `679`

### 2026-01-07 — Mode 7 plan v2.5 consistency pass (rr_offset semantics in config)

Status: 🟢 Done

What was done:
- Updated the v2.5 plan’s configuration section to explicitly include `s/S` behavior for `center_lane_rr_offset` (in addition to auto/ESC advancement) to keep semantics consistent across §5.3.1, §9.2, §11, and tests.

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `682`

### 2026-01-07 — Mode 7 plan v2.6: correctness/robustness hardening (EXHALE vertex completion)

Status: 🟢 Done

What was done:
- Refined the Mode 7 plan to v2.6 by hardening correctness/robustness: EXHALE completion is now vertex-based (outermost vertices by `d_max`) rather than any boundary/outer-segment heuristic.
- Updated INHALE plateau safety to allow equal-distance moves when the plateau vertex is the assigned center-adjacent goal.
- Tightened PAUSE fail-safe to be atomic (at most one beat processed per frame; real beats override virtual timeout beats).
- Clarified `s/S` determinism (reinitialization re-runs RNG-based start selection; deterministic given seed + keypress sequence).
- Added guardrail for `lane_count == 0` to prevent deadlock in partial/bench builds and added explicit memory/alloc constraints.
- Expanded the test plan to cover the above edge cases (no removals).

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `721`

### 2026-01-07 — Mode 7 plan v2.7: tunables, config table, generator tests, memory caps

Status: 🟢 Done

What was done:
- Refined the Mode 7 plan to v2.7 by documenting web-UI-ready tunable parameters (inhale dot speed/tail, exhale wave speed/target waves, pause beat bounds/timeout), and added a consolidated configuration table with defaults/ranges/units.
- Added a “Generator tests (Python)” plan for topology table validity/stability.
- Expanded the memory section with an approximate budget plus explicit hard caps (`MAX_DOTS`, `MAX_VERTEX_PATH_LENGTH`, `MAX_LED_PATH_LENGTH`) and deterministic overflow behavior.
- Explicitly documented center selection complexity (O(V³) with V≈25 is acceptable).

Files touched:
- breath_pattern_improvement_implementation_plan.md
- TASK_LOG.md

Proof-of-life:
- `wc -l breath_pattern_improvement_implementation_plan.md` → `775`

### 2026-01-07 — Render chromance map SVG/PNG (segments + vertices)

Status: 🟢 Done

What was done:
- Added a small renderer to visualize `mapping/ledmap.json` with segment ID labels (`S1..S40`) and derived vertex IDs (`V#`, junction points between segments).
- Generated `chromance_map.svg` and `chromance_map.png` in the repo root for quick reference during topology/mapping work.

Files touched:
- scripts/render_chromance_map.py
- chromance_map.svg
- chromance_map.png
- TASK_LOG.md

Proof-of-life:
- `python3 scripts/render_chromance_map.py --ledmap mapping/ledmap.json --out-svg chromance_map.svg --out-png chromance_map.png`

### 2026-01-07 — Improve chromance map label legibility

Status: 🟢 Done

What was done:
- Increased segment/vertex label font sizes and made labels bold with a white halo stroke for improved readability in `chromance_map.svg`/`chromance_map.png`.
- Regenerated both artifacts from the same `mapping/ledmap.json`.

Files touched:
- scripts/render_chromance_map.py
- chromance_map.svg
- chromance_map.png
- TASK_LOG.md

Proof-of-life:
- `python3 scripts/render_chromance_map.py --ledmap mapping/ledmap.json --out-svg chromance_map.svg --out-png chromance_map.png`

### 2026-01-05 — Runtime: “Seven_Comets” per-comet sequences (unique lengths + independent rerolls)

Status: 🟢 Done

What was done:
- Updated “Seven_Comets” so each comet has its own independently-timed “sequence” with:
  - randomized head length (`3..5`) + color per comet
  - randomized per-comet sequence length (unique across comets at any moment)
  - per-comet reroll when its sequence completes (no longer tied to full-ring traversals)
- Updated native tests to validate per-comet uniqueness and reset behavior.

Files touched:
- src/core/effects/pattern_two_dots.h
- test/test_effect_patterns.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (35 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: “Seven_Comets” per-comet speed (size-based)

Status: 🟢 Done

What was done:
- Updated “Seven_Comets” so each comet advances at its own speed derived from its head length:
  - larger head length => faster movement (smaller `step_ms`)
  - motion is integrated per-comet (no speed-change discontinuities when a comet rerolls size)
- Updated native tests to validate the `head_len -> step_ms` mapping and that per-comet sequence timers still reset independently.

Files touched:
- src/core/effects/pattern_two_dots.h
- test/test_effect_patterns.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (35 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: add pattern 6 “HRV hexagon” (random hex + internal edges)

Status: 🟢 Done

What was done:
- Added runtime mode `6` (“HRV hexagon”): lights one of the 8 topology-derived hexagons (perimeter + internal edges) in a random color, looping:
  - fade in 4s, hold 2s, fade out 9s, then pick a different hex and new color
- Extended persisted mode range to include `6` (sanitized persistence remains safe).
- Added native unit test for the new effect’s fade/hold/cycle behavior.

Files touched:
- src/core/effects/pattern_hrv_hexagon.h
- src/core/effects/pattern_hrv_hexagon.cpp
- src/core/settings/mode_setting.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- test/test_mode_setting.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (36 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: HRV hexagon debug output + manual next-hex

Status: 🟢 Done

What was done:
- Added serial debug output for mode `6` to print the currently-selected hex index, segment IDs, and RGB color whenever the hex changes.
- Added serial command `n`/`N` to force the next random hex immediately (for fast physical verification / topology correction).

Files touched:
- src/core/effects/pattern_hrv_hexagon.h
- src/main_runtime.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (36 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-05 — Runtime: replace mode 2 with strip segment stepper (physical remap aid)

Status: 🟢 Done

What was done:
- Replaced runtime mode `2` (formerly `XY_Scan_Test`) with a strip-colored segment stepper to help re-derive wiring/mapping:
  - strip0 red, strip1 blue, strip2 green, strip3 cyan
  - shows per-strip segment number `k` (1..12) simultaneously across all strips
  - auto-cycles `k`; press `n`/`N` to advance `k` manually (wraps 12→1)
  - strips with fewer than `k` segments stay black until `k` wraps
- Added serial logging whenever `k` changes: prints `k` and the current mapping’s `segId(dir)` for each strip at that position.

Files touched:
- src/core/effects/pattern_strip_segment_stepper.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (37 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: mode 2 manual step-and-hold (+ ESC resume auto)

Status: 🟢 Done

What was done:
- Updated mode `2` (strip segment stepper) control flow:
  - default: auto-rotates `k` (1..12)
  - press `n`/`N`: advances `k` once, logs the new `k`, then holds (no further auto-advance) until `n` pressed again
  - press ESC: exits hold and resumes auto-rotation
- Added a portable unit test to validate that auto-advance can be disabled.

Files touched:
- src/core/effects/pattern_strip_segment_stepper.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (38 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: HRV hexagon smoother fades (eased + dithered)

Status: 🟢 Done

What was done:
- Updated “HRV hexagon” fades to look smoother by using a 16-bit eased fade curve (smoothstep) plus per-LED spatial dithering instead of a coarse 8-bit linear step.
- Increased render cadence for mode `6` to ~60fps (`frame_ms=16`) to reduce visible stepping during fades.

Files touched:
- src/core/effects/pattern_hrv_hexagon.h
- src/main_runtime.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (38 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: add mode 7 “Breathing”

Status: 🟢 Done

What was done:
- Added runtime mode `7` (“Breathing”) with a 4-phase loop:
  - inhale (4s): single red/orange dots travel inward along 6 spokes, one-by-one
  - pause (3s): 60bpm heartbeat pulse; color shifts inhale→exhale
  - exhale (7s): outward sinusoidal gradient along 6 spokes (cyan/light-green hues)
  - pause (3s): 60bpm pulse; color shifts exhale→inhale
- Extended persisted mode range to include `7`.
- Mode `7` runs at ~60fps for smoother motion.
- Added a basic native unit test to sanity-check inhale/exhale phases.

Files touched:
- src/core/effects/pattern_breathing_mode.h
- src/core/effects/pattern_breathing_mode.cpp
- src/core/settings/mode_setting.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- test/test_mode_setting.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (39 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: adjust mode 7 breathing pacing + swap inhale/exhale visuals

Status: 🟢 Done

What was done:
- Slowed the heartbeat pulse in breathing pauses to ~30bpm (`2s` period).
- Swapped inhale/exhale visuals (with direction reversal):
  - inhale now uses the wave effect reversed inward (warm orange gradient)
  - exhale now uses the dot effect reversed outward (greenish)

Files touched:
- src/core/effects/pattern_breathing_mode.h
- src/core/effects/pattern_breathing_mode.cpp
- test/test_effect_patterns.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (39 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: mode 7 manual phase stepping (n / N) + ESC auto reset

Status: 🟢 Done

What was done:
- Added manual phase control for breathing mode:
  - `n`: advance to next phase (inhale → pause1 → exhale → pause2) and stay in that phase
  - `N`: go to previous phase and stay in that phase
  - `ESC`: exit manual mode and return to auto cycling (restarts the cycle)
- Added native unit test for manual phase selection.

Files touched:
- src/core/effects/pattern_breathing_mode.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (40 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: unify n/N/ESC stepping across modes 2/6/7

Status: 🟢 Done

What was done:
- Standardized step controls for all modes that support staged stepping:
  - `n`: advance to next stage and stay there
  - `N`: go to previous stage and stay there
  - `ESC`: return to auto mode
- Mode 2: added `prev()` so `N` walks `k` backward (wraps 1↔12).
- Mode 6: added manual hex stepping (next/prev) with `ESC` restoring auto random cycling; manual mode repeats the fade cycle on the selected hex without auto-switching.

Files touched:
- src/core/effects/pattern_strip_segment_stepper.h
- src/core/effects/pattern_hrv_hexagon.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (42 test cases)
- `pio run -e runtime`: SUCCESS
- `pio run -e runtime_bench`: SUCCESS

### 2026-01-05 — Runtime: mode 5 “Seven_Comets” (was Two_Comets)

Status: 🟢 Done

What was done:
- Expanded mode 5 from 2 comets → 7 comets, evenly distributed across the index ring with alternating direction.
- Each comet has a randomized head length (`3..5`) with a matching linear fading tail; colors/head lengths change each traversal.

Files touched:
- src/core/effects/pattern_two_dots.h
- src/main_runtime.cpp
- test/test_effect_patterns.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (35 test cases)
- `pio run -e runtime`: SUCCESS

### 2026-01-05 — Runtime: persist selected mode (pattern) across reboot

Status: 🟢 Done

What was done:
- Persisted the selected runtime mode (keys `1..5`) in ESP32 NVS (`Preferences`) and restored it on boot.
- Safety: stored values are sanitized; if invalid/unrecognized, firmware falls back to mode `1` (`Index_Walk_Test`) and writes the sanitized value back.

Files touched:
- src/core/settings/mode_setting.h
- src/platform/settings.h
- src/platform/settings.cpp
- src/main_runtime.cpp
- test/test_mode_setting.cpp
- test/test_main.cpp
- TASK_LOG.md

Proof-of-life:
- `pio test -e native`: PASSED (35 test cases)
- `pio run -e runtime`: SUCCESS
