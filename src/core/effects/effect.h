#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../mapping/pixels_map.h"
#include "../types.h"

namespace chromance {
namespace core {

class IEffect {
 public:
  virtual ~IEffect() = default;
  virtual const char* id() const = 0;
  virtual void reset(uint32_t now_ms) = 0;
  virtual void render(uint32_t now_ms, const PixelsMap& map, Rgb* out_rgb, size_t led_count) = 0;
};

}  // namespace core
}  // namespace chromance

