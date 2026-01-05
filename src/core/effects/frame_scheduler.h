#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

// Deterministic frame scheduler. Owns target_fps (0 = uncapped).
class FrameScheduler {
 public:
  explicit FrameScheduler(uint16_t target_fps = 0) : target_fps_(target_fps) {}

  void set_target_fps(uint16_t target_fps) { target_fps_ = target_fps; }
  uint16_t target_fps() const { return target_fps_; }

  void reset(uint32_t now_ms) {
    last_render_ms_ = now_ms;
    next_frame_ms_ = now_ms;
    remainder_acc_ = 0;
    last_dt_ms_ = 0;
  }

  // Returns true when a frame should be rendered at now_ms.
  // If true, dt_ms() reflects time since last rendered frame.
  bool should_render(uint32_t now_ms) {
    if (target_fps_ == 0) {
      last_dt_ms_ = now_ms - last_render_ms_;
      last_render_ms_ = now_ms;
      return true;
    }

    if (!time_reached(now_ms, next_frame_ms_)) {
      return false;
    }

    // Catch up deterministically if we missed multiple frame boundaries.
    do {
      advance_next_frame();
    } while (time_reached(now_ms, next_frame_ms_));

    last_dt_ms_ = now_ms - last_render_ms_;
    last_render_ms_ = now_ms;
    return true;
  }

  uint32_t dt_ms() const { return last_dt_ms_; }
  uint32_t next_frame_ms() const { return next_frame_ms_; }

 private:
  static bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
  }

  void advance_next_frame() {
    // Interval is 1000/fps with deterministic rounding spread over frames.
    // Example: 60fps => 1000/60 = 16 remainder 40 => pattern 16/17/17/16...
    const uint32_t base = 1000U / static_cast<uint32_t>(target_fps_);
    const uint16_t rem = static_cast<uint16_t>(1000U % static_cast<uint32_t>(target_fps_));

    next_frame_ms_ += base;
    remainder_acc_ = static_cast<uint16_t>(remainder_acc_ + rem);
    if (remainder_acc_ >= target_fps_) {
      next_frame_ms_ += 1;
      remainder_acc_ = static_cast<uint16_t>(remainder_acc_ - target_fps_);
    }
  }

  uint16_t target_fps_ = 0;
  uint32_t last_render_ms_ = 0;
  uint32_t next_frame_ms_ = 0;
  uint16_t remainder_acc_ = 0;
  uint32_t last_dt_ms_ = 0;
};

}  // namespace core
}  // namespace chromance

