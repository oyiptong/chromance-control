#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../mapping/mapping_tables.h"
#include "../types.h"
#include "effect.h"

namespace chromance {
namespace core {

// Mode 7: "Breathing" (topology-driven, event-driven)
//
// Phases:
// - INHALE: multiple dots traverse toward a center vertex along topology edges.
// - PAUSE_1: beat-count driven pulse, crossfade inhale->exhale.
// - EXHALE: outward wavefronts over topology distance layers; completion tracked discretely.
// - PAUSE_2: beat-count driven pulse, crossfade exhale->inhale.
//
// Manual control:
// - `n`/`N`: select phase and stay there (no automatic phase progression).
// - `ESC`: return to auto mode (restarts at INHALE).
// - INHALE-only lane stepping (manual only): `s`/`S` rotates center lane offset and reinitializes INHALE.
class BreathingEffect final : public IEffect {
 public:
  const char* id() const override { return "Breathing"; }

  enum class Phase : uint8_t { Inhale = 0, Pause1 = 1, Exhale = 2, Pause2 = 3 };

  struct Config {
    // Requested: fix Mode 7 center at vertex 12 when valid.
    uint8_t configured_center_vertex_id = 12;
    bool has_configured_center = true;

    // INHALE
    uint8_t num_dots = 6;
    uint16_t dot_speed_q16 = 5243;  // 0.08 LEDs/ms in 16.16 fixed
    uint8_t tail_length_leds = 5;

    // EXHALE
    uint8_t target_waves = 7;
    uint16_t wave_speed_layers_q16 = 262;  // 0.004 layers/ms in 16.16 fixed
    uint16_t exhale_band_width_q16 = 22938;  // 0.35 layers in 16.16 fixed

    // PAUSE
    uint8_t beats_target_min = 3;
    uint8_t beats_target_max = 7;
    uint16_t beat_period_ms = 2000;  // ~30bpm
    uint16_t max_pause_duration_ms = 6000;
  };

  void reset(uint32_t now_ms) override {
    built_ = false;
    built_led_count_ = 0;
    manual_enabled_ = false;
    manual_phase_ = Phase::Inhale;

    rng_state_ = seed_from_time(now_ms);
    center_lane_rr_offset_ = 0;

    phase_ = Phase::Inhale;
    phase_start_ms_ = now_ms;
    last_now_ms_ = now_ms;
  }

  // Manual phase selection (for interactive control).
  // When enabled, the effect stays in the chosen phase but continues animating within it.
  void set_manual_phase(uint8_t phase, uint32_t now_ms) {
    manual_enabled_ = true;
    manual_phase_ = static_cast<Phase>(phase & 3U);
    manual_start_ms_ = now_ms;
    phase_ = manual_phase_;
    phase_start_ms_ = now_ms;
    init_phase(now_ms, /*auto_transition_into_inhale=*/false);
  }

  void next_phase(uint32_t now_ms) {
    set_manual_phase(static_cast<uint8_t>(static_cast<uint8_t>(manual_phase_) + 1U), now_ms);
  }
  void prev_phase(uint32_t now_ms) {
    set_manual_phase(static_cast<uint8_t>(static_cast<uint8_t>(manual_phase_) + 3U), now_ms);
  }

  void set_auto(uint32_t now_ms) {
    manual_enabled_ = false;
    phase_ = Phase::Inhale;
    phase_start_ms_ = now_ms;
    last_now_ms_ = now_ms;
    init_phase(now_ms, /*auto_transition_into_inhale=*/true);
  }

  bool manual_enabled() const { return manual_enabled_; }
  uint8_t manual_phase() const { return static_cast<uint8_t>(manual_phase_); }

  // INHALE-only, manual-only: rotate center lane offset and reinit inhale.
  void lane_next(uint32_t now_ms) { lane_step(+1, now_ms); }
  void lane_prev(uint32_t now_ms) { lane_step(-1, now_ms); }

  // Debug / tests.
  Phase phase() const { return phase_; }
  uint8_t center_vertex_id() const { return center_vertex_id_; }
  uint8_t lane_count() const { return center_lane_count_; }
  uint8_t center_lane_rr_offset() const { return center_lane_rr_offset_; }
  uint8_t dot_count() const { return inhale_dot_count_; }
  uint8_t dot_start_vertex(uint8_t i) const { return (i < inhale_dot_count_) ? dots_[i].start_v : 0; }
  uint8_t dot_goal_vertex(uint8_t i) const { return (i < inhale_dot_count_) ? dots_[i].goal_v : 0; }
  uint8_t dot_step_count(uint8_t i) const { return (i < inhale_dot_count_) ? dots_[i].step_count : 0; }
  uint8_t dot_step_seg(uint8_t i, uint8_t step) const {
    return (i < inhale_dot_count_ && step < dots_[i].step_count) ? dots_[i].step_seg[step] : 0;
  }
  uint8_t dot_step_dir(uint8_t i, uint8_t step) const {
    return (i < inhale_dot_count_ && step < dots_[i].step_count) ? dots_[i].step_dir[step] : 0;
  }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) return;
    for (size_t i = 0; i < led_count; ++i) out_rgb[i] = kBlack;

