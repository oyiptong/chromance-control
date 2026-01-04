#pragma once

#include <stddef.h>
#include <stdint.h>

#include "effect.h"

namespace chromance {
namespace core {

class CoordColorEffect final : public IEffect {
 public:
  const char* id() const override { return "Coord_Color_Test"; }

  void reset(uint32_t /*now_ms*/) override {}

  void render(uint32_t /*now_ms*/, const PixelsMap& map, Rgb* out_rgb, size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) {
      return;
    }

    const int32_t w = static_cast<int32_t>(map.width());
    const int32_t h = static_cast<int32_t>(map.height());

    for (uint16_t i = 0; i < led_count; ++i) {
      const PixelCoord c = map.coord(i);
      const uint8_t r = normalize_0_255(c.x, w);
      const uint8_t g = normalize_0_255(c.y, h);
      out_rgb[i] = Rgb{r, g, 0};
    }
  }

 private:
  static uint8_t normalize_0_255(int16_t v, int32_t span) {
    if (span <= 1) {
      return 0;
    }
    int32_t x = static_cast<int32_t>(v);
    if (x < 0) x = 0;
    if (x > span - 1) x = span - 1;
    return static_cast<uint8_t>((x * 255) / (span - 1));
  }
};

}  // namespace core
}  // namespace chromance

