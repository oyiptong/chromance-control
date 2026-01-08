# Chromance Effects Refactor — Design + Implementation Document (UI-ready, UI-agnostic)

Audience: maintainers of `chromance-control` firmware (portable `src/core/**` + ESP32 `src/platform/**`).

Goal: refactor the runtime “effects” system so it is ready for a future Web UI (enumerable effects + typed params + persistence + staged/sub-state controls), **without implementing any Web UI**.

Hard constraints (embedded / frame budget):
- Do **not** introduce per-frame heap allocation, string parsing, or runtime introspection.
- Prefer compile-time/static tables and fixed-size buffers.
- Any heap usage must be outside the render loop and justified.

This doc is **code-grounded**. Wherever behavior is described, the relevant code is quoted.

Repository baseline reference:
- Canonical architecture intent: `docs/architecture/wled_integration_implementation_plan.md`

---

## 1) Context & non-goals

### Context: what exists today

The runtime firmware (`src/main_runtime.cpp`) currently:
- Instantiates a fixed set of effects (modes `1..7`) as global objects.
- Selects the active effect via serial keys `1..7`.
- Routes additional serial keys (`n/N`, `s/S`, `ESC`, `+/-`) through **`main_runtime.cpp`** into effect-specific helper methods.
- Persists **global brightness** (as a “soft percent”) and the **numeric mode** (1..7) using ESP32 NVS (`Preferences`).
- Runs a deterministic scheduler (`FrameScheduler`) and calls `IEffect::render()` each frame.

Portable core (`src/core/**`) contains:
- A minimal effect interface (`IEffect`) and `EffectFrame` (time + params + signals).
- A basic `EffectRegistry` (template, max capacity), currently not used by runtime selection logic.
- Multiple effect implementations (Mode 1..7) that are Arduino-free.

### Non-goals (explicit)

This refactor design does **not**:
- Implement a web UI.
- Finalize a serial/WS protocol.
- Add a verbose logging toggle (`v`) (we only discuss global-vs-effect logging boundaries).
- Replace WLED or implement WLED integration details (out of scope here).
- Re-architect the mapping generator or mapping artifacts.

---

## 2) Current implementation (code-grounded)

This section describes the system **exactly as it exists** today.

### 2.1 Effect interface (portable core)

`IEffect` is minimal: it has an ID string, a reset hook, and a render function.

File: `src/core/effects/effect.h`

```cpp
struct EffectFrame {
  uint32_t now_ms = 0;
  uint32_t dt_ms = 0;
  EffectParams params;
  Signals signals;
};

class IEffect {
 public:
  virtual ~IEffect() = default;
  virtual const char* id() const = 0;
  virtual void reset(uint32_t now_ms) = 0;
  virtual void render(const EffectFrame& frame,
                      const PixelsMap& map,
                      Rgb* out_rgb,
                      size_t led_count) = 0;
};
```

Notes:
- There is **no** standard mechanism for:
  - staged/sub-state control,
  - parameter enumeration,
  - parameter persistence,
  - handling input events.

#### 2.1.1 Current `EffectRegistry` (portable core; not used by runtime selection)

There is already a lightweight registry template in core, used in unit tests but not currently
used by `src/main_runtime.cpp` for selection.

File: `src/core/effects/effect_registry.h`

```cpp
template <size_t MaxEffects>
class EffectRegistry {
 public:
  bool add(IEffect* effect) { ... }
  size_t count() const { return count_; }
  IEffect* at(size_t i) const { ... }
  IEffect* find(const char* id) const { ... } // strcmp on IEffect::id()
 private:
  IEffect* effects_[MaxEffects] = {};
  size_t count_ = 0;
};
```

This is a useful building block for a UI-ready catalog, but it currently:
- uses string IDs (`IEffect::id()`), and
- has no config/schema metadata attached.

### 2.2 Runtime program: where effect selection and control live

Effect objects are instantiated as globals and selected via a `switch` on numeric mode.

File: `src/main_runtime.cpp`

