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
#include "core/mapping/mapping_tables.h"
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

void test_index_walk_effect_scan_mode_cycles_and_auto_resets() {
  IndexWalkEffect e(10);
  e.reset(0);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kIndex),
                          static_cast<uint8_t>(e.scan_mode()));
  TEST_ASSERT_EQUAL_STRING("INDEX", e.scan_mode_name());

  e.cycle_scan_mode(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kTopoLtrUtd),
                          static_cast<uint8_t>(e.scan_mode()));
  TEST_ASSERT_EQUAL_STRING("LTR/UTD", e.scan_mode_name());

  e.cycle_scan_mode(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kTopoRtlDtu),
                          static_cast<uint8_t>(e.scan_mode()));
  TEST_ASSERT_EQUAL_STRING("RTL/DTU", e.scan_mode_name());

  e.cycle_scan_mode(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kVertexToward),
                          static_cast<uint8_t>(e.scan_mode()));

  e.cycle_scan_mode(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kIndex),
                          static_cast<uint8_t>(e.scan_mode()));

  e.cycle_scan_mode(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kTopoLtrUtd),
                          static_cast<uint8_t>(e.scan_mode()));

  e.set_auto(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IndexWalkEffect::ScanMode::kIndex),
                          static_cast<uint8_t>(e.scan_mode()));
  TEST_ASSERT_EQUAL_STRING("INDEX", e.scan_mode_name());
}

void test_index_walk_effect_topology_scan_orders_leds_within_segments_by_axis() {
  PixelsMap map;
  EffectFrame frame;
  frame.params.brightness = 255;

  const size_t led_count = map.led_count();
  std::vector<Rgb> out(led_count);

  IndexWalkEffect e(1);
  e.reset(0);
  e.cycle_scan_mode(0);  // Index -> LTR/UTD topology scan

  std::vector<uint16_t> seq(led_count);
  for (uint32_t t = 0; t < led_count; ++t) {
    frame.now_ms = t;
    e.render(frame, map, out.data(), out.size());
    seq[static_cast<size_t>(t)] = e.active_index();
  }

  // Permutation of [0..led_count-1]
  std::vector<bool> seen(led_count, false);
  for (const uint16_t idx : seq) {
    TEST_ASSERT_TRUE(idx < led_count);
    TEST_ASSERT_FALSE(seen[idx]);
    seen[idx] = true;
  }

  auto assert_segment_sorted = [&](uint8_t seg_id, bool reverse) {
    // Find contiguous group in the topo scan sequence (segId order).
    size_t start = led_count;
    size_t count = 0;
    for (size_t i = 0; i < led_count; ++i) {
      const uint8_t s = chromance::core::MappingTables::global_to_seg()[seq[i]];
      if (s == seg_id) {
        if (start == led_count) start = i;
        ++count;
      } else if (start != led_count) {
        break;
      }
    }
    TEST_ASSERT_TRUE(start != led_count);
    TEST_ASSERT_EQUAL_UINT32(14, static_cast<uint32_t>(count));

    // Match the effect's vertical detection: x span ~0 => vertical => use y axis.
    int16_t min_x = chromance::core::MappingTables::pixel_x()[seq[start]];
    int16_t max_x = min_x;
    for (size_t i = 1; i < count; ++i) {
      const int16_t x = chromance::core::MappingTables::pixel_x()[seq[start + i]];
      if (x < min_x) min_x = x;
      if (x > max_x) max_x = x;
    }
    const bool vertical = static_cast<int16_t>(max_x - min_x) <= 1;

    int16_t prev = reverse ? 32767 : -32768;
    for (size_t i = 0; i < count; ++i) {
      const uint16_t idx = seq[start + i];
      const int16_t axis = vertical ? chromance::core::MappingTables::pixel_y()[idx]
                                    : chromance::core::MappingTables::pixel_x()[idx];
      if (reverse) {
        TEST_ASSERT_TRUE(axis <= prev);
      } else {
        TEST_ASSERT_TRUE(axis >= prev);
      }
      prev = axis;
    }
  };

  // Canonical topology segment IDs: seg 1 is vertical, seg 2 is angled/horizontal.
  assert_segment_sorted(/*seg_id=*/1, /*reverse=*/false);
  assert_segment_sorted(/*seg_id=*/2, /*reverse=*/false);

  // RTL/DTU: reverse within each segment group.
  e.cycle_scan_mode(0);  // LTR/UTD -> RTL/DTU
  for (uint32_t t = 0; t < led_count; ++t) {
    frame.now_ms = t;
    e.render(frame, map, out.data(), out.size());
    seq[static_cast<size_t>(t)] = e.active_index();
  }
  assert_segment_sorted(/*seg_id=*/1, /*reverse=*/true);
  assert_segment_sorted(/*seg_id=*/2, /*reverse=*/true);
}

