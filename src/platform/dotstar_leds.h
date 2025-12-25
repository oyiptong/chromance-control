#pragma once

#include <stdint.h>

#include <Adafruit_DotStar.h>

#include "core/diagnostic_pattern.h"
#include "core/layout.h"

namespace chromance {
namespace platform {

class DotstarLeds : public core::IDiagnosticRenderer {
 public:
  DotstarLeds() = default;

  void begin();
  void clear_all();
  void show_all();

  void set_segment_all(uint8_t strip_index,
                       uint16_t segment_index,
                       core::Rgb color,
                       bool on) override;
  void set_segment_single_led(uint8_t strip_index,
                              uint16_t segment_index,
                              uint8_t led_in_segment,
                              core::Rgb color) override;

 private:
  Adafruit_DotStar* strips_[core::kStripCount] = {nullptr, nullptr, nullptr, nullptr};
};

}  // namespace platform
}  // namespace chromance
