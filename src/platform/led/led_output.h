#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/types.h"

namespace chromance {
namespace platform {

struct PerfStats {
  uint32_t flush_ms;
  uint32_t frame_ms;
};

class ILedOutput {
 public:
  virtual ~ILedOutput() = default;
  virtual void begin() = 0;
  virtual void show(const chromance::core::Rgb* rgb, size_t len, PerfStats* stats) = 0;

  // Optional: per-strip API (strip-indexed). Implementations may ignore this.
  virtual void show_strips(const chromance::core::Rgb* const* /*rgb_by_strip*/,
                           const size_t* /*len_by_strip*/,
                           size_t /*strip_count*/,
                           PerfStats* /*stats*/) {}
};

}  // namespace platform
}  // namespace chromance
