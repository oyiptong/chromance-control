# Agent Prompt: Close Milestone 1 (WLED Feasibility) + Verify/Close Milestone 2 (Mapping Artifacts)

## Context (do not re-debug from scratch)
We have a working WLED spike that can drive Chromance’s 4 APA102/DotStar strips as a single contiguous index space (0..559) with correct bus starts/lengths. We also discovered two important constraints:
1) A WLED pin-pair incompatibility: bus1 (154..322 / 168 LEDs / 12 segments) did not work on GPIO19/18 but works on GPIO17/16; runtime was updated to match and confirmed working.
2) A power/voltage-drop behavior: at higher brightness, some pixels on bus1 lose the Blue channel (white→orange, magenta→red). Lower brightness fixes it. This indicates power injection / power limiting is required for “full-white at high brightness” stability.

Artifacts to use as source-of-truth for what exists today:
- Task log: `/mnt/data/TASK_LOG.md` :contentReference[oaicite:0]{index=0}
- Implementation plan: `/mnt/data/wled_integration_implementation_plan.md` :contentReference[oaicite:1]{index=1}

## Goal
1) Produce a clean “Milestone 1 COMPLETE” closeout:
   - Acceptance checks are recorded (contiguous index, bus boundaries, ledmap loading, stability at chosen operating point).
   - Repo docs/templates reflect the final working configuration.
   - Task log has a clear summary entry + corrections of earlier misdiagnoses.

2) Verify Milestone 2 is complete (and if any gaps exist, fill them):
   - Mapping generator hardened + reproducible.
   - Generated artifacts and headers are correct for both full + bench subset.
   - Proof-of-life commands and invariants documented.

---

# Milestone 1 Closeout Tasks (WLED Feasibility)

## M1.1 Update WLED spike “source of truth” config + docs
### Files
- `tools/wled_spike/chromance_led_prefs_template.json`
- `tools/wled_spike/README.md`

### Required edits
1) Ensure bus configuration reflects the *working* setup:
   - Starts: 0 / 154 / 322 / 406
   - Lengths: 154 / 168 / 84 / 154
   - Pins: bus1 must be DATA=17 / CLK=16 (not 19/18).
   - Keep strip3 pins at DATA=33 / CLK=27 if that is the current known-good WLED wiring; otherwise align to current runtime wiring.
2) Add a short “Known constraints” section in the README:
   - **Pin constraint:** GPIO19/18 fails for WLED APA102 on this Feather setup; GPIO17/16 works.
   - **Power constraint:** High-brightness white/magenta may cause Blue channel dropout in fixed areas without sufficient power injection; mitigated by power injection and/or WLED power limiter / brightness cap.
3) (Optional but recommended) Ensure the README includes the 4-color regression test procedure:
   - seg0 0–154 = red
   - seg1 154–322 = green
   - seg2 322–406 = blue
   - seg3 406–560 = white

## M1.2 Improve spike serial diagnostics (optional but recommended)
### File
- `tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp`

### Required edits
- In the periodic `ChromanceSpike:` banner, print each bus’s:
  - start, len, DATA pin, CLK pin
- Keep existing contiguous index debug prints.

## M1.3 Run/record Milestone 1 acceptance checks
### Required checks
1) **Contiguous index space**
   - Confirm serial logs show correct mapping for indices:
     - idx=0, idx=154, idx=322, idx=406 → bus local=0
2) **Bus boundary visual regression test**
   - Run the 4-segment color regression test (red/green/blue/white) and confirm all 4 strips show expected ranges.
3) **ledmap.json loading**
   - Confirm Chromance sparse 2D ledmap loads fully (no truncation) using the patched WLED clone behavior.
4) **Stability at chosen operating point**
   - Set a “chosen operating point” that is realistic and stable:
     - Either (A) enable WLED power limiting and/or cap brightness, OR (B) confirm adequate power injection is present.
   - Run a 10-minute soak test with a representative effect or solid colors; confirm no missing segments or random corruption.

### Proof-of-life commands to include in the log
- `cd tools/wled_spike/wled && pio run -e chromance_spike_featheresp32`
- `pio device monitor -e chromance_spike_featheresp32` (capture a snippet of the bus config banner)

## M1.4 Add a Milestone 1 completion entry to TASK_LOG.md
Add a new entry that summarizes:
- PASS: contiguous index space 0..559 across 4 busses
- PASS: 4-color regression test
- FIX: bus1 pins 19/18 → 17/16
- NOTE: power/voltage-drop behavior at high brightness; mitigation documented

Also add a correction note to any earlier entries that claimed 17/16 was unreliable (if present), clarifying that the actual deterministic failure was 19/18.

---

# Milestone 2 Verify/Closeout Tasks (Mapping Artifacts + Generator Hardening)

## M2.1 Verify generator reproducibility + invariants
### Required checks
1) Confirm generator emits:
   - `mapping/ledmap.json`
   - `mapping/pixels.json`
   - bench variants: `mapping/ledmap_bench.json`, `mapping/pixels_bench.json`
2) Confirm the rounding/collision behavior is deterministic (no reintroduced collisions).
3) Confirm wiring inputs are canonical:
   - `mapping/wiring.json`
   - `mapping/wiring_bench.json`
   - confidence markers / README reflect current state.

### Proof-of-life commands
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring.json --out-ledmap /dev/null --out-pixels /dev/null`
- `python3 scripts/generate_ledmap.py --wiring mapping/wiring_bench.json --out-ledmap /dev/null --out-pixels /dev/null`

## M2.2 Verify runtime headers generation pipeline
### Required checks
1) Generated headers exist and are produced by the pre-build hook:
   - `include/generated/chromance_mapping_full.h`
   - `include/generated/chromance_mapping_bench.h`
2) Confirm `include/generated/` is treated as generated output (ignored / not hand-edited).
3) Confirm build envs:
   - `pio run -e runtime`
   - `pio run -e runtime_bench`
   - `pio test -e native`

## M2.3 Add Milestone 2 completion entry to TASK_LOG.md
Summarize:
- generator + bench subset support
- header emission + pre-build hook
- runtime envs build successfully + native tests pass

---

# Deliverables
1) PR or patchset updating:
   - `tools/wled_spike/chromance_led_prefs_template.json`
   - `tools/wled_spike/README.md`
   - (optional) `tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp`
   - `TASK_LOG.md` (Milestone 1 completion entry + any corrections; Milestone 2 completion entry if not already explicit)
2) Paste the key serial banner lines and the 4-segment regression test result summary into the Milestone 1 log entry.

# Definition of Done
- Milestone 1: documented PASS + constraints + stable operating point procedure.
- Milestone 2: generator + headers + bench subset pipeline verified and documented, with proof-of-life commands recorded.

