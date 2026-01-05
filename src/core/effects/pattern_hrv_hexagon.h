#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../types.h"
#include "../mapping/mapping_tables.h"
#include "effect.h"

namespace chromance {
namespace core {

// Pattern 6: "HRV hexagon"
// - Picks a random hexagon cell (perimeter + internal edges) and random color.
// - Fades in (4s), holds (2s), fades out (9s), repeats forever with a new hex.
//
// Hex definitions are derived from the segment topology (vertex graph) in
// `docs/architecture/wled_integration_implementation_plan.md` / `mapping/README_wiring.md`.
// The "internal edges" here are the 3 segments whose midpoints lie inside the hex polygon.
class HrvHexagonEffect final : public IEffect {
 public:
  const char* id() const override { return "HRV hexagon"; }

  void reset(uint32_t now_ms) override {
    cycle_start_ms_ = now_ms;
    rng_ = 0xA5A5A5A5u ^ now_ms;
    build_segment_presence();
    pick_new_hex(/*avoid_current=*/false);
  }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) return;

    for (size_t i = 0; i < led_count; ++i) out_rgb[i] = kBlack;

    advance_cycles(frame.now_ms);

    const uint32_t elapsed = frame.now_ms - cycle_start_ms_;
    const uint8_t alpha = phase_alpha(elapsed);
    if (alpha == 0 || current_seg_count_ == 0) return;

    const uint8_t v =
        static_cast<uint8_t>((static_cast<uint16_t>(alpha) * frame.params.brightness) / 255U);
    const Rgb c = scale(current_color_, v);

    for (uint16_t i = 0; i < led_count; ++i) {
      const uint8_t seg = MappingTables::global_to_seg()[i];
      if (seg == 0 || seg > 40) continue;
      if (segment_in_current(seg)) {
        out_rgb[i] = c;
      }
    }
  }

  uint8_t current_hex_index() const { return current_hex_; }
  Rgb current_color() const { return current_color_; }
  uint8_t current_segment_count() const { return current_seg_count_; }
  const uint8_t* current_segments() const { return current_segs_; }

  void force_next(uint32_t now_ms) {
    cycle_start_ms_ = now_ms;
    pick_new_hex(/*avoid_current=*/true);
  }

 private:
  static constexpr uint32_t kFadeInMs = 4000;
  static constexpr uint32_t kHoldMs = 2000;
  static constexpr uint32_t kFadeOutMs = 9000;
  static constexpr uint32_t kCycleMs = kFadeInMs + kHoldMs + kFadeOutMs;

  // 8 hexagons in the sculpture:
  // Upright:
  //  - U1 {1,2,4,5,9,12} + internal {3,6,7}
  //  - U2 {12,13,15,23,26,28} + internal {14,24,25}
  //  - U3 {28,29,31,36,39,40} + internal {30,37,38}
  //  - U4 {8,9,11,15,18,20} + internal {10,16,17}
  //  - U5 {20,21,26,31,34,35} + internal {27,32,33}
  // Upside-down:
  //  - D1 {6,7,10,14,17,24} + internal {9,12,15}
  //  - D2 {24,25,27,30,33,37} + internal {26,28,31}
  //  - D3 {16,17,19,22,27,32} + internal {18,20,21}
  static constexpr uint8_t kHexCount = 8;
  static constexpr uint8_t kHexSegCount = 9;  // 6 perimeter + 3 internal edges
  static const uint8_t kHexSegs[kHexCount][kHexSegCount];

