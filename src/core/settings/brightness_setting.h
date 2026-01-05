#pragma once

#include <stdint.h>

#include "../brightness.h"
#include "kv_store.h"

namespace chromance {
namespace core {

class BrightnessSetting {
 public:
  void begin(IKeyValueStore& store, const char* key, uint8_t default_percent = 100) {
    const uint8_t fallback = quantize_percent_to_10(default_percent);
    uint8_t raw = fallback;
    if (key != nullptr) {
      uint8_t v = 0;
      if (store.read_u8(key, &v)) {
        raw = v;
      }
    }

    percent_ = quantize_percent_to_10(raw);
    if (key != nullptr) {
      (void)store.write_u8(key, percent_);
    }
  }

  uint8_t percent() const { return percent_; }

  void set_percent(IKeyValueStore& store, const char* key, uint8_t percent) {
    percent_ = quantize_percent_to_10(percent);
    if (key != nullptr) {
      (void)store.write_u8(key, percent_);
    }
  }

 private:
  uint8_t percent_ = 100;
};

}  // namespace core
}  // namespace chromance
