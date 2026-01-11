# Web UI (Firmware-hosted) — PRE-PLAN

This document is a **PRE-PLAN** for adding a firmware-hosted Web UI to Chromance Control.
It is **design-only**: it enumerates required behavior, data contracts, code locations, and build integration,
but does **not** include implementation code.

Target (non-negotiable): **ESP32 Feather HUZZAH32**, 4MB flash, OTA enabled.

UI stack (non-negotiable):
- **Astro** (static prerendered pages)
- **Tailwind CSS** (purged in production)
- **daisyUI** (**single theme only**)
- **Preact islands** for interactive components only (no SPA router, no global client store)

Asset constraints (non-negotiable):
- Assets are **minified and gzipped** at build time; firmware serves **precompressed** bytes (no on-device gzip).
- UI asset budget: **soft 200KB gzipped total**, **hard 300KB gzipped total**; build **MUST FAIL** on hard limit.

Persistence constraints (non-negotiable; already chosen/implemented in core):
- Per-effect config key: `"e" + 4-hex-digit EffectId` (e.g., `e0007`)
- Value: fixed-size **64-byte** blob (`kMaxEffectConfigSize`)
- Active effect selection key: `aeid` (u16)
- Legacy keys remain in the same namespace: `bright_pct`, `mode`
- Persistence UI is **per-effect with a selector** (no “dump all blobs at once” endpoint)

---

## 1. Summary

Goal: a firmware-hosted Web UI (LAN-only) that lets a user:
1) See all possible effects
2) Activate an effect
3) Configure an effect’s parameters
4) Restart an effect and navigate sub-stages (when supported)
5) Manage global settings (brightness) and persistence (inspect per-effect config blobs; wipe all)

Required page routes (no hash routing):
- `/` — effect list + activate controls
- `/effects/[EFFECT-SLUG]` — effect detail (params + restart + stages)
- `/settings` — global settings (brightness) + firmware reset
- `/settings/persistence` — persistence summary + per-effect blob viewer + “delete all persisted settings”

The firmware will serve:
- Static, gzipped UI assets (HTML shells + CSS + minimal JS) from flash
- A small JSON HTTP API for listing effects, reading schemas/values, applying changes, and exposing persistence summaries

This UI relies on the “v2 effects” control plane:
- Effects are **enumerable** (catalog)
- Effects expose **typed parameter schemas** (descriptor tables)
- All persistence is **cold path only** and debounced (no per-frame writes)

---

## Decision Log

This section resolves architectural forks; these decisions are binding for the design doc and implementation plan.

1) **HTTP server model:** synchronous Arduino `WebServer` running on the Arduino loop thread.
   - Rationale: smaller footprint than async stacks; avoids additional tasks and heap churn; simplest to keep effect control single-threaded.

2) **Render-loop protection model:** the loop must treat rendering as highest priority; HTTP handling is best-effort and strictly time-bounded.
   - Rationale: prevents UI traffic from causing missed frames or visible flicker.

3) **Routing strategy:** firmware serves real URLs; `/effects/<slug>` is served by an Astro prerendered **shell page** (no client router) that fetches effect data by slug.
   - Alias slugs are redirected by firmware to a canonical slug.

4) **JSON emission strategy:** API responses must be emitted with **bounded output** (streaming or bounded-buffer writer) and strict caps; no building “unbounded” JSON in memory.
   - Rationale: protects RAM; avoids blocking.

5) **Persistence inspection UX:** `/settings/persistence` is **summary + per-effect selector**; the API must never return all blobs in a single response.
   - Rationale: bounded response size and predictable CPU/RAM usage.

6) **Asset budget enforcement:** always-on once Web UI assets exist; build fails if gzipped total exceeds 300KB and reports per-asset sizes.
   - Rationale: prevents gradual bloat that would threaten OTA viability.

7) **WiFi availability:** Web UI is only available when WiFi credentials are present and STA connects (same gating as OTA today).
   - Rationale: avoids introducing AP provisioning as a new feature in this milestone set.

---

## 2. Non-goals

