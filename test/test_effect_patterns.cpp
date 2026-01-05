#include <vector>

#include <unity.h>

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
  std::vector<Rgb> out(10);
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);  // deterministic seed
  const Rgb a0 = e.color_a();
  const Rgb b0 = e.color_b();

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) {
    if (c.r || c.g || c.b) ++lit;
  }
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(lit));

  // After one full loop, colors should change.
  frame.now_ms = 10 * 10;  // step_ms * led_count
  e.render(frame, map, out.data(), out.size());
  const Rgb a1 = e.color_a();
  const Rgb b1 = e.color_b();
  const bool changed = !(a0 == a1 && b0 == b1);
  TEST_ASSERT_TRUE(changed);
}
