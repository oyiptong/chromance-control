#include <vector>

#include <unity.h>

#include <stdio.h>

#include "core/effects/pattern_coord_color.h"
#include "core/effects/pattern_breathing_mode.h"
#include "core/effects/pattern_hrv_hexagon.h"
#include "core/effects/pattern_index_walk.h"
#include "core/effects/pattern_rainbow_pulse.h"
#include "core/effects/pattern_strip_segment_stepper.h"
#include "core/effects/pattern_two_dots.h"
#include "core/effects/pattern_xy_scan.h"
#include "core/mapping/pixels_map.h"

using chromance::core::CoordColorEffect;
using chromance::core::BreathingEffect;
using chromance::core::EffectFrame;
using chromance::core::HrvHexagonEffect;
using chromance::core::IndexWalkEffect;
using chromance::core::PixelsMap;
using chromance::core::RainbowPulseEffect;
using chromance::core::Rgb;
using chromance::core::StripSegmentStepperEffect;
using chromance::core::TwoDotsEffect;
using chromance::core::XyScanEffect;
using chromance::core::kBlack;

namespace {

uint8_t normalize_0_255(int16_t v, int32_t span) {
  if (span <= 1) {
    return 0;
  }
  int32_t x = static_cast<int32_t>(v);
  if (x < 0) x = 0;
  if (x > span - 1) x = span - 1;
  return static_cast<uint8_t>((x * 255) / (span - 1));
}

Rgb render_single(const chromance::core::IEffect& effect,
                  const EffectFrame& frame,
                  const PixelsMap& map,
                  size_t led_count,
                  uint16_t idx) {
  std::vector<Rgb> out(led_count);
  chromance::core::IEffect& mut = const_cast<chromance::core::IEffect&>(effect);
  mut.render(frame, map, out.data(), out.size());
  return out[idx];
}

}  // namespace

void test_index_walk_effect_lights_one_pixel_and_wraps() {
  IndexWalkEffect e(10);
  PixelsMap map;

  std::vector<Rgb> out(5);
  EffectFrame frame;
  frame.params.brightness = 255;
  e.reset(0);

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL(kBlack.r, out[1].r);
  TEST_ASSERT_EQUAL_UINT8(255, out[0].r);

  frame.now_ms = 10;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(255, out[1].r);

  frame.now_ms = 50;  // 5 steps -> wraps to 0
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(255, out[0].r);

  // Only one pixel lit.
  for (size_t i = 0; i < out.size(); ++i) {
    const bool lit = out[i].r || out[i].g || out[i].b;
    TEST_ASSERT_TRUE((i == 0) == lit);
  }
}

void test_xy_scan_effect_uses_scan_order() {
  // 5 LEDs, scan order permutes indices.
  const uint16_t order[5] = {3, 1, 4, 0, 2};
  XyScanEffect e(order, 5, 10);
  PixelsMap map;
  std::vector<Rgb> out(5);

  EffectFrame frame;
  frame.params.brightness = 200;
  e.reset(0);

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(200, out[3].r);

  frame.now_ms = 10;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(200, out[1].r);

  frame.now_ms = 40;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(200, out[2].r);
}

void test_coord_color_effect_matches_expected_formula_and_scales_brightness() {
  CoordColorEffect e;
  PixelsMap map;
  const size_t n = map.led_count();
  std::vector<Rgb> out_full(n);
  std::vector<Rgb> out_dim(n);

  EffectFrame frame;
  frame.params.brightness = 255;
  e.render(frame, map, out_full.data(), out_full.size());

  // All blue should be zero by definition.
  for (size_t i = 0; i < n; ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, out_full[i].b);
  }

  // Exact expected for LED 0.
  const auto c0 = map.coord(0);
  const uint8_t ex_r =
      normalize_0_255(c0.x, static_cast<int32_t>(map.width()));
  const uint8_t ex_g =
      normalize_0_255(c0.y, static_cast<int32_t>(map.height()));
  TEST_ASSERT_EQUAL_UINT8(ex_r, out_full[0].r);
  TEST_ASSERT_EQUAL_UINT8(ex_g, out_full[0].g);

  // Brightness scaling: render at half-ish and verify a sample matches (floor).
  frame.params.brightness = 128;
  e.render(frame, map, out_dim.data(), out_dim.size());

  const uint16_t sample = static_cast<uint16_t>(n / 3);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>((static_cast<uint16_t>(out_full[sample].r) * 128U) / 255U),
      out_dim[sample].r);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>((static_cast<uint16_t>(out_full[sample].g) * 128U) / 255U),
      out_dim[sample].g);
}