- No WAN/cloud connectivity; LAN-only assumptions.
- No authentication/login system in the initial delivery.
- No WiFi provisioning UI (credentials remain build-time/env or `wifi_secrets.h`).
- No LED “live view” streaming, no simulators, no pixel preview.
- No heavy UI dependencies:
  - no client-side router
  - no global state stores
  - no chart libraries
  - no JSON syntax highlighting libraries
- No per-effect presets library (beyond persisted config blobs) in this pre-plan.

---

## 3. Constraints & tradeoffs (RAM / flash / CPU / network)

### MCU hardware + OTA constraints
- Device: ESP32 Feather HUZZAH32, 4MB flash, no PSRAM.
- OTA is enabled; firmware must remain within the OTA app partition size (PlatformIO already enforces max program size).
- The Web UI asset budget (hard 300KB gzipped) is in addition to firmware code and other embedded data.
- Firmware binary size (code + all embedded assets) MUST fit within the OTA app partition with a safety margin (exact partition size to be measured later; the requirement is mandatory regardless).
- The design MUST preserve OTA reliability:
  - no runtime gzip compression
  - no runtime filesystem dependency for UI assets
  - predictable binary growth controlled by budget gates

OTA partition / binary size awareness (mandatory):
- The build MUST report (at minimum) the final firmware binary size and total embedded Web UI asset size.
- The build MUST fail if the resulting image does not leave a reasonable OTA safety margin (recommended target: >=64KB or >=10% of the OTA app partition, whichever is larger).

### RAM constraints
The Web UI server and API MUST be designed so that worst-case RAM usage is bounded:
- API responses MUST NOT require “building an unbounded JSON string” in RAM (bounded buffering up to `MAX_JSON_RESPONSE_BYTES` is permitted when explicitly allowed by the API emission rules).
- Request bodies MUST have explicit size limits and be rejected if exceeded.
- Connection count MUST be bounded (see CPU section).

Hard caps (design-time; implementation MUST enforce):
- `MAX_UI_EFFECTS = 32` (effects listed in UI; if more exist later, UI MUST paginate or filter server-side within this bound)
- `MAX_PARAMS_PER_EFFECT = 24`
- `MAX_STAGES_PER_EFFECT = 8`
- `MAX_HTTP_BODY_BYTES = 1024`
- `MAX_JSON_RESPONSE_BYTES = 8192` for any single API response

### CPU constraints
Primary requirement: protect the LED render loop.
- HTTP work is cold-path and best-effort; render timing must not regress.
- Static assets MUST be served as precompressed bytes with a known content length (no dynamic compression).
- API endpoints MUST be small and fast; no expensive parsing or large allocations.

### Network constraints
- LAN-only usage; no TLS.
- UI assumes a single device instance per hostname/IP.
- Caching is required for immutable assets (fingerprinted filenames).

---

## CPU Minimization & Render-Loop Protection

This section defines guardrails so UI traffic cannot starve rendering.

1) **Scheduling contract**
- Rendering is the highest priority work in `loop()`.
- The web server handler MUST be invoked only when doing so cannot cause a missed render deadline.
- If a render is due (or imminent), web server work MUST be deferred to a later loop iteration.
- Policy (time-based deadline gate): treat the next render deadline as the scheduler’s next frame due time; the loop MAY service HTTP only if there is at least ~7ms of headroom before that deadline (5ms handler CPU budget + 2ms safety margin).
- The loop MUST also cap web handling to at most one handler pass per iteration; any additional pending web work waits for later iterations.

2) **Request/response budgets**
- Each request handler SHOULD complete its CPU work (excluding socket send time) within ~5ms.
- Any request requiring more than the budget MUST fail fast with a 503-style response (busy) rather than blocking the loop.

3) **Static asset caching**
- Fingerprinted assets (e.g., `app.<hash>.js.gz`) MUST be served with long cache headers (`Cache-Control: public, max-age=31536000, immutable`).
- HTML shells (`/`, `/settings`, `/settings/persistence`, `/effects/<slug>`) MUST be served with no-store or short caching to ensure UI updates match firmware updates.
- Static asset sending MUST be chunked for any asset larger than a small threshold (e.g., >1024 bytes) to avoid long blocking TCP writes; “send the whole response” helpers (e.g., `WebServer::send()`, `WebServer::send_P()`) MUST NOT be used above that threshold.