void test_index_walk_vertex_toward_lights_incident_segments_and_fills_toward_vertex() {
  IndexWalkEffect e(1);
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  // Cycle into VERTEX_TOWARD mode: INDEX -> LTR/UTD -> RTL/DTU -> VERTEX_TOWARD.
  e.cycle_scan_mode(0);
  e.cycle_scan_mode(0);
  e.cycle_scan_mode(0);
  TEST_ASSERT_TRUE(e.in_vertex_mode());

  // p=0: nothing lit.
  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(0, lit);

  // p=1: for each incident segment, exactly one LED should light at the far end (toward the vertex).
  frame.now_ms = 1;
  e.render(frame, map, out.data(), out.size());

  const uint8_t vid = e.active_vertex_id();
  const uint8_t seg_count = e.active_vertex_seg_count();
  const uint8_t* segs = e.active_vertex_segs();
  TEST_ASSERT_TRUE(seg_count > 0);

  auto seg_is_incident = [&](uint8_t seg) {
    for (uint8_t j = 0; j < seg_count; ++j) {
      if (segs[j] == seg) return true;
    }
    return false;
  };

  const uint8_t* global_seg = chromance::core::MappingTables::global_to_seg();
  const uint16_t* global_local = chromance::core::MappingTables::global_to_local();
  const uint8_t* global_dir = chromance::core::MappingTables::global_to_dir();
  const uint8_t* sva = chromance::core::MappingTables::seg_vertex_a();
  const uint8_t* svb = chromance::core::MappingTables::seg_vertex_b();

  lit = 0;
  for (size_t i = 0; i < out.size(); ++i) {
    const bool is_lit = out[i].r || out[i].g || out[i].b;
    if (!is_lit) continue;
    ++lit;

    const uint8_t seg = global_seg[i];
    TEST_ASSERT_TRUE_MESSAGE(seg_is_incident(seg), "lit LED belongs to a non-incident segment");

    const uint8_t local_in_seg = static_cast<uint8_t>(global_local[i] % 14U);
    const uint8_t ab_k =
        (global_dir[i] == 0) ? local_in_seg : static_cast<uint8_t>(13U - local_in_seg);
    if (vid == sva[seg]) {
      TEST_ASSERT_EQUAL_UINT8(13, ab_k);
    } else if (vid == svb[seg]) {
      TEST_ASSERT_EQUAL_UINT8(0, ab_k);
    } else {
      TEST_FAIL_MESSAGE("active vertex is neither endpoint A nor B for an incident segment");
    }
  }
  TEST_ASSERT_EQUAL_UINT32(seg_count, static_cast<uint32_t>(lit));

  // p=14: all LEDs of incident segments should be lit (14 per segment).
  frame.now_ms = 14;  // p=14 without auto-advancing the vertex (cycle boundary is 15ms)
  e.render(frame, map, out.data(), out.size());
  lit = 0;
  for (size_t i = 0; i < out.size(); ++i) {
    const bool is_lit = out[i].r || out[i].g || out[i].b;
    if (!is_lit) continue;
    ++lit;
    const uint8_t seg = global_seg[i];
    TEST_ASSERT_TRUE(seg_is_incident(seg));
  }
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(seg_count) * 14U, static_cast<uint32_t>(lit));

  // Manual stepping should change the active vertex deterministically.
  const uint8_t v0 = e.active_vertex_id();
  e.vertex_next(21);
  const uint8_t v1 = e.active_vertex_id();
  TEST_ASSERT_NOT_EQUAL_UINT8(v0, v1);
  e.vertex_prev(22);
  TEST_ASSERT_EQUAL_UINT8(v0, e.active_vertex_id());
}

