#pragma once

#include <stdint.h>

#include "signals.h"

namespace chromance {
namespace core {

class ModulationProvider {
 public:
  virtual ~ModulationProvider() = default;

  // Populate modulation signals for the current frame. Default values represent "not provided".
  virtual void get_signals(uint32_t now_ms, Signals* out) const = 0;
};

class NullModulationProvider final : public ModulationProvider {
 public:
  void get_signals(uint32_t /*now_ms*/, Signals* out) const override {
    if (out != nullptr) {
      *out = Signals{};
    }
  }
};

}  // namespace core
}  // namespace chromance