    const uint16_t n = static_cast<uint16_t>(
        led_count > MappingTables::led_count() ? MappingTables::led_count() : led_count);
    if (!built_ || built_led_count_ != n) {
      build_topology_cache(n);
      init_phase(frame.now_ms, /*auto_transition_into_inhale=*/false);
      built_ = true;
      built_led_count_ = n;
    }

    if (manual_enabled_) {
      render_phase(frame, out_rgb, n, phase_);
      return;  // manual: no phase progression
    }

    // Auto: render current phase and advance on completion.
    render_phase(frame, out_rgb, n, phase_);

    if (phase_complete_) {
      phase_complete_ = false;
      const Phase next = static_cast<Phase>((static_cast<uint8_t>(phase_) + 1U) & 3U);
      phase_ = next;
      phase_start_ms_ = frame.now_ms;
      init_phase(frame.now_ms, /*auto_transition_into_inhale=*/(phase_ == Phase::Inhale));
    }
  }

 private:
  static constexpr uint8_t kMaxVertices = 32;
  static constexpr uint8_t kMaxSegments = 40;
  static constexpr uint8_t kMaxDegree = 6;
  static constexpr uint8_t kLedsPerSegment = 14;
  static constexpr uint8_t kMaxDots = 8;
  static constexpr uint8_t kMaxVertexPathLen = 32;

  // Colors are in RGB space; note some hardware may be GRB ordered.
  static constexpr Rgb kInhaleDotColor{255, 80, 0};   // red-orange
  static constexpr Rgb kExhaleWaveColor{120, 255, 180};  // light green-ish
  static constexpr Rgb kInhalePauseColor{255, 80, 0};
  static constexpr Rgb kExhalePauseColor{120, 255, 180};

  static constexpr uint8_t kTailLutLen = 16;
  static constexpr uint8_t kTailLut[kTailLutLen] = {
      255, 170, 110, 70, 45, 30, 20, 14, 10, 7, 5, 4, 3, 2, 1, 1,
  };

