#include "pattern_breathing_mode.h"

namespace chromance {
namespace core {

// Out-of-class definitions for static constexpr members (C++11/14 compatibility).
constexpr Rgb BreathingEffect::kInhaleDotColor;
constexpr Rgb BreathingEffect::kExhaleWaveColor;
constexpr Rgb BreathingEffect::kInhalePauseColor;
constexpr Rgb BreathingEffect::kExhalePauseColor;
constexpr uint8_t BreathingEffect::kTailLut[BreathingEffect::kTailLutLen];

}  // namespace core
}  // namespace chromance