4) **API rate limiting / debounce**
- Param updates MUST be debounced client-side (sliders send at a low rate; ideally on release).
- Firmware MUST enforce a **global** (not per-client) rate limit for write endpoints to avoid CPU spikes and NVS wear:
  - brightness updates: max 4 requests/second (HTTP 429 on exceed)
  - parameter updates: max 8 requests/second (HTTP 429 on exceed)
- NVS writes MUST remain debounced (owned by the existing settings store + effect manager policy).

5) **No on-device gzip**
- Firmware MUST NOT perform gzip compression at runtime.
- Firmware MUST NOT accept content-encoding compressed uploads.

---

## 4. Architecture overview (firmware + UI build + routing)

### Ownership boundaries
- `src/core/**`:
  - Owns effects model (catalog, schemas, param setting, stage control)
  - Owns persistence interface abstraction (blob store)
  - MUST remain Arduino/ESP-free and testable in `env:native`
- `src/platform/**`:
  - Owns WiFi, web server, OTA, NVS `Preferences` implementation
  - Owns serving embedded assets from flash
- Web UI frontend:
  - Static Astro pages + minimal Preact islands
  - Talks to firmware via same-origin JSON API

### Route serving model (real URLs, no SPA router)
Firmware MUST serve these page routes as HTML shells:
- `GET /`
- `GET /settings`
- `GET /settings/persistence`
- `GET /effects/<slug>` (any slug; canonical or alias)

Deep-link behavior:
- `/effects/<slug>` always returns the same prerendered shell page.
- The shell page reads the current URL path and fetches effect detail JSON from the API.
- If `<slug>` is an alias, firmware MUST redirect to the canonical slug via 301/308 before serving the shell.

### Static assets
Firmware serves:
- CSS bundle (Tailwind purged, daisyUI single theme)
- Minimal JS bundle(s) for Preact islands
- Any required images/icons (strongly discouraged; prefer SVG inline)

All static assets MUST be:
- minified
- gzipped at build time
- served with correct `Content-Type` and `Content-Encoding: gzip`

---

## 5. Data model (effects, parameters, persistence, slugs)

### Effect identity and descriptors
Each effect MUST have:
- `EffectId` (u16, stable)
- `displayName` (string)
- `description` (optional string)
- `canonicalSlug` derived from `EffectId` (see below)
- optional `aliasSlugs[]` (stable, short, human-friendly)

### Slug strategy (deterministic; derived from EffectId)
Canonical slug MUST be:
- `e` + 4 uppercase hex digits of `EffectId` (example: `e0007`)

Alias rules:
- Alias slugs MUST be lowercase ASCII `[a-z0-9_-]` and <= 15 chars.
- Alias collision policy (deterministic):
  - At startup, firmware MUST validate all alias slugs against the current effect catalog.
  - If any alias slug collides (duplicate alias across effects, or alias equals any canonical slug), that alias slug MUST be disabled for all effects; boot MUST continue.
  - Firmware MUST log a warning identifying the alias slug and the conflicting effect ids; canonical slugs remain valid.
  - All non-colliding alias slugs remain valid and MUST redirect to their canonical slug.
- Alias redirects:
  - `GET /effects/<alias>` → 301/308 redirect to `/effects/<canonical>`
  - API requests to `/api/effects/<alias>` MUST return the same canonical effect payload (and may include `canonicalSlug` for clients).

### Parameter schema
Effects with configurable parameters MUST expose a schema with:
- stable param id (u16)
- machine name (string, stable)
- display name (string)
- type: `int` / `float` / `bool` / `enum` / `color`
- range: min/max, step, default
- optional: unit, grouping, display hints (slider vs input)

Float representation constraint:
- Firmware MUST NOT store free-form floats for UI parameters.
- “float” parameters MUST be represented as scaled integers (fixed-point or scale factor metadata).
- The schema MUST include an explicit `scale` field (e.g., `value = raw / 100`) when `type=float`; this scale MUST be a first-class field on the parameter descriptor (not inferred from name or UI hints).

