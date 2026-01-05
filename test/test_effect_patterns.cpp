#include <vector>

#include <unity.h>

#include <stdio.h>

#include "core/effects/pattern_coord_color.h"
#include "core/effects/pattern_index_walk.h"
#include "core/effects/pattern_rainbow_pulse.h"
#include "core/effects/pattern_two_dots.h"
#include "core/effects/pattern_xy_scan.h"
#include "core/mapping/pixels_map.h"

using chromance::core::CoordColorEffect;
using chromance::core::EffectFrame;
using chromance::core::IndexWalkEffect;
using chromance::core::PixelsMap;
using chromance::core::RainbowPulseEffect;
using chromance::core::Rgb;
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
