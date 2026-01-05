#pragma once

#include <stddef.h>
#include <stdint.h>

#include "effect.h"

namespace chromance {
namespace core {

class IndexWalkEffect final : public IEffect {
 public:
  explicit IndexWalkEffect(uint16_t hold_ms = 25) : hold_ms_(hold_ms) {}

  const char* id() const override { return "Index_Walk_Test"; }

  void reset(uint32_t now_ms) override { start_ms_ = now_ms; }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) {
      return;
    }

    for (size_t i = 0; i < led_count; ++i) {
      out_rgb[i] = kBlack;
    }

    const uint32_t elapsed = frame.now_ms - start_ms_;
    const uint32_t step = hold_ms_ ? (elapsed / hold_ms_) : elapsed;
    const size_t idx = static_cast<size_t>(step % led_count);
    const uint8_t v = frame.params.brightness;
    out_rgb[idx] = Rgb{v, v, v};
  }

 private:
  uint32_t start_ms_ = 0;
  uint16_t hold_ms_ = 25;
};

}  // namespace core
}  // namespace chromance
