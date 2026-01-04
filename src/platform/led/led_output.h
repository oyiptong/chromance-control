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
};

}  // namespace platform
}  // namespace chromance

