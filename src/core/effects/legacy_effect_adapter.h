#pragma once

#include <stddef.h>
#include <stdint.h>

#include "effect.h"
#include "effect_descriptor.h"
#include "effect_v2.h"

namespace chromance {
namespace core {

class LegacyEffectAdapter final : public IEffectV2 {
 public:
  LegacyEffectAdapter(const EffectDescriptor& descriptor, IEffect* legacy)
      : descriptor_(descriptor), legacy_(legacy) {}

  const EffectDescriptor& descriptor() const override { return descriptor_; }
  const EffectConfigSchema* schema() const override { return nullptr; }

  void start(const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    legacy_->reset(ctx.now_ms);
  }

  void reset_runtime(const EventContext& ctx) override {
    if (legacy_ == nullptr) {
      return;
    }
    legacy_->reset(ctx.now_ms);
  }

  void render(const RenderContext& ctx, Rgb* out_rgb, size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) {
      return;
    }
    if (legacy_ == nullptr || ctx.map == nullptr) {
      for (size_t i = 0; i < led_count; ++i) {
        out_rgb[i] = kBlack;
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
  EffectDescriptor descriptor_{};
  IEffect* legacy_ = nullptr;  // non-owning
};

}  // namespace core
}  // namespace chromance

