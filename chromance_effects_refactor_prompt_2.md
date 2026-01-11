You are a senior embedded systems software engineer reviewing and refining an existing design document.

Your task is to **refine and tighten the document**, not redesign it.  
Preserve all existing architectural intent, constraints, and scope.  
Do **not** introduce a web UI, dynamic loading, or runtime reflection.

## Goals of this refinement
Incorporate the following improvements **explicitly and concisely**, using the document’s existing tone and level of rigor:

---

## Required refinements

### 1. Explicit Effect Config Blob Size Limits
- Introduce a global compile-time limit for per-effect persisted config blobs.
- Add a short rationale (NVS limits, wear mitigation, predictability).
- Include a `constexpr size_t kMaxEffectConfigSize` example.
- Show how effects enforce this via `static_assert(sizeof(Config) <= kMaxEffectConfigSize)`.

---

### 2. Define the Settings Store Interface
- Add an explicit `ISettingsStore` (or similarly named) interface definition.
- It must support **blob-based read/write** (fixed-size buffers).
- Keep it minimal and platform-agnostic.
- Clarify that ESP32 Preferences is one implementation, not exposed to core.

---

### 3. Separate Render vs Event Context
- Refine `EffectContext` to avoid passing persistence/logging pointers into the render path.
- Either:
  - split into `RenderContext` and `EventContext`, or
  - explicitly state that persistence/logging members are **cold-path only** and must not be used during rendering.
- Ensure the render loop remains allocation-free and side-effect-free.

---

### 4. Error Handling & Fallback Strategy
Add a short, explicit section defining system behavior when:
- `set_param()` receives invalid values
- `enter_stage()` fails
- Config blobs are missing, corrupted, or version-mismatched
- Persistence writes fail or are retried

Use deterministic, embedded-appropriate rules:
- return codes (no exceptions)
- safe defaults
- retry with debounce/backoff
- never crash in render path

---

### 5. Clarify Out-of-Scope Parameter Types
- Explicitly state that **string/text parameters are intentionally unsupported**.
- Reinforce that enums, numeric IDs, or fixed-size values are preferred.
- Explain this choice in terms of heap safety, persistence simplicity, and MCU constraints.

---

## Optional but Recommended Additions

If they fit naturally, add:
- A short **Acceptance Criteria** checklist (e.g. no heap allocation in render loop, configs persist across reboot, serial shortcuts still work).
- A brief **Phased Implementation Plan** (interfaces → manager → persistence → migrate effects).

---

## Constraints (do not violate)
- No runtime allocation in render loop
- No runtime introspection or parsing
- No dynamic containers in core hot paths
- Maintain clear separation between `src/core/**` and `src/platform/**`
- Do not expand scope beyond effects, configuration, persistence, and control routing

---

## Output requirements
- Modify the document directly (as if preparing a revised version).
- Keep changes focused and proportional.
- Do not restate unchanged sections.
- Do not add speculative features.
- Use concise embedded-engineering language.

The result should feel like a **tightened, production-ready revision** of the original document, not a rewrite.

