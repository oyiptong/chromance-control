#include <Arduino.h>

#include "core/brightness.h"
#include "core/brightness_config.h"
#include "core/effects/pattern_coord_color.h"
#include "core/effects/pattern_hrv_hexagon.h"
#include "core/effects/pattern_index_walk.h"
#include "core/effects/pattern_rainbow_pulse.h"
#include "core/effects/pattern_strip_segment_stepper.h"
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
chromance::core::StripSegmentStepperEffect strip_segment_stepper{1000};
chromance::core::CoordColorEffect coord_color;
chromance::core::RainbowPulseEffect rainbow_pulse{700, 2000, 700};
chromance::core::TwoDotsEffect two_dots{25};
chromance::core::HrvHexagonEffect hrv_hexagon;
chromance::core::IEffect* current_effect = &index_walk;
uint8_t current_mode = 1;

chromance::core::FrameScheduler scheduler{50};  // 20ms default
chromance::core::NullModulationProvider modulation;

uint32_t last_render_ms = 0;
uint32_t last_stats_ms = 0;
uint16_t last_banner_led = 0xFFFF;
uint32_t last_banner_ms = 0;
uint8_t last_hrv_hex = 0xFF;
uint8_t last_strip_segment_k = 0xFF;
bool mode2_hold = false;

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

void select_mode(uint8_t mode) {
  const uint8_t safe_mode = chromance::core::ModeSetting::sanitize(mode);
  settings.set_mode(safe_mode);

  chromance::core::IEffect* effect = nullptr;
  switch (safe_mode) {
    case 1:
      effect = &index_walk;
      break;
    case 2:
      effect = &strip_segment_stepper;
      break;
    case 3:
      effect = &coord_color;
      break;
    case 4:
      effect = &rainbow_pulse;
      break;
    case 5:
      effect = &two_dots;
      break;
    case 6:
      effect = &hrv_hexagon;
      break;
    default:
      effect = &index_walk;
      break;
  }

  if (effect == nullptr) {
    effect = &index_walk;
  }

  current_mode = safe_mode;
  current_effect = effect;
  current_effect->reset(millis());
  last_banner_led = 0xFFFF;
  last_hrv_hex = 0xFF;
  last_strip_segment_k = 0xFF;
  mode2_hold = false;
  Serial.print("Mode ");
  Serial.print(static_cast<unsigned>(current_mode));
  Serial.print(": ");
  Serial.println(current_effect->id());
}

void print_hrv_hexagon_state() {
  Serial.print("HRV hexagon: hex=");
  Serial.print(static_cast<unsigned>(hrv_hexagon.current_hex_index()));
  Serial.print(" segs=[");
  const uint8_t* segs = hrv_hexagon.current_segments();
  const uint8_t n = hrv_hexagon.current_segment_count();
  for (uint8_t i = 0; i < n; ++i) {
    if (i) Serial.print(",");
    Serial.print(static_cast<unsigned>(segs[i]));
  }
  Serial.print("] color=(");
  const auto c = hrv_hexagon.current_color();
  Serial.print(static_cast<unsigned>(c.r));
  Serial.print(",");
  Serial.print(static_cast<unsigned>(c.g));
  Serial.print(",");
  Serial.print(static_cast<unsigned>(c.b));
  Serial.println(")");
}

uint16_t find_first_led_for_strip_segment(uint8_t strip, uint8_t seg_in_strip_0) {
  const uint8_t* strips = chromance::core::MappingTables::global_to_strip();
  const uint16_t* locals = chromance::core::MappingTables::global_to_local();
  for (uint16_t i = 0; i < kLedCount; ++i) {
    if (strips[i] != strip) continue;
    const uint16_t seg = static_cast<uint16_t>(locals[i] / chromance::core::kLedsPerSegment);
    if (seg == seg_in_strip_0) return i;
  }
  return 0xFFFF;
}

void print_strip_segment_stepper_state() {
  const uint8_t k = strip_segment_stepper.segment_number();
  Serial.print("Strip segment stepper: k=");
  Serial.print(static_cast<unsigned>(k));

  for (uint8_t strip = 0; strip < 4; ++strip) {
    const uint16_t i = find_first_led_for_strip_segment(strip, static_cast<uint8_t>(k - 1U));
    Serial.print(" strip");
    Serial.print(static_cast<unsigned>(strip));
    Serial.print("=");
    if (i == 0xFFFF) {
      Serial.print("-");
      continue;
    }

    const uint8_t seg = chromance::core::MappingTables::global_to_seg()[i];
    const uint8_t dir = chromance::core::MappingTables::global_to_dir()[i];
    Serial.print(static_cast<unsigned>(seg));
    Serial.print("(");
    Serial.print(dir ? "b_to_a" : "a_to_b");
    Serial.print(")");
  }
  Serial.println();
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
      "Commands: 1=Index_Walk_Test 2=Strip_Segment_Stepper 3=Coord_Color_Test 4=Rainbow_Pulse 5=Seven_Comets 6=HRV_hexagon n=next(mode2/mode6) esc=resume_auto(mode2) +=brightness_up -=brightness_down");
  Serial.print("Restored mode: ");
  Serial.println(static_cast<unsigned>(settings.mode()));
  print_brightness();
  select_mode(settings.mode());
}

void loop() {
  ota.handle();
  const uint32_t now_ms = millis();

  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '1') select_mode(1);
    if (c == '2') select_mode(2);
    if (c == '3') select_mode(3);
    if (c == '4') select_mode(4);
    if (c == '5') select_mode(5);
    if (c == '6') select_mode(6);
    if (c == 'n' || c == 'N') {
      if (current_mode == 2) {
        strip_segment_stepper.next(now_ms);
        strip_segment_stepper.set_auto_advance_enabled(false, now_ms);
        mode2_hold = true;
        last_strip_segment_k = 0xFF;
        print_strip_segment_stepper_state();
      } else if (current_mode == 6) {
        hrv_hexagon.force_next(millis());
        last_hrv_hex = 0xFF;
      }
    }
    if (c == 27) {  // ESC
      if (current_mode == 2) {
        strip_segment_stepper.set_auto_advance_enabled(true, now_ms);
        mode2_hold = false;
      }
    }
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

  if (current_effect == &strip_segment_stepper) {
    const uint8_t k = strip_segment_stepper.segment_number();
    if (k != last_strip_segment_k) {
      last_strip_segment_k = k;
      print_strip_segment_stepper_state();
    }
  }

  if (current_effect == &hrv_hexagon) {
    const uint8_t h = hrv_hexagon.current_hex_index();
    if (h != last_hrv_hex) {
      last_hrv_hex = h;
      print_hrv_hexagon_state();
    }
  }

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

    Serial.print("mode=");
    Serial.print(static_cast<unsigned>(current_mode));
    Serial.print(" ");
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