  static uint32_t xorshift32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }

  uint32_t next_u32() {
    rng_ = xorshift32(rng_);
    return rng_;
  }

  static Rgb hue_to_rgb(uint8_t hue) {
    if (hue < 85) {
      return Rgb{static_cast<uint8_t>(255U - hue * 3U), static_cast<uint8_t>(hue * 3U), 0};
    }
    hue = static_cast<uint8_t>(hue - 85);
    if (hue < 85) {
      return Rgb{0, static_cast<uint8_t>(255U - hue * 3U), static_cast<uint8_t>(hue * 3U)};
    }
    hue = static_cast<uint8_t>(hue - 85);
    return Rgb{static_cast<uint8_t>(hue * 3U), 0, static_cast<uint8_t>(255U - hue * 3U)};
  }

  static Rgb scale(const Rgb& c, uint8_t v) {
    return Rgb{static_cast<uint8_t>((static_cast<uint16_t>(c.r) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.g) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.b) * v) / 255U)};
  }

  void build_segment_presence() {
    for (uint8_t s = 0; s <= 40; ++s) seg_present_[s] = false;
    const uint16_t n = MappingTables::led_count();
    const uint8_t* segs = MappingTables::global_to_seg();
    for (uint16_t i = 0; i < n; ++i) {
      const uint8_t seg = segs[i];
      if (seg <= 40) seg_present_[seg] = true;
    }
  }

  bool segment_in_current(uint8_t seg_id) const {
    for (uint8_t i = 0; i < current_seg_count_; ++i) {
      if (current_segs_[i] == seg_id) return true;
    }
    return false;
  }

  void pick_new_hex(bool avoid_current) {
    uint8_t candidates[8];
    uint8_t candidate_count = 0;
    for (uint8_t h = 0; h < 8; ++h) {
      uint8_t present = 0;
      for (uint8_t i = 0; i < kHexSegCount; ++i) {
        const uint8_t seg = kHexSegs[h][i];
        if (seg_present_[seg]) {
          present = 1;
          break;
        }
      }
      if (present) candidates[candidate_count++] = h;
    }

    if (candidate_count == 0) {
      current_hex_ = 0;
    } else if (!avoid_current || candidate_count == 1) {
      current_hex_ = candidates[next_u32() % candidate_count];
    } else {
      uint8_t current_pos = 0xFF;
      for (uint8_t i = 0; i < candidate_count; ++i) {
        if (candidates[i] == current_hex_) {
          current_pos = i;
          break;
        }
      }
      const uint8_t r = static_cast<uint8_t>(next_u32() % (candidate_count - 1U));
      uint8_t pick_pos = r;
      if (current_pos != 0xFF && pick_pos >= current_pos) {
        pick_pos = static_cast<uint8_t>(pick_pos + 1U);
      }
      current_hex_ = candidates[pick_pos % candidate_count];
    }

    // Cache active segment ids for this hex (skip segments not present in current mapping).
    current_seg_count_ = 0;
    for (uint8_t i = 0; i < kHexSegCount; ++i) {
      const uint8_t seg = kHexSegs[current_hex_][i];
      if (seg_present_[seg]) current_segs_[current_seg_count_++] = seg;
    }

    current_color_ = hue_to_rgb(static_cast<uint8_t>(next_u32() & 0xFF));
  }

  void advance_cycles(uint32_t now_ms) {
    while (static_cast<int32_t>(now_ms - cycle_start_ms_) >= static_cast<int32_t>(kCycleMs)) {
      cycle_start_ms_ += kCycleMs;
      pick_new_hex(/*avoid_current=*/true);
    }
  }

  static uint8_t phase_alpha(uint32_t elapsed_ms) {
    if (elapsed_ms < kFadeInMs) {
      return static_cast<uint8_t>((elapsed_ms * 255U) / kFadeInMs);
    }
    elapsed_ms -= kFadeInMs;
    if (elapsed_ms < kHoldMs) {
      return 255;
    }
    elapsed_ms -= kHoldMs;
    if (elapsed_ms < kFadeOutMs) {
      const uint32_t t = kFadeOutMs - elapsed_ms;
      return static_cast<uint8_t>((t * 255U) / kFadeOutMs);
    }
    return 0;
  }

  uint32_t cycle_start_ms_ = 0;
  uint32_t rng_ = 0x12345678u;

  bool seg_present_[41] = {};
  uint8_t current_hex_ = 0;
  uint8_t current_segs_[9] = {};
  uint8_t current_seg_count_ = 0;
  Rgb current_color_{255, 0, 0};
};

}  // namespace core
}  // namespace chromance
