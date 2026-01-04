#include "dotstar_output.h"

#include <Arduino.h>

#include "core/mapping/mapping_tables.h"
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

void DotstarOutput::begin() {
  for (uint8_t i = 0; i < core::kStripCount; ++i) {
    if (strips_[i] == nullptr) {
      strips_[i] = make_strip(core::kStripConfigs[i]);
    }
    if (strips_[i] == nullptr) {
      continue;
    }
    strips_[i]->begin();
    strips_[i]->setBrightness(core::kDiagnosticBrightness);
    for (uint16_t p = 0; p < core::strip_led_count(core::kStripConfigs[i]); ++p) {
      strips_[i]->setPixelColor(p, 0, 0, 0);
    }
    strips_[i]->show();
  }
}

void DotstarOutput::show(const chromance::core::Rgb* rgb, size_t len, PerfStats* stats) {
  if (rgb == nullptr) {
    return;
  }
  const uint16_t expected = core::MappingTables::led_count();
  if (len != expected) {
    return;
  }

  const uint8_t* g2s = core::MappingTables::global_to_strip();
  const uint16_t* g2l = core::MappingTables::global_to_local();

  const uint32_t start_ms = millis();
  for (uint16_t i = 0; i < len; ++i) {
    const uint8_t strip = g2s[i];
    const uint16_t local = g2l[i];
    if (strip >= core::kStripCount) {
      continue;
    }
    if (strips_[strip] == nullptr) {
      continue;
    }
    strips_[strip]->setPixelColor(local, rgb[i].r, rgb[i].g, rgb[i].b);
  }

  for (uint8_t strip = 0; strip < core::kStripCount; ++strip) {
    if (strips_[strip] != nullptr) {
      strips_[strip]->show();
    }
  }

  if (stats != nullptr) {
    stats->flush_ms = millis() - start_ms;
  }
}

}  // namespace platform
}  // namespace chromance
