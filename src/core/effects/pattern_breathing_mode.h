#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "../mapping/mapping_tables.h"
#include "../types.h"
#include "effect.h"

namespace chromance {
namespace core {

// Mode 7: "Breathing"
//
// Cycle:
// - Inhale 4s: single red/orange dots travel from outer perimeter toward the center, one-by-one,
//   each along a different spoke direction (6 spokes total).
// - Pause 3s: whole display pulses like a heartbeat at 60bpm (1Hz), color shifts inhale->exhale.
// - Exhale 7s: outward-radiating sinusoidal gradient along 6 spokes, cyan/light-green hues.
// - Pause 3s: heartbeat pulse, color shifts exhale->inhale.
//
// Background is black (unlit pixels remain black).
class BreathingEffect final : public IEffect {
 public:
  const char* id() const override { return "Breathing"; }

  void reset(uint32_t now_ms) override {
    cycle_start_ms_ = now_ms;
    built_ = false;
    manual_enabled_ = false;
    manual_phase_ = 0;
    manual_start_ms_ = now_ms;
  }

  // Manual phase selection (for interactive control).
  // When enabled, the effect stays in the chosen phase but continues animating within it.
  //
  // Phase order:
  // 0 = inhale, 1 = pause 1, 2 = exhale, 3 = pause 2.
  void set_manual_phase(uint8_t phase, uint32_t now_ms) {
    manual_enabled_ = true;
    manual_phase_ = static_cast<uint8_t>(phase % 4U);
    manual_start_ms_ = now_ms;
  }

  void next_phase(uint32_t now_ms) { set_manual_phase(static_cast<uint8_t>(manual_phase_ + 1U), now_ms); }
  void prev_phase(uint32_t now_ms) { set_manual_phase(static_cast<uint8_t>(manual_phase_ + 3U), now_ms); }

  void set_auto(uint32_t now_ms) {
    manual_enabled_ = false;
    cycle_start_ms_ = now_ms;  // restart the full cycle
  }

  bool manual_enabled() const { return manual_enabled_; }
  uint8_t manual_phase() const { return manual_phase_; }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) return;
    for (size_t i = 0; i < led_count; ++i) out_rgb[i] = kBlack;

    if (!built_) {
      build_cache(static_cast<uint16_t>(led_count));
      built_ = true;
    }

    if (manual_enabled_) {
      render_manual(frame, out_rgb, static_cast<uint16_t>(led_count));
      return;
    }

    const uint32_t t = frame.now_ms - cycle_start_ms_;
    const uint32_t t_mod = t % kCycleMs;

    if (t_mod < kInhaleMs) {
      // Inhale: use previous exhale wave but reversed direction (inward).
      render_wave(frame,
                  out_rgb,
                  static_cast<uint16_t>(led_count),
                  t_mod,
                  kInhaleMs,
                  /*outward=*/false,
                  kInhaleA,
                  kInhaleB);
      return;
    }
    uint32_t tt = t_mod - kInhaleMs;
    if (tt < kPauseMs) {
      render_pause(frame, out_rgb, static_cast<uint16_t>(led_count), tt, kInhaleA, kExhaleB);
      return;
    }
    tt -= kPauseMs;
    if (tt < kExhaleMs) {
      // Exhale: use previous inhale dots but reversed direction (outward) and greenish color.
      render_dots(frame,
                  out_rgb,
                  static_cast<uint16_t>(led_count),
                  tt,
                  kExhaleMs,
                  /*outward=*/true,
                  kExhaleB);
      return;
    }
    tt -= kExhaleMs;
    render_pause(frame, out_rgb, static_cast<uint16_t>(led_count), tt, kExhaleB, kInhaleA);
  }

 private:
  static constexpr uint32_t kInhaleMs = 4000;
  static constexpr uint32_t kPauseMs = 3000;
  static constexpr uint32_t kExhaleMs = 7000;
  static constexpr uint32_t kCycleMs = kInhaleMs + kPauseMs + kExhaleMs + kPauseMs;

  static constexpr uint8_t kSpokes = 6;
  static constexpr uint32_t kHeartbeatPeriodMs = 2000;  // ~30bpm

  // Colors are in RGB space; note some hardware may be GRB ordered.
  static const Rgb kInhaleA;      // red-orange
  static const Rgb kInhaleB;      // warm yellow-orange
  static const Rgb kExhaleA;      // cyan-ish
  static const Rgb kExhaleB;      // light green-ish

