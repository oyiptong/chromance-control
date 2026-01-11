You are a senior embedded + web engineer. Produce a **PRE-PLAN** document for adding a firmware-hosted web UI to an MCU LED effects system.

This PRE-PLAN will be used later to generate (separately) a design doc and an implementation plan with code snippets. Therefore:

- DO NOT include code snippets in the PRE-PLAN.
- DO include ALL relevant code locations (files, directories, classes, functions, configs) that will be touched or referenced later.
- The PRE-PLAN must be detailed, explicit, and implementation-oriented, but NOT actual implementation.

────────────────────────────────────────────────────────
GOAL
────────────────────────────────────────────────────────
Design (not implement yet) a firmware-hosted web UI that lets a user:

1) See all possible effects  
2) Choose and activate an effect  
3) Configure an effect’s parameters  

This UI must be suitable for an MCU environment with strict RAM and flash constraints.

────────────────────────────────────────────────────────
REQUIRED PAGES + ROUTES (NO HASH ROUTING)
────────────────────────────────────────────────────────
The web UI must have **real URLs**, served by firmware (no `#` routes):

1) Home page: `/`
   - List all available effects
   - Indicate currently active effect
   - Allow user to activate an effect
   - Link to `/settings`

2) Effect Detail page: `/effects/[EFFECT-SLUG]`
   - View effect description and metadata
   - Configure effect parameters
   - Restart effect (if supported)
   - Enter/navigate effect sub-stages (if supported)

3) Settings page: `/settings`
   - Configure global settings (e.g. brightness via slider)
   - Reset firmware (with confirmation)
   - Link to `/settings/persistence`

4) Persistence Settings page: `/settings/persistence`
   - View all persisted settings as JSON
   - Delete ALL persisted settings (strong confirmation)

Do NOT use `/#/...` style routing.

────────────────────────────────────────────────────────
TECH STACK (FIXED)
────────────────────────────────────────────────────────
Assume the following stack:

- **Astro** (static prerendered pages)
- **Tailwind CSS**
- **daisyUI** (single theme only)
- **Preact islands** for interactive components only
- Assets must be **minified and gzipped** at build time and served with proper headers by firmware

The design must:
- Minimize shipped JS (prefer static HTML where possible)
- Use Preact only where interactivity is required
- Avoid heavy client-side routing

────────────────────────────────────────────────────────
HARD CONSTRAINTS (MCU)
────────────────────────────────────────────────────────
- Minimal active RAM usage at runtime
- Minimal flash/storage footprint
- Render hot path must remain side-effect-free
- Persistence and IO must be cold-path only
- User selections/config must persist across reboot (NVS / settings store)
- Serial logging is primarily a control surface, not verbose debugging
- Performance tradeoffs must be explicitly discussed

────────────────────────────────────────────────────────
FRONTEND REQUIREMENTS
────────────────────────────────────────────────────────
- Tailwind CSS must be fully purged (content-scanned) in production
- daisyUI must be limited to a SINGLE theme
- Avoid dynamic class name generation unless explicitly safelisted
- Prefer progressive enhancement

### JSON Display Requirement (Milestone 1)
On `/settings/persistence`:
- Do NOT use any JSON syntax highlighting library
- Display JSON as:
  - pretty-printed (2-space indentation)
  - monospace font
  - subtle background
  - padded, scrollable `<pre>`
- Optional: “Copy JSON” button (Clipboard API)
- No tree viewers, no Prism, no Highlight.js

────────────────────────────────────────────────────────
BUILD + PLATFORMIO INTEGRATION
────────────────────────────────────────────────────────
Propose a build pipeline that:

- Produces static HTML/CSS/JS assets
- Minifies and gzips assets
- Embeds assets into firmware (e.g. arrays, LittleFS, SPIFFS)
- Can be run alongside PlatformIO commands such as:
  - `pio run -e runtime_ota`
  - `pio run -e runtime_ota -t upload`

Explain:
- Where the UI build lives
- How it is invoked (pre/post build hook, wrapper script, etc.)
- How artifacts are consumed by firmware

────────────────────────────────────────────────────────
FIRMWARE / BACKEND API DESIGN
────────────────────────────────────────────────────────
Define minimal HTTP endpoints (prefer HTTP over WebSockets unless justified) for:

- Listing effects + metadata (including slug + aliases)
- Activating an effect
- Reading/writing effect configuration
- Restarting an effect
- Navigating sub-stages (if applicable)
- Reading/writing global settings (brightness)
- Firmware reset
- Fetching all persisted settings as JSON
- Deleting all persisted settings

Include:
- Example request/response shapes
- Versioning strategy
- Notes on RAM-safe response generation (streaming/chunking)

────────────────────────────────────────────────────────
EFFECTS MODEL REQUIREMENTS
────────────────────────────────────────────────────────
Describe how effects expose:

- Stable machine-readable `id`
- Human display name + description
- Slug strategy:
  - Slug must be derived from stable effect id (NOT display name)
  - Deterministic transformation rules
  - Collision handling
  - Backwards compatibility via alias list
  - Redirect semantics from old slug → canonical slug
- Parameter schema:
  - types: int / float / bool / enum / color
  - ranges, defaults, step size, units, grouping
- Restart support
- Sub-stage support (if any)
- Stable serialization format for persistence

Persistence must cover:
- Selected effect
- Per-effect config blobs (with size limits)
- Global settings (brightness, etc.)

Discuss:
- Flash vs RAM tradeoffs
- Corruption handling
- Migration/versioning strategy

────────────────────────────────────────────────────────
UI / UX REQUIREMENTS
────────────────────────────────────────────────────────
### `/`
- Effect list
- Activate button
- Active effect indicator
- Link to `/settings`

### `/effects/[EFFECT-SLUG]`
- Effect description
- Generated form controls from schema
- Save / Activate / Restart actions
- Sub-stage navigation (if supported)
- Redirect if slug is an alias

### `/settings`
- Brightness slider
- Reset firmware button (confirm)
- Firmware info (version/build hash if cheap)
- Link to `/settings/persistence`

### `/settings/persistence`
- JSON display (as specified above)
- “Delete all persisted settings” with strong confirmation
- Explicit discussion of safety/abuse considerations

────────────────────────────────────────────────────────
ROUTING & SERVING REQUIREMENTS
────────────────────────────────────────────────────────
Explain how firmware serves these routes:
- Static files vs single HTML shell
- Handling deep links
- Redirects for slug aliases
- 404 behavior
- Gzip handling and caching implications

────────────────────────────────────────────────────────
DELIVERABLE FORMAT (REQUIRED HEADINGS)
────────────────────────────────────────────────────────
1. Summary  
2. Non-goals  
3. Constraints & tradeoffs (RAM / flash / CPU / network)  
4. Architecture overview (firmware + UI build + routing)  
5. Data model (effects, parameters, persistence, slugs)  
6. HTTP API spec  
7. UI page specs (`/`, `/effects/[slug]`, `/settings`, `/settings/persistence`)  
8. Build pipeline & PlatformIO integration  
9. Testing strategy (frontend unit tests + minimal firmware tests)  
10. Security considerations (LAN-only assumptions, destructive actions)  
11. Milestones (thin-slice plan)  
12. CODE MAP (MANDATORY)

────────────────────────────────────────────────────────
CODE MAP REQUIREMENTS (MANDATORY)
────────────────────────────────────────────────────────
Include a CODE MAP section listing ALL relevant code locations.

For each entry:
- Path (relative to repo root)
- Key symbols (classes/functions/types/constants)
- Role (effects registry, persistence, render loop, web server, OTA, etc.)
- Why it’s relevant
- Risk notes (hot path, coupling, size constraints)

If exact locations are unknown:
- Label as ASSUMPTION
- Provide at least 2 likely candidate paths/patterns
- Provide exact ripgrep search terms to locate them

────────────────────────────────────────────────────────
HAND-WAVE GUARD (MANDATORY)
────────────────────────────────────────────────────────
If anything is uncertain:
- Label it as ASSUMPTION or OPEN QUESTION
- Provide at least 2 viable options
- Recommend one option with MCU-based reasoning

────────────────────────────────────────────────────────
ADDITIONAL CONTEXT (ASSUME TRUE)
────────────────────────────────────────────────────────
- Effects already exist and are selectable via serial (keys 1–7, shortcuts n/N s/S ESC +/- brightness)
- Effects library will outgrow numeric selection
- Performance matters: hot render loop vs cold event path

Now produce the PRE-PLAN document.