Instantiation:
```cpp
chromance::core::IndexWalkEffect index_walk{25};
chromance::core::StripSegmentStepperEffect strip_segment_stepper{1000};
chromance::core::CoordColorEffect coord_color;
chromance::core::RainbowPulseEffect rainbow_pulse{700, 2000, 700};
chromance::core::TwoDotsEffect two_dots{25};
chromance::core::HrvHexagonEffect hrv_hexagon;
chromance::core::BreathingEffect breathing;
chromance::core::IEffect* current_effect = &index_walk;
uint8_t current_mode = 1;
```

Selection:
```cpp
void select_mode(uint8_t mode) {
  const uint8_t safe_mode = chromance::core::ModeSetting::sanitize(mode);
  settings.set_mode(safe_mode);

  chromance::core::IEffect* effect = nullptr;
  switch (safe_mode) {
    case 1: effect = &index_walk; break;
    case 2: effect = &strip_segment_stepper; break;
    case 3: effect = &coord_color; break;
    case 4: effect = &rainbow_pulse; break;
    case 5: effect = &two_dots; break;
    case 6: effect = &hrv_hexagon; break;
    case 7: effect = &breathing; break;
    default: effect = &index_walk; break;
  }

  current_mode = safe_mode;
  current_effect = effect ? effect : &index_walk;
  current_effect->reset(millis());
  Serial.print("Mode ");
  Serial.print(static_cast<unsigned>(current_mode));
  Serial.print(": ");
  Serial.println(current_effect->id());
}
```

Why it’s structured this way (inferred but strongly supported by code):
- It’s a straightforward “demo/runtime harness” for serial-driven physical validation.
- It minimizes runtime registry/metadata overhead by hardcoding selection.

### 2.3 Serial input handling and shortcut routing

Serial key handling is centralized in the main loop and contains effect-specific branches.

File: `src/main_runtime.cpp`

```cpp
while (Serial.available() > 0) {
  const int c = Serial.read();
  if (c == '1') select_mode(1);
  ...
  if (c == 'n') {
    if (current_mode == 1) {
      if (index_walk.in_vertex_mode()) {
        index_walk.vertex_next(now_ms);
        print_index_walk_vertex_state();
      } else {
        index_walk.cycle_scan_mode(now_ms);
        print_index_walk_scan_mode();
        if (index_walk.in_vertex_mode()) print_index_walk_vertex_state();
      }
    } else if (current_mode == 2) {
      strip_segment_stepper.next(now_ms);
      strip_segment_stepper.set_auto_advance_enabled(false, now_ms);
      print_strip_segment_stepper_state();
    } else if (current_mode == 6) {
      hrv_hexagon.next(now_ms);
    } else if (current_mode == 7) {
      breathing.next_phase(now_ms);
    }
  }
  if (c == 'N') { /* similar mode branching */ }
  if (c == 's') {
    if (current_mode == 7) breathing.lane_next(now_ms);
    else if (current_mode == 1) index_walk.step_hold_next(now_ms);
  }
  if (c == 27) {  // ESC
    if (current_mode == 1) index_walk.set_auto(now_ms);
    else if (current_mode == 2) strip_segment_stepper.set_auto_advance_enabled(true, now_ms);
    else if (current_mode == 6) hrv_hexagon.set_auto(now_ms);
    else if (current_mode == 7) breathing.set_auto(now_ms);
  }
  if (c == '+') set_brightness_percent(brightness_step_up_10(settings.brightness_percent()));
  if (c == '-') set_brightness_percent(brightness_step_down_10(settings.brightness_percent()));
}
```

Observations:
- The “command routing” logic lives in `main_runtime.cpp`, not in the effect implementations.
- Some shortcuts are **global** (`1..7`, `+/-`), but many are **mode-scoped** (`n/N/s/S/ESC`) and handled via `if (current_mode == X)`.
- This is exactly the structure motivating a refactor toward UI-driven command routing (serial today, web later).

### 2.4 Frame scheduling and render loop

The runtime chooses a frame interval (in ms) based on the current effect and uses `FrameScheduler` to decide when to render.

File: `src/main_runtime.cpp`

