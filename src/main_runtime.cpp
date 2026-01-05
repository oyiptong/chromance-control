#include <Arduino.h>

#include "core/brightness.h"
#include "core/brightness_config.h"
#include "core/effects/pattern_coord_color.h"
#include "core/effects/pattern_index_walk.h"
#include "core/effects/pattern_rainbow_pulse.h"
#include "core/effects/pattern_two_dots.h"
#include "core/effects/pattern_xy_scan.h"
#include "core/effects/frame_scheduler.h"
#include "core/effects/modulation_provider.h"
#include "core/mapping/mapping_tables.h"
#include "core/mapping/pixels_map.h"
#include "platform/led/dotstar_output.h"
#include "platform/ota.h"
#include "platform/settings.h"

namespace {

constexpr char kFirmwareVersion[] = "runtime-0.1.0";

chromance::platform::DotstarOutput led_out;
chromance::platform::OtaManager ota;
chromance::platform::RuntimeSettings settings;

chromance::core::PixelsMap pixels_map;

constexpr size_t kLedCount = chromance::core::MappingTables::led_count();
chromance::core::Rgb rgb[kLedCount];
uint16_t scan_order[kLedCount];

chromance::core::EffectParams params;

chromance::core::IndexWalkEffect index_walk{25};
chromance::core::XyScanEffect xy_scan{scan_order, kLedCount, 25};
chromance::core::CoordColorEffect coord_color;
chromance::core::RainbowPulseEffect rainbow_pulse{700, 2000, 700};
chromance::core::TwoDotsEffect two_dots{25};
chromance::core::IEffect* current_effect = &index_walk;

chromance::core::FrameScheduler scheduler{50};  // 20ms default
chromance::core::NullModulationProvider modulation;

uint32_t last_render_ms = 0;
uint32_t last_stats_ms = 0;
uint16_t last_banner_led = 0xFFFF;
uint32_t last_banner_ms = 0;

void print_brightness() {
  const uint8_t soft = settings.brightness_percent();
  const uint8_t ceiling = chromance::core::kHardwareBrightnessCeilingPercent;
  const uint8_t effective = chromance::core::soft_percent_to_hw_percent(soft, ceiling);

  Serial.print("Brightness: ");
  Serial.print(static_cast<unsigned>(soft));
  Serial.print("% (ceiling=");
  Serial.print(static_cast<unsigned>(ceiling));
  Serial.print("%, effective=");
  Serial.print(static_cast<unsigned>(effective));
  Serial.println("%) (+/- moves 10% of ceiling, persisted)");
}

void set_brightness_percent(uint8_t percent) {
  settings.set_brightness_percent(percent);
  params.brightness = chromance::core::soft_percent_to_u8_255(
      settings.brightness_percent(), chromance::core::kHardwareBrightnessCeilingPercent);
  print_brightness();
}

void select_effect(chromance::core::IEffect* effect) {
  if (effect == nullptr) {
    return;
  }
  current_effect = effect;
  current_effect->reset(millis());
  last_banner_led = 0xFFFF;
  Serial.print("Mode: ");
  Serial.println(current_effect->id());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Chromance Control boot: ");
  Serial.println(kFirmwareVersion);
  Serial.print("Mapping version: ");
  Serial.println(chromance::core::MappingTables::mapping_version());
  Serial.print("Bench subset: ");
  Serial.println(chromance::core::MappingTables::is_bench_subset() ? "true" : "false");
  Serial.print("LED_COUNT: ");
  Serial.println(static_cast<unsigned>(chromance::core::MappingTables::led_count()));

  pixels_map.build_scan_order(scan_order, kLedCount);

  led_out.begin();
  ota.begin(kFirmwareVersion);
  scheduler.reset(millis());

  settings.begin();
  params = chromance::core::EffectParams{};
  params.brightness = chromance::core::soft_percent_to_u8_255(
      settings.brightness_percent(), chromance::core::kHardwareBrightnessCeilingPercent);

  Serial.println(
      "Commands: 1=Index_Walk_Test 2=XY_Scan_Test 3=Coord_Color_Test 4=Rainbow_Pulse 5=Two_Dots +=brightness_up -=brightness_down");
  print_brightness();
  select_effect(&index_walk);
}

void loop() {
  ota.handle();
  const uint32_t now_ms = millis();

  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '1') select_effect(&index_walk);
    if (c == '2') select_effect(&xy_scan);
    if (c == '3') select_effect(&coord_color);
    if (c == '4') select_effect(&rainbow_pulse);
    if (c == '5') select_effect(&two_dots);
    if (c == '+') {
      set_brightness_percent(
          chromance::core::brightness_step_up_10(settings.brightness_percent()));
    }
    if (c == '-') {
      set_brightness_percent(
          chromance::core::brightness_step_down_10(settings.brightness_percent()));
    }
  }

  const uint32_t frame_ms = ota.is_updating() ? 100 : 20;
  scheduler.set_target_fps(frame_ms ? static_cast<uint16_t>(1000U / frame_ms) : 0);
  if (!scheduler.should_render(now_ms)) return;
  last_render_ms = now_ms;

  chromance::platform::PerfStats stats{0, 0};
  chromance::core::Signals signals;
  modulation.get_signals(now_ms, &signals);
  chromance::core::EffectFrame frame;
  frame.now_ms = now_ms;
  frame.dt_ms = scheduler.dt_ms();
  frame.signals = signals;
  frame.params = params;
  current_effect->render(frame, pixels_map, rgb, kLedCount);
  const uint32_t frame_start_ms = millis();
  led_out.show(rgb, kLedCount, &stats);
  stats.frame_ms = millis() - frame_start_ms;

  // Print a small banner for note-taking while physically validating wiring.
  // Throttle to avoid spamming the UART.
  if (current_effect == &index_walk) {
    uint16_t lit = 0xFFFF;
    for (uint16_t i = 0; i < kLedCount; ++i) {
      if (rgb[i].r || rgb[i].g || rgb[i].b) {
        lit = i;
        break;
      }
    }
    if (lit != 0xFFFF && lit != last_banner_led &&
        static_cast<int32_t>(now_ms - last_banner_ms) >= 250) {
      last_banner_led = lit;
      last_banner_ms = now_ms;

      const uint8_t strip = chromance::core::MappingTables::global_to_strip()[lit];
      const uint16_t local = chromance::core::MappingTables::global_to_local()[lit];
      const uint8_t seg = chromance::core::MappingTables::global_to_seg()[lit];
      const uint8_t k = chromance::core::MappingTables::global_to_seg_k()[lit];
      const uint8_t dir = chromance::core::MappingTables::global_to_dir()[lit];

      const uint16_t seg_in_strip = static_cast<uint16_t>(local / chromance::core::kLedsPerSegment);
      const uint8_t local_in_seg = static_cast<uint8_t>(local % chromance::core::kLedsPerSegment);

      Serial.print("i=");
      Serial.print(lit);
      Serial.print(" seg=");
      Serial.print(seg);
      Serial.print(" k=");
      Serial.print(k);
      Serial.print(" dir=");
      Serial.print(dir ? "b_to_a" : "a_to_b");
      Serial.print(" strip=");
      Serial.print(strip);
      Serial.print(" local=");
      Serial.print(local);
      Serial.print(" segInStrip=");
      Serial.print(seg_in_strip);
      Serial.print(" localInSeg=");
      Serial.println(local_in_seg);
    }
  }

  if (static_cast<int32_t>(now_ms - last_stats_ms) >= 1000) {
    last_stats_ms = now_ms;
    const uint8_t soft = settings.brightness_percent();
    const uint8_t ceiling = chromance::core::kHardwareBrightnessCeilingPercent;
    const uint8_t effective = chromance::core::soft_percent_to_hw_percent(soft, ceiling);

    Serial.print("soft_brightness_pct=");
    Serial.print(static_cast<unsigned>(soft));
    Serial.print(" hw_ceiling_pct=");
    Serial.print(static_cast<unsigned>(ceiling));
    Serial.print(" effective_brightness_pct=");
    Serial.print(static_cast<unsigned>(effective));
    Serial.print(" ");
    Serial.print("flush_ms=");
    Serial.print(stats.flush_ms);
    Serial.print(" frame_ms=");
    Serial.println(stats.frame_ms);
  }
}
