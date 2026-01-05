#pragma once

#include <stdint.h>

#include "core/settings/brightness_setting.h"
#include "core/settings/mode_setting.h"

namespace chromance {
namespace platform {

class RuntimeSettings {
 public:
  void begin();

  uint8_t brightness_percent() const { return brightness_.percent(); }
  void set_brightness_percent(uint8_t percent);

  uint8_t mode() const { return mode_.mode(); }
  void set_mode(uint8_t mode);

 private:
  chromance::core::BrightnessSetting brightness_;
  chromance::core::ModeSetting mode_;
};

}  // namespace platform
}  // namespace chromance
