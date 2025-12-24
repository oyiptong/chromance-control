#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chromance {
namespace core {

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  constexpr bool operator==(const Rgb& other) const {
    return r == other.r && g == other.g && b == other.b;
  }
};

constexpr Rgb kBlack{0, 0, 0};

}  // namespace core
}  // namespace chromance

