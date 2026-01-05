#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../types.h"
#include "effect.h"

namespace chromance {
namespace core {

class TwoDotsEffect final : public IEffect {
 public:
  explicit TwoDotsEffect(uint16_t step_ms = 25) : step_ms_(step_ms) {}

  const char* id() const override { return "Seven_Comets"; }

  void reset(uint32_t now_ms) override {
    start_ms_ = now_ms;
    last_step_ = 0;
    rng_ = 0x9E3779B9u ^ now_ms;
    for (uint8_t i = 0; i < kCometCount; ++i) {
      reset_comet(i);
    }
  }

  void render(const EffectFrame& frame,
              const PixelsMap& /*map*/,
              Rgb* out_rgb,
              size_t led_count) override {
    if (out_rgb == nullptr || led_count == 0) {
      return;
    }

    for (size_t i = 0; i < led_count; ++i) {
      out_rgb[i] = kBlack;
    }

    const uint32_t elapsed = frame.now_ms - start_ms_;
    const uint32_t step = step_ms_ ? (elapsed / step_ms_) : elapsed;
    const size_t n = led_count;

    if (step != last_step_) {
      advance_sequences(static_cast<uint32_t>(step - last_step_));
      last_step_ = step;
    }

    const size_t base = static_cast<size_t>(step % n);
    for (uint8_t i = 0; i < kCometCount; ++i) {
      const size_t offset = (n * static_cast<size_t>(i)) / kCometCount;
      const bool forward = (i % 2U) == 0;
      const size_t head_pos =
          forward ? ((base + offset) % n) : ((offset + (n - 1U - base)) % n);
      const uint8_t head_len = head_len_[i];
      const uint16_t comet_len = static_cast<uint16_t>(head_len * 2U);
      if (forward) {
        render_comet_forward(head_pos, color_[i], frame.params.brightness, out_rgb, n, comet_len,
                             head_len);
      } else {
        render_comet_backward(head_pos, color_[i], frame.params.brightness, out_rgb, n, comet_len,
                              head_len);
      }
    }
  }

  static constexpr uint8_t comet_count() { return kCometCount; }
  Rgb color(uint8_t i) const { return i < kCometCount ? color_[i] : kBlack; }
  uint8_t head_len(uint8_t i) const { return i < kCometCount ? head_len_[i] : 0; }
  uint32_t sequence_len_steps(uint8_t i) const { return i < kCometCount ? seq_len_steps_[i] : 0; }
  uint32_t sequence_remaining_steps(uint8_t i) const {
    return i < kCometCount ? seq_remaining_steps_[i] : 0;
  }

 private:
  static constexpr uint32_t kMinSeqLenSteps = 40;   // 1.0s at 25ms
  static constexpr uint32_t kMaxSeqLenSteps = 240;  // 6.0s at 25ms

  void render_comet_forward(size_t head_pos,
                            const Rgb& base,
                            uint8_t brightness,
                            Rgb* out_rgb,
                            size_t n,
                            uint16_t comet_len,
                            uint8_t head_len) const {
    for (uint16_t d = 0; d < comet_len; ++d) {
      const size_t idx = static_cast<size_t>((head_pos + n - (d % n)) % n);
      out_rgb[idx] = add_sat(out_rgb[idx], scale(base, scale_for_offset(d, head_len, brightness)));
    }
  }

  void render_comet_backward(size_t head_pos,
                             const Rgb& base,
                             uint8_t brightness,
                             Rgb* out_rgb,
                             size_t n,
                             uint16_t comet_len,
                             uint8_t head_len) const {
    for (uint16_t d = 0; d < comet_len; ++d) {
      const size_t idx = static_cast<size_t>((head_pos + (d % n)) % n);
      out_rgb[idx] = add_sat(out_rgb[idx], scale(base, scale_for_offset(d, head_len, brightness)));
    }
  }

  static uint8_t scale_for_offset(uint16_t d, uint8_t head, uint8_t brightness) {
    if (head == 0) {
      return 0;
    }

    uint8_t alpha = 0;
    if (d < head) {
      alpha = 255;
    } else if (d < static_cast<uint16_t>(head * 2U)) {
      const uint8_t t = static_cast<uint8_t>(d - head);  // 0..head-1
      // Tail length equals head length; fade linearly to near-black.
      const uint8_t denom = static_cast<uint8_t>(head - 1U);
      alpha = denom ? static_cast<uint8_t>(((head - 1U - t) * 255U) / denom) : 0;
    } else {
      alpha = 0;
    }

    return static_cast<uint8_t>((static_cast<uint16_t>(alpha) * brightness) / 255U);
  }