  static Rgb scale(const Rgb& c, uint8_t v) {
    return Rgb{static_cast<uint8_t>((static_cast<uint16_t>(c.r) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.g) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.b) * v) / 255U)};
  }

  static uint32_t seed_from_time(uint32_t now_ms) {
    // Avoid 0 state for xorshift.
    uint32_t s = now_ms ^ 0x9E3779B9u;
    if (s == 0) s = 1;
    return s;
  }

  static uint32_t xorshift32(uint32_t& s) {
    uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x ? x : 1;
    return s;
  }

  uint32_t rand_u32() { return xorshift32(rng_state_); }

  uint32_t dt_ms_from_frame(const EffectFrame& frame) {
    if (frame.dt_ms != 0) return frame.dt_ms;
    const uint32_t dt = frame.now_ms - last_now_ms_;
    last_now_ms_ = frame.now_ms;
    return dt;
  }

  static Rgb lerp(const Rgb& a, const Rgb& b, uint16_t t16) {
    const uint16_t ia = static_cast<uint16_t>(65535U - t16);
    return Rgb{
        static_cast<uint8_t>((static_cast<uint32_t>(a.r) * ia + static_cast<uint32_t>(b.r) * t16) >> 16),
        static_cast<uint8_t>((static_cast<uint32_t>(a.g) * ia + static_cast<uint32_t>(b.g) * t16) >> 16),
        static_cast<uint8_t>((static_cast<uint32_t>(a.b) * ia + static_cast<uint32_t>(b.b) * t16) >> 16),
    };
  }

  static uint8_t max_u8(uint8_t a, uint8_t b) { return a > b ? a : b; }

  static void blend_max(Rgb* out, uint16_t idx, const Rgb& c) {
    out[idx].r = max_u8(out[idx].r, c.r);
    out[idx].g = max_u8(out[idx].g, c.g);
    out[idx].b = max_u8(out[idx].b, c.b);
  }

  void lane_step(int8_t dir, uint32_t now_ms) {
    if (!manual_enabled_) return;
    if (phase_ != Phase::Inhale) return;
    if (center_lane_count_ == 0) return;

    if (dir > 0) {
      center_lane_rr_offset_ = static_cast<uint8_t>((center_lane_rr_offset_ + 1U) % center_lane_count_);
    } else if (dir < 0) {
      center_lane_rr_offset_ = static_cast<uint8_t>((center_lane_rr_offset_ + center_lane_count_ - 1U) % center_lane_count_);
    }

    // Reinitialize inhale with updated lane offset (manual tool).
    phase_start_ms_ = now_ms;
    init_inhale(now_ms, /*advance_rr_offset=*/false, /*regenerate_paths=*/true);
  }

  void build_topology_cache(uint16_t led_count) {
    // Segment presence + canonical A->B lookup (physical coordinate).
    for (uint8_t s = 0; s <= kMaxSegments; ++s) seg_present_[s] = false;
    for (uint8_t s = 0; s <= kMaxSegments; ++s) {
      for (uint8_t k = 0; k < kLedsPerSegment; ++k) seg_ab_to_global_[s][k] = 0xFFFF;
    }

    const uint8_t* seg = MappingTables::global_to_seg();
    const uint16_t* local = MappingTables::global_to_local();
    const uint8_t* dir = MappingTables::global_to_dir();
    for (uint16_t i = 0; i < led_count; ++i) {
      const uint8_t seg_id = seg[i];
      if (seg_id < 1 || seg_id > kMaxSegments) continue;
      seg_present_[seg_id] = true;
      const uint8_t local_in_seg = static_cast<uint8_t>(local[i] % kLedsPerSegment);
      const uint8_t ab_k = (dir[i] == 0) ? local_in_seg : static_cast<uint8_t>((kLedsPerSegment - 1U) - local_in_seg);
      seg_ab_to_global_[seg_id][ab_k] = i;
    }

    // Build adjacency on present segments.
    const uint8_t vcount = MappingTables::vertex_count();
    for (uint8_t v = 0; v < kMaxVertices; ++v) vertex_deg_[v] = 0;

    const uint8_t* sva = MappingTables::seg_vertex_a();
    const uint8_t* svb = MappingTables::seg_vertex_b();
    const uint8_t scount = MappingTables::segment_count();
    for (uint8_t seg_id = 1; seg_id <= scount && seg_id <= kMaxSegments; ++seg_id) {
      if (!seg_present_[seg_id]) continue;
      const uint8_t va = sva[seg_id];
      const uint8_t vb = svb[seg_id];
      if (va >= vcount || vb >= vcount) continue;
      if (va < kMaxVertices && vertex_deg_[va] < kMaxDegree) {
        vertex_nbr_[va][vertex_deg_[va]] = vb;
        vertex_nbr_seg_[va][vertex_deg_[va]] = seg_id;
        vertex_deg_[va] = static_cast<uint8_t>(vertex_deg_[va] + 1U);
      }
      if (vb < kMaxVertices && vertex_deg_[vb] < kMaxDegree) {
        vertex_nbr_[vb][vertex_deg_[vb]] = va;
        vertex_nbr_seg_[vb][vertex_deg_[vb]] = seg_id;
        vertex_deg_[vb] = static_cast<uint8_t>(vertex_deg_[vb] + 1U);
      }
    }

    // Active vertices.
    active_vertex_count_ = 0;
    for (uint8_t v = 0; v < vcount && v < kMaxVertices; ++v) {
      if (vertex_deg_[v] == 0) continue;
      if (active_vertex_count_ < kMaxVertices) active_vertices_[active_vertex_count_++] = v;
    }

    choose_center_vertex();
    compute_dist_to_center();
    compute_center_lanes();
    compute_outermost_vertices();
  }

  bool vertex_is_active(uint8_t v) const {
    if (v >= kMaxVertices) return false;
    return vertex_deg_[v] != 0;
  }

  void choose_center_vertex() {
    const uint8_t vcount = MappingTables::vertex_count();
    if (cfg_.has_configured_center && cfg_.configured_center_vertex_id < vcount &&
        vertex_is_active(cfg_.configured_center_vertex_id)) {
      center_vertex_id_ = cfg_.configured_center_vertex_id;
      return;
    }

    // Fallback: minimax eccentricity center on the active subgraph.
    uint8_t best_v = 0;
    uint8_t best_ecc = 0xFF;

    for (uint8_t i = 0; i < active_vertex_count_; ++i) {
      const uint8_t v = active_vertices_[i];
      bfs_distances(v, tmp_dist_);
      uint8_t ecc = 0;
      for (uint8_t j = 0; j < active_vertex_count_; ++j) {
        const uint8_t u = active_vertices_[j];
        if (tmp_dist_[u] == 0xFF) {
          ecc = 0xFF;
          break;
        }
        if (tmp_dist_[u] > ecc) ecc = tmp_dist_[u];
      }
      if (ecc == 0xFF) continue;
      if (ecc < best_ecc || (ecc == best_ecc && v < best_v)) {
        best_ecc = ecc;
        best_v = v;
      }
    }
    center_vertex_id_ = best_v;
  }

  void bfs_distances(uint8_t start_v, uint8_t* out_dist) const {
    for (uint8_t i = 0; i < kMaxVertices; ++i) out_dist[i] = 0xFF;
    if (!vertex_is_active(start_v)) return;

    uint8_t q[kMaxVertices];
    uint8_t qh = 0, qt = 0;
    out_dist[start_v] = 0;
    q[qt++] = start_v;
    while (qh != qt) {
      const uint8_t v = q[qh++];
      const uint8_t dv = out_dist[v];
      const uint8_t deg = vertex_deg_[v];
      for (uint8_t i = 0; i < deg; ++i) {
        const uint8_t u = vertex_nbr_[v][i];
        if (u >= kMaxVertices) continue;
        if (out_dist[u] != 0xFF) continue;
        out_dist[u] = static_cast<uint8_t>(dv + 1U);
        q[qt++] = u;
      }
    }
  }

  void compute_dist_to_center() { bfs_distances(center_vertex_id_, dist_to_center_); }

  void compute_center_lanes() {
    center_lane_count_ = 0;
    if (!vertex_is_active(center_vertex_id_)) return;
    const uint8_t deg = vertex_deg_[center_vertex_id_];
    for (uint8_t i = 0; i < deg && i < kMaxDegree; ++i) {
      center_lane_neighbor_[i] = vertex_nbr_[center_vertex_id_][i];
      center_lane_seg_[i] = vertex_nbr_seg_[center_vertex_id_][i];
      ++center_lane_count_;
    }
  }

  void compute_outermost_vertices() {
    d_max_ = 0;
    for (uint8_t i = 0; i < active_vertex_count_; ++i) {
      const uint8_t v = active_vertices_[i];
      const uint8_t d = dist_to_center_[v];
      if (d != 0xFF && d > d_max_) d_max_ = d;
    }
    outermost_count_ = 0;
    for (uint8_t i = 0; i < active_vertex_count_; ++i) {
      const uint8_t v = active_vertices_[i];
      if (dist_to_center_[v] == d_max_) {
        outermost_vertices_[outermost_count_++] = v;
      }
    }
  }

  void init_phase(uint32_t now_ms, bool auto_transition_into_inhale) {
    phase_complete_ = false;
    switch (phase_) {
      case Phase::Inhale:
        init_inhale(now_ms, /*advance_rr_offset=*/auto_transition_into_inhale, /*regenerate_paths=*/true);
        return;
      case Phase::Pause1:
        init_pause(now_ms, /*pause2=*/false);
        return;
      case Phase::Exhale:
        init_exhale(now_ms);
        return;
      case Phase::Pause2:
      default:
        init_pause(now_ms, /*pause2=*/true);
        return;
    }
  }

  struct Dot {
    uint8_t start_v = 0;
    uint8_t goal_v = 0;  // center neighbor used as lane
    uint8_t step_count = 0;
    uint8_t step_seg[kMaxVertexPathLen] = {};
    uint8_t step_dir[kMaxVertexPathLen] = {};  // 0=A->B, 1=B->A (canonical endpoints)
    uint16_t total_leds = 0;
    uint32_t pos16 = 0;
    bool done = true;
    bool failed = false;
  };

  void init_inhale(uint32_t now_ms, bool advance_rr_offset, bool regenerate_paths) {
    (void)now_ms;
    if (advance_rr_offset && center_lane_count_ > 0) {
      center_lane_rr_offset_ = static_cast<uint8_t>((center_lane_rr_offset_ + 1U) % center_lane_count_);
    }

    inhale_dot_count_ = cfg_.num_dots;
    if (inhale_dot_count_ > kMaxDots) inhale_dot_count_ = kMaxDots;
    if (inhale_dot_count_ > active_vertex_count_) inhale_dot_count_ = active_vertex_count_;

    // If the center has no lanes, inhale is a no-op.
    if (center_lane_count_ == 0 || inhale_dot_count_ == 0) {
      for (uint8_t i = 0; i < kMaxDots; ++i) dots_[i] = Dot{};
      inhale_all_done_ = true;
      phase_complete_ = true;
      return;
    }

    if (!regenerate_paths) {
      for (uint8_t i = 0; i < inhale_dot_count_; ++i) {
        dots_[i].pos16 = 0;
        dots_[i].done = false;
      }
      inhale_all_done_ = false;
      return;
    }

    inhale_all_done_ = false;

    // Build sorted vertex candidates for farthest pool selection.
    uint8_t cand[kMaxVertices];
    uint8_t cand_len = 0;
    for (uint8_t i = 0; i < active_vertex_count_; ++i) {
      const uint8_t v = active_vertices_[i];
      if (v == center_vertex_id_) continue;
      if (dist_to_center_[v] == 0xFF) continue;
      cand[cand_len++] = v;
    }

    // Sort: dist desc, degree asc, vertex_id asc.
    for (uint8_t i = 1; i < cand_len; ++i) {
      const uint8_t key = cand[i];
      int8_t j = static_cast<int8_t>(i) - 1;
      while (j >= 0) {
        const uint8_t v = cand[static_cast<uint8_t>(j)];
        const uint8_t da = dist_to_center_[key];
        const uint8_t db = dist_to_center_[v];
        bool before = false;
        if (da != db) before = da > db;
        else if (vertex_deg_[key] != vertex_deg_[v]) before = vertex_deg_[key] < vertex_deg_[v];
        else before = key < v;
        if (!before) break;
        cand[static_cast<uint8_t>(j + 1)] = cand[static_cast<uint8_t>(j)];
        --j;
      }
      cand[static_cast<uint8_t>(j + 1)] = key;
    }

    uint8_t n_farthest = static_cast<uint8_t>(inhale_dot_count_ * 2U);
    if (n_farthest < inhale_dot_count_) n_farthest = inhale_dot_count_;
    if (n_farthest > cand_len) n_farthest = cand_len;
    if (n_farthest == 0) {
      inhale_all_done_ = true;
      phase_complete_ = true;
      return;
    }

    uint8_t pool[kMaxVertices];
    uint8_t pool_len = n_farthest;
    for (uint8_t i = 0; i < pool_len; ++i) pool[i] = cand[i];

    for (uint8_t i = 0; i < inhale_dot_count_; ++i) {
      dots_[i] = Dot{};
      const uint8_t pick = static_cast<uint8_t>(rand_u32() % pool_len);
      const uint8_t start_v = pool[pick];
      pool[pick] = pool[static_cast<uint8_t>(pool_len - 1U)];
      pool_len = static_cast<uint8_t>(pool_len - 1U);

      dots_[i].start_v = start_v;
      dots_[i].done = false;
      dots_[i].failed = false;
      dots_[i].pos16 = 0;

      // Assign a lane (goal vertex adjacent to center) with round-robin offset.
      uint8_t lane_index = static_cast<uint8_t>((i + center_lane_rr_offset_) % center_lane_count_);
      uint8_t goal_v = center_lane_neighbor_[lane_index];

      // Try to route to assigned goal; if it fails, try alternates.
      bool ok = false;
      for (uint8_t attempt = 0; attempt < center_lane_count_; ++attempt) {
        lane_index = static_cast<uint8_t>((i + center_lane_rr_offset_ + attempt) % center_lane_count_);
        goal_v = center_lane_neighbor_[lane_index];
        ok = build_inhale_dot_path(start_v, goal_v, &dots_[i]);
        if (ok) break;
      }
      if (!ok) {
        dots_[i].failed = true;
        dots_[i].done = true;
      }
    }

    // Completion for inhale ignores failed dots.
    inhale_all_done_ = true;
    for (uint8_t i = 0; i < inhale_dot_count_; ++i) {
      if (!dots_[i].done) {
        inhale_all_done_ = false;
        break;
      }
    }
  }

  bool plateau_safe(uint8_t v, uint8_t goal, uint32_t visited_mask) const {
    if (v == goal) return true;
    const uint8_t dv = dist_to_center_[v];
    if (dv == 0xFF) return false;
    const uint8_t deg = vertex_deg_[v];
    for (uint8_t i = 0; i < deg; ++i) {
      const uint8_t u = vertex_nbr_[v][i];
      if (u >= kMaxVertices) continue;
      if ((visited_mask & (1u << u)) != 0) continue;
      const uint8_t du = dist_to_center_[u];
      if (du != 0xFF && du < dv) return true;
    }
    return false;
  }

  bool build_inhale_dot_path(uint8_t start_v, uint8_t goal_v, Dot* out_dot) {
    if (out_dot == nullptr) return false;
    out_dot->goal_v = goal_v;
    out_dot->step_count = 0;
    out_dot->total_leds = 0;

    // Find a vertex path from start_v -> goal_v with dist non-increasing and no cycles.
    if (start_v == goal_v) {
      // Direct lane to center.
      const uint8_t seg_lane = find_seg_between(goal_v, center_vertex_id_);
      if (seg_lane == 0) return false;
      out_dot->step_seg[0] = seg_lane;
      out_dot->step_dir[0] = traversal_dir(goal_v, center_vertex_id_, seg_lane);
      out_dot->step_count = 1;
      out_dot->total_leds = kLedsPerSegment;
      return true;
    }

    uint8_t vpath[kMaxVertexPathLen + 1] = {};
    uint8_t segpath[kMaxVertexPathLen] = {};
    uint8_t cand_v[kMaxVertexPathLen + 1][kMaxDegree] = {};
    uint8_t cand_seg[kMaxVertexPathLen + 1][kMaxDegree] = {};
    uint8_t cand_count[kMaxVertexPathLen + 1] = {};
    uint8_t cand_pos[kMaxVertexPathLen + 1] = {};

    uint32_t visited = 0;
    uint8_t depth = 0;
    vpath[0] = start_v;
    visited |= (1u << start_v);
    cand_count[0] = 0xFF;

    while (true) {
      const uint8_t v = vpath[depth];
      if (v == goal_v) {
        break;
      }
      if (depth >= kMaxVertexPathLen) {
        return false;
      }

      if (cand_count[depth] == 0xFF) {
        // Build candidate list for this depth.
        cand_count[depth] = 0;
        cand_pos[depth] = 0;
        const uint8_t dv = dist_to_center_[v];
        const uint8_t deg = vertex_deg_[v];
        for (uint8_t i = 0; i < deg; ++i) {
          const uint8_t u = vertex_nbr_[v][i];
          const uint8_t seg_id = vertex_nbr_seg_[v][i];
          if (u >= kMaxVertices) continue;
          if ((visited & (1u << u)) != 0) continue;
          const uint8_t du = dist_to_center_[u];
          if (du == 0xFF) continue;
          if (du > dv) continue;  // no backtracking
          if (du == dv && !plateau_safe(u, goal_v, visited)) continue;
          // Append; we'll sort below.
          const uint8_t c = cand_count[depth];
          if (c < kMaxDegree) {
            cand_v[depth][c] = u;
            cand_seg[depth][c] = seg_id;
            cand_count[depth] = static_cast<uint8_t>(c + 1U);
          }
        }

        // Sort candidates: prefer downhill (smaller dist), then vertex id.
        const uint8_t c = cand_count[depth];
        for (uint8_t i = 1; i < c; ++i) {
          const uint8_t key_u = cand_v[depth][i];
          const uint8_t key_s = cand_seg[depth][i];
          int8_t j = static_cast<int8_t>(i) - 1;
          while (j >= 0) {
            const uint8_t u = cand_v[depth][static_cast<uint8_t>(j)];
            const uint8_t du_key = dist_to_center_[key_u];
            const uint8_t du_u = dist_to_center_[u];
            bool before = false;
            if (du_key != du_u) before = du_key < du_u;  // closer to center first
            else before = key_u < u;
            if (!before) break;
            cand_v[depth][static_cast<uint8_t>(j + 1)] = cand_v[depth][static_cast<uint8_t>(j)];
            cand_seg[depth][static_cast<uint8_t>(j + 1)] = cand_seg[depth][static_cast<uint8_t>(j)];
            --j;
          }
          cand_v[depth][static_cast<uint8_t>(j + 1)] = key_u;
          cand_seg[depth][static_cast<uint8_t>(j + 1)] = key_s;
        }
      }

      if (cand_pos[depth] >= cand_count[depth]) {
        // Backtrack.
        if (depth == 0) return false;
        visited &= ~(1u << vpath[depth]);
        cand_count[depth] = 0;
        cand_pos[depth] = 0;
        --depth;
        continue;
      }

      const uint8_t u = cand_v[depth][cand_pos[depth]];
      const uint8_t s = cand_seg[depth][cand_pos[depth]];
      cand_pos[depth] = static_cast<uint8_t>(cand_pos[depth] + 1U);

      // Take edge v->u.
      segpath[depth] = s;
      vpath[depth + 1] = u;
      visited |= (1u << u);
      ++depth;
      cand_count[depth] = 0xFF;
    }

    const uint8_t vlen = static_cast<uint8_t>(depth + 1U);
    // Append final edge goal_v -> center.
    const uint8_t seg_lane = find_seg_between(goal_v, center_vertex_id_);
    if (seg_lane == 0) return false;

    // Convert to dot step list (segments).
    uint8_t step_count = 0;
    for (uint8_t i = 0; i + 1 < vlen; ++i) {
      const uint8_t seg_id = segpath[i];
      out_dot->step_seg[step_count] = seg_id;
      out_dot->step_dir[step_count] = traversal_dir(vpath[i], vpath[i + 1], seg_id);
      ++step_count;
    }
    out_dot->step_seg[step_count] = seg_lane;
    out_dot->step_dir[step_count] = traversal_dir(goal_v, center_vertex_id_, seg_lane);
    ++step_count;

    out_dot->step_count = step_count;
    out_dot->total_leds = static_cast<uint16_t>(step_count) * kLedsPerSegment;
    return true;
  }

  uint8_t traversal_dir(uint8_t from_v, uint8_t to_v, uint8_t seg_id) const {
    const uint8_t* sva = MappingTables::seg_vertex_a();
    const uint8_t* svb = MappingTables::seg_vertex_b();
    const uint8_t va = sva[seg_id];
    const uint8_t vb = svb[seg_id];
    if (from_v == va && to_v == vb) return 0;  // A->B
    if (from_v == vb && to_v == va) return 1;  // B->A
    return 0;
  }

  uint8_t find_seg_between(uint8_t a, uint8_t b) const {
    if (a >= kMaxVertices || b >= kMaxVertices) return 0;
    const uint8_t deg = vertex_deg_[a];
    for (uint8_t i = 0; i < deg; ++i) {
      if (vertex_nbr_[a][i] == b) return vertex_nbr_seg_[a][i];
    }
    return 0;
  }

  uint16_t dot_global_at(const Dot& d, uint16_t led_pos) const {
    if (d.step_count == 0) return 0xFFFF;
    const uint16_t step = static_cast<uint16_t>(led_pos / kLedsPerSegment);
    const uint8_t k = static_cast<uint8_t>(led_pos % kLedsPerSegment);
    if (step >= d.step_count) return 0xFFFF;
    const uint8_t seg_id = d.step_seg[step];
    const uint8_t sdir = d.step_dir[step];
    const uint8_t ab_k = (sdir == 0) ? k : static_cast<uint8_t>((kLedsPerSegment - 1U) - k);
    return seg_ab_to_global_[seg_id][ab_k];
  }

  void render_inhale(const EffectFrame& frame, Rgb* out, uint16_t led_count) {
    const uint32_t dt = dt_ms_from_frame(frame);
    (void)led_count;

    inhale_all_done_ = true;
    for (uint8_t i = 0; i < inhale_dot_count_; ++i) {
      if (dots_[i].failed) continue;
      if (!dots_[i].done) inhale_all_done_ = false;
    }

    if (inhale_all_done_) {
      if (manual_enabled_) {
        // Loop inhale motion in manual mode.
        init_inhale(frame.now_ms, /*advance_rr_offset=*/false, /*regenerate_paths=*/false);
        inhale_all_done_ = false;
      } else {
        phase_complete_ = true;
        return;
      }
    }

    for (uint8_t i = 0; i < inhale_dot_count_; ++i) {
      Dot& d = dots_[i];
      if (d.failed || d.done || d.total_leds == 0) continue;
      const uint32_t delta = static_cast<uint32_t>(cfg_.dot_speed_q16) * dt;
      d.pos16 += delta;
      const uint32_t end16 = static_cast<uint32_t>(d.total_leds) << 16;
      if (d.pos16 >= end16) {
        d.pos16 = end16 ? (end16 - 1U) : 0;
        d.done = true;
      }
    }

    // Render dots with brightness-only tails.
    for (uint8_t i = 0; i < inhale_dot_count_; ++i) {
      const Dot& d = dots_[i];
      if (d.failed || d.total_leds == 0) continue;
      const uint16_t head = static_cast<uint16_t>(d.pos16 >> 16);
      const uint8_t tail = cfg_.tail_length_leds;
      for (uint8_t t = 0; t <= tail && t < kTailLutLen; ++t) {
        if (head < t) break;
        const uint16_t p = static_cast<uint16_t>(head - t);
        const uint16_t gi = dot_global_at(d, p);
        if (gi == 0xFFFF || gi >= built_led_count_) continue;
        const uint8_t v =
            static_cast<uint8_t>((static_cast<uint16_t>(kTailLut[t]) * frame.params.brightness) / 255U);
        blend_max(out, gi, scale(kInhaleDotColor, v));
      }
    }
  }

  void init_exhale(uint32_t now_ms) {
    (void)now_ms;
    exhale_pos16_ = 0;
    exhale_last_int_ = 0;
    exhale_wave_complete_ = false;
    for (uint8_t i = 0; i < outermost_count_; ++i) {
      exhale_received_[i] = 0;
      exhale_last_wave_seen_[i] = 0xFF;
    }
  }

  void render_exhale(const EffectFrame& frame, Rgb* out, uint16_t led_count) {
    const uint32_t dt = dt_ms_from_frame(frame);
    if (d_max_ == 0 || outermost_count_ == 0) {
      if (!manual_enabled_) phase_complete_ = true;
      return;
    }

    const uint32_t delta = static_cast<uint32_t>(cfg_.wave_speed_layers_q16) * dt;
    exhale_pos16_ += delta;

    const uint32_t prev_int = exhale_last_int_;
    const uint32_t cur_int = exhale_pos16_ >> 16;
    exhale_last_int_ = cur_int;

    // Process integer layer crossings (bounded).
    uint32_t steps = 0;
    for (uint32_t x = prev_int + 1; x <= cur_int && steps < 64; ++x, ++steps) {
      const uint32_t wave_span = static_cast<uint32_t>(d_max_) + 1U;
      const uint32_t wave_id = x / wave_span;
      const uint32_t layer = x % wave_span;
      if (layer != d_max_) continue;
      for (uint8_t i = 0; i < outermost_count_; ++i) {
        if (exhale_last_wave_seen_[i] == wave_id) continue;
        exhale_last_wave_seen_[i] = static_cast<uint8_t>(wave_id);
        if (exhale_received_[i] < cfg_.target_waves) {
          exhale_received_[i] = static_cast<uint8_t>(exhale_received_[i] + 1U);
        }
      }
    }

    // Completion condition: all outermost vertices received target waves.
    exhale_wave_complete_ = true;
    for (uint8_t i = 0; i < outermost_count_; ++i) {
      if (exhale_received_[i] < cfg_.target_waves) {
        exhale_wave_complete_ = false;
        break;
      }
    }
    if (exhale_wave_complete_ && !manual_enabled_) {
      phase_complete_ = true;
    }

    // Visual: emphasize a band around current radius (distance layer).
    const uint16_t span = static_cast<uint16_t>(d_max_ + 1U);
    const uint16_t layer_int = static_cast<uint16_t>((cur_int % span));
    const uint16_t layer_frac = static_cast<uint16_t>(exhale_pos16_ & 0xFFFFu);
    const uint32_t radius_q16 = (static_cast<uint32_t>(layer_int) << 16) | layer_frac;
    const uint16_t bw_q16 = cfg_.exhale_band_width_q16 ? cfg_.exhale_band_width_q16 : 1;

    const uint8_t* seg = MappingTables::global_to_seg();
    const uint16_t* local = MappingTables::global_to_local();
    const uint8_t* dir = MappingTables::global_to_dir();
    const uint8_t* sva = MappingTables::seg_vertex_a();
    const uint8_t* svb = MappingTables::seg_vertex_b();

    for (uint16_t i = 0; i < led_count; ++i) {
      const uint8_t seg_id = seg[i];
      if (seg_id < 1 || seg_id > kMaxSegments) continue;
      const uint8_t va = sva[seg_id];
      const uint8_t vb = svb[seg_id];
      const uint8_t da = (va < kMaxVertices) ? dist_to_center_[va] : 0xFF;
      const uint8_t db = (vb < kMaxVertices) ? dist_to_center_[vb] : 0xFF;
      if (da == 0xFF || db == 0xFF) continue;

      const uint8_t local_in_seg = static_cast<uint8_t>(local[i] % kLedsPerSegment);
      const uint8_t ab_k = (dir[i] == 0) ? local_in_seg : static_cast<uint8_t>((kLedsPerSegment - 1U) - local_in_seg);
      const uint32_t d_led_q16 =
          ((static_cast<uint32_t>(da) * (kLedsPerSegment - 1U - ab_k) + static_cast<uint32_t>(db) * ab_k) << 16) /
          (kLedsPerSegment - 1U);

      const uint32_t diff = (d_led_q16 > radius_q16) ? (d_led_q16 - radius_q16) : (radius_q16 - d_led_q16);
      if (diff >= static_cast<uint32_t>(bw_q16)) continue;
      const uint32_t amp_q16 = (static_cast<uint32_t>(bw_q16 - diff) << 16) / bw_q16;  // 0..1
      const uint8_t v = static_cast<uint8_t>(
          (amp_q16 * frame.params.brightness) >> 16);
      if (v == 0) continue;
      blend_max(out, i, scale(kExhaleWaveColor, v));
    }
  }

  static uint8_t pulse_u8(uint32_t ms_since_beat) {
    // Attack + decay envelope per beat.
    if (ms_since_beat < 180U) {
      return static_cast<uint8_t>((ms_since_beat * 255U) / 180U);
    }
    if (ms_since_beat < 1200U) {
      const uint32_t t = ms_since_beat - 180U;
      const uint32_t rem = 1020U - t;
      return static_cast<uint8_t>((rem * 200U) / 1020U);
    }
    return 0;
  }

  void init_pause(uint32_t now_ms, bool pause2) {
    (void)pause2;
    pause_beats_done_ = 0;
    pause_beats_target_ = cfg_.beats_target_min;
    if (cfg_.beats_target_max >= cfg_.beats_target_min) {
      const uint8_t span = static_cast<uint8_t>(cfg_.beats_target_max - cfg_.beats_target_min + 1U);
      pause_beats_target_ = static_cast<uint8_t>(cfg_.beats_target_min + (rand_u32() % span));
    }
    pause_last_beat_ms_ = now_ms;
  }

  void render_pause(const EffectFrame& frame, Rgb* out, uint16_t led_count, bool pause2) {
    const uint32_t dt = dt_ms_from_frame(frame);
    (void)dt;

    bool beat_event = false;
    if (static_cast<int32_t>(frame.now_ms - pause_last_beat_ms_) >= static_cast<int32_t>(cfg_.beat_period_ms)) {
      beat_event = true;
      pause_last_beat_ms_ = frame.now_ms;
    } else if (static_cast<int32_t>(frame.now_ms - pause_last_beat_ms_) >=
               static_cast<int32_t>(cfg_.max_pause_duration_ms)) {
      beat_event = true;
      pause_last_beat_ms_ = frame.now_ms;
    }

    if (beat_event && pause_beats_done_ < pause_beats_target_) {
      pause_beats_done_ = static_cast<uint8_t>(pause_beats_done_ + 1U);
    }

    const uint16_t t16 =
        pause_beats_target_ ? static_cast<uint16_t>((static_cast<uint32_t>(pause_beats_done_) * 65535U) /
                                                    static_cast<uint32_t>(pause_beats_target_))
                            : 65535U;
    const Rgb from = pause2 ? kExhalePauseColor : kInhalePauseColor;
    const Rgb to = pause2 ? kInhalePauseColor : kExhalePauseColor;
    const Rgb base = lerp(from, to, t16);

    const uint8_t hb = pulse_u8(static_cast<uint32_t>(frame.now_ms - pause_last_beat_ms_));
    const uint8_t v = static_cast<uint8_t>((static_cast<uint16_t>(hb) * frame.params.brightness) / 255U);
    if (v == 0) {
      // Keep black background.
    } else {
      const Rgb c = scale(base, v);
      for (uint16_t i = 0; i < led_count; ++i) out[i] = c;
    }

    if (pause_beats_done_ >= pause_beats_target_ && !manual_enabled_) {
      phase_complete_ = true;
    }
  }

  void render_phase(const EffectFrame& frame, Rgb* out, uint16_t led_count, Phase p) {
    switch (p) {
      case Phase::Inhale:
        render_inhale(frame, out, led_count);
        return;
      case Phase::Pause1:
        render_pause(frame, out, led_count, /*pause2=*/false);
        return;
      case Phase::Exhale:
        render_exhale(frame, out, led_count);
        return;
      case Phase::Pause2:
      default:
        render_pause(frame, out, led_count, /*pause2=*/true);
        return;
    }
  }

  // Core state.
  bool built_ = false;
  uint16_t built_led_count_ = 0;

  Config cfg_{};

  bool manual_enabled_ = false;
  Phase manual_phase_ = Phase::Inhale;
  uint32_t manual_start_ms_ = 0;

  Phase phase_ = Phase::Inhale;
  uint32_t phase_start_ms_ = 0;
  uint32_t last_now_ms_ = 0;
  bool phase_complete_ = false;

  uint32_t rng_state_ = 1;
  uint8_t center_lane_rr_offset_ = 0;

  // Topology cache (active subgraph).
  bool seg_present_[kMaxSegments + 1] = {};
  uint16_t seg_ab_to_global_[kMaxSegments + 1][kLedsPerSegment] = {};

  uint8_t vertex_deg_[kMaxVertices] = {};
  uint8_t vertex_nbr_[kMaxVertices][kMaxDegree] = {};
  uint8_t vertex_nbr_seg_[kMaxVertices][kMaxDegree] = {};

  uint8_t active_vertices_[kMaxVertices] = {};
  uint8_t active_vertex_count_ = 0;

  uint8_t center_vertex_id_ = 0;
  uint8_t dist_to_center_[kMaxVertices] = {};
  uint8_t tmp_dist_[kMaxVertices] = {};

  uint8_t center_lane_neighbor_[kMaxDegree] = {};
  uint8_t center_lane_seg_[kMaxDegree] = {};
  uint8_t center_lane_count_ = 0;

  uint8_t d_max_ = 0;
  uint8_t outermost_vertices_[kMaxVertices] = {};
  uint8_t outermost_count_ = 0;

  // INHALE state.
  Dot dots_[kMaxDots] = {};
  uint8_t inhale_dot_count_ = 0;
  bool inhale_all_done_ = false;

  // EXHALE state.
  uint32_t exhale_pos16_ = 0;
  uint32_t exhale_last_int_ = 0;
  uint8_t exhale_received_[kMaxVertices] = {};
  uint8_t exhale_last_wave_seen_[kMaxVertices] = {};
  bool exhale_wave_complete_ = false;

  // PAUSE state.
  uint8_t pause_beats_target_ = 0;
  uint8_t pause_beats_done_ = 0;
  uint32_t pause_last_beat_ms_ = 0;
};

}  // namespace core
}  // namespace chromance
