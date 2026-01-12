#include <Arduino.h>
#include <WiFi.h>

#include "core/brightness.h"
#include "core/brightness_config.h"
#include "core/effects/effect_catalog.h"
#include "core/effects/effect_manager.h"
#include "core/effects/legacy_effect_adapter.h"
#include "core/effects/pattern_breathing_mode.h"
#include "core/effects/pattern_breathing_mode_v2.h"
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
#include "platform/effect_config_store_preferences.h"
#include "platform/settings.h"
#include "platform/webui_server.h"

namespace {

constexpr char kFirmwareVersion[] = "runtime-0.1.0";

chromance::platform::DotstarOutput led_out;
chromance::platform::OtaManager ota;
chromance::platform::RuntimeSettings settings;
chromance::platform::PreferencesSettingsStore effect_store;

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
chromance::core::BreathingEffect breathing;

constexpr size_t kMaxEffects = 32;
chromance::core::EffectCatalog<kMaxEffects> effect_catalog;
chromance::core::EffectManager<kMaxEffects> effect_manager;

chromance::platform::WebuiServer webui{kFirmwareVersion, &settings, &params, &effect_manager, &effect_catalog};
static bool webui_started = false;

constexpr chromance::core::EffectDescriptor kMode1Desc{chromance::core::EffectId{1}, "index_walk",
                                                       "Index_Walk_Test", nullptr};
constexpr chromance::core::EffectDescriptor kMode2Desc{chromance::core::EffectId{2},
                                                       "strip_segment_stepper",
                                                       "Strip segment stepper", nullptr};
constexpr chromance::core::EffectDescriptor kMode3Desc{chromance::core::EffectId{3}, "coord_color",
                                                       "Coord_Color_Test", nullptr};
constexpr chromance::core::EffectDescriptor kMode4Desc{chromance::core::EffectId{4}, "rainbow_pulse",
                                                       "Rainbow_Pulse", nullptr};
constexpr chromance::core::EffectDescriptor kMode5Desc{chromance::core::EffectId{5}, "seven_comets",
                                                       "Seven_Comets", nullptr};
constexpr chromance::core::EffectDescriptor kMode6Desc{chromance::core::EffectId{6}, "hrv_hexagon",
                                                       "HRV hexagon", nullptr};
constexpr chromance::core::EffectDescriptor kMode7Desc{chromance::core::EffectId{7}, "breathing",
                                                       "Breathing", nullptr};

chromance::core::LegacyEffectAdapter mode1_adapter{kMode1Desc, &index_walk};
chromance::core::LegacyEffectAdapter mode2_adapter{kMode2Desc, &strip_segment_stepper};
chromance::core::LegacyEffectAdapter mode3_adapter{kMode3Desc, &coord_color};
chromance::core::LegacyEffectAdapter mode4_adapter{kMode4Desc, &rainbow_pulse};
chromance::core::LegacyEffectAdapter mode5_adapter{kMode5Desc, &two_dots};
chromance::core::LegacyEffectAdapter mode6_adapter{kMode6Desc, &hrv_hexagon};
chromance::core::BreathingEffectV2 mode7_effect{kMode7Desc, &breathing};

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
uint8_t last_indexwalk_scan_mode = 0xFF;
uint8_t last_indexwalk_seg = 0xFF;
uint8_t last_indexwalk_vertex = 0xFF;

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
  effect_manager.set_global_params(params);
  print_brightness();
}

void reset_mode_print_state() {
  last_banner_led = 0xFFFF;
  last_hrv_hex = 0xFF;
  last_strip_segment_k = 0xFF;
  mode2_hold = false;
  last_indexwalk_scan_mode = 0xFF;
  last_indexwalk_seg = 0xFF;
  last_indexwalk_vertex = 0xFF;
  last_banner_ms = 0;
}