  static Rgb scale(const Rgb& c, uint8_t v) {
    return Rgb{static_cast<uint8_t>((static_cast<uint16_t>(c.r) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.g) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.b) * v) / 255U)};
  }

  static Rgb lerp(const Rgb& a, const Rgb& b, uint16_t t16) {
    const uint16_t ia = static_cast<uint16_t>(65535U - t16);
    return Rgb{
        static_cast<uint8_t>((static_cast<uint32_t>(a.r) * ia + static_cast<uint32_t>(b.r) * t16) >>
                             16),
        static_cast<uint8_t>((static_cast<uint32_t>(a.g) * ia + static_cast<uint32_t>(b.g) * t16) >>
                             16),
        static_cast<uint8_t>((static_cast<uint32_t>(a.b) * ia + static_cast<uint32_t>(b.b) * t16) >>
                             16),
    };
  }

  static uint16_t clamp16(int32_t v) {
    if (v < 0) return 0;
    if (v > 65535) return 65535;
    return static_cast<uint16_t>(v);
  }

  static uint16_t smoothstep_u16(uint16_t t) {
    const uint32_t t2 = (static_cast<uint32_t>(t) * t + 0x8000U) >> 16;
    const uint32_t t3 = (t2 * t + 0x8000U) >> 16;
    const int32_t y = static_cast<int32_t>(3 * t2) - static_cast<int32_t>(2 * t3);
    return clamp16(y);
  }

  static uint8_t heartbeat_u8(uint32_t ms_in_pause) {
    // ~30bpm = 0.5Hz. One short pulse with decay per beat period.
    const uint32_t phase_ms = ms_in_pause % kHeartbeatPeriodMs;
    if (phase_ms < 180U) {
      const uint16_t t = static_cast<uint16_t>((phase_ms * 65535U) / 180U);
      return static_cast<uint8_t>((smoothstep_u16(t) * 255U) >> 16);
    }
    if (phase_ms < 1200U) {
      const uint16_t t = static_cast<uint16_t>(((phase_ms - 180U) * 65535U) / 1020U);
      const uint16_t y = static_cast<uint16_t>(65535U - smoothstep_u16(t));
      return static_cast<uint8_t>((y * 200U) >> 16);
    }
    return 0;
  }

  uint32_t phase_duration_ms(uint8_t phase) const {
    switch (phase & 3U) {
      case 0:
        return kInhaleMs;
      case 1:
        return kPauseMs;
      case 2:
        return kExhaleMs;
      case 3:
      default:
        return kPauseMs;
    }
  }

  void render_manual(const EffectFrame& frame, Rgb* out, uint16_t n) {
    const uint32_t dur = phase_duration_ms(manual_phase_);
    const uint32_t local = dur ? ((frame.now_ms - manual_start_ms_) % dur) : 0;

    switch (manual_phase_ & 3U) {
      case 0:  // inhale (wave inward)
        render_wave(frame, out, n, local, kInhaleMs, /*outward=*/false, kInhaleA, kInhaleB);
        break;
      case 1:  // pause 1
        render_pause(frame, out, n, local, kInhaleA, kExhaleB);
        break;
      case 2:  // exhale (dot outward)
        render_dots(frame, out, n, local, kExhaleMs, /*outward=*/true, kExhaleB);
        break;
      case 3:  // pause 2
      default:
        render_pause(frame, out, n, local, kExhaleB, kInhaleA);
        break;
    }
  }

  void build_cache(uint16_t led_count) {
    // Define 6 spoke directions (unit vectors around the circle).
    // 0 degrees points +x, increasing CCW.
    const float dirs_x[kSpokes] = {1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f};
    const float dirs_y[kSpokes] = {0.0f, 0.8660254f, 0.8660254f, 0.0f, -0.8660254f, -0.8660254f};

    const int16_t cx = static_cast<int16_t>((MappingTables::width() - 1) / 2);
    const int16_t cy = static_cast<int16_t>((MappingTables::height() - 1) / 2);

    for (uint8_t s = 0; s < kSpokes; ++s) {
      max_pos_[s] = 1.0f;
      farthest_led_[s] = 0;
    }

    for (uint16_t i = 0; i < led_count; ++i) {
      const int16_t x = MappingTables::pixel_x()[i];
      const int16_t y = MappingTables::pixel_y()[i];
      const float dx = static_cast<float>(x - cx);
      const float dy = static_cast<float>(y - cy);
      float best = -1e9f;
      uint8_t best_s = 0;
      for (uint8_t s = 0; s < kSpokes; ++s) {
        const float dot = dx * dirs_x[s] + dy * dirs_y[s];
        if (dot > best) {
          best = dot;
          best_s = s;
        }
      }
      if (best < 0) best = 0;
      spoke_[i] = best_s;
      pos_[i] = best;
      if (best > max_pos_[best_s]) {
        max_pos_[best_s] = best;
        farthest_led_[best_s] = i;
      }
    }
  }

  void render_dots(const EffectFrame& frame,
                   Rgb* out,
                   uint16_t n,
                   uint32_t t_ms,
                   uint32_t duration_ms,
                   bool outward,
                   const Rgb& color) const {
    const uint8_t beat_brightness = frame.params.brightness;
    const Rgb c = scale(color, beat_brightness);

    // 6 dots, one after another through distinct spokes.
    const uint32_t per = duration_ms / kSpokes;  // floor; close enough
    uint8_t dot_index = static_cast<uint8_t>((t_ms / per) % kSpokes);
    if (dot_index >= kSpokes) dot_index = 0;

    const uint32_t t0 = dot_index * per;
    const uint32_t local = t_ms - t0;
    const float phase = per ? static_cast<float>(local) / static_cast<float>(per) : 1.0f;  // 0..1

    const float target = outward ? (phase * max_pos_[dot_index]) : ((1.0f - phase) * max_pos_[dot_index]);
    uint16_t best_i = farthest_led_[dot_index];
    float best_d = 1e9f;
    for (uint16_t i = 0; i < n; ++i) {
      if (spoke_[i] != dot_index) continue;
      const float d = fabsf(pos_[i] - target);
      if (d < best_d) {
        best_d = d;
        best_i = i;
      }
    }

    out[best_i] = c;
  }

  void render_pause(const EffectFrame& frame,
                    Rgb* out,
                    uint16_t n,
                    uint32_t t_ms,
                    const Rgb& from,
                    const Rgb& to) const {
    const uint8_t hb = heartbeat_u8(t_ms);
    if (hb == 0) return;

    const uint16_t t16 = static_cast<uint16_t>((t_ms * 65535U) / kPauseMs);
    const Rgb base = lerp(from, to, t16);
    const uint8_t v =
        static_cast<uint8_t>((static_cast<uint16_t>(hb) * frame.params.brightness) / 255U);
    const Rgb c = scale(base, v);

    for (uint16_t i = 0; i < n; ++i) out[i] = c;
  }

  void render_wave(const EffectFrame& frame,
                   Rgb* out,
                   uint16_t n,
                   uint32_t t_ms,
                   uint32_t duration_ms,
                   bool outward,
                   const Rgb& a,
                   const Rgb& b) const {
    // Radiating waves along 6 spokes. Sinusoidal intensity based on normalized position and time.
    const float tt = static_cast<float>(t_ms) / 1000.0f;
    const float speed = 0.55f;        // waves per second
    const float wavelength = 0.33f;   // in normalized spoke units

    for (uint16_t i = 0; i < n; ++i) {
      const uint8_t s = spoke_[i];
      const float denom = max_pos_[s] > 0.5f ? max_pos_[s] : 1.0f;
      const float u = pos_[i] / denom;  // 0..1-ish

      // Wave direction: outward uses negative time term; inward uses positive.
      const float phase = (u / wavelength) + (outward ? -1.0f : 1.0f) * (tt * speed);
      const float w = 0.5f + 0.5f * sinf(6.2831853f * phase);

      // Add a gentle breath envelope so the wave feels like a single inhale/exhale.
      const float env_t = static_cast<float>(t_ms) / static_cast<float>(duration_ms);
      const float env = 0.2f + 0.8f * (0.5f - 0.5f * cosf(3.1415926f * env_t));  // 0.2..1.0

      const float amp = w * env;
      if (amp <= 0.01f) continue;

      const uint16_t mix = static_cast<uint16_t>(clamp16(static_cast<int32_t>(u * 65535.0f)));
      const Rgb base = lerp(a, b, mix);
      const uint8_t v = static_cast<uint8_t>(amp * static_cast<float>(frame.params.brightness));
      out[i] = scale(base, v);
    }
  }

  uint32_t cycle_start_ms_ = 0;
  bool built_ = false;
  bool manual_enabled_ = false;
  uint8_t manual_phase_ = 0;
  uint32_t manual_start_ms_ = 0;

  uint8_t spoke_[MappingTables::led_count()] = {};
  float pos_[MappingTables::led_count()] = {};
  float max_pos_[kSpokes] = {};
  uint16_t farthest_led_[kSpokes] = {};
};

}  // namespace core
}  // namespace chromance
