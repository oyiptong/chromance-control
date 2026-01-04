#pragma once

#include <stddef.h>
#include <stdint.h>

#include "effect.h"

namespace chromance {
namespace core {

class XyScanEffect final : public IEffect {
 public:
  XyScanEffect(const uint16_t* scan_order, size_t scan_len, uint16_t hold_ms = 25)
      : scan_order_(scan_order), scan_len_(scan_len), hold_ms_(hold_ms) {}

  const char* id() const override { return "XY_Scan_Test"; }

  void reset(uint32_t now_ms) override { start_ms_ = now_ms; }

  void render(uint32_t now_ms, const PixelsMap& /*map*/, Rgb* out_rgb, size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0 || scan_order_ == nullptr || scan_len_ < led_count) {
      return;
    }

    for (size_t i = 0; i < led_count; ++i) {
      out_rgb[i] = kBlack;
    }

    const uint32_t elapsed = now_ms - start_ms_;
    const uint32_t step = hold_ms_ ? (elapsed / hold_ms_) : elapsed;
    const size_t cursor = static_cast<size_t>(step % led_count);
    const uint16_t led_index = scan_order_[cursor];
    out_rgb[led_index] = Rgb{255, 255, 255};
  }

 private:
  const uint16_t* scan_order_ = nullptr;
  size_t scan_len_ = 0;
  uint32_t start_ms_ = 0;
  uint16_t hold_ms_ = 25;
};

}  // namespace core
}  // namespace chromance

