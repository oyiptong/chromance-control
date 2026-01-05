#include "dotstar_output.h"

#include <Arduino.h>

#include "core/mapping/mapping_tables.h"

namespace chromance {
namespace platform {

namespace {

constexpr uint8_t kDotstarColorOrder = DOTSTAR_BRG;
constexpr uint8_t kDotstarBrightness = 255;

Adafruit_DotStar* make_strip(uint16_t led_count, const core::StripConfig& cfg) {
  return new Adafruit_DotStar(led_count,
                              cfg.data_pin,
                              cfg.clock_pin,
                              kDotstarColorOrder);
}

}  // namespace

void DotstarOutput::begin() {
  for (uint8_t i = 0; i < core::kStripCount; ++i) {
    strip_used_len_[i] = 0;
  }

  const uint16_t expected = core::MappingTables::led_count();
  const uint8_t* g2s = core::MappingTables::global_to_strip();
  const uint16_t* g2l = core::MappingTables::global_to_local();
  for (uint16_t i = 0; i < expected; ++i) {
    const uint8_t strip = g2s[i];
    const uint16_t local = g2l[i];
    if (strip >= core::kStripCount) {
      continue;
    }
    const uint16_t needed = static_cast<uint16_t>(local + 1);
    if (needed > strip_used_len_[strip]) {
      strip_used_len_[strip] = needed;
    }
  }

  for (uint8_t i = 0; i < core::kStripCount; ++i) {
    if (strip_used_len_[i] == 0) {
      continue;
    }
    if (strips_[i] == nullptr) {
      strips_[i] = make_strip(strip_used_len_[i], core::kStripConfigs[i]);
    }
    if (strips_[i] == nullptr) {
      continue;
    }
    strips_[i]->begin();
    strips_[i]->setBrightness(kDotstarBrightness);
    for (uint16_t p = 0; p < strip_used_len_[i]; ++p) {
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
    if (strip_used_len_[strip] == 0) {
      continue;
    }
    if (local >= strip_used_len_[strip]) {
      continue;
    }
    if (strips_[strip] == nullptr) {
      continue;
    }
    strips_[strip]->setPixelColor(local, rgb[i].r, rgb[i].g, rgb[i].b);
  }

  for (uint8_t strip = 0; strip < core::kStripCount; ++strip) {
    if (strip_used_len_[strip] != 0 && strips_[strip] != nullptr) {
      strips_[strip]->show();
    }
  }

  if (stats != nullptr) {
    stats->flush_ms = millis() - start_ms;
  }
}

void DotstarOutput::show_strips(const chromance::core::Rgb* const* rgb_by_strip,
                                const size_t* len_by_strip,
                                size_t strip_count,
                                PerfStats* stats) {
  if (rgb_by_strip == nullptr || len_by_strip == nullptr) {
    return;
  }

  const uint32_t start_ms = millis();
  const size_t n = strip_count < core::kStripCount ? strip_count : core::kStripCount;
  for (uint8_t strip = 0; strip < n; ++strip) {
    if (strip_used_len_[strip] == 0 || strips_[strip] == nullptr) {
      continue;
    }
    const chromance::core::Rgb* buf = rgb_by_strip[strip];
    const size_t len = len_by_strip[strip];

    for (uint16_t p = 0; p < strip_used_len_[strip]; ++p) {
      if (buf != nullptr && p < len) {
        strips_[strip]->setPixelColor(p, buf[p].r, buf[p].g, buf[p].b);
      } else {
        strips_[strip]->setPixelColor(p, 0, 0, 0);
      }
    }
  }

  for (uint8_t strip = 0; strip < n; ++strip) {
    if (strip_used_len_[strip] != 0 && strips_[strip] != nullptr) {
      strips_[strip]->show();
    }
  }

  if (stats != nullptr) {
    stats->flush_ms = millis() - start_ms;
  }
}

}  // namespace platform
}  // namespace chromance
