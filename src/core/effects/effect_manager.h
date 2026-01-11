#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../settings/effect_config_store.h"
#include "effect_catalog.h"

namespace chromance {
namespace core {

template <size_t MaxEffects>
class EffectManager final {
 public:
  void init(ISettingsStore& store, const EffectCatalog<MaxEffects>& catalog, const PixelsMap& map,
            uint32_t now_ms, EffectId fallback_active_id = EffectId{}) {
    store_ = &store;
    catalog_ = &catalog;
    map_ = &map;
    now_ms_ = now_ms;

    for (size_t i = 0; i < MaxEffects; ++i) {
      memset(configs_[i].bytes, 0, sizeof(configs_[i].bytes));
      configs_[i].dirty = false;
      configs_[i].last_change_ms = 0;
      configs_[i].next_write_due_ms = 0;
      configs_[i].backoff_ms = 0;
    }

    // Initialize each effect config (defaults, then load persisted blob if present).
    for (size_t i = 0; i < catalog_->count(); ++i) {
      const EffectDescriptor* d = catalog_->descriptor_at(i);
      IEffectV2* e = catalog_->effect_at(i);
      if (d == nullptr || e == nullptr) {
        continue;
      }

      const EffectConfigSchema* schema = e->schema();
      const bool has_schema = (schema != nullptr && schema->params != nullptr && schema->param_count > 0);
      if (has_schema) {
        reset_config_bytes_to_defaults(i);

        char key[8] = {};
        if (make_effect_key(d->id, key, sizeof(key))) {
          if (store_->read_blob(key, configs_[i].bytes, kMaxEffectConfigSize)) {
            // Loaded persisted config bytes.
          } else {
            // Missing/corrupt: defaults will be used; persist once (debounced).
            mark_dirty(i, now_ms_);
          }
        }
      } else {
        // Effects without a schema have no persisted per-effect config.
        memset(configs_[i].bytes, 0, kMaxEffectConfigSize);
        configs_[i].dirty = false;
      }

      e->bind_config(configs_[i].bytes, kMaxEffectConfigSize);
    }

    // Restore active effect id if present; else default to first catalog entry.
    EffectId id = EffectId{0};
    uint16_t stored_id = 0;
    if (store_->read_blob(kActiveEffectKey, &stored_id, sizeof(stored_id))) {
      id = EffectId{stored_id};
    }
    if (!id.valid() || catalog_->find_by_id(id) == nullptr) {
      if (fallback_active_id.valid() && catalog_->find_by_id(fallback_active_id) != nullptr) {
        id = fallback_active_id;
      }
    }
    if (!id.valid() || catalog_->find_by_id(id) == nullptr) {
      const EffectDescriptor* first = catalog_->descriptor_at(0);
      id = first ? first->id : EffectId{0};
    }

    (void)set_active(id, now_ms_);
  }

  void set_global_params(const EffectParams& params) { global_params_ = params; }

  EffectId active_id() const { return active_id_; }
  IEffectV2* active() const { return active_effect_; }

  bool set_active(EffectId id, uint32_t now_ms) {
    if (catalog_ == nullptr || store_ == nullptr || map_ == nullptr || !id.valid()) {
      return false;
    }
    IEffectV2* next = catalog_->find_by_id(id);
    if (next == nullptr) {
      return false;
    }

    now_ms_ = now_ms;

    // Best effort: persist the old active config on effect change to reduce loss on reboot.
    const int old_idx = find_index(active_id_);
    if (old_idx >= 0) {
      try_persist_config_now(static_cast<size_t>(old_idx), now_ms_);
    }

    if (active_effect_ != nullptr && active_id_.valid()) {
      EventContext ctx = make_event_context(now_ms_);
      active_effect_->stop(ctx);
    }

    active_effect_ = next;
    active_id_ = id;

    // Persist active id immediately (best effort, with retry/backoff policy).
    persist_active_id_now(now_ms_);

    EventContext ctx = make_event_context(now_ms_);
    active_effect_->start(ctx);
    return true;
  }

  void restart_active(uint32_t now_ms) {
    if (active_effect_ == nullptr) {
      return;
    }
    now_ms_ = now_ms;
    EventContext ctx = make_event_context(now_ms_);
    active_effect_->reset_runtime(ctx);
  }

