# Chromance – WLED Spike: Coding Agent Instructions

## Objective
Fix the WLED feasibility spike so that **all 560 LEDs** (across 4 APA102/DotStar busses) are actually driven.  
Currently, **12 physical segments are dark for all colors** despite correct UI configuration. This is due to a **compile-time WLED LED buffer limit**, not wiring, mapping, effects, or segmentation.

---

## Confirmed Facts
- Hardware and wiring are correct (Chromance runtime drives all LEDs successfully).
- WLED bus configuration is correct:
  - bus0: start=0, len=154
  - bus1: start=154, len=168
  - bus2: start=322, len=84
  - bus3: start=406, len=154
  - **total LEDs = 560**
- WLED UI:
  - Single segment (0–560)
  - Solid color (white/red/green/blue)
- Exactly **12 physical segments are dark for all colors**, consistently.
- This behavior indicates **WLED is not clocking LEDs beyond a compile-time maximum**, even though the UI allows setting 560.

---

## Root Cause
WLED’s ledmap implementation assumes the mapping table length is approximately `LED_COUNT`.

Chromance’s `mapping/ledmap.json` is a **sparse 2D** ledmap where:
- `map.length == width * height`
- `width * height` is significantly larger than `LED_COUNT` (e.g. 169×112 = 18928)

Upstream WLED currently truncates the loaded map to `LED_COUNT`, which can leave large regions dark even with correct bus wiring/config.

Secondarily, WLED uses `MAX_LEDS` as a compile-time limit in some 2D paths; it must be large enough for the chosen `width*height`.

---

## Required Fix
Patch WLED to load the full sparse 2D ledmap (`mapSize = width*height`) and set a sufficiently large `MAX_LEDS` build flag.

---

## Implementation Steps

### 1) Apply the spike installer (patches WLED clone)
- Run `./tools/wled_spike/install_into_wled.sh`
- This patches:
  - WLED web UI to allow 4 SPI busses
  - WLED ledmap loader to allocate and load `mapSize = width*height`

### 2) Ensure `MAX_LEDS` is large enough
- Add/update the active environment build flags (typically via `platformio_override.ini`):

```ini
build_flags =
  -D MAX_LEDS=20000

```