void select_mode(uint8_t mode) {
  const uint8_t safe_mode = chromance::core::ModeSetting::sanitize(mode);
  settings.set_mode(safe_mode);
  const uint32_t now_ms = millis();
  if (!effect_manager.set_active(chromance::core::EffectId{safe_mode}, now_ms)) {
    (void)effect_manager.set_active(chromance::core::EffectId{1}, now_ms);
  }
  current_mode =
      chromance::core::ModeSetting::sanitize(static_cast<uint8_t>(effect_manager.active_id().value));
  settings.set_mode(current_mode);
  reset_mode_print_state();
  Serial.print("Mode ");
  Serial.print(static_cast<unsigned>(current_mode));
  Serial.print(": ");
  const chromance::core::IEffectV2* e = effect_manager.active();
  Serial.println(e ? e->descriptor().display_name : "?");
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

void print_index_walk_scan_mode() {
  Serial.print("Index walk scan mode: ");
  Serial.println(index_walk.scan_mode_name());
}

void print_index_walk_vertex_state() {
  Serial.print("Index walk vertex: ");
  Serial.print("V");
  Serial.print(static_cast<unsigned>(index_walk.active_vertex_id()));
  Serial.print(" (");
  const uint8_t vid = index_walk.active_vertex_id();
  const int8_t vx = chromance::core::MappingTables::vertex_vx()[vid];
  const int8_t vy = chromance::core::MappingTables::vertex_vy()[vid];
  Serial.print(static_cast<int>(vx));
  Serial.print(",");
  Serial.print(static_cast<int>(vy));
  Serial.print(") segs=[");
  const uint8_t n = index_walk.active_vertex_seg_count();
  const uint8_t* segs = index_walk.active_vertex_segs();
  for (uint8_t i = 0; i < n; ++i) {
    if (i) Serial.print(",");
    Serial.print(static_cast<unsigned>(segs[i]));
  }
  Serial.println("]");
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
  effect_store.begin();

  params = chromance::core::EffectParams{};
  params.brightness = chromance::core::soft_percent_to_u8_255(
      settings.brightness_percent(), chromance::core::kHardwareBrightnessCeilingPercent);
  effect_manager.set_global_params(params);

  (void)effect_catalog.add(mode1_adapter.descriptor(), &mode1_adapter);
  (void)effect_catalog.add(mode2_adapter.descriptor(), &mode2_adapter);
  (void)effect_catalog.add(mode3_adapter.descriptor(), &mode3_adapter);
  (void)effect_catalog.add(mode4_adapter.descriptor(), &mode4_adapter);
  (void)effect_catalog.add(mode5_adapter.descriptor(), &mode5_adapter);
  (void)effect_catalog.add(mode6_adapter.descriptor(), &mode6_adapter);
  (void)effect_catalog.add(mode7_effect.descriptor(), &mode7_effect);

  Serial.println(
      "Commands: 1=Index_Walk_Test 2=Strip_Segment_Stepper 3=Coord_Color_Test 4=Rainbow_Pulse 5=Seven_Comets 6=HRV_hexagon 7=Breathing n=next(mode1/2/6/7) N=prev(mode2/6/7) s/S=step(mode1) lane(mode7 manual inhale) esc=auto(mode1/2/6/7) +=brightness_up -=brightness_down");
  Serial.print("Restored mode: ");
  Serial.println(static_cast<unsigned>(settings.mode()));
  print_brightness();

  const uint8_t safe_mode = chromance::core::ModeSetting::sanitize(settings.mode());
  effect_manager.init(effect_store, effect_catalog, pixels_map, millis(), chromance::core::EffectId{safe_mode});
  current_mode = chromance::core::ModeSetting::sanitize(static_cast<uint8_t>(effect_manager.active_id().value));
  settings.set_mode(current_mode);
  reset_mode_print_state();
  Serial.print("Restored effect: ");
  Serial.print(static_cast<unsigned>(effect_manager.active_id().value));
  Serial.print(" ");
  const chromance::core::IEffectV2* e = effect_manager.active();
  Serial.println(e ? e->descriptor().display_name : "?");
  Serial.print("Mode ");
  Serial.print(static_cast<unsigned>(current_mode));
  Serial.print(": ");
  Serial.println(e ? e->descriptor().display_name : "?");
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
    if (c == '7') select_mode(7);
    if (c == 'n') {
      if (current_mode == 1) {
        if (index_walk.in_vertex_mode()) {
          index_walk.vertex_next(now_ms);
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = 0;
          print_index_walk_vertex_state();
        } else {
          index_walk.cycle_scan_mode(now_ms);
          last_banner_led = 0xFFFF;
          last_indexwalk_scan_mode = 0xFF;
          last_indexwalk_seg = 0xFF;
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = 0;
          print_index_walk_scan_mode();
          if (index_walk.in_vertex_mode()) {
            print_index_walk_vertex_state();
          }
        }
      } else if (current_mode == 2) {
        strip_segment_stepper.next(now_ms);
        strip_segment_stepper.set_auto_advance_enabled(false, now_ms);
        mode2_hold = true;
        last_strip_segment_k = 0xFF;
        print_strip_segment_stepper_state();
      } else if (current_mode == 6) {
        hrv_hexagon.next(now_ms);
        last_hrv_hex = 0xFF;
      } else if (current_mode == 7) {
        chromance::core::InputEvent ev;
        ev.key = chromance::core::Key::N;
        ev.now_ms = now_ms;
        effect_manager.on_event(ev, now_ms);
      }
    }
    if (c == 'N') {
      if (current_mode == 1) {
        if (index_walk.in_vertex_mode()) {
          index_walk.vertex_prev(now_ms);
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = 0;
          print_index_walk_vertex_state();
        }
      } else if (current_mode == 2) {
        strip_segment_stepper.prev(now_ms);
        strip_segment_stepper.set_auto_advance_enabled(false, now_ms);
        mode2_hold = true;
        last_strip_segment_k = 0xFF;
        print_strip_segment_stepper_state();
      } else if (current_mode == 6) {
        hrv_hexagon.prev(now_ms);
        last_hrv_hex = 0xFF;
      } else if (current_mode == 7) {
        chromance::core::InputEvent ev;
        ev.key = chromance::core::Key::ShiftN;
        ev.now_ms = now_ms;
        effect_manager.on_event(ev, now_ms);
      }
    }
    if (c == 's') {
      if (current_mode == 7) {
        chromance::core::InputEvent ev;
        ev.key = chromance::core::Key::S;
        ev.now_ms = now_ms;
        effect_manager.on_event(ev, now_ms);
      } else if (current_mode == 1) {
        if (index_walk.in_vertex_mode()) {
          // Pause vertex selection (manual), but keep looping the fill animation.
          index_walk.clear_manual_hold(now_ms);
          index_walk.vertex_next(now_ms);
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = (now_ms >= 250) ? (now_ms - 250) : 0;
          print_index_walk_vertex_state();
        } else {
          index_walk.step_hold_next(now_ms);
          last_banner_led = 0xFFFF;
          last_indexwalk_scan_mode = 0xFF;
          last_indexwalk_seg = 0xFF;
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = (now_ms >= 250) ? (now_ms - 250) : 0;
        }
      }
    }
    if (c == 'S') {
      if (current_mode == 7) {
        chromance::core::InputEvent ev;
        ev.key = chromance::core::Key::ShiftS;
        ev.now_ms = now_ms;
        effect_manager.on_event(ev, now_ms);
      } else if (current_mode == 1) {
        if (index_walk.in_vertex_mode()) {
          index_walk.clear_manual_hold(now_ms);
          index_walk.vertex_prev(now_ms);
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = (now_ms >= 250) ? (now_ms - 250) : 0;
          print_index_walk_vertex_state();
        } else {
          index_walk.step_hold_prev(now_ms);
          last_banner_led = 0xFFFF;
          last_indexwalk_scan_mode = 0xFF;
          last_indexwalk_seg = 0xFF;
          last_indexwalk_vertex = 0xFF;
          last_banner_ms = (now_ms >= 250) ? (now_ms - 250) : 0;
        }
      }
    }
    if (c == 27) {  // ESC
      if (current_mode == 1) {
        index_walk.set_auto(now_ms);
        last_banner_led = 0xFFFF;
        last_indexwalk_scan_mode = 0xFF;
        last_indexwalk_seg = 0xFF;
        last_indexwalk_vertex = 0xFF;
        last_banner_ms = 0;
        print_index_walk_scan_mode();
      } else if (current_mode == 2) {
        strip_segment_stepper.set_auto_advance_enabled(true, now_ms);
        mode2_hold = false;
      } else if (current_mode == 7) {
        chromance::core::InputEvent ev;
        ev.key = chromance::core::Key::Esc;
        ev.now_ms = now_ms;
        effect_manager.on_event(ev, now_ms);
      } else if (current_mode == 6) {
        hrv_hexagon.set_auto(now_ms);
        last_hrv_hex = 0xFF;
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

  uint32_t frame_ms = ota.is_updating() ? 100 : 20;
  if (current_mode == 6) {
    frame_ms = 16;  // smoother fades for mode 6
  }
  if (current_mode == 7) {
    frame_ms = 16;
  }
  scheduler.set_target_fps(frame_ms ? static_cast<uint16_t>(1000U / frame_ms) : 0);

  if (WiFi.status() == WL_CONNECTED) {
    if (!webui_started) {
      webui.begin();
      webui_started = true;
    }
    webui.handle(now_ms, scheduler.next_frame_ms());
    if (webui.take_pending_restart()) {
      ESP.restart();
      return;
    }
  }

  if (!scheduler.should_render(now_ms)) return;
  last_render_ms = now_ms;

  chromance::platform::PerfStats stats{0, 0};
  chromance::core::Signals signals;
  modulation.get_signals(now_ms, &signals);
  effect_manager.tick(now_ms, scheduler.dt_ms(), signals);
  effect_manager.render(rgb, kLedCount);
  const uint32_t frame_start_ms = millis();
  led_out.show(rgb, kLedCount, &stats);
  stats.frame_ms = millis() - frame_start_ms;

  if (current_mode == 2) {
    const uint8_t k = strip_segment_stepper.segment_number();
    if (k != last_strip_segment_k) {
      last_strip_segment_k = k;
      print_strip_segment_stepper_state();
    }
  }

  if (current_mode == 6) {
    const uint8_t h = hrv_hexagon.current_hex_index();
    if (h != last_hrv_hex) {
      last_hrv_hex = h;
      print_hrv_hexagon_state();
    }
  }

  // Print a small banner for note-taking while physically validating wiring.
  // Throttle to avoid spamming the UART.
  if (current_mode == 1) {
    const uint16_t lit = index_walk.active_index();
    const uint8_t seg = index_walk.active_seg();
    const uint8_t scan_mode = static_cast<uint8_t>(index_walk.scan_mode());
    const uint8_t vertex = index_walk.active_vertex_id();
    if (static_cast<int32_t>(now_ms - last_banner_ms) >= 250 &&
        (lit != last_banner_led || seg != last_indexwalk_seg || scan_mode != last_indexwalk_scan_mode ||
         vertex != last_indexwalk_vertex)) {
      last_banner_led = lit;
      last_indexwalk_seg = seg;
      last_indexwalk_scan_mode = scan_mode;
      last_indexwalk_vertex = vertex;
      last_banner_ms = now_ms;

      if (index_walk.in_vertex_mode()) {
        print_index_walk_vertex_state();
        return;
      }

      const uint8_t strip = chromance::core::MappingTables::global_to_strip()[lit];
      const uint16_t local = chromance::core::MappingTables::global_to_local()[lit];
      const uint8_t k = chromance::core::MappingTables::global_to_seg_k()[lit];
      const uint8_t dir = chromance::core::MappingTables::global_to_dir()[lit];

      const uint16_t seg_in_strip =
          static_cast<uint16_t>(local / chromance::core::kLedsPerSegment);
      const uint8_t local_in_seg =
          static_cast<uint8_t>(local % chromance::core::kLedsPerSegment);

      Serial.print("IndexWalk ");
      Serial.print(index_walk.scan_mode_name());
      Serial.print(" seg=");
      Serial.print(seg);
      Serial.print(" i=");
      Serial.print(lit);
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