void test_index_walk_effect_step_hold_freezes_until_next_step() {
  IndexWalkEffect e(10);
  PixelsMap map;
  std::vector<Rgb> out(5);
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);

  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(0, e.active_index());

  // Press 's' to step once and hold.
  e.step_hold_next(5);
  frame.now_ms = 5;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(1, e.active_index());

  // Time passing does not advance while held.
  frame.now_ms = 100;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(1, e.active_index());

  // Additional steps move one position per keypress.
  e.step_hold_next(101);
  frame.now_ms = 101;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(2, e.active_index());

  e.step_hold_prev(102);
  frame.now_ms = 102;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(1, e.active_index());

  // Clearing hold returns to time-based progression.
  e.clear_manual_hold(200);
  frame.now_ms = 200;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(0, e.active_index());

  frame.now_ms = 210;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT16(1, e.active_index());
}

void test_index_walk_vertex_mode_step_hold_freezes_fill_progress() {
  IndexWalkEffect e(1);
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  // Cycle into VERTEX_TOWARD mode: INDEX -> LTR/UTD -> RTL/DTU -> VERTEX_TOWARD.
  e.cycle_scan_mode(0);
  e.cycle_scan_mode(0);
  e.cycle_scan_mode(0);
  TEST_ASSERT_TRUE(e.in_vertex_mode());

  // Step once and hold at p=1 (one LED per incident segment).
  e.step_hold_next(0);
  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());

  const uint8_t seg_count = e.active_vertex_seg_count();
  size_t lit0 = 0;
  for (const auto& c : out) lit0 += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(seg_count, static_cast<uint32_t>(lit0));

  // Later timestamps should not auto-advance the fill while held.
  frame.now_ms = 100;
  e.render(frame, map, out.data(), out.size());
  size_t lit1 = 0;
  for (const auto& c : out) lit1 += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(seg_count, static_cast<uint32_t>(lit1));
}

