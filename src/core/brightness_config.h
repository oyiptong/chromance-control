#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

// Compile-time brightness ceiling (percent of full 0..255 output).
// Start conservative for power/thermal safety; raise only after validating power injection.
constexpr uint8_t kHardwareBrightnessCeilingPercent = 20;
static_assert(kHardwareBrightnessCeilingPercent <= 100, "ceiling must be 0..100");

}  // namespace core
}  // namespace chromance

