#include "dotstar_leds.h"

#include <Arduino.h>

#include "core/strip_layout.h"

namespace chromance {
namespace platform {

namespace {

constexpr uint8_t kDotstarColorOrder = DOTSTAR_BRG;

Adafruit_DotStar* make_strip(const core::StripConfig& cfg) {
  return new Adafruit_DotStar(core::strip_led_count(cfg),
                              cfg.data_pin,
                              cfg.clock_pin,
                              kDotstarColorOrder);
}

}  // namespace

void DotstarLeds::begin() {
  for (uint8_t i = 0; i < core::kStripCount; ++i) {
    if (strips_[i] == nullptr) {
      strips_[i] = make_strip(core::kStripConfigs[i]);
    }
    if (strips_[i] == nullptr) {
      continue;
    }

    strips_[i]->begin();
    strips_[i]->setBrightness(core::kDiagnosticBrightness);
    strips_[i]->show();
  }
}

void DotstarLeds::clear_all() {
  for (uint8_t strip = 0; strip < core::kStripCount; ++strip) {
    if (strips_[strip] == nullptr) {
      continue;
    }
    for (uint16_t i = 0; i < core::strip_led_count(core::kStripConfigs[strip]); ++i) {
      strips_[strip]->setPixelColor(i, 0, 0, 0);
    }
  }
}

void DotstarLeds::show_all() {
  for (uint8_t strip = 0; strip < core::kStripCount; ++strip) {
    if (strips_[strip] != nullptr) {
      strips_[strip]->show();
    }
  }
}

void DotstarLeds::set_segment_all(uint8_t strip_index,
                                  uint16_t segment_index,
                                  core::Rgb color,
                                  bool on) {
  if (strip_index >= core::kStripCount) {
    return;
  }
  Adafruit_DotStar* strip = strips_[strip_index];
  if (strip == nullptr) {
    return;
  }

  const core::StripConfig& cfg = core::kStripConfigs[strip_index];
  if (!core::is_valid_segment_index(cfg, segment_index)) {
    return;
  }

  const uint16_t start_led = core::segment_start_led(cfg, segment_index);
  const uint8_t r = on ? color.r : 0;
  const uint8_t g = on ? color.g : 0;
  const uint8_t b = on ? color.b : 0;

  for (uint8_t i = 0; i < core::kLedsPerSegment; ++i) {
    strip->setPixelColor(start_led + i, r, g, b);
  }
}

void DotstarLeds::set_segment_single_led(uint8_t strip_index,
                                         uint16_t segment_index,
                                         uint8_t led_in_segment,
                                         core::Rgb color) {
  if (strip_index >= core::kStripCount) {
    return;
  }
  Adafruit_DotStar* strip = strips_[strip_index];
  if (strip == nullptr) {
    return;
  }

  const core::StripConfig& cfg = core::kStripConfigs[strip_index];
  if (!core::is_valid_segment_index(cfg, segment_index)) {
    return;
  }
  if (led_in_segment >= core::kLedsPerSegment) {
    return;
  }

  const uint16_t start_led = core::segment_start_led(cfg, segment_index);
  for (uint8_t i = 0; i < core::kLedsPerSegment; ++i) {
    strip->setPixelColor(start_led + i, 0, 0, 0);
  }
  strip->setPixelColor(start_led + led_in_segment, color.r, color.g, color.b);
}

}  // namespace platform
}  // namespace chromance
