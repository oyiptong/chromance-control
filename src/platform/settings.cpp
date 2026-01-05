#include "settings.h"

#include <Preferences.h>

namespace chromance {
namespace platform {

namespace {

constexpr char kNamespace[] = "chromance";
constexpr char kBrightnessKey[] = "bright_pct";
constexpr char kModeKey[] = "mode";

class PreferencesStore final : public chromance::core::IKeyValueStore {
 public:
  explicit PreferencesStore(Preferences* prefs) : prefs_(prefs) {}

  bool read_u8(const char* key, uint8_t* out) const override {
    if (prefs_ == nullptr || key == nullptr || out == nullptr) {
      return false;
    }
    if (!prefs_->isKey(key)) {
      return false;
    }
    *out = prefs_->getUChar(key, 0);
    return true;
  }

  bool write_u8(const char* key, uint8_t value) override {
    if (prefs_ == nullptr || key == nullptr) {
      return false;
    }
    return prefs_->putUChar(key, value) > 0;
  }

 private:
  Preferences* prefs_ = nullptr;
};

Preferences prefs;

}  // namespace

void RuntimeSettings::begin() {
  prefs.begin(kNamespace, false);
  PreferencesStore store(&prefs);
  brightness_.begin(store, kBrightnessKey, 100);
  mode_.begin(store, kModeKey, 1);
}

void RuntimeSettings::set_brightness_percent(uint8_t percent) {
  PreferencesStore store(&prefs);
  brightness_.set_percent(store, kBrightnessKey, percent);
}

void RuntimeSettings::set_mode(uint8_t mode) {
  PreferencesStore store(&prefs);
  mode_.set_mode(store, kModeKey, mode);
}

}  // namespace platform
}  // namespace chromance
