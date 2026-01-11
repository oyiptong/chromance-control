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

static_assert(sizeof(BreathingEffect::Config) <= kMaxEffectConfigSize, "Breathing config too large");

static constexpr ParamId kPidUseConfiguredCenter = ParamId(1);
static constexpr ParamId kPidCenterVertex = ParamId(2);
static constexpr ParamId kPidDotCount = ParamId(3);

static const ParamDescriptor kParams[] = {
    {kPidUseConfiguredCenter, "use_configured_center", "Use Configured Center", ParamType::Bool,
     static_cast<uint16_t>(offsetof(BreathingEffect::Config, has_configured_center)), 1, 0, 1, 1, 1},
    {kPidCenterVertex, "center_vertex_id", "Center Vertex", ParamType::U8,
     static_cast<uint16_t>(offsetof(BreathingEffect::Config, configured_center_vertex_id)), 1, 0, 31, 1, 12},
    {kPidDotCount, "num_dots", "Dot Count", ParamType::U8,
     static_cast<uint16_t>(offsetof(BreathingEffect::Config, num_dots)), 1, 1, 36, 1, 9},
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
    if (legacy_ == nullptr || config_bytes == nullptr || config_size < sizeof(BreathingEffect::Config)) {
      cfg_ = nullptr;
      return;
    }
    cfg_ = static_cast<const BreathingEffect::Config*>(config_bytes);
    legacy_->set_config(*cfg_);
  }

  void start(const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    legacy_->reset(ctx.now_ms);
    if (cfg_ != nullptr) {
      legacy_->set_config(*cfg_);
    }
  }

  void reset_runtime(const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    legacy_->reset(ctx.now_ms);
    if (cfg_ != nullptr) {
      legacy_->set_config(*cfg_);
    }
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
  EffectDescriptor descriptor_;
  BreathingEffect* legacy_ = nullptr;                  // non-owning
  const BreathingEffect::Config* cfg_ = nullptr;       // points into manager-owned bytes
};

}  // namespace core
}  // namespace chromance
