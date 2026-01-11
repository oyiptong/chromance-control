#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>

#include "core/settings/effect_config_store.h"

namespace chromance {
namespace platform {

class PreferencesSettingsStore final : public chromance::core::ISettingsStore {
 public:
  void begin();

  bool read_blob(const char* key, void* out, size_t out_size) const override;
  bool write_blob(const char* key, const void* data, size_t size) override;

 private:
  mutable Preferences prefs_;
};

}  // namespace platform
}  // namespace chromance
