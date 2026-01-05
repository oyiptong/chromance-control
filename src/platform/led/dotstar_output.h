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
  void show_strips(const chromance::core::Rgb* const* rgb_by_strip,
                   const size_t* len_by_strip,
                   size_t strip_count,
                   PerfStats* stats) override;

 private:
  Adafruit_DotStar* strips_[core::kStripCount] = {nullptr, nullptr, nullptr, nullptr};
  uint16_t strip_used_len_[core::kStripCount] = {0, 0, 0, 0};
};

}  // namespace platform
}  // namespace chromance
