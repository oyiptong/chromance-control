#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../layout.h"
#include "../mapping/mapping_tables.h"
#include "../types.h"
#include "effect.h"

namespace chromance {
namespace core {

// Mode 2: strip-colored per-strip segment stepper.
//
// Behavior:
// - Each strip has a fixed color:
//   - strip0: red, strip1: blue, strip2: green, strip3: cyan
// - Shows segment number k (1..12) on every strip simultaneously.
//   - For strips with fewer than k segments, that strip remains black.
// - Auto-advances k forever; serial 'n'/'N' advances k immediately.
//
// This is intended for physically re-mapping wiring order/direction per strip.
class StripSegmentStepperEffect final : public IEffect {
 public:
  explicit StripSegmentStepperEffect(uint16_t step_ms = 1000) : step_ms_(step_ms) {}

  const char* id() const override { return "Strip segment stepper"; }

  void reset(uint32_t now_ms) override {
    last_step_ms_ = now_ms;
    segment_number_ = 1;
    auto_advance_enabled_ = true;
  }

  void next(uint32_t now_ms) {
    segment_number_ = static_cast<uint8_t>((segment_number_ % 12U) + 1U);
    last_step_ms_ = now_ms;
  }

  void prev(uint32_t now_ms) {
    segment_number_ = segment_number_ <= 1 ? 12 : static_cast<uint8_t>(segment_number_ - 1U);
    last_step_ms_ = now_ms;
  }

  void set_auto_advance_enabled(bool enabled, uint32_t now_ms) {
    auto_advance_enabled_ = enabled;
    last_step_ms_ = now_ms;
  }

  bool auto_advance_enabled() const { return auto_advance_enabled_; }
  uint8_t segment_number() const { return segment_number_; }  // 1..12

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) return;
    for (size_t i = 0; i < led_count; ++i) out_rgb[i] = kBlack;

    auto_advance(frame.now_ms);

    const uint8_t v = frame.params.brightness;
    const Rgb colors[4] = {
        scale(Rgb{255, 0, 0}, v),    // strip0 red
        scale(Rgb{0, 0, 255}, v),    // strip1 blue
        scale(Rgb{0, 255, 0}, v),    // strip2 green
        scale(Rgb{0, 255, 255}, v),  // strip3 cyan
    };

    const uint8_t* strips = MappingTables::global_to_strip();
    const uint16_t* locals = MappingTables::global_to_local();

    const uint16_t kLedsPerSeg = chromance::core::kLedsPerSegment;
    const uint16_t seg0 = static_cast<uint16_t>(segment_number_ - 1U);

    for (uint16_t i = 0; i < led_count; ++i) {
      const uint8_t strip = strips[i];
      if (strip >= 4) continue;
      const uint16_t local = locals[i];
      const uint16_t seg_in_strip = static_cast<uint16_t>(local / kLedsPerSeg);
      if (seg_in_strip == seg0) {
        out_rgb[i] = colors[strip];
      }
    }
  }

 private:
  static Rgb scale(const Rgb& c, uint8_t v) {
    return Rgb{static_cast<uint8_t>((static_cast<uint16_t>(c.r) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.g) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.b) * v) / 255U)};
  }

  void auto_advance(uint32_t now_ms) {
    if (!auto_advance_enabled_ || step_ms_ == 0) return;
    while (static_cast<int32_t>(now_ms - last_step_ms_) >= static_cast<int32_t>(step_ms_)) {
      last_step_ms_ += step_ms_;
      segment_number_ = static_cast<uint8_t>((segment_number_ % 12U) + 1U);
    }
  }

  uint16_t step_ms_ = 1000;
  uint32_t last_step_ms_ = 0;
  uint8_t segment_number_ = 1;
  bool auto_advance_enabled_ = true;
};

}  // namespace core
}  // namespace chromance