```cpp
uint32_t frame_ms = ota.is_updating() ? 100 : 20;
if (current_effect == &hrv_hexagon) frame_ms = 16;
if (current_effect == &breathing) frame_ms = 16;
scheduler.set_target_fps(frame_ms ? static_cast<uint16_t>(1000U / frame_ms) : 0);
if (!scheduler.should_render(now_ms)) return;

chromance::core::EffectFrame frame;
frame.now_ms = now_ms;
frame.dt_ms = scheduler.dt_ms();
frame.signals = signals;
frame.params = params;
current_effect->render(frame, pixels_map, rgb, kLedCount);
led_out.show(rgb, kLedCount, &stats);
```

This establishes an important design constraint for the refactor:
- The render loop is already lean and must remain lean.
- Any new “UI ready metadata” must not introduce per-frame overhead (no reflection, no parsing, no allocations).

### 2.5 Brightness control and persistence

Brightness is treated as a *persisted setting* in NVS and also influences `EffectParams::brightness`.

Where it is applied:
File: `src/main_runtime.cpp`

```cpp
params.brightness = chromance::core::soft_percent_to_u8_255(
    settings.brightness_percent(), chromance::core::kHardwareBrightnessCeilingPercent);
```

Where it is persisted:
File: `src/platform/settings.cpp`

```cpp
constexpr char kNamespace[] = "chromance";
constexpr char kBrightnessKey[] = "bright_pct";

void RuntimeSettings::begin() {
  prefs.begin(kNamespace, false);
  PreferencesStore store(&prefs);
  brightness_.begin(store, kBrightnessKey, 100);
}

void RuntimeSettings::set_brightness_percent(uint8_t percent) {
  PreferencesStore store(&prefs);
  brightness_.set_percent(store, kBrightnessKey, percent);
}
```

Quantization/wear mitigation:
File: `src/core/settings/brightness_setting.h`

```cpp
percent_ = quantize_percent_to_10(raw);
(void)store.write_u8(key, percent_);
```

Observations:
- Persistence is immediate on every brightness change (a write per `+/-`).
- Values are quantized to 10% steps, reducing write variety but not write frequency.
- There is no persistence debounce/batching yet.

### 2.6 Mode (effect selection) persistence

Mode is persisted as an integer `1..7`.

File: `src/core/settings/mode_setting.h`

```cpp
static uint8_t sanitize(uint8_t mode) {
  if (mode < 1) return 1;
  if (mode > 7) return 1;
  return mode;
}
```

File: `src/platform/settings.cpp`

```cpp
constexpr char kModeKey[] = "mode";
mode_.begin(store, kModeKey, 1);
...
mode_.set_mode(store, kModeKey, mode);
```

Observations:
- This is “safe by default” (invalid → 1) but tightly couples persistence to numeric modes.
- A UI-ready catalog needs stable IDs beyond `1..7`.

### 2.7 Staged/sub-state behavior today (examples)

There is no unified stage interface; stages exist as ad-hoc methods per effect and are invoked by `main_runtime.cpp`.

Example A — Mode 2 stepper:
File: `src/core/effects/pattern_strip_segment_stepper.h`

```cpp
void next(uint32_t now_ms) { segment_number_ = (segment_number_ % 12U) + 1U; }
void prev(uint32_t now_ms) { segment_number_ = segment_number_ <= 1 ? 12 : (segment_number_ - 1U); }
void set_auto_advance_enabled(bool enabled, uint32_t now_ms) { auto_advance_enabled_ = enabled; }
```

Example B — Mode 6 manual hex stepping:
File: `src/core/effects/pattern_hrv_hexagon.h`

```cpp
void set_auto(uint32_t now_ms) { manual_enabled_ = false; cycle_start_ms_ = now_ms; ... }
void next(uint32_t now_ms) { step_manual(+1, now_ms); }
void prev(uint32_t now_ms) { step_manual(-1, now_ms); }
```

Example C — Mode 1 (Index walk) has multiple scan modes + manual hold:
File: `src/core/effects/pattern_index_walk.h`

```cpp
enum class ScanMode : uint8_t { kIndex, kTopoLtrUtd, kTopoRtlDtu, kVertexToward };
void cycle_scan_mode(uint32_t now_ms) { ... }
void step_hold_next(uint32_t now_ms) { step_hold(+1, now_ms); }
void vertex_next(uint32_t now_ms) { if (scan_mode_ != kVertexToward) return; vertex_manual_ = true; ... }
```

Takeaway:
- The refactor should **not** remove stages; it should standardize them behind a common interface.