### Restart support
- Effects MAY support restart; UI must show the button only when supported.
- Restart semantics: reset runtime state; do not change persisted config.

### Stage/sub-stage support
- Effects MAY expose stages (e.g., inhale/pause/exhale).
- Stage API MUST support:
  - list stages
  - query current stage
  - enter stage by id

### Persistence model (chosen)
Persistence keys:
- Active effect selection: `aeid` (u16)
- Global brightness (legacy): `bright_pct` (u8)
- Legacy numeric mode (legacy): `mode` (u8)
- Per-effect config blob: `eXXXX` (64 bytes)

Blob versioning/migration requirements:
- Each per-effect blob MUST be treated as opaque bytes for storage, but MUST include a small version header for safety.
- Behavior on version mismatch/corruption:
  - firmware MUST fall back to defaults for that effect
  - firmware MUST persist a sanitized/default blob (debounced)

Persistence inspection constraints:
- Firmware MUST NOT enumerate arbitrary NVS keys for the UI.
- The persistence UI and endpoints MUST only expose Chromance-owned keys derived from:
  - known global keys (`aeid`, `bright_pct`, `mode`)
  - the current effect catalog (`eXXXX` for each effect id)

---

## 6. HTTP API spec

All endpoints are same-origin and LAN-local.
All write endpoints MUST be `POST`/`DELETE` (no state-changing `GET`s).

### Common response rules
- JSON responses MUST be <= `MAX_JSON_RESPONSE_BYTES`.
- Errors MUST be explicit and machine-readable (e.g., `{ ok:false, error:{code,message} }`).
- Firmware MUST reject requests with bodies > `MAX_HTTP_BODY_BYTES`.
- Write endpoints MUST enforce the global rate limits defined in “CPU Minimization & Render-Loop Protection” and return HTTP 429 on exceed (with a small JSON error code like `rate_limited`).

### JSON emission rules (tight, RAM-safe)
- Firmware MUST NOT construct JSON via unbounded `String` concatenation (e.g., repeated `+=` growth).
- Endpoint classes:
  - **MUST stream/chunk:** `GET /api/effects`, `GET /api/effects/<slug>`, `GET /api/settings/persistence/summary`
  - **MAY use bounded buffering:** all other endpoints (bounded by `MAX_JSON_RESPONSE_BYTES`)
- Exceed behavior: if a response would exceed `MAX_JSON_RESPONSE_BYTES` after applying all caps, firmware MUST reject with HTTP 500 and a small JSON error (`error.code = "response_too_large"`). Streaming endpoints MUST preflight their worst-case size before sending any body bytes so they can reject without emitting partial JSON.

### Effects

`GET /api/effects`
- Returns:
  - list of effects: `{ id, canonicalSlug, displayName, description? }`
  - active effect: `{ activeId, activeCanonicalSlug }`

`GET /api/effects/<slug>`
- Returns a single effect descriptor plus:
  - schema (if any)
  - current param values (schema-aware)
  - restart support boolean
  - stages list (if any) + current stage id
- If `<slug>` is an alias, response MUST include the canonical slug (and the page route MUST redirect).

`POST /api/effects/<slug>/activate`
- Sets active effect to `<slug>`; persists `aeid` (debounced).

`POST /api/effects/<slug>/restart`
- Restarts active effect runtime state (no persisted config changes).

`POST /api/effects/<slug>/stage`
- Enters a stage by numeric stage id.

`POST /api/effects/<slug>/params`
- Updates one or more params by param id.
- MUST validate:
  - param id exists
  - type matches schema
  - range constraints
- MUST debounce persistence (no immediate write-per-click).

### Global settings

`GET /api/settings`
- Returns:
  - firmware version
  - mapping version (optional but cheap)
  - brightness (soft percent + ceiling percent, if surfaced)
  - active effect summary (id + slug)

`POST /api/settings/brightness`
- Sets global brightness percent (quantized per firmware rules) and persists `bright_pct`.

`POST /api/settings/reset`
- Triggers a device reboot.
- MUST require strong confirmation (see Security).
  - Request MUST include both a per-boot confirmation token and a confirmation phrase (`RESET`).