  static Rgb scale(const Rgb& c, uint8_t v) {
    return Rgb{static_cast<uint8_t>((static_cast<uint16_t>(c.r) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.g) * v) / 255U),
               static_cast<uint8_t>((static_cast<uint16_t>(c.b) * v) / 255U)};
  }

  static uint8_t add_u8_sat(uint8_t a, uint8_t b) {
    const uint16_t s = static_cast<uint16_t>(a) + b;
    return static_cast<uint8_t>(s > 255 ? 255 : s);
  }

  static Rgb add_sat(const Rgb& a, const Rgb& b) {
    return Rgb{add_u8_sat(a.r, b.r), add_u8_sat(a.g, b.g), add_u8_sat(a.b, b.b)};
  }

  static uint32_t xorshift32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
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

  uint32_t next_u32() {
    rng_ = xorshift32(rng_);
    return rng_;
  }

  uint32_t pick_unique_seq_len_steps(uint8_t comet_index) {
    // Pick a sequence length that is unique among comets (at this moment).
    for (uint8_t attempt = 0; attempt < 32; ++attempt) {
      const uint32_t span = kMaxSeqLenSteps - kMinSeqLenSteps + 1U;
      const uint32_t candidate = kMinSeqLenSteps + (next_u32() % span);
      bool unique = true;
      for (uint8_t j = 0; j < kCometCount; ++j) {
        if (j == comet_index) continue;
        if (seq_len_steps_[j] == candidate) {
          unique = false;
          break;
        }
      }
      if (unique) {
        return candidate;
      }
    }

    // Fallback: walk the range until unique.
    for (uint32_t candidate = kMinSeqLenSteps; candidate <= kMaxSeqLenSteps; ++candidate) {
      bool unique = true;
      for (uint8_t j = 0; j < kCometCount; ++j) {
        if (j == comet_index) continue;
        if (seq_len_steps_[j] == candidate) {
          unique = false;
          break;
        }
      }
      if (unique) {
        return candidate;
      }
    }
    return kMinSeqLenSteps;
  }

  void reset_comet(uint8_t i) {
    const uint8_t hue = static_cast<uint8_t>(next_u32() & 0xFF);
    color_[i] = hue_to_rgb(hue);
    head_len_[i] = static_cast<uint8_t>(3U + (next_u32() % 3U));  // 3..5
    seq_len_steps_[i] = pick_unique_seq_len_steps(i);
    seq_remaining_steps_[i] = seq_len_steps_[i];
  }

  void advance_sequences(uint32_t delta_steps) {
    if (delta_steps == 0) return;

    for (uint8_t i = 0; i < kCometCount; ++i) {
      uint32_t remaining = seq_remaining_steps_[i];
      if (remaining == 0) {
        reset_comet(i);
        remaining = seq_remaining_steps_[i];
      }

      uint32_t d = delta_steps;
      while (d >= remaining) {
        d -= remaining;
        reset_comet(i);
        remaining = seq_remaining_steps_[i];
        if (remaining == 0) {
          remaining = 1;
        }
      }
      seq_remaining_steps_[i] = static_cast<uint32_t>(remaining - d);
    }
  }

  static constexpr uint8_t kCometCount = 7;

  uint32_t start_ms_ = 0;
  uint16_t step_ms_ = 25;
  uint32_t last_step_ = 0;
  uint32_t rng_ = 0x12345678u;
  uint8_t head_len_[kCometCount] = {3, 3, 3, 3, 3, 3, 3};
  uint32_t seq_len_steps_[kCometCount] = {kMinSeqLenSteps,
                                         kMinSeqLenSteps + 1,
                                         kMinSeqLenSteps + 2,
                                         kMinSeqLenSteps + 3,
                                         kMinSeqLenSteps + 4,
                                         kMinSeqLenSteps + 5,
                                         kMinSeqLenSteps + 6};
  uint32_t seq_remaining_steps_[kCometCount] = {kMinSeqLenSteps,
                                               kMinSeqLenSteps + 1,
                                               kMinSeqLenSteps + 2,
                                               kMinSeqLenSteps + 3,
                                               kMinSeqLenSteps + 4,
                                               kMinSeqLenSteps + 5,
                                               kMinSeqLenSteps + 6};
  Rgb color_[kCometCount] = {
      Rgb{255, 0, 0}, Rgb{0, 255, 0}, Rgb{0, 0, 255}, Rgb{255, 255, 0},
      Rgb{255, 0, 255}, Rgb{0, 255, 255}, Rgb{255, 255, 255},
  };
};

}  // namespace core
}  // namespace chromance
