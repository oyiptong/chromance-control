#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

inline uint8_t clamp_percent_0_100(uint8_t percent) {
  return percent > 100 ? 100 : percent;
}

inline uint8_t quantize_percent_to_10(uint8_t percent) {
  const uint8_t p = clamp_percent_0_100(percent);
  const uint8_t q = static_cast<uint8_t>(((static_cast<uint16_t>(p) + 5U) / 10U) * 10U);
  return clamp_percent_0_100(q);
}

inline uint8_t brightness_step_up_10(uint8_t percent) {
  const uint8_t p = quantize_percent_to_10(percent);
  return p >= 100 ? 100 : static_cast<uint8_t>(p + 10U);
}

inline uint8_t brightness_step_down_10(uint8_t percent) {
  const uint8_t p = quantize_percent_to_10(percent);
  return p <= 0 ? 0 : static_cast<uint8_t>(p - 10U);
}

inline uint8_t percent_to_u8_255(uint8_t percent) {
  const uint16_t p = clamp_percent_0_100(percent);
  return static_cast<uint8_t>((p * 255U) / 100U);
}

inline uint8_t soft_percent_to_hw_percent(uint8_t soft_percent, uint8_t ceiling_percent) {
  const uint16_t soft = clamp_percent_0_100(soft_percent);
  const uint16_t ceiling = clamp_percent_0_100(ceiling_percent);
  return static_cast<uint8_t>((soft * ceiling) / 100U);
}

inline uint8_t soft_percent_to_u8_255(uint8_t soft_percent, uint8_t ceiling_percent) {
  return percent_to_u8_255(soft_percent_to_hw_percent(soft_percent, ceiling_percent));
}

}  // namespace core
}  // namespace chromance