### Persistence (per-effect; no “dump all blobs”)

`GET /api/settings/persistence/summary`
- Returns:
  - globals: `aeid`, `bright_pct`, `mode`
  - list of effects with `present` boolean for `eXXXX`
  - a confirmation token for destructive actions (see Security)
- MUST NOT include any per-effect blob bytes.

`GET /api/settings/persistence/effects/<slug>`
- Returns ONLY one effect’s persisted config record:
  - `{ id, canonicalSlug, present, blobHex }` when present
  - minimal metadata: `{ blobVersion, crc16 }` (optional but bounded and recommended)
- MUST NOT return other effects’ blobs.

`DELETE /api/settings/persistence`
- Deletes ALL persisted settings for the Chromance namespace (globals + all per-effect blobs).
- MUST require strong confirmation (see Security).
  - Request MUST include both a per-boot confirmation token and a confirmation phrase (`DELETE`).

---

## 7. UI page specs (`/`, `/effects/[slug]`, `/settings`, `/settings/persistence`)

Global frontend rules:
- Preact islands MUST be leaf widgets only.
- The UI MUST NOT implement a client-side router.
- The UI MUST NOT depend on heavy libraries (no charts, no JSON highlighters, no state stores).
- Use progressive enhancement: pages render meaningful static HTML; islands add interactivity.

Forbidden frontend dependencies (guardrail; enforceable):
- Client routers (e.g., `react-router`, `preact-router`)
- Global state managers (e.g., `redux`, `zustand`, `mobx`, `recoil`)
- Charting libraries (e.g., `chart.js`, `d3`, `echarts`)
- Syntax highlighting (e.g., `prismjs`, `highlight.js`)
- Date/time libraries (e.g., `moment`, `dayjs`, `date-fns`, `luxon`)
- Color picker libraries (e.g., `react-colorful`, `iro.js`)
- CI/build SHOULD fail if any of the above categories appear in the `webui/` lockfile (e.g., `package-lock.json`, `pnpm-lock.yaml`, `yarn.lock`).

### `/`
- Shows list of effects, active indicator, and “Activate” actions.
- Links to `/settings` and to each effect detail page.
- Uses a small island to fetch `/api/effects` and wire buttons.

### `/effects/[slug]`
- Displays effect display name and description.
- Renders controls generated from the schema:
  - bool → toggle
  - enum → select
  - int/float (scaled) → slider or numeric input with step
  - color → minimal RGB inputs (no heavy picker)
- Buttons:
  - Activate (if not active)
  - Restart (if supported)
  - Stage navigation (if supported)

### `/settings`
- Brightness slider (debounced client-side) + current value display.
- Firmware reset button with strong confirmation UX.
- Link to `/settings/persistence`.

### `/settings/persistence`
- Shows persistence summary JSON (globals + effects list with present flags).
- Provides an effect selector (dropdown) populated from the summary.
- On selection, fetches `/api/settings/persistence/effects/<slug>` and displays:
  - minimal metadata
  - `blobHex` as pretty-printed JSON in a `<pre>` block
- JSON display requirements:
  - 2-space indentation
  - monospace
  - subtle background
  - padded, scrollable `<pre>`
  - optional “Copy” button (Clipboard API); MUST NOT add libraries
- “Delete all persisted settings” with strong confirmation.

---

## 8. Build pipeline & PlatformIO integration

### Web UI build
- The Astro build MUST produce static HTML shells for:
  - `/`, `/settings`, `/settings/persistence`, and the generic `/effects/[slug]` shell.
- Tailwind MUST be purged via content scanning.
- daisyUI MUST be configured for a single theme.
- JS MUST be minimized; islands only.

### Gzip + embedding (required)
- Build pipeline MUST gzip all assets that firmware will serve.
- Firmware MUST embed the gzipped bytes into flash-resident arrays (generated header).
- Firmware MUST serve those bytes with correct headers, including `Content-Encoding: gzip`.

### Asset budget enforcement (always-on)
Once Web UI assets exist, the build MUST:
- produce a per-asset gzipped size report
- compute the gzipped total size
- WARN when total > 200KB
- FAIL when total > 300KB

