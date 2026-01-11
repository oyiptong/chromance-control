You are a senior firmware + embedded architecture lead. Your task is to REFINE the existing implementation plan document to close the remaining gaps identified in peer feedback. You MUST edit the existing file in place.

## Inputs (MUST READ)
1) `docs/architecture/wled_integration_preplan.md` (source of truth for requirements)
2) `docs/architecture/wled_integration_implementation_plan.md` (the plan to refine)
3) Inspect the repo as needed to confirm PlatformIO environments and file structure.

## Deliverable
Update (edit in place):
`docs/architecture/wled_integration_implementation_plan.md`

This is still a plan-only task. Do NOT implement features.

---

## Scope constraints (MUST KEEP)
- Do NOT add deep planning for networking responsiveness, liveview/peek streaming, packet-loss tests.
- Do NOT implement sound or heartbeat/biofeedback.
- Keep modular seams for future extension (`Signals`, `ModulationProvider`).
- Keep the plan comprehensive for mapping + effects + FPS + multi-strip output + optional web simulator.

---

## Required refinements (apply all)

### A) Add “Milestone 0: Establish wiring.json source of truth” (CRITICAL)
The plan must treat `mapping/wiring.json` as a prerequisite.
Add a Milestone 0 before the current Milestone 1 that includes:
- Define/confirm the canonical `wiring.json` schema (4 strips; segment order; per-segment direction).
- Create the initial canonical `mapping/wiring.json`:
  - At minimum, include a **Bench Mode** variant (strip 0 only) to unblock development.
  - If full 4-strip wiring is already known, include it as the canonical file and optionally provide a bench subset file.
- Acceptance criteria:
  - `wiring.json` references all expected segments exactly once (bench subset may reference a subset, but must be explicit).
  - Strip segment counts match hardware distribution where applicable (e.g., 11/12/6/11) OR document intentional deviations.
  - Generator script can run successfully using this wiring file.
- Explicitly state that subsequent milestones depend on this artifact.

### B) Promote the WLED multi-bus index model to the PRIMARY kill gate (Option B)
In the WLED spike milestone (Milestone 1), elevate “WLED multi-bus LED index model” to:
- The #1 decision gate for Option B viability.
- Include a concrete experiment spec (not just a bullet), e.g.:
  - Configure 4 buses with known lengths.
  - Set distinct colors on bus0 LED0, bus1 LED0, bus2 LED0, bus3 LED0.
  - Determine whether WLED exposes a single contiguous index space (0..559) or per-bus index spaces.
  - Verify how `ledmap.json` indices are interpreted relative to multi-bus configuration.
- Add explicit outcomes:
  - If contiguous indexing works as required → Option B remains viable.
  - If per-bus indexing or overlap occurs → Option B is not viable without a remapping/fork; document fallback (Option C or custom remap layer).

### C) Add compilation/build context per file/module (PlatformIO environments)
In the “File-by-file implementation plan” section, add two new required fields for each file entry:
- **Build environments**: which PlatformIO environment(s) compile it (e.g., `env:diagnostic`, `env:native`, or both).
- **Dependencies / prerequisites**: what must exist before the file can be implemented (schemas, generator outputs, interfaces).

Also add an explicit rule in the plan:
- `src/core/**` (portable) must compile in both firmware and native test builds; avoid Arduino headers.
- `src/platform/**` is firmware-only; may include Arduino/ESP-IDF specifics.

Confirm actual environment names by inspecting `platformio.ini` (or equivalent) and use the real names in the plan.

### D) Re-sequence milestones to reflect dependencies
After adding Milestone 0, ensure the milestone ordering reflects:
- wiring.json available before mapping generation and before any WLED ledmap usage.
- mapping generator + artifacts early enough to support validation patterns and bench testing.
- If the plan currently requires 560-only assumptions, ensure Bench Mode is supported (variable LED count) OR clearly define how bench config maps into 560-index space.

### E) Clarify “obsolete files under Option B” as conditional
In Option B section, keep the obsolete-files list but frame it explicitly as:
- “If Option B is chosen: these are likely obsolete and should be archived or deleted after migration is stable.”
- “If Option A/C: these are retained and enhanced.”

Prefer “archive to attic/” guidance before deletion.

---

## Quality bar
- Apply changes by editing the existing plan, not by appending a separate addendum.
- Keep the document cohesive and readable.
- Ensure acceptance criteria are measurable.
- Keep within the original scope constraints (no network/audio deep dives).

## Acceptance criteria
- Implementation plan includes a Milestone 0 for `wiring.json`.
- WLED multi-bus indexing is the primary kill gate for Option B and includes a concrete test spec.
- File-by-file entries include build environments and dependencies, using real PlatformIO env names.
- Milestones are re-ordered to respect artifact dependencies.

