#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

struct EffectId {
  uint16_t value;

  constexpr EffectId() : value(0) {}
  constexpr explicit EffectId(uint16_t v) : value(v) {}
  constexpr bool valid() const { return value != 0; }
};

constexpr bool operator==(EffectId a, EffectId b) { return a.value == b.value; }
constexpr bool operator!=(EffectId a, EffectId b) { return a.value != b.value; }

}  // namespace core
}  // namespace chromance
