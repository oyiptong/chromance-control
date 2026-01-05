#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

class IKeyValueStore {
 public:
  virtual ~IKeyValueStore() = default;
  virtual bool read_u8(const char* key, uint8_t* out) const = 0;
  virtual bool write_u8(const char* key, uint8_t value) = 0;
};

}  // namespace core
}  // namespace chromance

