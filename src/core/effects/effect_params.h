#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

// Baseline parameter model shared across effects.
// Values are intentionally kept small and portable (no heap, no STL).
struct EffectParams {
  uint8_t brightness = 255;  // 0..255
  uint8_t speed = 128;       // 0..255 (effect-defined)
  uint8_t intensity = 128;   // 0..255 (effect-defined)
  uint8_t palette = 0;       // 0..255 (palette index / selection)
};

}  // namespace core
}  // namespace chromance

