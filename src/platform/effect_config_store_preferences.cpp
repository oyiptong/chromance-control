#include "effect_config_store_preferences.h"

namespace chromance {
namespace platform {

namespace {
constexpr char kNamespace[] = "chromance";
}  // namespace

void PreferencesSettingsStore::begin() {
  prefs_.begin(kNamespace, false);
}

bool PreferencesSettingsStore::read_blob(const char* key, void* out, size_t out_size) const {
  if (key == nullptr || out == nullptr || out_size == 0) {
    return false;
  }
  if (!prefs_.isKey(key)) {
    return false;
  }
  const size_t stored_len = prefs_.getBytesLength(key);
  if (stored_len != out_size) {
    return false;
  }
  const size_t read_len = prefs_.getBytes(key, out, out_size);
  return read_len == out_size;
}

bool PreferencesSettingsStore::write_blob(const char* key, const void* data, size_t size) {
  if (key == nullptr || data == nullptr || size == 0) {
    return false;
  }
  const size_t written = prefs_.putBytes(key, data, size);
  return written == size;
}

}  // namespace platform
}  // namespace chromance

