# Chromance – Milestone 1 (WLED spike) Update: Pin Fix + Regression Proof

## Summary
We resolved the “missing 12 segments” issue in the WLED spike. The root cause was **GPIO19/18** being incompatible (in this setup) for an APA102 bus under WLED on Feather ESP32. Moving the affected bus to **GPIO17/16** fixed it. Runtime firmware was updated to match and also works.

## What was happening
- “12 physical segments” were dark for all colors under WLED.
- Segment distribution is 11/12/6/11 → **12 segments corresponds to the 168-LED strip** (index range **154–322**, aka bus1).
- WLED had bus1 on **DATA=19 / CLK=18**, and it would not drive that strip.
- Moving bus1 to **DATA=17 / CLK=16** immediately restored output.

## Verified Fix
- After moving bus1 pins to **17/16**, Solid colors **R/G/B** work correctly.
- Regression test (4-segment color ranges) passes:
  - 0–154 = Red
  - 154–322 = Green
  - 322–406 = Blue
  - 406–560 = White
- Runtime firmware pin mapping was updated accordingly and confirmed working.

## Required Repo Updates (Agent Actions)

### 1) Update WLED spike LED Preferences template
File: `tools/wled_spike/chromance_led_prefs_template.json`
- Change **bus1** pin pair from **19/18** → **17/16**
- Keep start/lengths unchanged:
  - bus0: start 0 len 154
  - bus1: start 154 len 168 (pins now 17/16)
  - bus2: start 322 len 84
  - bus3: start 406 len 154

### 2) Update WLED spike README with pin constraint
File: `tools/wled_spike/README.md`
Add a note like:
- “Feather ESP32 + WLED APA102: bus1 failed on GPIO19/18 but works on GPIO17/16 (this setup). Treat 19/18 as known-bad for WLED APA102 soft-SPI.”

### 3) (Optional but recommended) Improve spike serial diagnostics
File: `tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp`
- Include DATA/CLK pin numbers in the periodic `ChromanceSpike:` bus banner output (alongside start/len) so future debugging doesn’t rely on UI screenshots.

### 4) Update runtime pin mapping to match
File: `src/core/layout.h` (or wherever strip pin pairs are defined)
- Ensure the 168-LED strip bus uses **DATA=17 / CLK=16** (replacing 19/18).
- Confirm `pio run -e runtime` and physical output remain correct.

## Task Log Entry (add verbatim)
Append to `TASK_LOG.md`:

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

## Success Criteria
- WLED: Single segment 0–560 Solid White lights all physical segments
- WLED: 4-segment regression colors show correctly across the wall
- Runtime: unchanged behavior + still drives all strips correctly
- Template + README updated so future flashes reproduce the working setup

