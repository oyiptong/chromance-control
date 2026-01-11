You are a senior embedded systems software engineer refining an existing design document.

Your task is to **apply a final tightening pass** to the document.  
Do **not** redesign the architecture, add new features, or expand scope.  
Preserve all existing constraints, intent, and tone.

This refinement should resolve a small number of remaining ambiguities so the document can be treated as **authoritative for implementation**.

---

## Goals of this refinement

Incorporate the following clarifications and small additions **only where they naturally fit** in the existing structure.

---

## Required refinements

### 1. Clarify How Effects Access Their Config During Render
Explicitly state **one supported pattern** for how effects read their persisted configuration in `render()`.

Constraints:
- No heap allocation
- No map/dictionary lookups in render
- No `void*` casting in hot paths unless unavoidable

Preferred approach (unless the document already implies otherwise):
- `EffectManager` owns config storage
- Each effect holds a **typed pointer/reference** to its config, set during activation or config load
- `render()` reads directly from that pointer

Add a short subsection explaining this wiring clearly.

---

### 2. Complete the EffectManager Public API
Update the `EffectManager` API section so it fully reflects behaviors already described elsewhere in the document.

Add the following methods if not already present:
- `restart_active()` — resets runtime state only
- `reset_config_to_defaults(EffectId)` — resets config, persists, and restarts if active

Keep the API minimal and procedural. Do not introduce policy logic.

---

### 3. Define What “EffectCatalog” Is
The document references `EffectCatalog` in diagrams and text but does not define it concretely.

Clarify **one** of the following:
- `EffectCatalog` is an alias for a fixed-capacity registry (e.g. `EffectRegistryV2<MaxEffects>`), or
- `EffectCatalog` is a thin wrapper around such a registry with minimal convenience methods

Add a short explanation and (optionally) a minimal interface or alias declaration.

---

### 4. Clarify System vs Effect Key Routing
Without redesigning the key system:
- Add a short comment, helper, or note clarifying which keys are handled as **system/global** commands vs **effect-scoped** commands.
- This clarification should live near the `Key` enum or in the `CommandRouter` discussion.

Do not introduce complex unions or new event types.

---

### 5. Clarify Effect Instance Lifetime
Add a brief note stating:
- Where effect instances live (e.g., statically allocated globals during migration)
- That the catalog/registry stores **non-owning pointers**
- That factories/dynamic allocation are explicitly out of scope

This should be a sentence or two only.

---

## Explicitly out of scope (do not add)

- New stage navigation enums or UI-driven abstractions
- String/text parameter support
- Dynamic effect loading or factories
- Changes to render-loop constraints
- Web UI implementation details
- Major rewrites of existing sections

---

## Output requirements

- Modify the document directly, as a revised version.
- Keep changes proportional and localized.
- Do not restate unchanged sections.
- Prefer short clarifying paragraphs over new abstractions.
- Maintain embedded-systems discipline throughout.

The result should read as a **final pre-implementation revision**, not a new design.