  bool reset_config_to_defaults(EffectId id, uint32_t now_ms) {
    const int idx = find_index(id);
    if (idx < 0) {
      return false;
    }
    IEffectV2* e = catalog_ ? catalog_->effect_at(static_cast<size_t>(idx)) : nullptr;
    const EffectConfigSchema* schema = e ? e->schema() : nullptr;
    if (schema == nullptr || schema->params == nullptr || schema->param_count == 0) {
      return false;
    }
    now_ms_ = now_ms;
    reset_config_bytes_to_defaults(static_cast<size_t>(idx));
    if (e != nullptr) {
      e->bind_config(configs_[idx].bytes, kMaxEffectConfigSize);
    }
    mark_dirty(static_cast<size_t>(idx), now_ms_);
    if (id == active_id_) {
      restart_active(now_ms_);
    }
    return true;
  }

  void on_event(const InputEvent& ev, uint32_t now_ms) {
    if (active_effect_ == nullptr) {
      return;
    }
    now_ms_ = now_ms;
    EventContext ctx = make_event_context(now_ms_);
    active_effect_->on_event(ev, ctx);
  }

  void tick(uint32_t now_ms, uint32_t dt_ms, const Signals& signals) {
    now_ms_ = now_ms;
    dt_ms_ = dt_ms;
    signals_ = signals;
    flush_persist_due(now_ms_, /*force=*/false);
  }

  void render(Rgb* out, size_t n) const {
    if (out == nullptr || n == 0) {
      return;
    }
    if (active_effect_ == nullptr || map_ == nullptr) {
      for (size_t i = 0; i < n; ++i) {
        out[i] = kBlack;
      }
      return;
    }
    RenderContext ctx;
    ctx.now_ms = now_ms_;
    ctx.dt_ms = dt_ms_;
    ctx.map = map_;
    ctx.global_params = global_params_;
    ctx.signals = signals_;
    active_effect_->render(ctx, out, n);
  }

  bool set_param(EffectId id, ParamId pid, const ParamValue& v) {
    const int idx = find_index(id);
    if (idx < 0 || pid.value == 0) {
      return false;
    }
    IEffectV2* e = catalog_ ? catalog_->effect_at(static_cast<size_t>(idx)) : nullptr;
    const EffectConfigSchema* schema = e ? e->schema() : nullptr;
    if (schema == nullptr || schema->params == nullptr || schema->param_count == 0) {
      return false;
    }

    const ParamDescriptor* d = find_param(*schema, pid);
    if (d == nullptr) {
      return false;
    }
    if (!validate_descriptor(*d)) {
      return false;
    }
    if (!param_value_type_matches(*d, v.type)) {
      return false;
    }
    if (!apply_param_value(*d, v, configs_[idx].bytes, kMaxEffectConfigSize)) {
      return false;
    }

    if (e != nullptr) {
      e->bind_config(configs_[idx].bytes, kMaxEffectConfigSize);
    }
    mark_dirty(static_cast<size_t>(idx), now_ms_);
    return true;
  }

  bool get_param(EffectId id, ParamId pid, ParamValue* out) const {
    if (out == nullptr) {
      return false;
    }
    const int idx = find_index(id);
    if (idx < 0 || pid.value == 0) {
      return false;
    }
    IEffectV2* e = catalog_ ? catalog_->effect_at(static_cast<size_t>(idx)) : nullptr;
    const EffectConfigSchema* schema = e ? e->schema() : nullptr;
    if (schema == nullptr || schema->params == nullptr || schema->param_count == 0) {
      return false;
    }
    const ParamDescriptor* d = find_param(*schema, pid);
    if (d == nullptr || !validate_descriptor(*d)) {
      return false;
    }
    return read_param_value(*d, configs_[idx].bytes, kMaxEffectConfigSize, out);
  }

 private:
  struct ConfigState {
    alignas(4) uint8_t bytes[kMaxEffectConfigSize];
    bool dirty = false;
    uint32_t last_change_ms = 0;
    uint32_t next_write_due_ms = 0;
    uint32_t backoff_ms = 0;
  };

  static constexpr uint32_t kDebounceMs = 500;
  static constexpr uint32_t kMaxBackoffMs = 4000;
  static constexpr const char* kActiveEffectKey = "aeid";

  ISettingsStore* store_ = nullptr;
  const EffectCatalog<MaxEffects>* catalog_ = nullptr;
  const PixelsMap* map_ = nullptr;

  EffectParams global_params_{};
  Signals signals_{};

