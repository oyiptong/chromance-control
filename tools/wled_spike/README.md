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

Note: upstream WLED’s LED settings WebUI currently limits “2 pin” (SPI) busses to 2.
The installer patches the local clone’s `wled00/data/settings_leds.htm` to allow 4
so you can configure the 4 DotStar strips needed for this spike (rebuild required).

Note: Chromance `mapping/ledmap.json` is a sparse 2D ledmap where `map.length == width*height` and `width*height` is much larger than `LED_COUNT`.
Upstream WLED truncates such ledmaps to `LED_COUNT` entries; the installer patches WLED’s ledmap loader so the full 2D map can be used.

WiFi credentials (optional):
- If `include/wifi_secrets.h` exists (local-only in this repo), the installer seeds WLED compile-time defaults (`CLIENT_SSID`/`CLIENT_PASS`) into the local WLED clone so it can join WiFi without AP provisioning.
- If not present (or not parsable), configure WiFi using the AP WebUI.

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
  - Strip2 DATA=17 CLK=16
  - Strip3 DATA=33 CLK=27
  - Strip4 DATA=14 CLK=32

Pin note (Feather ESP32, this setup):
- WLED APA102 bus1 (the 168-LED / 12-segment strip) did not drive reliably on GPIO19/18 but works on GPIO17/16; treat GPIO19/18 as known-bad for WLED APA102 here.

Power note (this setup):
- At higher brightness, some pixels on bus1 may lose the Blue channel (white→orange, magenta→red). Lower brightness fixes it.
- Mitigation: add/verify power injection and/or enable WLED power limiting (set “Maximum PSU Current”) and cap brightness.

Optional: apply a pre-made LED Preferences template (faster than clicking):
- In WLED: `Config → LED Preferences`
- Under “Config template”, choose `tools/wled_spike/chromance_led_prefs_template.json` and click “Apply”
- Then click “Save” and reboot

If you don’t know the IP address yet:
- If WLED joins your WiFi, the spike prints `ChromanceSpike: sta_ip=...` on Serial.
- If WLED is not configured for WiFi, it should start an AP; the spike prints `apSSID` and `ap_ip` (usually `4.3.2.1`). Connect to that SSID and open `http://4.3.2.1/`.

## Expected results

On Serial logs you should see output repeating every ~2s (not just at boot), e.g.:
- `ChromanceSpike: busses=4 total_len=560`
- `ChromanceSpike: contiguous_index_space=true`
- `ChromanceSpike: idx=154 -> bus1 local=0` (and similar for the other Chromance boundary indices)

Visually, each strip’s first LED should show:
- bus0: Red
- bus1: Green
- bus2: Blue
- bus3: Magenta

Additionally, LEDs at indices `(0+1)`, `(154+1)`, `(322+1)`, `(406+1)` should be White (boundary markers).

If the overlay colors look wrong (e.g. red appears orange):
- Set each APA102 output’s “Color Order” to `BRG` in `Config → LED Preferences`, Save, reboot.

4-segment regression test (recommended):
- Configure 4 segments and confirm all ranges light:
  - seg0: start 0, len 154 = Red
  - seg1: start 154, len 168 = Green
  - seg2: start 322, len 84 = Blue
  - seg3: start 406, len 154 = White

If you cannot configure non-overlapping bus start indices (or the printed bus starts are not contiguous),
the Milestone 1 multi-bus “kill gate” fails for Option B.
