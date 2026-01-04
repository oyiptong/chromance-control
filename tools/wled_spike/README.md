# Milestone 1 — WLED Feasibility Spike (multi-bus index model)

This folder contains a small WLED usermod + build override to test the Milestone 1 “kill gate”:
whether WLED exposes a single **contiguous** LED index space compatible with `mapping/ledmap.json`
indices (`0..559`) when using **4 DotStar/APA102 outputs**.

## What this spike checks

- WLED bus start/length configuration (`BusManager`) and whether the configured busses form a contiguous index space.
- Visual confirmation: each bus’s first pixel is forced to a unique color (Red/Green/Blue/Magenta) every frame.

## Setup

1) Clone WLED (local-only; gitignored by this repo):
- `git clone --depth 1 https://github.com/Aircoookie/WLED.git tools/wled_spike/wled`

2) Install the spike usermod + PlatformIO override into the clone:
- `./tools/wled_spike/install_into_wled.sh`

3) Build WLED:
- `cd tools/wled_spike/wled`
- `pio run -e chromance_spike_featheresp32`

4) Flash and configure LED outputs in WLED UI (manual):
- Configure 4 APA102/DotStar busses with lengths:
  - bus0 = 154, bus1 = 168, bus2 = 84, bus3 = 154 (total 560)
- Set bus start indices to make a single contiguous index space:
  - bus0 start = 0
  - bus1 start = 154
  - bus2 start = 322
  - bus3 start = 406
- Wire pins to match Chromance (`src/core/layout.h`):
  - Strip1 DATA=23 CLK=22
  - Strip2 DATA=19 CLK=18
  - Strip3 DATA=17 CLK=16
  - Strip4 DATA=14 CLK=32

## Expected results

On Serial boot logs you should see something like:
- `ChromanceSpike: busses=4 total_len=560`
- `ChromanceSpike: contiguous_index_space=true`
- `ChromanceSpike: idx=154 -> bus1 local=0` (and similar for the other Chromance boundary indices)

Visually, each strip’s first LED should show:
- bus0: Red
- bus1: Green
- bus2: Blue
- bus3: Magenta

Additionally, LEDs at indices `(0+1)`, `(154+1)`, `(322+1)`, `(406+1)` should be White (boundary markers).

If you cannot configure non-overlapping bus start indices (or the printed bus starts are not contiguous),
the Milestone 1 multi-bus “kill gate” fails for Option B.