**What counts in “gzipped total” (exact):**
- Sum of gzipped byte sizes for every distinct, firmware-served static asset, including:
  - HTML shells
  - CSS bundle
  - JS bundles
  - icons/images/fonts (strongly discouraged; if present, counted)
- Excludes:
  - API responses
  - firmware binary size (separately enforced by PlatformIO max program size)

Regression detection mechanism:
- CI (or local build) MUST preserve the per-asset report as an artifact.
- A simple gate SHOULD fail the build if total increases by more than a small threshold without explicit acknowledgement (tracked by updating a baseline file in-repo).

### PlatformIO integration
- Add a PlatformIO pre-build script (patterned after `scripts/generate_mapping_headers.py`) that:
  - runs the Astro build
  - gzips outputs
  - generates/updates the embedded asset header
  - enforces asset budgets
- The script MUST be deterministic and MUST fail fast with a clear message when:
  - node tooling is missing
  - the hard asset budget is exceeded

---

## 9. Testing strategy (frontend unit tests + minimal firmware tests)

Frontend:
- Unit tests for Preact islands (effect list, effect detail param form, persistence selector).
- Build checks for:
  - tailwind purge effectiveness
  - daisyUI single theme enforcement
  - asset budget enforcement (hard fail)
  - forbidden dependency scan of `webui/` lockfile (routers/state managers/charts/syntax highlighting/date/color pickers)

Firmware:
- Native tests for core-level logic (no Arduino):
  - canonical slug derivation from EffectId
  - alias collision handling (core catalog helper or platform layer)
  - schema serialization sizing bounds (max params/stages)
- Firmware builds:
  - `pio run -e runtime` and `pio run -e runtime_ota`
  - ensure Web UI build step runs and budget check passes

---

## 10. Security considerations (LAN-only assumptions, destructive actions)

Assumptions:
- LAN-only; no auth.

Required safety measures:
- Destructive endpoints (`/api/settings/reset`, `DELETE /api/settings/persistence`) MUST require strong confirmation:
  - Requests MUST include BOTH:
    - a per-boot confirmation token returned from `GET /api/settings/persistence/summary`, AND
    - a user-facing confirmation phrase in the request body (`DELETE` or `RESET`)
  - If either field is missing, firmware MUST reject with HTTP 400; if either is wrong, firmware MUST reject with HTTP 403.
  - Example request body shapes (JSON):
    - reset: `{ "confirmToken": "<token>", "confirmPhrase": "RESET" }`
    - wipe: `{ "confirmToken": "<token>", "confirmPhrase": "DELETE" }`
- Firmware MUST rate-limit write endpoints to reduce accidental abuse.
- Firmware MUST NOT expose secrets in persistence endpoints (WiFi credentials are not part of Chromance persistence).
- CORS MUST remain same-origin (no permissive `Access-Control-Allow-Origin: *`).

---

## 11. Milestones (thin-slice plan)

Milestone 0 — Static serving skeleton
- Embed and serve the four required pages as static shells.
- Serve gzipped assets with correct headers and deep-link handling.
- Enforce asset budget (hard fail).

Milestone 1 — Read-only UI + persistence summary
- `GET /api/effects`, `GET /api/settings`
- `GET /api/settings/persistence/summary` + `/settings/persistence` JSON viewer + per-effect selector

Milestone 2 — Effect activation
- `POST /api/effects/<slug>/activate` and UI wiring
- Ensure persistence across reboot via `aeid`

Milestone 3 — Parameter editing + persistence
- `GET /api/effects/<slug>` includes schema + values
- `POST /api/effects/<slug>/params` with validation and debounced persistence

Milestone 4 — Restart + stages
- `POST /api/effects/<slug>/restart`, `POST /api/effects/<slug>/stage`
- UI stage navigation controls

Milestone 5 — Hardening
- Strict rate limits and request size caps
- Verify render-loop protection under UI usage
- Enforce regression gates for asset size drift

---

## 12. CODE MAP (MANDATORY)

This section lists code locations relevant to implementing the Web UI. Paths are relative to repo root.

### Existing code (authoritative; already in repo)