### 2.8 Current control flow diagram

```mermaid
flowchart TD
  A[setup()] --> B[init LED output, OTA, settings, scheduler]
  B --> C[select_mode(settings.mode())]
  C --> D[loop()]
  D --> E[ota.handle()]
  E --> F[read Serial bytes]
  F --> G[global commands: 1..7, +/-]
  F --> H[mode-scoped: n/N/s/S/ESC routed in main]
  H --> I[update scheduler target_fps]
  I --> J[scheduler.should_render()]
  J -->|false| D
  J -->|true| K[build EffectFrame]
  K --> L[current_effect->render()]
  L --> M[led_out.show()]
  M --> N[optional per-effect logging]
  N --> D
```

### 2.9 Searches performed (to confirm “where things live”)

These searches were used to locate code paths described above:

```bash
rg -n "void select_mode\\(" src/main_runtime.cpp
rg -n "while \\(Serial\\.available\\(\\)" src/main_runtime.cpp
rg -n "set_brightness_percent\\(" src/main_runtime.cpp
rg -n "class RuntimeSettings" -S src/platform
rg -n "ModeSetting" -S src/core/settings
rg -n "BrightnessSetting" -S src/core/settings
```

---

## 3) Problems & motivation for refactor (tied to code)

1) **Numeric effect selection (1..7) does not scale**
- Persisted mode is hard-bounded to 7: `ModeSetting::sanitize()` in `src/core/settings/mode_setting.h`.
- Runtime selection is a hardcoded `switch` in `src/main_runtime.cpp`.

2) **Control logic is centralized and growing**
- The serial routing block in `src/main_runtime.cpp` contains nested per-mode branches.
- Adding a new effect (or a new stage) requires editing main and remembering global semantics (`ESC`, holds, etc).

3) **Global vs effect-specific commands are unclear**
- Example: `n` means “cycle scan mode” in Mode 1, but “advance k” in Mode 2, “next hex” in Mode 6, “next phase” in Mode 7.
- This works for a human at a serial terminal, but a web UI needs a discoverable and routable command model.

4) **No typed, enumerable effect parameters**
- `EffectParams` exists (`brightness/speed/intensity/palette`) but it’s not effect-discoverable, not persisted per effect, and not UI-enumerable.

5) **Persistence is “global-only”**
- Current persistence covers brightness percent and numeric mode only (`src/platform/settings.cpp`).
- Effect-specific configuration (e.g., breathing dot count/center vertex, hexagon timing, etc) is not persisted.

6) **Logging is tightly coupled to runtime main**
- Some prints happen in main (e.g., “banner” output, mode changes).
- A UI-ready system will need a consistent “telemetry” concept (effect state summary) without mixing it into the render loop.

---

## 4) Target architecture overview (UI-ready, UI-agnostic)

### Design principles
- Keep `src/core/**` Arduino-free and testable in `pio test -e native`.
- Render path must remain:
  - allocation-free,
  - parsing-free,
  - O(LED_COUNT) for pixel loops + O(1) control overhead per frame.
- Make “UI-ready metadata” retrievable **on demand** (on events / UI requests), not per-frame.

### Proposed core components

1) **Effect Catalog / Registry**
- Stable `EffectId` (numeric) + stable `slug` (string) + display name (string).
- Enumerability for UI and serial control (list effects, select by id/slug).

2) **Effect Interface (v2)**
- Separate **runtime state** from **persisted configuration**.
- Add an **event handling** entry point so control logic can live with the effect, not in `main_runtime.cpp`.
- Add optional stage/sub-state enumeration.

3) **Configuration Model**
- Each effect exposes a lightweight `EffectConfigSchema` (static descriptors).
- Parameter values live in a packed `EffectConfig` struct (per effect).
- Parameters can be set via:
  - serial shortcuts (current),
  - future web UI (later).

4) **Persistence Layer**
- Platform-owned store (ESP32 Preferences/NVS), accessed via an abstract interface in core.
- Debounced/batched writes to reduce wear.
- Versioning + migration support.

5) **Command Routing**
- A small router maps “global” commands to system actions (select effect, brightness).
- Effect-scoped commands are delegated to the active effect via `on_event()` or `handle_command()`.

