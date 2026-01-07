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
    kVertexToward = 3,  // topology-oriented: light segments incident to a vertex, filling toward the vertex
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

    vertex_built_ = false;
    vertex_manual_ = false;
    active_vertex_id_ = 0;
    active_vertex_list_len_ = 0;
    active_vertex_list_pos_ = 0;
    active_vertex_seg_count_ = 0;

    manual_hold_ = false;
    manual_pos_ = 0;
    manual_p_ = 0;
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
        scan_mode_ = ScanMode::kVertexToward;
        break;
      case ScanMode::kVertexToward:
      default:
        scan_mode_ = ScanMode::kIndex;
        break;
    }
    start_ms_ = now_ms;
    manual_hold_ = false;
    manual_p_ = 0;

    // Reset vertex scan state when entering vertex mode.
    if (scan_mode_ == ScanMode::kVertexToward) {
      vertex_manual_ = false;
      active_vertex_list_pos_ = 0;
    }
  }

  void set_auto(uint32_t now_ms) {
    scan_mode_ = ScanMode::kIndex;
    start_ms_ = now_ms;
    vertex_manual_ = false;
    manual_hold_ = false;
  }

  ScanMode scan_mode() const { return scan_mode_; }
  const char* scan_mode_name() const {
    switch (scan_mode_) {
      case ScanMode::kTopoLtrUtd:
        return "LTR/UTD";
      case ScanMode::kTopoRtlDtu:
        return "RTL/DTU";
      case ScanMode::kVertexToward:
        return "VERTEX_TOWARD";
      case ScanMode::kIndex:
      default:
        return "INDEX";
    }
  }

  uint16_t active_index() const { return active_index_; }
  uint8_t active_seg() const { return active_seg_; }
  uint8_t active_vertex_id() const { return active_vertex_id_; }
  uint8_t active_vertex_seg_count() const { return active_vertex_seg_count_; }
  const uint8_t* active_vertex_segs() const { return active_vertex_segs_; }

  bool in_vertex_mode() const { return scan_mode_ == ScanMode::kVertexToward; }

  bool manual_hold_enabled() const { return manual_hold_; }

  void clear_manual_hold(uint32_t now_ms) {
    manual_hold_ = false;
    vertex_manual_ = false;
    start_ms_ = now_ms;
  }

  void step_hold_next(uint32_t now_ms) { step_hold(+1, now_ms); }
  void step_hold_prev(uint32_t now_ms) { step_hold(-1, now_ms); }

  void vertex_next(uint32_t now_ms) {
    if (scan_mode_ != ScanMode::kVertexToward) return;
    vertex_manual_ = true;
    start_ms_ = now_ms;
    step_vertex(+1);
  }

  void vertex_prev(uint32_t now_ms) {
    if (scan_mode_ != ScanMode::kVertexToward) return;
    vertex_manual_ = true;
    start_ms_ = now_ms;
    step_vertex(-1);
  }

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

    if (scan_mode_ == ScanMode::kVertexToward) {
      render_vertex_toward(frame, out_rgb, n);
      return;
    }

    if (manual_hold_) {
      const uint16_t order_len = scan_order_len(n);
      if (order_len == 0) return;
      manual_pos_ = static_cast<uint16_t>(manual_pos_ % order_len);
      const uint16_t idx = idx_for_pos(manual_pos_, n);
      active_index_ = idx;
      active_seg_ = (idx < n) ? MappingTables::global_to_seg()[idx] : 0;

      const uint8_t v = frame.params.brightness;
      out_rgb[idx] = Rgb{v, v, v};
      return;
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
  static constexpr uint16_t kMaxVertices = 32;
  static constexpr uint8_t kMaxSegments = 40;
  static constexpr uint8_t kMaxVertexDegree = 6;  // safe upper bound for this topology

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

  uint16_t scan_order_len(uint16_t n) const {
    if (scan_mode_ == ScanMode::kIndex || topo_len_ == 0) return n;
    return topo_len_;
  }

  uint16_t idx_for_pos(uint16_t pos, uint16_t n) const {
    if (scan_mode_ == ScanMode::kIndex || topo_len_ == 0) {
      return static_cast<uint16_t>(pos % n);
    }
    const uint16_t p = static_cast<uint16_t>(pos % topo_len_);
    if (scan_mode_ == ScanMode::kTopoLtrUtd) return topo_seq_ltr_[p];
    return topo_seq_rtl_[p];
  }

  uint16_t pos_for_idx(uint16_t idx, uint16_t n) const {
    if (scan_mode_ == ScanMode::kIndex || topo_len_ == 0) return idx;
    const uint16_t order_len = scan_order_len(n);
    if (order_len == 0) return 0;
    const uint16_t* seq = (scan_mode_ == ScanMode::kTopoLtrUtd) ? topo_seq_ltr_ : topo_seq_rtl_;
    for (uint16_t p = 0; p < order_len; ++p) {
      if (seq[p] == idx) return p;
    }
    return 0;
  }

  uint16_t auto_active_idx(uint32_t now_ms, uint16_t n) const {
    const uint32_t elapsed = now_ms - start_ms_;
    const uint32_t step = hold_ms_ ? (elapsed / hold_ms_) : elapsed;
    if (scan_mode_ == ScanMode::kIndex || topo_len_ == 0) {
      return static_cast<uint16_t>(step % n);
    }
    if (scan_mode_ == ScanMode::kTopoLtrUtd) {
      return topo_seq_ltr_[static_cast<uint16_t>(step % topo_len_)];
    }
    return topo_seq_rtl_[static_cast<uint16_t>(step % topo_len_)];
  }

  void step_hold(int8_t dir, uint32_t now_ms) {
    const uint16_t n = built_ ? built_led_count_ : MappingTables::led_count();
    if (n == 0) return;

    if (!built_ || built_led_count_ != n) {
      build_topo_sequences(n);
      built_ = true;
      built_led_count_ = n;
    }

    const bool was_manual = manual_hold_;
    manual_hold_ = true;

    if (scan_mode_ == ScanMode::kVertexToward) {
      if (!vertex_built_) {
        build_vertex_adjacency(n);
        vertex_built_ = true;
      }

      vertex_manual_ = true;  // disable time-based vertex cycling

      // On first step, snap to the current fill progress.
      if (!was_manual && now_ms >= start_ms_) {
        const uint32_t elapsed = now_ms - start_ms_;
        const uint32_t step = hold_ms_ ? (elapsed / hold_ms_) : elapsed;
        manual_p_ = static_cast<uint8_t>(step > 14 ? 14 : step);
      }

      if (dir > 0) {
        manual_p_ = static_cast<uint8_t>(manual_p_ + 1U);
        if (manual_p_ > 14) {
          manual_p_ = 0;
          step_vertex(+1);
        }
      } else if (dir < 0) {
        if (manual_p_ == 0) {
          manual_p_ = 14;
          step_vertex(-1);
        } else {
          manual_p_ = static_cast<uint8_t>(manual_p_ - 1U);
        }
      }

      start_ms_ = now_ms;
      return;
    }

    // On first step, snap to the current auto position.
    if (!was_manual) {
      const uint16_t idx = auto_active_idx(now_ms, n);
      manual_pos_ = pos_for_idx(idx, n);
    }

    const uint16_t order_len = scan_order_len(n);
    if (order_len == 0) return;
    if (dir > 0) {
      manual_pos_ = static_cast<uint16_t>((manual_pos_ + 1U) % order_len);
    } else if (dir < 0) {
      manual_pos_ =
          static_cast<uint16_t>((manual_pos_ + order_len - 1U) % order_len);
    }
    active_index_ = idx_for_pos(manual_pos_, n);
    active_seg_ = MappingTables::global_to_seg()[active_index_];
    start_ms_ = now_ms;
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

  // Vertex-incident segment scan (diagnostic).
  bool vertex_built_ = false;
  bool vertex_manual_ = false;
  uint8_t active_vertex_id_ = 0;

  uint8_t active_vertex_list_[kMaxVertices] = {};
  uint8_t active_vertex_list_len_ = 0;
  uint8_t active_vertex_list_pos_ = 0;

  bool seg_present_[kMaxSegments + 1] = {};
  uint8_t vertex_incident_[kMaxVertices][kMaxVertexDegree] = {};
  uint8_t vertex_incident_count_[kMaxVertices] = {};

  uint8_t active_vertex_segs_[kMaxVertexDegree] = {};
  uint8_t active_vertex_seg_count_ = 0;

  bool manual_hold_ = false;
  uint16_t manual_pos_ = 0;
  uint8_t manual_p_ = 0;  // vertex fill progress [0..14]

  void build_vertex_adjacency(uint16_t led_count) {
    // Presence by segId (bench/full safe).
    for (uint8_t s = 0; s <= kMaxSegments; ++s) seg_present_[s] = false;
    const uint8_t* segs = MappingTables::global_to_seg();
    for (uint16_t i = 0; i < led_count; ++i) {
      const uint8_t seg = segs[i];
      if (seg >= 1 && seg <= kMaxSegments) seg_present_[seg] = true;
    }

    const uint8_t vcount = MappingTables::vertex_count();
    for (uint8_t v = 0; v < kMaxVertices; ++v) vertex_incident_count_[v] = 0;

    const uint8_t* sva = MappingTables::seg_vertex_a();
    const uint8_t* svb = MappingTables::seg_vertex_b();
    const uint8_t scount = MappingTables::segment_count();
    for (uint8_t seg_id = 1; seg_id <= scount && seg_id <= kMaxSegments; ++seg_id) {
      if (!seg_present_[seg_id]) continue;
      const uint8_t va = sva[seg_id];
      const uint8_t vb = svb[seg_id];
      if (va >= vcount || vb >= vcount) continue;
      if (va < kMaxVertices) {
        const uint8_t c = vertex_incident_count_[va];
        if (c < kMaxVertexDegree) {
          vertex_incident_[va][c] = seg_id;
          vertex_incident_count_[va] = static_cast<uint8_t>(c + 1U);
        }
      }
      if (vb < kMaxVertices) {
        const uint8_t c = vertex_incident_count_[vb];
        if (c < kMaxVertexDegree) {
          vertex_incident_[vb][c] = seg_id;
          vertex_incident_count_[vb] = static_cast<uint8_t>(c + 1U);
        }
      }
    }

    // Build active vertex list (only vertices with at least one present incident segment).
    active_vertex_list_len_ = 0;
    for (uint8_t v = 0; v < vcount && v < kMaxVertices; ++v) {
      if (vertex_incident_count_[v] == 0) continue;
      if (active_vertex_list_len_ < kMaxVertices) {
        active_vertex_list_[active_vertex_list_len_++] = v;
      }
    }
    if (active_vertex_list_len_ == 0) {
      active_vertex_list_len_ = 1;
      active_vertex_list_[0] = 0;
    }
    active_vertex_list_pos_ %= active_vertex_list_len_;
    set_active_vertex(active_vertex_list_[active_vertex_list_pos_]);
  }

  void set_active_vertex(uint8_t vertex_id) {
    active_vertex_id_ = vertex_id;
    active_vertex_seg_count_ = 0;
    if (vertex_id >= kMaxVertices) return;
    const uint8_t c = vertex_incident_count_[vertex_id];
    for (uint8_t i = 0; i < c && i < kMaxVertexDegree; ++i) {
      active_vertex_segs_[active_vertex_seg_count_++] = vertex_incident_[vertex_id][i];
    }
  }

  void step_vertex(int8_t dir) {
    if (active_vertex_list_len_ == 0) return;
    if (dir > 0) {
      active_vertex_list_pos_ = static_cast<uint8_t>((active_vertex_list_pos_ + 1U) % active_vertex_list_len_);
    } else if (dir < 0) {
      active_vertex_list_pos_ = static_cast<uint8_t>((active_vertex_list_pos_ + active_vertex_list_len_ - 1U) %
                                                     active_vertex_list_len_);
    }
    set_active_vertex(active_vertex_list_[active_vertex_list_pos_]);
  }

  void render_vertex_toward(const EffectFrame& frame, Rgb* out, uint16_t led_count) {
    if (!vertex_built_) {
      build_vertex_adjacency(led_count);
      vertex_built_ = true;
    }

    uint8_t p = manual_hold_ ? manual_p_ : 0;
    if (!manual_hold_) {
      // Auto-cycle vertices unless manually held.
      if (!vertex_manual_ && hold_ms_ > 0) {
        const uint32_t elapsed = frame.now_ms - start_ms_;
        const uint32_t cycle_ms = static_cast<uint32_t>(hold_ms_) * 15U;  // fill 0..14 then advance
        if (cycle_ms > 0) {
          const uint32_t cycles = elapsed / cycle_ms;
          if (cycles > 0) {
            start_ms_ += cycles * cycle_ms;
            for (uint32_t i = 0; i < cycles; ++i) step_vertex(+1);
          }
        }
      }

      // Fill progress p in [0..14], derived from hold_ms_ cadence.
      // When a vertex is selected manually (pause mode), keep looping the fill animation;
      // only vertex selection is paused.
      const uint32_t elapsed = frame.now_ms - start_ms_;
      const uint32_t step = hold_ms_ ? (elapsed / hold_ms_) : elapsed;
      p = static_cast<uint8_t>(step % 15U);
    }

    const uint8_t v = frame.params.brightness;
    const Rgb c{v, v, v};

    const uint8_t* segs = MappingTables::global_to_seg();
    const uint16_t* local = MappingTables::global_to_local();
    const uint8_t* dir = MappingTables::global_to_dir();
    const uint8_t* sva = MappingTables::seg_vertex_a();
    const uint8_t* svb = MappingTables::seg_vertex_b();

    // Render all LEDs for segments incident to active vertex; fill toward vertex.
    active_index_ = 0xFFFF;
    active_seg_ = 0;
    for (uint16_t i = 0; i < led_count; ++i) {
      const uint8_t seg = segs[i];
      if (seg < 1 || seg > kMaxSegments) continue;

      bool incident = false;
      for (uint8_t j = 0; j < active_vertex_seg_count_; ++j) {
        if (active_vertex_segs_[j] == seg) {
          incident = true;
          break;
        }
      }
      if (!incident) continue;

      // Canonical segment coordinate (A->B) must be derived from the physical local LED index
      // within the segment, not from the global index iteration order.
      const uint8_t local_in_seg = static_cast<uint8_t>(local[i] % 14U);
      const uint8_t ab_k =
          (dir[i] == 0) ? local_in_seg : static_cast<uint8_t>(13U - local_in_seg);
      const uint8_t va = sva[seg];
      const uint8_t vb = svb[seg];

      bool lit = false;
      if (active_vertex_id_ == va) {
        // Fill from B -> A: start at ab_k==13, expand toward 0.
        lit = (p > 0) && (ab_k >= static_cast<uint8_t>(14U - p));
      } else if (active_vertex_id_ == vb) {
        // Fill from A -> B: start at ab_k==0, expand toward 13.
        lit = (p > 0) && (ab_k <= static_cast<uint8_t>(p - 1U));
      }

      if (lit) {
        out[i] = c;
        if (active_index_ == 0xFFFF) {
          active_index_ = i;
          active_seg_ = seg;
        }
      }
    }
    if (active_index_ == 0xFFFF) {
      active_index_ = 0;
      active_seg_ = 0;
    }
  }
};

}  // namespace core
}  // namespace chromance