void test_rainbow_pulse_fades_and_holds() {
  RainbowPulseEffect e(700, 2000, 700);
  PixelsMap map;
  std::vector<Rgb> out(10);
  EffectFrame frame;
  frame.params.brightness = 255;
  e.reset(0);

  // Start: black.
  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(0, out[0].r + out[0].g + out[0].b);

  // Mid fade-in: non-black.
  frame.now_ms = 350;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_TRUE(out[0].r || out[0].g || out[0].b);

  // Hold: still non-black.
  frame.now_ms = 800;  // after fade-in
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_TRUE(out[0].r || out[0].g || out[0].b);

  // End of cycle: back to black.
  frame.now_ms = 3400;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(0, out[0].r + out[0].g + out[0].b);
}

void test_two_dots_lights_two_pixels_and_changes_colors_on_sequence() {
  TwoDotsEffect e(10);
  PixelsMap map;
  std::vector<Rgb> out(100);
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);  // deterministic seed
  const uint8_t kComets = TwoDotsEffect::comet_count();
  std::vector<uint32_t> seq0(kComets);
  for (uint8_t i = 0; i < kComets; ++i) {
    seq0[i] = e.sequence_len_ms(i);
    TEST_ASSERT_TRUE(seq0[i] >= 1000 && seq0[i] <= 6000);
    TEST_ASSERT_EQUAL_UINT32(seq0[i], e.sequence_remaining_ms(i));
    const uint8_t h = e.head_len(i);
    TEST_ASSERT_TRUE(h >= 3 && h <= 5);
    const uint16_t step_ms = e.step_ms_for_comet(i);
    if (h == 3) TEST_ASSERT_EQUAL_UINT16(13, step_ms);
    if (h == 4) TEST_ASSERT_EQUAL_UINT16(10, step_ms);
    if (h == 5) TEST_ASSERT_EQUAL_UINT16(8, step_ms);
    for (uint8_t j = 0; j < i; ++j) {
      TEST_ASSERT_NOT_EQUAL_UINT32(seq0[j], seq0[i]);
    }
  }

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) {
    if (c.r || c.g || c.b) ++lit;
  }
  TEST_ASSERT_TRUE(lit > 0);

  // Larger comets should be faster (smaller step_ms).
  uint8_t min_head = 255;
  uint8_t max_head = 0;
  uint16_t min_step_ms = 0xFFFF;
  uint16_t max_step_ms = 0;
  for (uint8_t i = 0; i < kComets; ++i) {
    const uint8_t h = e.head_len(i);
    const uint16_t s = e.step_ms_for_comet(i);
    if (h < min_head) {
      min_head = h;
      max_step_ms = s;
    }
    if (h > max_head) {
      max_head = h;
      min_step_ms = s;
    }
  }
  if (max_head > min_head) {
    TEST_ASSERT_TRUE_MESSAGE(min_step_ms < max_step_ms,
                             "expected larger head_len => smaller step_ms (faster)");
  }

  // Comet 0 should reset independently when its per-comet sequence elapses (ms-based).
  const uint32_t len0 = e.sequence_len_ms(0);
  frame.now_ms = len0 - 1U;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT32(1, e.sequence_remaining_ms(0));

  frame.now_ms = len0;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT32(e.sequence_len_ms(0), e.sequence_remaining_ms(0));

  // Sequence lengths remain unique after resets.
  for (uint8_t i = 0; i < kComets; ++i) {
    const uint32_t li = e.sequence_len_ms(i);
    TEST_ASSERT_TRUE(li >= 1000 && li <= 6000);
    for (uint8_t j = 0; j < i; ++j) {
      TEST_ASSERT_NOT_EQUAL_UINT32(e.sequence_len_ms(j), li);
    }
  }
}

void test_hrv_hexagon_fades_holds_and_switches_hex() {
  HrvHexagonEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);

  // Start of cycle: fully black.
  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(0, lit);

  // Mid fade-in: should be non-black.
  const uint8_t first_hex = e.current_hex_index();
  frame.now_ms = 2000;
  e.render(frame, map, out.data(), out.size());
  lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_TRUE(lit > 0);

  // Hold: should still be non-black.
  frame.now_ms = 4500;  // after fade-in
  e.render(frame, map, out.data(), out.size());
  lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_TRUE(lit > 0);

  // End of cycle: back to black and hex advances (when possible).
  frame.now_ms = 15000;
  e.render(frame, map, out.data(), out.size());
  lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(0, lit);
  if (e.current_segment_count() > 0) {
    TEST_ASSERT_NOT_EQUAL_UINT8(first_hex, e.current_hex_index());
  }
}

