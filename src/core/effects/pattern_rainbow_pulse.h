#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../brightness.h"
#include "../types.h"
#include "effect.h"

namespace chromance {
namespace core {

class RainbowPulseEffect final : public IEffect {
 public:
  RainbowPulseEffect(uint16_t fade_in_ms = 700, uint16_t hold_ms = 2000, uint16_t fade_out_ms = 700)
      : fade_in_ms_(fade_in_ms), hold_ms_(hold_ms), fade_out_ms_(fade_out_ms) {}

  const char* id() const override { return "Rainbow_Pulse"; }

  void reset(uint32_t now_ms) override {
    start_ms_ = now_ms;
    base_hue_ = 0;
  }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) {
      return;
    }

    const uint32_t cycle_ms = static_cast<uint32_t>(fade_in_ms_) + hold_ms_ + fade_out_ms_;
    const uint32_t elapsed = frame.now_ms - start_ms_;
    const uint32_t cycle = cycle_ms ? (elapsed / cycle_ms) : 0;
    const uint32_t t = cycle_ms ? (elapsed % cycle_ms) : 0;

    // Step hue once per full pulse cycle (12 steps around a full wheel).
    const uint8_t hue = static_cast<uint8_t>(base_hue_ + static_cast<uint8_t>(cycle * 21U));
    const Rgb base = hue_to_rgb(hue);

    const uint8_t alpha = compute_alpha(t);
    const uint8_t v = static_cast<uint8_t>((static_cast<uint16_t>(alpha) * frame.params.brightness) /
                                           255U);

    const Rgb color{static_cast<uint8_t>((static_cast<uint16_t>(base.r) * v) / 255U),
                    static_cast<uint8_t>((static_cast<uint16_t>(base.g) * v) / 255U),
                    static_cast<uint8_t>((static_cast<uint16_t>(base.b) * v) / 255U)};

    for (size_t i = 0; i < led_count; ++i) {
      out_rgb[i] = color;
    }
  }

 private:
  uint8_t compute_alpha(uint32_t t) const {
    if (fade_in_ms_ == 0 && fade_out_ms_ == 0) {
      return 255;
    }

    if (fade_in_ms_ != 0 && t < fade_in_ms_) {
      return static_cast<uint8_t>((t * 255U) / fade_in_ms_);
    }
    t -= fade_in_ms_;
    if (t < hold_ms_) {
      return 255;
    }
    t -= hold_ms_;
    if (fade_out_ms_ != 0 && t < fade_out_ms_) {
      return static_cast<uint8_t>(((fade_out_ms_ - t) * 255U) / fade_out_ms_);
    }
    return 0;
  }

  static Rgb hue_to_rgb(uint8_t hue) {
    // Simple RGB wheel: 0=red, 85=green, 170=blue.
    if (hue < 85) {
      return Rgb{static_cast<uint8_t>(255U - hue * 3U), static_cast<uint8_t>(hue * 3U), 0};
    }
    hue = static_cast<uint8_t>(hue - 85);
    if (hue < 85) {
      return Rgb{0, static_cast<uint8_t>(255U - hue * 3U), static_cast<uint8_t>(hue * 3U)};
    }
    hue = static_cast<uint8_t>(hue - 85);
    return Rgb{static_cast<uint8_t>(hue * 3U), 0, static_cast<uint8_t>(255U - hue * 3U)};
  }

  uint32_t start_ms_ = 0;
  uint8_t base_hue_ = 0;
  uint16_t fade_in_ms_ = 700;
  uint16_t hold_ms_ = 2000;
  uint16_t fade_out_ms_ = 700;
};

}  // namespace core
}  // namespace chromance

