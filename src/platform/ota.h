#pragma once

#include <stdint.h>

namespace chromance {
namespace platform {

class OtaManager {
 public:
  bool begin(const char* firmware_version);
  void handle();
  bool is_updating() const { return updating_; }

 private:
  enum class WifiState : uint8_t { Disabled, Connecting, Connected, Failed };

  WifiState wifi_state_ = WifiState::Disabled;
  bool ota_started_ = false;
  bool updating_ = false;
  uint32_t wifi_start_ms_ = 0;
  const char* firmware_version_ = nullptr;
};

}  // namespace platform
}  // namespace chromance

