#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../mapping/mapping_tables.h"
#include "effect.h"

namespace chromance {
namespace core {

class IndexWalkEffect final : public IEffect {
 public:
  enum class ScanMode : uint8_t {
    kIndex = 0,         // existing behavior: global LED index order
    kTopoLtrUtd = 1,    // topology-oriented: left-to-right (or up-to-down if vertical)
    kTopoRtlDtu = 2,    // topology-oriented: right-to-left (or down-to-up if vertical)
  };

  explicit IndexWalkEffect(uint16_t hold_ms = 25) : hold_ms_(hold_ms) {}

  const char* id() const override { return "Index_Walk_Test"; }

  void reset(uint32_t now_ms) override {
    start_ms_ = now_ms;
    scan_mode_ = ScanMode::kIndex;
    built_ = false;
    built_led_count_ = 0;
    topo_len_ = 0;
    active_index_ = 0;
    active_seg_ = 0;
  }

  void cycle_scan_mode(uint32_t now_ms) {
    // Cycle order: LTR/UTD -> RTL/DTU -> Index (existing) -> ...
    switch (scan_mode_) {
      case ScanMode::kIndex:
        scan_mode_ = ScanMode::kTopoLtrUtd;
        break;
      case ScanMode::kTopoLtrUtd:
        scan_mode_ = ScanMode::kTopoRtlDtu;
        break;
      case ScanMode::kTopoRtlDtu:
      default:
        scan_mode_ = ScanMode::kIndex;
        break;
    }
    start_ms_ = now_ms;
  }

  void set_auto(uint32_t now_ms) {
    scan_mode_ = ScanMode::kIndex;
    start_ms_ = now_ms;
  }

  ScanMode scan_mode() const { return scan_mode_; }
  const char* scan_mode_name() const {
    switch (scan_mode_) {
      case ScanMode::kTopoLtrUtd:
        return "LTR/UTD";
      case ScanMode::kTopoRtlDtu:
        return "RTL/DTU";
      case ScanMode::kIndex:
      default:
        return "INDEX";
    }
  }

  uint16_t active_index() const { return active_index_; }
  uint8_t active_seg() const { return active_seg_; }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) {
      return;
    }

    const uint16_t n = static_cast<uint16_t>(
        led_count > MappingTables::led_count() ? MappingTables::led_count() : led_count);

    for (size_t i = 0; i < led_count; ++i) {
      out_rgb[i] = kBlack;
    }

    if (!built_ || built_led_count_ != n) {
      build_topo_sequences(n);
      built_ = true;
      built_led_count_ = n;
    }

    const uint32_t elapsed = frame.now_ms - start_ms_;
    const uint32_t step = hold_ms_ ? (elapsed / hold_ms_) : elapsed;
    uint16_t idx = 0;
    if (scan_mode_ == ScanMode::kIndex || topo_len_ == 0) {
      idx = static_cast<uint16_t>(step % n);
    } else if (scan_mode_ == ScanMode::kTopoLtrUtd) {
      idx = topo_seq_ltr_[static_cast<uint16_t>(step % topo_len_)];
    } else {  // kTopoRtlDtu
      idx = topo_seq_rtl_[static_cast<uint16_t>(step % topo_len_)];
    }

    active_index_ = idx;
    active_seg_ = (idx < n) ? MappingTables::global_to_seg()[idx] : 0;

    const uint8_t v = frame.params.brightness;
    out_rgb[idx] = Rgb{v, v, v};
  }

 private:
  static constexpr uint8_t kMaxLedsPerSegment = 14;

  static bool is_vertical(const uint16_t* idxs, uint8_t count) {
    if (idxs == nullptr || count == 0) return false;
    int16_t min_x = MappingTables::pixel_x()[idxs[0]];
    int16_t max_x = min_x;
    for (uint8_t i = 1; i < count; ++i) {
      const int16_t x = MappingTables::pixel_x()[idxs[i]];
      if (x < min_x) min_x = x;
      if (x > max_x) max_x = x;
    }
    return static_cast<int16_t>(max_x - min_x) <= 1;
  }

  static bool less_axis_then_secondary(uint16_t a, uint16_t b, bool vertical) {
    const int16_t ax_a = vertical ? MappingTables::pixel_y()[a] : MappingTables::pixel_x()[a];
    const int16_t ax_b = vertical ? MappingTables::pixel_y()[b] : MappingTables::pixel_x()[b];
    if (ax_a != ax_b) return ax_a < ax_b;
    const int16_t sec_a = vertical ? MappingTables::pixel_x()[a] : MappingTables::pixel_y()[a];
    const int16_t sec_b = vertical ? MappingTables::pixel_x()[b] : MappingTables::pixel_y()[b];
    if (sec_a != sec_b) return sec_a < sec_b;
    return a < b;
  }

  static void insertion_sort(uint16_t* idxs, uint8_t count, bool vertical) {
    for (uint8_t i = 1; i < count; ++i) {
      const uint16_t key = idxs[i];
      int16_t j = static_cast<int16_t>(i) - 1;
      while (j >= 0 && less_axis_then_secondary(key, idxs[j], vertical)) {
        idxs[j + 1] = idxs[j];
        --j;
      }
      idxs[j + 1] = key;
    }
  }

  void build_topo_sequences(uint16_t n) {
    topo_len_ = 0;

    const uint8_t* segs = MappingTables::global_to_seg();
    for (uint8_t seg_id = 1; seg_id <= 40; ++seg_id) {
      uint16_t idxs[kMaxLedsPerSegment];
      uint8_t count = 0;

      // Collect indices for this segment (should be 14 when present, but be tolerant).
      for (uint16_t i = 0; i < n; ++i) {
        if (segs[i] == seg_id) {
          if (count < kMaxLedsPerSegment) {
            idxs[count++] = i;
          }
        }
      }
      if (count == 0) continue;

      const bool vertical = is_vertical(idxs, count);
      insertion_sort(idxs, count, vertical);

      for (uint8_t i = 0; i < count && topo_len_ < n; ++i) {
        topo_seq_ltr_[topo_len_] = idxs[i];
        topo_seq_rtl_[topo_len_] = idxs[static_cast<uint8_t>(count - 1U - i)];
        ++topo_len_;
      }
      if (topo_len_ >= n) break;
    }

    // Safety: if we couldn't fully populate (unexpected), fall back to identity.
    if (topo_len_ != n) {
      topo_len_ = n;
      for (uint16_t i = 0; i < n; ++i) {
        topo_seq_ltr_[i] = i;
        topo_seq_rtl_[i] = static_cast<uint16_t>(n - 1U - i);
      }
    }
  }

  uint32_t start_ms_ = 0;
  uint16_t hold_ms_ = 25;

  ScanMode scan_mode_ = ScanMode::kIndex;
  bool built_ = false;
  uint16_t built_led_count_ = 0;
  uint16_t topo_len_ = 0;
  uint16_t topo_seq_ltr_[MappingTables::led_count()] = {};
  uint16_t topo_seq_rtl_[MappingTables::led_count()] = {};

  uint16_t active_index_ = 0;
  uint8_t active_seg_ = 0;
};

}  // namespace core
}  // namespace chromance
