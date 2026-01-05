#pragma once

#include <stdint.h>

namespace chromance {
namespace platform {

class RuntimeSettings {
 public:
  void begin();

  uint8_t brightness_percent() const { return brightness_percent_; }
  void set_brightness_percent(uint8_t percent);

 private:
  uint8_t brightness_percent_ = 100;  // 0..100
};

}  // namespace platform
}  // namespace chromance

