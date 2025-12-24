#pragma once

#include <stdint.h>

#include "layout.h"

namespace chromance {
namespace core {

constexpr uint16_t strip_led_count(const StripConfig& strip) {
  return static_cast<uint16_t>(strip.segment_count) * kLedsPerSegment;
}

constexpr uint16_t segment_start_led(const StripConfig& strip, uint16_t segment_index) {
  return strip.reversed
             ? static_cast<uint16_t>(strip.segment_count - 1 - segment_index) * kLedsPerSegment
             : segment_index * kLedsPerSegment;
}

constexpr bool is_valid_segment_index(const StripConfig& strip, uint16_t segment_index) {
  return segment_index < strip.segment_count;
}

constexpr bool is_valid_led_index(const StripConfig& strip, uint16_t led_index) {
  return led_index < strip_led_count(strip);
}

}  // namespace core
}  // namespace chromance