void test_index_walk_vertex_manual_selection_loops_fill_animation() {
  IndexWalkEffect e(1);
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  // Cycle into VERTEX_TOWARD mode: INDEX -> LTR/UTD -> RTL/DTU -> VERTEX_TOWARD.
  e.cycle_scan_mode(0);
  e.cycle_scan_mode(0);
  e.cycle_scan_mode(0);
  TEST_ASSERT_TRUE(e.in_vertex_mode());

  // Manually select a vertex to disable auto vertex cycling.
  e.vertex_next(0);

  // Force initial build of adjacency + active vertex selection.
  frame.now_ms = 0;
  e.render(frame, map, out.data(), out.size());
  const uint8_t seg_count = e.active_vertex_seg_count();
  const uint8_t vid0 = e.active_vertex_id();
  TEST_ASSERT_TRUE(seg_count > 0);

  // p=1: one LED per incident segment.
  frame.now_ms = 1;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(vid0, e.active_vertex_id());
  size_t lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(seg_count, static_cast<uint32_t>(lit));

  // p wraps every 15 steps: now_ms=16 => p=1 again.
  frame.now_ms = 16;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(vid0, e.active_vertex_id());
  lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  TEST_ASSERT_EQUAL_UINT32(seg_count, static_cast<uint32_t>(lit));
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

  // Center must be the configured vertex (12) for the full mapping.
  frame.now_ms = 0;
  frame.dt_ms = 20;
  e.render(frame, map, out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT8(12, e.center_vertex_id());

  // Auto should progress through phases based on completion.
  // Simulate time quickly with a large dt so inhale/exhale complete fast.
  uint32_t now = 0;
  BreathingEffect::Phase last = e.phase();
  bool saw_pause1 = false;
  bool saw_exhale = false;
  bool saw_pause2 = false;
  bool saw_lit_inhale = false;
  bool saw_lit_pause1 = false;
  bool saw_lit_exhale = false;
  bool saw_lit_pause2 = false;
  for (uint16_t iter = 0; iter < 200; ++iter) {
    now += 500;
    frame.now_ms = now;
    frame.dt_ms = 500;
    e.render(frame, map, out.data(), out.size());
    const auto p = e.phase();
    if (p != last) {
      last = p;
      if (p == BreathingEffect::Phase::Pause1) saw_pause1 = true;
      if (p == BreathingEffect::Phase::Exhale) saw_exhale = true;
      if (p == BreathingEffect::Phase::Pause2) saw_pause2 = true;
    }

    // Some phases are allowed to be fully black for brief moments (e.g., between beats),
    // so just track that each phase produces light at least once.
    size_t lit = 0;
    for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
    if (lit > 0) {
      if (p == BreathingEffect::Phase::Inhale) saw_lit_inhale = true;
      if (p == BreathingEffect::Phase::Pause1) saw_lit_pause1 = true;
      if (p == BreathingEffect::Phase::Exhale) saw_lit_exhale = true;
      if (p == BreathingEffect::Phase::Pause2) saw_lit_pause2 = true;
    }
    if (saw_pause1 && saw_exhale && saw_pause2 && p == BreathingEffect::Phase::Inhale) {
      break;  // completed a full cycle
    }
  }
  TEST_ASSERT_TRUE(saw_pause1);
  TEST_ASSERT_TRUE(saw_exhale);
  TEST_ASSERT_TRUE(saw_pause2);
  TEST_ASSERT_TRUE(saw_lit_inhale);
  TEST_ASSERT_TRUE(saw_lit_pause1);
  TEST_ASSERT_TRUE(saw_lit_exhale);
  TEST_ASSERT_TRUE(saw_lit_pause2);
}

void test_breathing_effect_manual_phase_selection() {
  BreathingEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  TEST_ASSERT_FALSE(e.manual_enabled());

  // Force exhale phase: should keep rendering and not auto-transition.
  e.set_manual_phase(2, 0);
  TEST_ASSERT_TRUE(e.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(2, e.manual_phase());
  bool saw_lit_exhale = false;
  for (uint8_t i = 0; i < 5; ++i) {
    frame.now_ms = static_cast<uint32_t>(1000U * i);
    frame.dt_ms = 1000;
    e.render(frame, map, out.data(), out.size());
    TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(e.phase()));
    size_t lit = 0;
    for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
    if (lit > 0) saw_lit_exhale = true;
  }
  TEST_ASSERT_TRUE(saw_lit_exhale);

  // Previous phase (pause 1): should pulse and remain in pause 1.
  e.prev_phase(0);
  TEST_ASSERT_EQUAL_UINT8(1, e.manual_phase());
  frame.now_ms = 50;
  frame.dt_ms = 50;
  e.render(frame, map, out.data(), out.size());
  size_t lit = 0;
  for (const auto& c : out) lit += (c.r || c.g || c.b) ? 1 : 0;
  // Pulse may be zero at this exact time; just ensure the phase is active.
  TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(e.phase()));

  e.set_auto(0);
  TEST_ASSERT_FALSE(e.manual_enabled());
}

static void bfs_dist(uint8_t start,
                     const uint8_t* sva,
                     const uint8_t* svb,
                     const bool* present,
                     uint8_t vcount,
                     uint8_t scount,
                     uint8_t* out) {
  for (uint8_t i = 0; i < vcount; ++i) out[i] = 0xFF;
  uint8_t q[32];
  uint8_t qh = 0, qt = 0;
  out[start] = 0;
  q[qt++] = start;
  while (qh != qt) {
    const uint8_t v = q[qh++];
    const uint8_t dv = out[v];
    for (uint8_t seg = 1; seg <= scount; ++seg) {
      if (!present[seg]) continue;
      const uint8_t a = sva[seg];
      const uint8_t b = svb[seg];
      uint8_t u = 0xFF;
      if (a == v) u = b;
      if (b == v) u = a;
      if (u == 0xFF || u >= vcount) continue;
      if (out[u] != 0xFF) continue;
      out[u] = static_cast<uint8_t>(dv + 1U);
      q[qt++] = u;
    }
  }
}

