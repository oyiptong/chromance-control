#pragma once

#include <stdint.h>

#include "kv_store.h"

namespace chromance {
namespace core {

class ModeSetting {
 public:
  void begin(IKeyValueStore& store, const char* key, uint8_t default_mode) {
    const uint8_t fallback = sanitize(default_mode);
    uint8_t raw = fallback;
    if (key != nullptr) {
      uint8_t v = 0;
      if (store.read_u8(key, &v)) {
        raw = v;
      }
    }

    mode_ = sanitize(raw);
    if (key != nullptr) {
      (void)store.write_u8(key, mode_);
    }
  }

  uint8_t mode() const { return mode_; }

  void set_mode(IKeyValueStore& store, const char* key, uint8_t mode) {
    mode_ = sanitize(mode);
    if (key != nullptr) {
      (void)store.write_u8(key, mode_);
    }
  }

  static uint8_t sanitize(uint8_t mode) {
    // Runtime patterns are bound to numeric modes for persistence.
    // Keep this range check conservative to avoid bricking the control path.
    if (mode < 1) return 1;
    if (mode > 7) return 1;
    return mode;
  }

 private:
  uint8_t mode_ = 1;
};

}  // namespace core
}  // namespace chromance