### Proposed architecture diagram

```mermaid
flowchart LR
  subgraph platform["src/platform/** (Arduino/ESP32)"]
    SerialIn[Serial input]
    Store[Preferences/NVS store]
    LedOut[DotStar output]
    Ota[OTA/WiFi]
  end

  subgraph core["src/core/** (portable)"]
    Manager[EffectManager\n(active effect + configs)]
    Router[CommandRouter\n(global vs effect)]
    Catalog[EffectCatalog\n(descriptors + factory)]
    Effect[IEffectV2]
    Schema[EffectConfigSchema\n(ParamDescriptor[])]
  end

  SerialIn --> Router
  Router -->|global| Manager
  Router -->|effect-scoped| Effect
  Catalog --> Manager
  Store <-->|load/save| Manager
  Manager --> Effect
  Effect -->|render| LedOut
  Ota --> Manager
```

---

## 5) Detailed APIs (implementation-grade)

This is an implementable plan; exact filenames are proposed under `$ROOT`.

### 5.1 Identifiers and descriptors

**Effect IDs**
- Use a small integer for stable identity: `uint16_t` is enough.
- Keep the current numeric modes as aliases during migration (see §6.4).

```cpp
// src/core/effects/effect_id.h
namespace chromance::core {
struct EffectId {
  uint16_t value = 0;
  constexpr bool valid() const { return value != 0; }
};
}
```

**EffectDescriptor**

```cpp
// src/core/effects/effect_descriptor.h
namespace chromance::core {
struct EffectDescriptor {
  EffectId id;
  const char* slug;        // "breathing", "index_walk", ...
  const char* display_name;// "Breathing", ...
  const char* description; // optional (can be nullptr in minimal build)
};
}
```

### 5.2 Param descriptors and schema

Minimal, compile-time descriptor tables:

```cpp
// src/core/effects/params.h
namespace chromance::core {
enum class ParamType : uint8_t { U8, I16, U16, Bool, Enum, ColorRgb };

struct ParamId { uint16_t value = 0; };

struct ParamDescriptor {
  ParamId id;
  const char* name;         // "dot_count"
  const char* display_name; // "Dot Count"
  ParamType type;
  // storage mapping:
  uint16_t offset;          // byte offset into effect's config struct
  uint8_t size;             // bytes (1/2/4)

  // validation / UI hints:
  int32_t min;
  int32_t max;
  int32_t step;
  int32_t def;
};

struct EffectConfigSchema {
  const ParamDescriptor* params;
  uint8_t param_count;
};
}
```

**ParamValue (typed value container)**

This is the value type used by control surfaces (serial/web) to set/get parameters without
knowing the underlying config struct layout.

```cpp
// src/core/effects/params.h
namespace chromance::core {

struct ParamValue {
  ParamType type = ParamType::U8;
  union {
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    bool b;
    uint8_t e;      // enum as u8
    Rgb color_rgb;  // 3 bytes
  } v{};
};

}
```

Notes:
- Using `offset` avoids maps/dictionaries and keeps set/get O(1).
- `name` strings can live in flash; in a “minimal footprint” build, `display_name/description` can be compiled out.

### 5.3 Effect interface (v2)

```cpp
// src/core/effects/effect_v2.h
namespace chromance::core {

struct EffectContext {
  uint32_t now_ms;
  uint32_t dt_ms;
  const PixelsMap& map;
  const MappingTables& mapping; // or static access, depending on current design
  EffectParams global_params;   // includes brightness
  Signals signals;

  // Side channels (no Arduino types; implemented by platform layer):
  class ISettingsStore* store;  // persisted config load/save
  class ILogger* logger;        // optional
};

enum class Key : uint8_t { Digit1, Digit2, N, ShiftN, S, ShiftS, Esc, Plus, Minus };
struct InputEvent { Key key; uint32_t now_ms; };

struct StageId { uint8_t value = 0; };
struct StageDescriptor { StageId id; const char* name; const char* display_name; };

class IEffectV2 {
 public:
  virtual ~IEffectV2() = default;

  virtual const EffectDescriptor& descriptor() const = 0;
  virtual const EffectConfigSchema* schema() const = 0; // nullptr if no params

  // Called when this effect becomes active.
  virtual void start(const EffectContext& ctx) = 0;

  // Called when leaving the effect (optional).
  virtual void stop(const EffectContext& ctx) { (void)ctx; }

  // Reset runtime state (not persisted config).
  virtual void reset_runtime(const EffectContext& ctx) = 0;

  // Handle discrete input events (serial, web UI later).
  virtual void on_event(const InputEvent& ev, EffectContext& ctx) { (void)ev; (void)ctx; }

  // Optional stage support.
  virtual uint8_t stage_count() const { return 0; }
  virtual const StageDescriptor* stage_at(uint8_t i) const { (void)i; return nullptr; }
  virtual StageId current_stage() const { return StageId{0}; }
  virtual bool enter_stage(StageId id, const EffectContext& ctx) { (void)id; (void)ctx; return false; }

  // Render always uses current runtime + config; must be allocation-free.
  virtual void render(const EffectContext& ctx, Rgb* out_rgb, size_t led_count) = 0;
};

} // namespace chromance::core
```

