#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../settings/effect_config_store.h"
#include "effect_descriptor.h"
#include "effect_v2.h"
#include "params.h"
#include "pattern_breathing_mode.h"

namespace chromance {
namespace core {

namespace breathing_v2 {

// Persisted subset of Breathing config.
// Note: field ordering is intentionally aligned with the previous (buggy) persisted layout so that
// existing NVS blobs map cleanly:
//   offset0: configured_center_vertex_id
//   offset1: has_configured_center
//   offset2: num_dots
struct PersistedConfig {
  uint8_t configured_center_vertex_id;
  uint8_t has_configured_center;  // 0/1
  uint8_t num_dots;
  uint8_t _reserved0;
};

static_assert(sizeof(PersistedConfig) <= kMaxEffectConfigSize, "Breathing persisted config too large");

static constexpr ParamId kPidUseConfiguredCenter = ParamId(1);
static constexpr ParamId kPidCenterVertex = ParamId(2);
static constexpr ParamId kPidDotCount = ParamId(3);

static const ParamDescriptor kParams[] = {
    {kPidUseConfiguredCenter, "use_configured_center", "Use Configured Center", ParamType::Bool,
     static_cast<uint16_t>(offsetof(PersistedConfig, has_configured_center)), 1, 0, 1, 1, 1, 1},
    {kPidCenterVertex, "center_vertex_id", "Center Vertex", ParamType::U8,
     static_cast<uint16_t>(offsetof(PersistedConfig, configured_center_vertex_id)), 1, 0, 31, 1, 12, 1},
    {kPidDotCount, "num_dots", "Dot Count", ParamType::U8,
     static_cast<uint16_t>(offsetof(PersistedConfig, num_dots)), 1, 1, 36, 1, 9, 1},
};

static const EffectConfigSchema kSchema{
    kParams, static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0]))};

static const StageDescriptor kStages[] = {
    {StageId(0), "inhale", "Inhale"},
    {StageId(1), "pause1", "Pause 1"},
    {StageId(2), "exhale", "Exhale"},
    {StageId(3), "pause2", "Pause 2"},
};

}  // namespace breathing_v2

class BreathingEffectV2 final : public IEffectV2 {
 public:
  explicit BreathingEffectV2(const EffectDescriptor& descriptor, BreathingEffect* legacy)
      : descriptor_(descriptor), legacy_(legacy) {}

  const EffectDescriptor& descriptor() const override { return descriptor_; }
  const EffectConfigSchema* schema() const override { return &breathing_v2::kSchema; }

  void bind_config(const void* config_bytes, size_t config_size) override {
    if (legacy_ == nullptr || config_bytes == nullptr ||
        config_size < sizeof(breathing_v2::PersistedConfig)) {
      cfg_ = nullptr;
      return;
    }
    cfg_ = static_cast<const breathing_v2::PersistedConfig*>(config_bytes);
    apply_config_to_legacy();
  }

  void start(const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    legacy_->reset(ctx.now_ms);
    apply_config_to_legacy();
  }

  void reset_runtime(const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    legacy_->reset(ctx.now_ms);
    apply_config_to_legacy();
  }

  void on_event(const InputEvent& ev, const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    switch (ev.key) {
      case Key::N:
        legacy_->next_phase(ctx.now_ms);
        break;
      case Key::ShiftN:
        legacy_->prev_phase(ctx.now_ms);
        break;
      case Key::Esc:
        legacy_->set_auto(ctx.now_ms);
        break;
      case Key::S:
        legacy_->lane_next(ctx.now_ms);
        break;
      case Key::ShiftS:
        legacy_->lane_prev(ctx.now_ms);
        break;
      default:
        break;
    }
  }

  uint8_t stage_count() const override { return 4; }

  const StageDescriptor* stage_at(uint8_t i) const override {
    if (i >= stage_count()) {
      return nullptr;
    }
    return &breathing_v2::kStages[i];
  }

  StageId current_stage() const override {
    if (legacy_ == nullptr) {
      return StageId();
    }
    return StageId(static_cast<uint8_t>(legacy_->phase()) & 3U);
  }

  bool enter_stage(StageId id, const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return false;
    }
    if (id.value >= stage_count()) {
      return false;
    }
    legacy_->set_manual_phase(id.value, ctx.now_ms);
    return true;
  }

  void render(const RenderContext& ctx, Rgb* out_rgb, size_t led_count) override {
    if (legacy_ == nullptr || ctx.map == nullptr) {
      if (out_rgb != nullptr) {
        for (size_t i = 0; i < led_count; ++i) {
          out_rgb[i] = kBlack;
        }
      }
      return;
    }

    EffectFrame frame;
    frame.now_ms = ctx.now_ms;
    frame.dt_ms = ctx.dt_ms;
    frame.params = ctx.global_params;
    frame.signals = ctx.signals;
    legacy_->render(frame, *ctx.map, out_rgb, led_count);
  }

 private:
  void apply_config_to_legacy() {
    if (legacy_ == nullptr) {
      return;
    }

    // Start from legacy defaults to avoid "zeroed fields" regressions.
    BreathingEffect::Config cfg;
    if (cfg_ != nullptr) {
      cfg.has_configured_center = (cfg_->has_configured_center != 0);
      cfg.configured_center_vertex_id = cfg_->configured_center_vertex_id;
      cfg.num_dots = cfg_->num_dots;
    }
    legacy_->set_config(cfg);
  }

  EffectDescriptor descriptor_;
  BreathingEffect* legacy_ = nullptr;                  // non-owning
  const breathing_v2::PersistedConfig* cfg_ = nullptr;  // points into manager-owned bytes
};

}  // namespace core
}  // namespace chromance