- `src/main_runtime.cpp`
  - Key symbols: runtime loop; scheduler; `EffectCatalog`/`EffectManager` wiring; serial controls; brightness integration; OTA integration
  - Role: Web UI server will be started and serviced from here; API handlers will ultimately call into the manager
  - Risk notes: render-loop priority must remain; avoid blocking operations in `loop()`

- `src/core/effects/effect_v2.h`
  - Key symbols: `IEffectV2`, `RenderContext`, `EventContext`, `InputEvent`, stage methods
  - Role: UI-ready effect interface surface
  - Risk notes: must remain platform-free; render context must stay hot-path safe

- `src/core/effects/effect_catalog.h`
  - Key symbols: `EffectCatalog<MaxEffects>` enumeration + lookup
  - Role: drives `/api/effects` and slug resolution
  - Risk notes: avoid dynamic allocations; keep lookups out of render path

- `src/core/effects/effect_manager.h`
  - Key symbols: selection, restart, param set/get, debounced persistence
  - Role: the server-side “controller” behind Web UI actions
  - Risk notes: persistence must remain cold path; keep debounce/backoff deterministic

- `src/core/effects/params.h`
  - Key symbols: `ParamDescriptor`, `EffectConfigSchema`, `ParamValue`
  - Role: schema → UI form generation and param set/get validation
  - Risk notes: “float” must be represented as scaled-int with an explicit `scale` field on the descriptor; avoid adding heap/string param values

- `src/core/settings/effect_config_store.h`
  - Key symbols: `ISettingsStore`, `kMaxEffectConfigSize`
  - Role: persistence abstraction used by manager; referenced by persistence endpoints
  - Risk notes: does not support arbitrary key enumeration (by design)

- `src/platform/effect_config_store_preferences.h`, `src/platform/effect_config_store_preferences.cpp`
  - Key symbols: `PreferencesSettingsStore`
  - Role: ESP32 NVS blob store implementation
  - Risk notes: key length constraints; wipe-all behavior lives here or in a thin wrapper

- `src/platform/settings.h`, `src/platform/settings.cpp`
  - Key symbols: `RuntimeSettings` (brightness/mode persistence)
  - Role: global settings page (`/settings`) integration
  - Risk notes: coordinate legacy keys with new persistence wipe behavior

- `src/platform/ota.h`, `src/platform/ota.cpp`, `src/platform/wifi_config.h`
  - Key symbols: WiFi connect gating; OTA hostname `chromance-control`
  - Role: network bring-up and OTA; Web UI shares the WiFi lifecycle
  - Risk notes: currently disables WiFi when credentials missing; Web UI must follow that in this plan

- `platformio.ini`
  - Key symbols: envs (`runtime`, `runtime_ota`, `diagnostic`, `native`), `extra_scripts`
  - Role: add Web UI asset generation + budget enforcement as a pre-build script

- `scripts/generate_mapping_headers.py`
  - Role: example of a deterministic PlatformIO pre-build generator script (pattern to follow)

- `docs/architecture/wled_integration_implementation_plan.md`
  - Role: architecture constraints (portable core, build hygiene) that Web UI must respect

- `docs/plans/chromance_effects_refactor_design_document.md`
  - Role: canonical plan for UI-ready effects/persistence/hot-cold separation; Web UI must align

### New code (to be added; required by this pre-plan)

- `src/platform/webui_server.h` / `src/platform/webui_server.cpp` (new)
  - Role: start/handle HTTP server; serve static assets; implement `/api/*` endpoints; enforce caps/rate limits
  - Risk notes: must remain cold-path; must be time-bounded per loop iteration

- `include/generated/webui_assets.h` (generated) (new)
  - Role: flash-resident gzipped assets and metadata (content-type, route mapping)
  - Risk notes: asset bloat directly impacts OTA; enforce budgets

- `scripts/generate_webui_assets.py` (new)
  - Role: run Astro build, gzip, generate embedded header, enforce size budgets, emit size report
  - Risk notes: must be deterministic; must fail fast on hard budget exceed

- `webui/` (new)
  - Role: Astro/Tailwind/daisyUI/Preact project
  - Risk notes: must avoid heavy deps; keep JS minimal; enforce single theme and purge