Migration note:
- Current effects already implement `reset(now_ms)` and `render(frame,map,...)`.
- We can wrap them behind an adapter in the interim (see §6.4).

### 5.4 EffectManager (single owner of active effect + persistence glue)

Responsibilities:
- hold the active effect instance (or pointer),
- hold persisted configs for each effect,
- manage “dirty” flags and persistence debounce,
- provide “restart effect” behavior consistently,
- keep render loop allocation-free.

Proposed API:

```cpp
// src/core/effects/effect_manager.h
namespace chromance::core {

class EffectManager {
 public:
  void init(ISettingsStore& store, const EffectCatalog& catalog);

  bool set_active(EffectId id, uint32_t now_ms);  // persists active id (debounced)
  EffectId active_id() const;
  IEffectV2& active();

  void on_event(const InputEvent& ev, uint32_t now_ms);
  void tick(uint32_t now_ms, uint32_t dt_ms, const Signals& signals);
  void render(Rgb* out, size_t n) const;

  // Param set/get (for serial now, web UI later).
  bool set_param(EffectId id, ParamId pid, const ParamValue& v);
  bool get_param(EffectId id, ParamId pid, ParamValue* out) const;

 private:
  // fixed-size storage for config blobs + dirty flags
};

}
```

#### 5.4.1 EffectCatalog vs EffectRegistry (recommended naming)

To align with existing code, you can either:

- **Option A (recommended)**: keep a “catalog” type that combines:
  - descriptors (id/slug/name),
  - factories or pointers,
  - schema access,
  - and optionally grouping/tags.

- **Option B**: evolve the existing `EffectRegistry<MaxEffects>` into:
  - `EffectRegistry<MaxEffects, IEffectV2>` keyed by `EffectId`,
  - storing `EffectDescriptor` alongside the pointer.

Example “registry v2” shape (compile-time capacity, no heap):

```cpp
// src/core/effects/effect_registry_v2.h (proposed)
template <size_t MaxEffects>
class EffectRegistryV2 {
 public:
  bool add(const EffectDescriptor& d, IEffectV2* e);
  size_t count() const;
  const EffectDescriptor* descriptor_at(size_t i) const;
  IEffectV2* effect_at(size_t i) const;
  IEffectV2* find_by_id(EffectId id) const;
  IEffectV2* find_by_slug(const char* slug) const; // optional (strcmp), used only on UI/serial commands
 private:
  struct Entry { EffectDescriptor d; IEffectV2* e; };
  Entry entries_[MaxEffects] = {};
  size_t count_ = 0;
};
```

### 5.5 Sub-stage design (recommended + alternative)

Recommended:
- Stages are effect-defined and enumerated via `stage_count/stage_at/current_stage/enter_stage`.
- Stages are not a global enum; only meaningful within an effect.
- Shortcut keys (`n/N/ESC`) become *default bindings* that an effect may override.

Alternative:
- Global `StageEvent` model:
  - `StageEvent::Next`, `StageEvent::Prev`, `StageEvent::Auto`, `StageEvent::Restart`
  - Each effect optionally implements `on_stage_event(StageEvent)`.
- This reduces UI complexity but is less expressive than named stages.

### 5.6 Restart semantics

