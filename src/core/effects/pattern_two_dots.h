#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../types.h"
#include "effect.h"

namespace chromance {
namespace core {

class TwoDotsEffect final : public IEffect {
 public:
  explicit TwoDotsEffect(uint16_t step_ms = 25) : step_ms_(step_ms) {}

  const char* id() const override { return "Two_Dots"; }

  void reset(uint32_t now_ms) override {
    start_ms_ = now_ms;
    rng_ = 0x9E3779B9u ^ now_ms;
    seq_index_ = 0;
    pick_new_colors();
  }

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
    const uint32_t step = step_ms_ ? (elapsed / step_ms_) : elapsed;
    const size_t n = led_count;

    const uint32_t loop = n ? (step / n) : 0;
    if (loop != seq_index_) {
      seq_index_ = loop;
      pick_new_colors();
    }

    const size_t pos_a = static_cast<size_t>(step % n);
    const size_t pos_b = static_cast<size_t>((n - 1U) - (step % n));

    const uint8_t v = frame.params.brightness;
    const Rgb a = scale(color_a_, v);
    const Rgb b = scale(color_b_, v);

    out_rgb[pos_a] = add_sat(out_rgb[pos_a], a);
    out_rgb[pos_b] = add_sat(out_rgb[pos_b], b);
  }

  Rgb color_a() const { return color_a_; }
  Rgb color_b() const { return color_b_; }

 private:
  static Rgb scale(const Rgb& c, uint8_t v) {
    return Rgb{static_cast<uint8_t>((static_cast<uint16_t>(c.r) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.g) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.b) * v) / 255U)};
  }

  static uint8_t add_u8_sat(uint8_t a, uint8_t b) {
    const uint16_t s = static_cast<uint16_t>(a) + b;
    return static_cast<uint8_t>(s > 255 ? 255 : s);
  }

  static Rgb add_sat(const Rgb& a, const Rgb& b) {
    return Rgb{add_u8_sat(a.r, b.r), add_u8_sat(a.g, b.g), add_u8_sat(a.b, b.b)};
  }

  uint32_t next_u32() {
    // xorshift32
    uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x;
    return x;
  }

  static Rgb hue_to_rgb(uint8_t hue) {
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

  void pick_new_colors() {
    // Pick two bright colors based on random hues, ensuring they are not identical.
    const uint8_t hue_a = static_cast<uint8_t>(next_u32() & 0xFF);
    uint8_t hue_b = static_cast<uint8_t>(next_u32() & 0xFF);
    if (hue_b == hue_a) {
      hue_b = static_cast<uint8_t>(hue_b + 85);
    }
    color_a_ = hue_to_rgb(hue_a);
    color_b_ = hue_to_rgb(hue_b);
  }

  uint32_t start_ms_ = 0;
  uint16_t step_ms_ = 25;
  uint32_t rng_ = 0x12345678u;
  uint32_t seq_index_ = 0;
  Rgb color_a_{255, 0, 0};
  Rgb color_b_{0, 255, 0};
};

}  // namespace core
}  // namespace chromance

