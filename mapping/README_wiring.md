# Chromance wiring source-of-truth (`mapping/wiring*.json`)

This folder contains the **authoritative** wiring inputs for the mapping generator (`scripts/generate_ledmap.py`).

Important:
- `mapping/wiring.json` is currently a **PLACEHOLDER template**: strip segment counts are derived from firmware, but per-strip segment order and per-segment direction are not yet derivable from repo sources and must be validated on the physical sculpture.
- Generated artifacts (`mapping/ledmap.json`, `mapping/pixels.json`, `include/generated/*.h`) must never be hand-edited; change `mapping/wiring*.json` and/or the generator instead.

## Topology key (segment ID → vertex endpoints)

Endpoints are labeled **A** and **B** exactly as listed in `docs/architecture/wled_integration_implementation_plan.md` (Section 6.1) and `scripts/generate_ledmap.py` (`SEGMENTS`).

Direction semantics:
- `dir: "a_to_b"` means LED index 0 is near endpoint **A**, LED index 13 is near endpoint **B**
- `dir: "b_to_a"` is reversed

| segId | endpoint A (vx,vy) | endpoint B (vx,vy) |
| --- | --- | --- |
| 1 | (0,1) | (0,3) |
| 2 | (0,1) | (1,0) |
| 3 | (0,1) | (1,2) |
| 4 | (0,3) | (1,4) |
| 5 | (1,0) | (2,1) |
| 6 | (1,2) | (1,4) |
| 7 | (1,2) | (2,1) |
| 8 | (1,4) | (1,6) |
| 9 | (1,4) | (2,3) |
| 10 | (1,4) | (2,5) |
| 11 | (1,6) | (2,7) |
| 12 | (2,1) | (2,3) |
| 13 | (2,1) | (3,0) |
| 14 | (2,1) | (3,2) |
| 15 | (2,3) | (3,4) |
| 16 | (2,5) | (2,7) |
| 17 | (2,5) | (3,4) |
| 18 | (2,7) | (3,6) |
| 19 | (2,7) | (3,8) |
| 20 | (3,6) | (3,4) |
| 21 | (3,6) | (4,7) |
| 22 | (3,8) | (4,7) |
| 23 | (3,0) | (4,1) |
| 24 | (3,2) | (3,4) |
| 25 | (3,2) | (4,1) |
| 26 | (3,4) | (4,3) |
| 27 | (3,4) | (4,5) |
| 28 | (4,1) | (4,3) |
| 29 | (4,1) | (5,0) |
| 30 | (4,1) | (5,2) |
| 31 | (4,3) | (5,4) |
| 32 | (4,5) | (4,7) |
| 33 | (4,5) | (5,4) |
| 34 | (4,7) | (5,6) |
| 35 | (5,6) | (5,4) |
| 36 | (5,0) | (6,1) |
| 37 | (5,2) | (5,4) |
| 38 | (5,2) | (6,1) |
| 39 | (5,4) | (6,3) |
| 40 | (6,1) | (6,3) |

## Current wiring template status

Because the current firmware only encodes per-strip segment counts/pins (not which topology segments are physically chained, nor direction), `mapping/wiring.json` and `mapping/wiring_bench.json` are placeholders using this default distribution:
- strip1: seg 1..11
- strip2: seg 12..23
- strip3: seg 24..29
- strip4: seg 30..40

All segments default to `dir: "a_to_b"` and must be corrected after physical validation.

## Validation + correction checklist

### A) Structural checks (no hardware required)

1. `mapping/wiring.json`
   - Exactly 4 strips.
   - Segment IDs `{1..40}` appear exactly once total.
   - Each segment has `dir` in `{a_to_b, b_to_a}`.
   - `isBenchSubset` is `false`.
2. `mapping/wiring_bench.json`
   - `isBenchSubset` is `true`.
   - Segment IDs are a strict subset of `{1..40}` with no duplicates.
   - Strip list is explicit (typically 1 strip).

### B) Generator run (full wiring)

Run:
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring.json --out-ledmap mapping/ledmap.json --out-pixels mapping/pixels.json`

Expect:
- No collisions reported by the generator.
- Output summary includes `leds=560`.

### C) Generator run (bench wiring)

Planned enhancement:
- `scripts/generate_ledmap.py` currently requires all 40 segments; it must be updated to support subset wiring for bench mode (see `docs/architecture/wled_integration_implementation_plan.md`, Section 6.2).

### D) Physical validation (later; requires hardware + a known test)

Use the planned on-device mapping validation patterns to correct strip order and directions:
- `Index_Walk_Test`: walk LED index 0→N-1 and confirm continuity and expected segment chaining.
- `XY_Scan_Test` / `Coord_Color_Test`: confirm 2D orientation and that segment directions match the topology A/B endpoints.

When a segment direction is found to be reversed, flip only that segment’s `dir` field to `b_to_a`.
