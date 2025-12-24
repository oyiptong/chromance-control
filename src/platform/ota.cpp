#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "wifi_config.h"

namespace chromance {
namespace platform {

namespace {

bool wifi_credentials_present() {
  return WIFI_SSID[0] != '\0' && WIFI_PASSWORD[0] != '\0';
}

constexpr uint32_t kWifiConnectTimeoutMs = 15000;

}  // namespace

bool OtaManager::begin(const char* firmware_version) {
  firmware_version_ = firmware_version;

  if (!wifi_credentials_present()) {
    Serial.println("WiFi credentials missing (WIFI_SSID/WIFI_PASSWORD); OTA disabled.");
    wifi_state_ = WifiState::Disabled;
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  wifi_state_ = WifiState::Connecting;
  wifi_start_ms_ = millis();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([this]() {
    updating_ = true;
    Serial.print("OTA start (");
    Serial.print(firmware_version_ ? firmware_version_ : "unknown");
    Serial.println(")");
  });
  ArduinoOTA.onEnd([this]() {
    updating_ = false;
    Serial.println("OTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint32_t last_print_ms = 0;
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - last_print_ms) < 500) {
      return;
    }
    last_print_ms = now;
    Serial.printf("OTA progress: %u%%\n", (progress * 100U) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error: %u\n", static_cast<unsigned>(error));
  });

  return true;
}

void OtaManager::handle() {
  switch (wifi_state_) {
    case WifiState::Disabled:
    case WifiState::Failed:
      return;

    case WifiState::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        wifi_state_ = WifiState::Connected;
        Serial.print("WiFi connected: ");
        Serial.println(WiFi.localIP());
      } else if (static_cast<int32_t>(millis() - wifi_start_ms_) > kWifiConnectTimeoutMs) {
        wifi_state_ = WifiState::Failed;
        Serial.println("WiFi connect timeout; OTA disabled.");
        return;
      } else {
        return;
      }
      // fallthrough

    case WifiState::Connected:
      if (!ota_started_) {
        ArduinoOTA.begin();
        ota_started_ = true;
        Serial.print("OTA ready (hostname: ");
        Serial.print(OTA_HOSTNAME);
        Serial.println(")");
      }
      ArduinoOTA.handle();
      return;
  }
}

}  // namespace platform
}  // namespace chromance