void test_breathing_inhale_paths_are_monotone_and_segment_simple() {
  BreathingEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;
  frame.now_ms = 0;
  frame.dt_ms = 20;

  e.reset(0);
  e.render(frame, map, out.data(), out.size());  // build caches + init inhale

  const uint8_t center = e.center_vertex_id();
  const uint8_t vcount = chromance::core::MappingTables::vertex_count();
  const uint8_t scount = chromance::core::MappingTables::segment_count();
  const uint8_t* sva = chromance::core::MappingTables::seg_vertex_a();
  const uint8_t* svb = chromance::core::MappingTables::seg_vertex_b();

  // Determine present segments in this mapping build.
  bool present[41] = {};
  const uint8_t* gseg = chromance::core::MappingTables::global_to_seg();
  for (uint16_t i = 0; i < chromance::core::MappingTables::led_count(); ++i) {
    const uint8_t s = gseg[i];
    if (s >= 1 && s <= scount) present[s] = true;
  }

  uint8_t dist[32] = {};
  bfs_dist(center, sva, svb, present, vcount, scount, dist);

  // Starts are distinct.
  const uint8_t dots = e.dot_count();
  TEST_ASSERT_TRUE(dots > 0);
  for (uint8_t i = 0; i < dots; ++i) {
    for (uint8_t j = 0; j < i; ++j) {
      TEST_ASSERT_NOT_EQUAL_UINT8(e.dot_start_vertex(i), e.dot_start_vertex(j));
    }
  }

  // Each dot path:
  // - ends at center
  // - dist is non-increasing
  // - segments are not repeated
  for (uint8_t i = 0; i < dots; ++i) {
    const uint8_t steps = e.dot_step_count(i);
    TEST_ASSERT_TRUE(steps >= 1);

    bool used_seg[41] = {};
    uint8_t cur = e.dot_start_vertex(i);
    uint8_t prev_d = dist[cur];
    TEST_ASSERT_TRUE(prev_d != 0xFF);

    for (uint8_t s = 0; s < steps; ++s) {
      const uint8_t seg_id = e.dot_step_seg(i, s);
      TEST_ASSERT_TRUE(seg_id >= 1 && seg_id <= scount);
      TEST_ASSERT_FALSE(used_seg[seg_id]);
      used_seg[seg_id] = true;

      const uint8_t va = sva[seg_id];
      const uint8_t vb = svb[seg_id];
      uint8_t next = 0xFF;
      if (cur == va) next = vb;
      if (cur == vb) next = va;
      TEST_ASSERT_TRUE(next != 0xFF);

      const uint8_t dnext = dist[next];
      TEST_ASSERT_TRUE(dnext != 0xFF);
      TEST_ASSERT_TRUE(dnext <= prev_d);
      prev_d = dnext;
      cur = next;
    }
    TEST_ASSERT_EQUAL_UINT8(center, cur);
  }
}

void test_breathing_lane_step_only_affects_manual_inhale() {
  BreathingEffect e;
  PixelsMap map;
  std::vector<Rgb> out(map.led_count());
  EffectFrame frame;
  frame.params.brightness = 255;

  e.reset(0);
  frame.now_ms = 0;
  frame.dt_ms = 20;
  e.render(frame, map, out.data(), out.size());
  const uint8_t lanes = e.lane_count();
  TEST_ASSERT_TRUE(lanes > 0);

  // Not manual: lane_next is a no-op.
  const uint8_t rr0 = e.center_lane_rr_offset();
  e.lane_next(0);
  TEST_ASSERT_EQUAL_UINT8(rr0, e.center_lane_rr_offset());

  // Manual inhale: lane_next/prev wraps.
  e.set_manual_phase(0, 0);
  const uint8_t rr1 = e.center_lane_rr_offset();
  e.lane_next(1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>((rr1 + 1U) % lanes), e.center_lane_rr_offset());
  e.lane_prev(2);
  TEST_ASSERT_EQUAL_UINT8(rr1, e.center_lane_rr_offset());

  // Manual non-inhale: no-op.
  e.set_manual_phase(1, 3);
  const uint8_t rr2 = e.center_lane_rr_offset();
  e.lane_next(4);
  TEST_ASSERT_EQUAL_UINT8(rr2, e.center_lane_rr_offset());
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
