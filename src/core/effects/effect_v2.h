#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../mapping/pixels_map.h"
#include "../settings/effect_config_store.h"
#include "effect_descriptor.h"
#include "effect_params.h"
#include "params.h"
#include "signals.h"

namespace chromance {
namespace core {

class ILogger;  // optional platform-owned logger (forward-declared)

// Render-time context: hot path, per-frame, must be allocation-free and side-effect-free.
struct RenderContext {
  uint32_t now_ms = 0;
  uint32_t dt_ms = 0;
  const PixelsMap* map = nullptr;
  EffectParams global_params;
  Signals signals;
};

// Event-time context: cold path only (serial input, UI actions, persistence).
// Must NOT be passed into render; effects must not do persistence/logging from render().
struct EventContext {
  uint32_t now_ms = 0;
  const PixelsMap* map = nullptr;
  EffectParams global_params;
  Signals signals;
  ISettingsStore* store = nullptr;  // persisted config load/save (manager-owned policy)
  ILogger* logger = nullptr;        // optional
};

// Key routing note:
// - System/global keys (handled by the runtime/controller): effect selection, global brightness, global restart.
// - Effect-scoped keys (forwarded to the active effect): stage stepping, effect-local toggles, etc.
enum class Key : uint8_t { Digit1, Digit2, N, ShiftN, S, ShiftS, Esc, Plus, Minus };

struct InputEvent {
  Key key;
  uint32_t now_ms = 0;
};

struct StageId {
  uint8_t value;

  constexpr StageId() : value(0) {}
  constexpr explicit StageId(uint8_t v) : value(v) {}
};

struct StageDescriptor {
  StageId id;
  const char* name;
  const char* display_name;
};

class IEffectV2 {
 public:
  virtual ~IEffectV2() = default;

  virtual const EffectDescriptor& descriptor() const = 0;
  virtual const EffectConfigSchema* schema() const = 0;  // nullptr if no params

  // Provide a stable config storage pointer (owned by EffectManager).
  // This is cold-path only; effects may cache a typed pointer for render().
  virtual void bind_config(const void* config_bytes, size_t config_size) {
    (void)config_bytes;
    (void)config_size;
  }

  // Called when this effect becomes active.
  virtual void start(const EventContext& ctx) = 0;

  // Called when leaving the effect (optional).
  virtual void stop(const EventContext& ctx) { (void)ctx; }

  // Reset runtime state (not persisted config).
  virtual void reset_runtime(const EventContext& ctx) = 0;

  // Handle discrete input events (serial, web UI later).
  virtual void on_event(const InputEvent& ev, const EventContext& ctx) {
    (void)ev;
    (void)ctx;
  }

  // Optional stage support.
  virtual uint8_t stage_count() const { return 0; }
  virtual const StageDescriptor* stage_at(uint8_t i) const {
    (void)i;
    return nullptr;
  }
  virtual StageId current_stage() const { return StageId(); }
  virtual bool enter_stage(StageId id, const EventContext& ctx) {
    (void)id;
    (void)ctx;
    return false;
  }

  // Render always uses current runtime + config; must be allocation-free.
  virtual void render(const RenderContext& ctx, Rgb* out_rgb, size_t led_count) = 0;
};

}  // namespace core
}  // namespace chromance
