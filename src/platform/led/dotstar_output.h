#pragma once

#include <stdint.h>

#include <Adafruit_DotStar.h>

#include "core/layout.h"
#include "led_output.h"

namespace chromance {
namespace platform {

class DotstarOutput final : public ILedOutput {
 public:
  DotstarOutput() = default;
  ~DotstarOutput() override = default;

  void begin() override;
  void show(const chromance::core::Rgb* rgb, size_t len, PerfStats* stats) override;

 private:
  Adafruit_DotStar* strips_[core::kStripCount] = {nullptr, nullptr, nullptr, nullptr};
};

}  // namespace platform
}  // namespace chromance
