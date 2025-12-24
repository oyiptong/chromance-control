#pragma once

#include <stdint.h>

#include "types.h"

namespace chromance {
namespace core {

constexpr uint8_t kStripCount = 4;
constexpr uint16_t kTotalSegments = 40;
constexpr uint8_t kLedsPerSegment = 14;
constexpr uint8_t kDiagnosticBrightness = 64;

struct StripConfig {
  uint8_t segment_count;
  bool reversed;
  uint8_t data_pin;
  uint8_t clock_pin;
  Rgb diagnostic_color;
};

constexpr uint8_t kStrip0Segments = 11;
constexpr uint8_t kStrip1Segments = 12;
constexpr uint8_t kStrip2Segments = 6;
constexpr uint8_t kStrip3Segments = 11;

constexpr uint16_t kStrip0Leds = static_cast<uint16_t>(kStrip0Segments) * kLedsPerSegment;
constexpr uint16_t kStrip1Leds = static_cast<uint16_t>(kStrip1Segments) * kLedsPerSegment;
constexpr uint16_t kStrip2Leds = static_cast<uint16_t>(kStrip2Segments) * kLedsPerSegment;
constexpr uint16_t kStrip3Leds = static_cast<uint16_t>(kStrip3Segments) * kLedsPerSegment;
constexpr uint16_t kTotalLeds = static_cast<uint16_t>(kTotalSegments) * kLedsPerSegment;

static_assert(kStrip0Segments > 0, "strip 0 must have >0 segments");
static_assert(kStrip1Segments > 0, "strip 1 must have >0 segments");
static_assert(kStrip2Segments > 0, "strip 2 must have >0 segments");
static_assert(kStrip3Segments > 0, "strip 3 must have >0 segments");
static_assert(kStrip0Segments + kStrip1Segments + kStrip2Segments + kStrip3Segments ==
                  kTotalSegments,
              "segment counts must sum to kTotalSegments");
static_assert(kStrip0Leds == 154, "strip 0 LED count mismatch");
static_assert(kStrip1Leds == 168, "strip 1 LED count mismatch");
static_assert(kStrip2Leds == 84, "strip 2 LED count mismatch");
static_assert(kStrip3Leds == 154, "strip 3 LED count mismatch");
static_assert(kTotalLeds == 560, "total LED count mismatch");

constexpr StripConfig kStripConfigs[kStripCount] = {
    // Pins are placeholders; adjust to match your wiring.
    {kStrip0Segments, false, 13, 14, {255, 0, 0}},
    {kStrip1Segments, false, 16, 17, {0, 255, 0}},
    {kStrip2Segments, false, 21, 22, {0, 0, 255}},
    {kStrip3Segments, false, 25, 26, {255, 0, 255}},
};

}  // namespace core
}  // namespace chromance
