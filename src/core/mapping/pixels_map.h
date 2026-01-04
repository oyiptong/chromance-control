#pragma once

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "mapping_tables.h"

namespace chromance {
namespace core {

struct PixelCoord {
  int16_t x;
  int16_t y;
};

class PixelsMap {
 public:
  constexpr size_t led_count() const { return MappingTables::led_count(); }
  constexpr uint16_t width() const { return MappingTables::width(); }
  constexpr uint16_t height() const { return MappingTables::height(); }

  constexpr PixelCoord coord(uint16_t led_index) const {
    return PixelCoord{MappingTables::pixel_x()[led_index], MappingTables::pixel_y()[led_index]};
  }

  constexpr PixelCoord center() const {
    // Center in raster coordinates (0..width-1, 0..height-1).
    return PixelCoord{static_cast<int16_t>((width() - 1) / 2),
                      static_cast<int16_t>((height() - 1) / 2)};
  }

  void build_scan_order(uint16_t* out_led_indices, size_t out_len) const {
    const size_t n = led_count();
    if (out_led_indices == nullptr || out_len < n) {
      return;
    }

    for (uint16_t i = 0; i < n; ++i) {
      out_led_indices[i] = i;
    }

    const int16_t* px = MappingTables::pixel_x();
    const int16_t* py = MappingTables::pixel_y();
    std::sort(out_led_indices, out_led_indices + n, [&](uint16_t a, uint16_t b) {
      if (py[a] != py[b]) return py[a] < py[b];
      if (px[a] != px[b]) return px[a] < px[b];
      return a < b;
    });
  }
};

}  // namespace core
}  // namespace chromance