Define two distinct operations:
- **Restart effect**: reset runtime state to “start” but keep persisted config.
- **Factory reset effect config**: reset config to schema defaults and persist.

Current behavior in runtime:
- switching modes calls `reset(millis())` on the new effect and clears some main-local state.

Refactor target:
- `EffectManager::set_active()` triggers `active.start(...)`.
- `EffectManager::restart_active()` triggers `active.reset_runtime(...)`.

### 5.7 Brightness integration

Recommendation:
- Keep brightness as a **global** setting (current design), persisted in `RuntimeSettings`.
- Effects read it via `EffectContext.global_params.brightness` and apply it as they do today.

If a future effect needs local brightness:
- Model it as a per-effect parameter (e.g., “local_intensity”) that multiplies with global brightness.
- Conflict resolution rule: effective = global * local (clamped).

### 5.8 Proposed file layout (no implementation in this doc)

New/updated core files (portable):
- `src/core/effects/effect_v2.h`
- `src/core/effects/effect_id.h`
- `src/core/effects/effect_descriptor.h`
- `src/core/effects/params.h`
- `src/core/effects/effect_catalog.h`
- `src/core/effects/effect_manager.h`
- `src/core/effects/command_router.h`
- `src/core/settings/effect_config_store.h` (interfaces only)

Platform files (ESP32-only):
- `src/platform/effect_config_store_preferences.h/.cpp` (Preferences-backed store)

---

## 6) Persistence design (implementation-grade)

### 6.1 Storage mechanism

Use ESP32 NVS (`Preferences`) as already used for:
- `bright_pct`
- `mode`

Rationale:
- Proven working in current repo (`src/platform/settings.cpp`).
- Suitable for small config payloads.

### 6.2 Key constraints (important)

ESP32 NVS keys are limited in length (commonly 15 chars). The current keys (`bright_pct`, `mode`) are short.

Therefore, per-effect key naming must be compact. Two viable options:

**Option A (recommended): store per-effect config as a single blob**
- Key per effect: e.g. `e01` / `e02` / `e0A` (hex id).
- Value: fixed-size packed struct bytes.
- Pros: few keys; easy debounce; future schema versioning inside blob.
- Cons: needs `putBytes/getBytes` support in store wrapper (not present in current `IKeyValueStore` which is u8-only).

**Option B: store per-param u8/u16 keys**
- Keys like `e01p03` (still short).
- Pros: easier partial updates.
- Cons: more keys, more writes, higher wear and more complex migration.

Given the “UI ready” goal and wear concerns, **Option A** is preferred.

### 6.3 Versioning & migration

Define:
- Global config schema version: `cfg_v` (u8).
- Per-effect config version stored in the blob header:
  - `{ uint8_t version; uint8_t payload[...]; }`

Migration behavior:
- If global version mismatch:
  - run a migration function that reads old blobs and writes new blobs (debounced / one-time).
- If per-effect blob missing:
  - use schema defaults and write once.
- If blob present but wrong size/version:
  - attempt migration; if not possible, reset that effect to defaults (safe fallback).

### 6.4 Transition from numeric `mode` to catalog `EffectId`

Current: persisted `mode` is `1..7`.

Migration plan:
- Introduce a persisted `active_effect_id` (`aeid`, u16) **alongside** `mode`.
- On boot:
  1) if `aeid` exists and is valid → use it,
  2) else fall back to `mode` mapping table `{1->index_walk, 2->strip_stepper, ...}`,
  3) write `aeid` for future boots.
- Keep writing `mode` during transition for compatibility.

### 6.5 Persistence timing / wear mitigation

Requirements:
- No write per frame.
- Avoid write-per-keystroke when rapidly adjusting.

Recommended policy:
- Mark dirty on config change.
- Debounce: write if no changes for `>= 500ms` OR on explicit “save” command (later).
- Additionally: write on effect change (so config is not lost).

Pseudo:
```cpp
if (dirty && (now_ms - last_change_ms) > 500) {
  store.write_blob(key, bytes, size);
  dirty = false;
}
```

Failure behavior:
- If a write fails, keep dirty and retry with backoff.
- If reads fail or data corrupt, fall back to defaults (safe mode).

---

