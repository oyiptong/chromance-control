#include "settings.h"

#include <Preferences.h>

namespace chromance {
namespace platform {

namespace {

constexpr char kNamespace[] = "chromance";
constexpr char kBrightnessKey[] = "bright_pct";

Preferences prefs;

uint8_t clamp_0_100(uint8_t v) {
  if (v > 100) return 100;
  return v;
}

uint8_t round_to_10(uint8_t v) {
  v = clamp_0_100(v);
  const uint8_t rounded = static_cast<uint8_t>(((static_cast<uint16_t>(v) + 5U) / 10U) * 10U);
  return clamp_0_100(rounded);
}

}  // namespace

void RuntimeSettings::begin() {
  prefs.begin(kNamespace, false);
  brightness_percent_ = round_to_10(prefs.getUChar(kBrightnessKey, 100));
  prefs.putUChar(kBrightnessKey, brightness_percent_);
}

void RuntimeSettings::set_brightness_percent(uint8_t percent) {
  brightness_percent_ = round_to_10(percent);
  prefs.putUChar(kBrightnessKey, brightness_percent_);
}

}  // namespace platform
}  // namespace chromance

