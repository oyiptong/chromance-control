You are a senior systems architect + technical writer. Your task is to ITERATE on the existing PRE-PLAN markdown document at:

/docs/plans/webui_preplan.md

Goal: refine the PRE-PLAN so it is design-doc-ready (no guesswork, no open architectural forks that would cause churn). Do NOT write implementation code.
Update the document as needed.

NON-NEGOTIABLE CONTEXT (must remain true)
- Target: ESP32 Feather HUZZAH32, 4MB flash, OTA enabled
- UI served from firmware; assets are minified + gzipped at build time
- UI stack is fixed: Astro (static prerender), Tailwind, daisyUI (SINGLE theme only), Preact islands
- UI asset budget: soft 200KB gzipped total, hard 300KB gzipped total; build MUST FAIL if hard ceiling exceeded
- Priority: minimize server CPU usage AND protect LED render loop (hot path)

NEW DECISIONS (must be incorporated)
- Persistence storage is blob-per-effect:
  - Per-effect config key: "e" + 4-hex-digit EffectId (e.g. e0007)
  - Value is a fixed-size 64-byte (kMaxEffectConfigSize) blob per effect
  - Active effect selection key: aeid (u16)
  - Legacy keys remain: bright_pct, mode (same namespace)
- Persistence UI is per-effect with a selector (no “dump all blobs at once” endpoint)

WHAT TO DO
1) Remove/resolve any “OPEN QUESTION” that affects architecture or safety, by making a clear recommendation + rationale.
   - Especially: HTTP server model, JSON emission strategy, routing strategy, OTA/partition sizing, budget enforcement timing.
   - If you cannot fully decide, narrow to ONE default path and define a migration path (but no multiple-option hedging).

2) Strengthen the document across these dimensions (add concrete constraints/guardrails):
   A) Firmware architecture choices
      - Hot path vs cold path separation: define the scheduling contract (what can run in render loop vs deferred)
      - HTTP serving model: choose sync vs async vs dual-core-task; justify for CPU + render-loop protection
      - JSON serialization: define bounded-buffer vs streaming rules per endpoint class
      - RAM risks: define “no full-response buffering” for unbounded endpoints; define caps
      - OTA interactions: add explicit flash/partition constraints and binary-size budget checks

   B) Frontend architecture choices
      - Real URLs: `/`, `/effects/[slug]`, `/settings`, `/settings/persistence`
      - Clarify how deep links are served (shell pages) without SPA routing libraries
      - Preact islands must be leaf widgets; no global client store; no client router

   C) Asset budget discipline
      - Move enforcement earlier: the hard 300KB check must be always-on once assets exist
      - Define EXACTLY what is counted in the “gzipped total”
      - Add build output requirements: per-asset size report + total + warnings/failures
      - Add regression detection mechanism (baseline or CI gate) to catch slow bloat

   D) End-to-end cohesion
      - Make API contracts explicit and consistent with the UI assumptions
      - Clarify ownership boundaries (core vs platform web server vs UI)
      - Ensure future features (live logs, simple stats) are explicitly constrained to avoid bundle bloat:
        - no chart libraries
        - logs are bounded (ring buffer), polling-first; streaming only if justified later

3) Update the persistence view section to match blob-per-effect + selector:
   - Define endpoints:
     - A small “persistence summary” endpoint (globals + list of effects with present? + optional version/CRC on-demand)
     - A per-effect detail endpoint that returns ONLY one 64-byte blob (hex/base64) and minimal metadata
   - Explicitly forbid “return all blobs in one response”
   - Add guardrails: caps on list sizes, no NVS key enumeration beyond Chromance-owned registry
   - Add minimal versioning/migration expectations inside the blob (e.g., version byte) and behavior on mismatch

4) Add a short section titled “CPU Minimization & Render-Loop Protection”
   - Define request/response budgets (rough numbers are OK but must be explicit)
   - Define caching strategy for immutable assets (fingerprinted names, long cache headers)
   - For static assets: require precompressed serving (no on-device gzip)
   - For APIs: rate limits / debounce expectations for param updates to avoid excessive work and NVS wear

OUTPUT REQUIREMENTS
- Output MUST be the updated PRE-PLAN markdown (a complete rewritten doc, not bullet feedback).
- Keep scope identical: no new features beyond what’s already planned; only add constraints, decisions, and clarifications.
- Maintain the chosen stack and asset budget. Do not propose switching away from Astro/Tailwind/daisyUI/Preact.
- No implementation code. Pseudocode is allowed only for build checks or conceptual flow, and must be minimal.
- Use explicit “MUST / MUST NOT / SHOULD” language for guardrails.
- Add a small “Decision Log” section listing each resolved open question and the chosen default.

Before finalizing, scan the entire document for:
- leftover ambiguous language (“maybe”, “possibly”, “open question”)
- missing caps/limits (response sizes, effect list sizes, persistence list)
- any plan that could accidentally enable SPA bloat or large dependencies