void test_strip_segment_stepper_lights_one_segment_per_strip_and_blanks_short_strips() {
  StripSegmentStepperEffect e(/*step_ms=*/0);  // disable auto-advance for test
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  TEST_ASSERT_EQUAL_UINT8(1, e.segment_number());

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());

  uint16_t lit_per_strip[4] = {0, 0, 0, 0};
  const uint8_t* strips = chromance::core::MappingTables::global_to_strip();
  for (size_t i = 0; i < out.size(); ++i) {
    if (!(out[i].r || out[i].g || out[i].b)) continue;
    const uint8_t s = strips[i];
    if (s < 4) lit_per_strip[s]++;
  }
  TEST_ASSERT_EQUAL_UINT16(14, lit_per_strip[0]);
  TEST_ASSERT_EQUAL_UINT16(14, lit_per_strip[1]);
  TEST_ASSERT_EQUAL_UINT16(14, lit_per_strip[2]);
  TEST_ASSERT_EQUAL_UINT16(14, lit_per_strip[3]);

  // Advance to k=12; strip2 has only 6 segments in the full mapping, so it should go black.
  for (uint8_t i = 0; i < 11; ++i) e.next(0);
  TEST_ASSERT_EQUAL_UINT8(12, e.segment_number());
  e.render(frame, map, out.data(), out.size());

  for (uint8_t i = 0; i < 4; ++i) lit_per_strip[i] = 0;
  for (size_t i = 0; i < out.size(); ++i) {
    if (!(out[i].r || out[i].g || out[i].b)) continue;
    const uint8_t s = strips[i];
    if (s < 4) lit_per_strip[s]++;
  }
  TEST_ASSERT_EQUAL_UINT16(0, lit_per_strip[2]);
  TEST_ASSERT_EQUAL_UINT16(0, lit_per_strip[0]);   // strip0 has 11 segments
  TEST_ASSERT_EQUAL_UINT16(14, lit_per_strip[1]);  // strip1 has 12 segments
  TEST_ASSERT_EQUAL_UINT16(0, lit_per_strip[3]);   // strip3 has 11 segments
}

void test_strip_segment_stepper_auto_advance_can_be_disabled() {
  StripSegmentStepperEffect e(/*step_ms=*/10);
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  TEST_ASSERT_TRUE(e.auto_advance_enabled());
  TEST_ASSERT_EQUAL_UINT8(1, e.segment_number());

  frame.now_ms = 25;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(3, e.segment_number());  // 0->10->20 advanced twice

  e.set_auto_advance_enabled(false, 25);
  TEST_ASSERT_FALSE(e.auto_advance_enabled());

  frame.now_ms = 100;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(3, e.segment_number());  // frozen
}

void test_breathing_effect_has_expected_phases() {
  BreathingEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);

  // Inhale mid: inward wave should light many pixels.
  frame.now_ms = 2000;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_TRUE(lit > 20);

  // Exhale mid: outward dot should light a single pixel.
  frame.now_ms = 4000 + 3000 + 3500;
  e.render(frame, map, out.data(), out.size());
  lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(1, lit);
}

void test_breathing_effect_manual_phase_selection() {
  BreathingEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  TEST_ASSERT_FALSE(e.manual_enabled());

  // Force exhale phase (dot outward): should still be a single pixel.
  e.set_manual_phase(2, 0);
  TEST_ASSERT_TRUE(e.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(2, e.manual_phase());
  frame.now_ms = 100;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(1, lit);

  // Previous phase (pause 1): pick a time within the beat to ensure it lights.
  e.prev_phase(0);
  TEST_ASSERT_EQUAL_UINT8(1, e.manual_phase());
  frame.now_ms = 50;
  e.render(frame, map, out.data(), out.size());
  lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_TRUE(lit > 0);

  e.set_auto(0);
  TEST_ASSERT_FALSE(e.manual_enabled());
}

void test_strip_segment_stepper_prev_wraps() {
  StripSegmentStepperEffect e(/*step_ms=*/0);
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  TEST_ASSERT_EQUAL_UINT8(1, e.segment_number());
  e.prev(0);
  TEST_ASSERT_EQUAL_UINT8(12, e.segment_number());
  e.prev(0);
  TEST_ASSERT_EQUAL_UINT8(11, e.segment_number());
  e.next(0);
  TEST_ASSERT_EQUAL_UINT8(12, e.segment_number());
  e.next(0);
  TEST_ASSERT_EQUAL_UINT8(1, e.segment_number());

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
}

void test_hrv_hexagon_manual_next_prev_and_auto_reset() {
  HrvHexagonEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  const uint8_t h0 = e.current_hex_index();

  e.next(0);
  const uint8_t h1 = e.current_hex_index();
  TEST_ASSERT_NOT_EQUAL_UINT8(h0, h1);

  e.prev(0);
  TEST_ASSERT_EQUAL_UINT8(h0, e.current_hex_index());

  // In manual mode, hex should not change when crossing a cycle boundary.
  frame.now_ms = 15000;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(h0, e.current_hex_index());

  // ESC-equivalent: back to auto, now boundary crossing should pick a new hex.
  e.set_auto(15000);
  const uint8_t ha = e.current_hex_index();
  frame.now_ms = 30000;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_NOT_EQUAL_UINT8(ha, e.current_hex_index());
}
