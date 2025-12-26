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

// Diagnostic colors (per strip index). Physical strip labels are 1-based:
// - strip index 0 = "Strip 1"
// - strip index 1 = "Strip 2"
// - strip index 2 = "Strip 3"
// - strip index 3 = "Strip 4"
constexpr Rgb kDiagnosticColorRed{255, 0, 0};
constexpr Rgb kDiagnosticColorGreen{0, 255, 0};
constexpr Rgb kDiagnosticColorBlue{0, 0, 255};
constexpr Rgb kDiagnosticColorMagenta{255, 0, 255};

constexpr Rgb kStrip0DiagnosticColor = kDiagnosticColorRed;          // Red
constexpr Rgb kStrip1DiagnosticColor = kDiagnosticColorGreen;        // Green
constexpr Rgb kStrip2DiagnosticColor = kDiagnosticColorBlue;         // Blue
constexpr Rgb kStrip3DiagnosticColor = kDiagnosticColorMagenta;      // Magenta

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
    // Pin assignment (DATA, CLOCK):
    // Strip 1: GPIO23, GPIO22
    // Strip 2: GPIO19, GPIO18
    // Strip 3: GPIO17, GPIO16
    // Strip 4: GPIO14, GPIO32
    {kStrip0Segments, false, 23, 22, kStrip0DiagnosticColor},
    {kStrip1Segments, false, 19, 18, kStrip1DiagnosticColor},
    {kStrip2Segments, false, 17, 16, kStrip2DiagnosticColor},
    {kStrip3Segments, false, 14, 32, kStrip3DiagnosticColor},
};

}  // namespace core
}  // namespace chromance