  uint32_t now_ms_ = 0;
  uint32_t dt_ms_ = 0;

  EffectId active_id_{0};
  IEffectV2* active_effect_ = nullptr;

  bool active_dirty_ = false;
  uint32_t active_last_change_ms_ = 0;
  uint32_t active_next_write_due_ms_ = 0;
  uint32_t active_backoff_ms_ = 0;

  ConfigState configs_[MaxEffects] = {};

  static char hex_digit(uint8_t v) { return v < 10 ? static_cast<char>('0' + v) : static_cast<char>('A' + (v - 10)); }

  static bool make_effect_key(EffectId id, char* out, size_t out_size) {
    if (!id.valid() || out == nullptr || out_size < 6) {  // "eFFFF\0"
      return false;
    }
    out[0] = 'e';
    out[1] = hex_digit(static_cast<uint8_t>((id.value >> 12) & 0xF));
    out[2] = hex_digit(static_cast<uint8_t>((id.value >> 8) & 0xF));
    out[3] = hex_digit(static_cast<uint8_t>((id.value >> 4) & 0xF));
    out[4] = hex_digit(static_cast<uint8_t>((id.value >> 0) & 0xF));
    out[5] = '\0';
    return true;
  }

  int find_index(EffectId id) const {
    if (catalog_ == nullptr || !id.valid()) {
      return -1;
    }
    for (size_t i = 0; i < catalog_->count(); ++i) {
      const EffectDescriptor* d = catalog_->descriptor_at(i);
      if (d != nullptr && d->id == id) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  EventContext make_event_context(uint32_t now_ms) const {
    EventContext ctx;
    ctx.now_ms = now_ms;
    ctx.map = map_;
    ctx.global_params = global_params_;
    ctx.signals = signals_;
    ctx.store = store_;
    ctx.logger = nullptr;
    return ctx;
  }

  void mark_active_dirty(uint32_t now_ms, bool immediate) {
    active_dirty_ = true;
    active_last_change_ms_ = now_ms;
    active_backoff_ms_ = 0;
    active_next_write_due_ms_ = immediate ? now_ms : (now_ms + kDebounceMs);
  }

  void persist_active_id_now(uint32_t now_ms) {
    active_dirty_ = true;
    active_last_change_ms_ = now_ms;
    const uint16_t v = active_id_.value;

    if (store_ != nullptr && store_->write_blob(kActiveEffectKey, &v, sizeof(v))) {
      active_dirty_ = false;
      active_backoff_ms_ = 0;
      active_next_write_due_ms_ = 0;
      return;
    }

    // Schedule retries (debounced/backoff) if immediate write failed.
    mark_active_dirty(now_ms, /*immediate=*/true);
  }

  void mark_dirty(size_t idx, uint32_t now_ms) {
    if (idx >= MaxEffects) {
      return;
    }
    configs_[idx].dirty = true;
    configs_[idx].last_change_ms = now_ms;
    configs_[idx].backoff_ms = 0;
    configs_[idx].next_write_due_ms = now_ms + kDebounceMs;
  }

  void flush_persist_due(uint32_t now_ms, bool force) {
    if (store_ == nullptr || catalog_ == nullptr) {
      return;
    }

    if (active_dirty_ && (force || now_ms >= active_next_write_due_ms_)) {
      const uint16_t v = active_id_.value;
      if (store_->write_blob(kActiveEffectKey, &v, sizeof(v))) {
        active_dirty_ = false;
        active_backoff_ms_ = 0;
      } else {
        const uint32_t next = (active_backoff_ms_ == 0) ? kDebounceMs : (active_backoff_ms_ * 2);
        active_backoff_ms_ = next > kMaxBackoffMs ? kMaxBackoffMs : next;
        active_next_write_due_ms_ = now_ms + active_backoff_ms_;
      }
    }

    for (size_t i = 0; i < catalog_->count(); ++i) {
      if (!configs_[i].dirty) {
        continue;
      }
      if (!effect_has_persisted_config(i)) {
        configs_[i].dirty = false;
        continue;
      }
      if (!force && now_ms < configs_[i].next_write_due_ms) {
        continue;
      }

      const EffectDescriptor* d = catalog_->descriptor_at(i);
      if (d == nullptr) {
        continue;
      }

      char key[8] = {};
      if (!make_effect_key(d->id, key, sizeof(key))) {
        continue;
      }

      if (store_->write_blob(key, configs_[i].bytes, kMaxEffectConfigSize)) {
        configs_[i].dirty = false;
        configs_[i].backoff_ms = 0;
      } else {
        const uint32_t next = (configs_[i].backoff_ms == 0) ? kDebounceMs : (configs_[i].backoff_ms * 2);
        configs_[i].backoff_ms = next > kMaxBackoffMs ? kMaxBackoffMs : next;
        configs_[i].next_write_due_ms = now_ms + configs_[i].backoff_ms;
      }
    }
  }

  void try_persist_config_now(size_t idx, uint32_t now_ms) {
    if (store_ == nullptr || catalog_ == nullptr || idx >= catalog_->count() || !configs_[idx].dirty) {
      return;
    }
    if (!effect_has_persisted_config(idx)) {
      configs_[idx].dirty = false;
      return;
    }

    const EffectDescriptor* d = catalog_->descriptor_at(idx);
    if (d == nullptr) {
      return;
    }

    char key[8] = {};
    if (!make_effect_key(d->id, key, sizeof(key))) {
      return;
    }

    if (store_->write_blob(key, configs_[idx].bytes, kMaxEffectConfigSize)) {
      configs_[idx].dirty = false;
      configs_[idx].backoff_ms = 0;
      configs_[idx].next_write_due_ms = 0;
      return;
    }

    const uint32_t next = (configs_[idx].backoff_ms == 0) ? kDebounceMs : (configs_[idx].backoff_ms * 2);
    configs_[idx].backoff_ms = next > kMaxBackoffMs ? kMaxBackoffMs : next;
    configs_[idx].next_write_due_ms = now_ms + configs_[idx].backoff_ms;
  }

  static const ParamDescriptor* find_param(const EffectConfigSchema& schema, ParamId pid) {
    if (schema.params == nullptr || schema.param_count == 0 || pid.value == 0) {
      return nullptr;
    }
    for (uint8_t i = 0; i < schema.param_count; ++i) {
      if (schema.params[i].id.value == pid.value) {
        return &schema.params[i];
      }
    }
    return nullptr;
  }

  static bool validate_descriptor(const ParamDescriptor& d) {
    if (!d.id.valid() || d.name == nullptr || d.display_name == nullptr) {
      return false;
    }
    if (d.size == 0) {
      return false;
    }
    if (static_cast<size_t>(d.offset) + static_cast<size_t>(d.size) > kMaxEffectConfigSize) {
      return false;
    }
    if (d.step <= 0) {
      return false;
    }
    if (d.min > d.max) {
      return false;
    }
    if (d.def < d.min || d.def > d.max) {
      return false;
    }
    switch (d.type) {
      case ParamType::U8:
      case ParamType::Bool:
      case ParamType::Enum:
        return d.size == 1;
      case ParamType::I16:
      case ParamType::U16:
        return d.size == 2;
      case ParamType::ColorRgb:
        return d.size == 3;
      default:
        return false;
    }
  }

  static bool param_value_type_matches(const ParamDescriptor& d, ParamType t) {
    // Permit Enum values to be provided as U8 (common for serial surfaces).
    if (d.type == ParamType::Enum && t == ParamType::U8) {
      return true;
    }
    if (d.type == ParamType::Bool && t == ParamType::U8) {
      return true;
    }
    return d.type == t;
  }

  static bool apply_param_value(const ParamDescriptor& d, const ParamValue& v, void* config_bytes,
                                size_t config_size) {
    if (config_bytes == nullptr || config_size < kMaxEffectConfigSize) {
      return false;
    }

    uint8_t* dst = static_cast<uint8_t*>(config_bytes) + d.offset;
    switch (d.type) {
      case ParamType::U8: {
        if (d.size != 1 || v.type != ParamType::U8) return false;
        const int32_t x = v.v.u8;
        if (x < d.min || x > d.max) return false;
        *dst = static_cast<uint8_t>(x);
        return true;
      }
      case ParamType::I16: {
        if (d.size != 2 || v.type != ParamType::I16) return false;
        const int32_t x = v.v.i16;
        if (x < d.min || x > d.max) return false;
        memcpy(dst, &v.v.i16, 2);
        return true;
      }
      case ParamType::U16: {
        if (d.size != 2 || v.type != ParamType::U16) return false;
        const int32_t x = v.v.u16;
        if (x < d.min || x > d.max) return false;
        memcpy(dst, &v.v.u16, 2);
        return true;
      }
      case ParamType::Bool: {
        if (d.size != 1) return false;
        bool b = false;
        if (v.type == ParamType::Bool) {
          b = v.v.b;
        } else if (v.type == ParamType::U8) {
          b = v.v.u8 != 0;
        } else {
          return false;
        }
        *dst = b ? 1 : 0;
        return true;
      }
      case ParamType::Enum: {
        if (d.size != 1) return false;
        uint8_t e = 0;
        if (v.type == ParamType::Enum) {
          e = v.v.e;
        } else if (v.type == ParamType::U8) {
          e = v.v.u8;
        } else {
          return false;
        }
        const int32_t x = e;
        if (x < d.min || x > d.max) return false;
        *dst = e;
        return true;
      }
      case ParamType::ColorRgb: {
        if (d.size != 3 || v.type != ParamType::ColorRgb) return false;
        memcpy(dst, &v.v.color_rgb, 3);
        return true;
      }
      default:
        return false;
    }
  }

  static bool read_param_value(const ParamDescriptor& d, const void* config_bytes, size_t config_size,
                               ParamValue* out) {
    if (config_bytes == nullptr || out == nullptr || config_size < kMaxEffectConfigSize) {
      return false;
    }
    const uint8_t* src = static_cast<const uint8_t*>(config_bytes) + d.offset;
    out->type = d.type;
    switch (d.type) {
      case ParamType::U8:
        if (d.size != 1) return false;
        out->v.u8 = *src;
        return true;
      case ParamType::I16:
        if (d.size != 2) return false;
        memcpy(&out->v.i16, src, 2);
        return true;
      case ParamType::U16:
        if (d.size != 2) return false;
        memcpy(&out->v.u16, src, 2);
        return true;
      case ParamType::Bool:
        if (d.size != 1) return false;
        out->v.b = (*src != 0);
        return true;
      case ParamType::Enum:
        if (d.size != 1) return false;
        out->v.e = *src;
        return true;
      case ParamType::ColorRgb:
        if (d.size != 3) return false;
        memcpy(&out->v.color_rgb, src, 3);
        return true;
      default:
        return false;
    }
  }

  void reset_config_bytes_to_defaults(size_t idx) {
    if (catalog_ == nullptr || idx >= catalog_->count()) {
      return;
    }
    memset(configs_[idx].bytes, 0, kMaxEffectConfigSize);

    IEffectV2* e = catalog_->effect_at(idx);
    const EffectConfigSchema* schema = e ? e->schema() : nullptr;
    if (schema == nullptr || schema->params == nullptr || schema->param_count == 0) {
      return;
    }

    for (uint8_t i = 0; i < schema->param_count; ++i) {
      const ParamDescriptor& d = schema->params[i];
      if (!validate_descriptor(d)) {
        continue;
      }
      uint8_t* dst = configs_[idx].bytes + d.offset;
      switch (d.type) {
        case ParamType::U8:
        case ParamType::Bool:
        case ParamType::Enum: {
          if (d.size == 1) {
            const uint8_t x = static_cast<uint8_t>(d.def);
            *dst = x;
          }
          break;
        }
        case ParamType::I16: {
          if (d.size == 2) {
            const int16_t x = static_cast<int16_t>(d.def);
            memcpy(dst, &x, 2);
          }
          break;
        }
        case ParamType::U16: {
          if (d.size == 2) {
            const uint16_t x = static_cast<uint16_t>(d.def);
            memcpy(dst, &x, 2);
          }
          break;
        }
        case ParamType::ColorRgb: {
          if (d.size == 3) {
            // Default for colors is currently interpreted as packed 0xRRGGBB in d.def.
            const uint8_t r = static_cast<uint8_t>((d.def >> 16) & 0xFF);
            const uint8_t g = static_cast<uint8_t>((d.def >> 8) & 0xFF);
            const uint8_t b = static_cast<uint8_t>((d.def >> 0) & 0xFF);
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
          }
          break;
        }
        default:
          break;
      }
    }
  }

  bool effect_has_persisted_config(size_t idx) const {
    if (catalog_ == nullptr || idx >= catalog_->count()) {
      return false;
    }
    IEffectV2* e = catalog_->effect_at(idx);
    const EffectConfigSchema* schema = e ? e->schema() : nullptr;
    return schema != nullptr && schema->params != nullptr && schema->param_count > 0;
  }
};

}  // namespace core
}  // namespace chromance
