#include <Arduino.h>

#include "core/diagnostic_pattern.h"
#include "platform/dotstar_leds.h"
#include "platform/ota.h"

#if defined(__has_include)
#if __has_include(<esp_ota_ops.h>)
#include <esp_ota_ops.h>
#define CHROMANCE_HAS_ESP_OTA_OPS 1
#else
#define CHROMANCE_HAS_ESP_OTA_OPS 0
#endif
#else
#define CHROMANCE_HAS_ESP_OTA_OPS 0
#endif

namespace {

constexpr char kFirmwareVersion[] = "diagnostic-0.1.0";

chromance::platform::DotstarLeds leds;
chromance::platform::OtaManager ota;
chromance::core::DiagnosticPattern pattern;

constexpr uint32_t kFrameMs = 20;
uint32_t last_render_ms = 0;

void maybe_mark_app_valid() {
#if CHROMANCE_HAS_ESP_OTA_OPS
  const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    Serial.println("OTA: app marked valid (cancel rollback)");
  } else {
    Serial.printf("OTA: mark valid failed (%d)\n", static_cast<int>(err));
  }
#else
  Serial.println("OTA: esp_ota_mark_app_valid_cancel_rollback unavailable");
#endif
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Chromance Control boot: ");
  Serial.println(kFirmwareVersion);

  leds.begin();
  leds.clear_all();
  leds.show_all();

  ota.begin(kFirmwareVersion);
  pattern.reset(millis());

  maybe_mark_app_valid();
}

void loop() {
  ota.handle();

  const uint32_t now_ms = millis();
  pattern.tick(now_ms);

  const uint32_t frame_ms = ota.is_updating() ? 100 : kFrameMs;
  if (static_cast<int32_t>(now_ms - last_render_ms) < static_cast<int32_t>(frame_ms)) {
    return;
  }
  last_render_ms = now_ms;

  pattern.render(leds);
  leds.show_all();
}

