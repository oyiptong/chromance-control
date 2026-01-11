#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chromance {
namespace core {

// Per-effect persisted config payload cap (bytes).
// Effects must enforce their config structs remain within this bound.
constexpr size_t kMaxEffectConfigSize = 64;

// Platform-agnostic persistence interface for small fixed-size blobs.
// Implementations must not partially fill the output buffer.
class ISettingsStore {
 public:
  virtual ~ISettingsStore() = default;

  // Returns false when key is missing or on read failure.
  virtual bool read_blob(const char* key, void* out, size_t out_size) const = 0;

  // Returns false on write failure.
  virtual bool write_blob(const char* key, const void* data, size_t size) = 0;
};

}  // namespace core
}  // namespace chromance