## 7) Shortcut & serial streamlining plan

### 7.1 How shortcuts work today (code-grounded)

Today, shortcuts are “flat bytes” read from Serial and routed in `src/main_runtime.cpp` with `if (current_mode == X)` branching (see §2.3).

Key semantics (current):
- `1..7`: select mode (global)
- `+/-`: brightness percent step (global)
- `n/N`: mode-scoped next/prev (varies by effect)
- `s/S`: mode-scoped stepping (varies)
- `ESC`: mode-scoped “return to auto”

### 7.2 Proposed refactor: mapping layer + event routing

Introduce:
- `CommandRouter` that converts raw serial bytes into `InputEvent`s and/or higher-level commands.
- A global keymap for “system” commands (select effect, brightness).
- Effect-scoped key handling via `IEffectV2::on_event()`.

Serial protocol direction (examples only; do NOT implement here):
- `effect list`
- `effect set breathing`
- `effect restart`
- `param list`
- `param set breathing.dot_count 9`
- `stage list`
- `stage enter inhale`

This aligns with the goal: serial as a control surface now, web UI later.

---

## 8) Embedded performance & memory tradeoffs (required)

Assumptions (since exact MCU specs aren’t listed in this doc):
- Target board: ESP32 Feather (confirmed via `platformio.ini` envs).
- RAM: ~320KB; Flash: 4MB (board default), but app partitions constrain usable flash.

### 8.1 Component cost table (qualitative + approximate)

| Component | RAM cost | Flash cost | CPU per frame | Notes |
|---|---:|---:|---:|---|
| Effect render loops | O(LED_COUNT) | existing | dominant | unchanged |
| Event routing (`on_event`) | O(1) | small | O(1) per event | event-driven only |
| Static param descriptors | ~0 RAM (flash const) | medium | 0 per frame | only accessed on UI/serial queries |
| Config structs (per effect) | small | small | 0 per frame | accessed by direct struct fields |
| Persistence debounce | a few bytes | small | O(1) per tick | writes are infrequent |
| Registry/catalog | small | small/medium | 0 per frame | selection is event-driven |

### 8.2 Two schema storage approaches

**Approach 1 (recommended): compile-time descriptors**
- Each effect compiles a `static constexpr ParamDescriptor[]`.
- Config values live in a packed struct in RAM.
- Pros:
  - zero heap,
  - minimal CPU overhead,
  - fully deterministic,
  - easy to expose to UI by serializing descriptors on demand.
- Cons:
  - adding/changing params requires firmware update.

**Approach 2: runtime/dynamic descriptors**
- Build descriptors at runtime (e.g., vectors, strings).
- Pros:
  - more flexible (could load effect schemas externally).
- Cons:
  - risks heap churn/fragmentation,
  - larger code and RAM footprint,
  - harder to keep render loop clean.

Recommendation: **Approach 1**.

### 8.3 String storage strategies

To keep footprint small:
- Use short `slug` strings as stable identifiers (ASCII, lowercase).
- Keep `display_name/description` optional behind a compile flag.
- Prefer storing `const char*` in flash (ESP32 places `const` in flash by default).
- In “minimal footprint mode”, omit descriptions and long names entirely.

### 8.4 Hotspots and mitigations

Potential hotspots:
- Persistence writes when rapidly changing params (wear + latency).
  - Mitigation: debounce + batch.
- Serial parsing of textual commands (if added later).
  - Mitigation: keep current single-char shortcuts; if textual protocol is added later, parse outside render loop and keep it optional.
- Parameter get/set via offsets.
  - Mitigation: validate param type/size; keep everything O(1); avoid string lookup in the render path.

---

## Appendix: “Move control logic into effects” — how this doc supports that

The current pain point is `main_runtime.cpp` needing to know every effect’s internal stage rules.

This design moves that knowledge into:
- `IEffectV2::on_event()` for serial/web input handling,
- `IEffectV2` stage methods for UI-driven stage control,
- `EffectManager` for shared concerns (selection, persistence, restart).

`main_runtime.cpp` becomes a thin adapter that:
- reads Serial and creates events,
- calls `manager.on_event(...)`,
- runs the scheduler and calls `manager.render(...)`,
- owns platform services (OTA, Preferences, LED output).
