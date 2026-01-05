#pragma once

#include <stdint.h>

#include "core/settings/brightness_setting.h"

namespace chromance {
namespace platform {

class RuntimeSettings {
 public:
  void begin();

  uint8_t brightness_percent() const { return brightness_.percent(); }
  void set_brightness_percent(uint8_t percent);

 private:
  chromance::core::BrightnessSetting brightness_;
};

}  // namespace platform
}  // namespace chromance
