#pragma once

#include <stdint.h>

#include "layout.h"

namespace chromance {
namespace core {

struct DiagnosticStripTiming {
  uint32_t step_ms;
  uint32_t latched_hold_ms;

  constexpr DiagnosticStripTiming(uint32_t step_ms_, uint32_t latched_hold_ms_)
      : step_ms(step_ms_), latched_hold_ms(latched_hold_ms_) {}

  constexpr DiagnosticStripTiming() : step_ms(500), latched_hold_ms(100) {}
};

class DiagnosticStripStateMachine {
 public:
  static const uint8_t kChaseRepeatCount = 5;

  enum class Phase : uint8_t {
    ChaseSingleLed,
    LatchedOn,
  };

  explicit DiagnosticStripStateMachine(uint16_t segment_count,
                                       DiagnosticStripTiming timing = DiagnosticStripTiming())
      : segment_count_(segment_count), timing_(timing) {
    reset(0);
  }

  void reset(uint32_t now_ms) {
    current_segment_ = 0;
    chase_repeat_index_ = 0;
    current_led_ = 0;
    phase_ = Phase::ChaseSingleLed;
    next_transition_ms_ = now_ms + timing_.step_ms;
  }

  void tick(uint32_t now_ms) {
    if (segment_count_ == 0) {
      return;
    }

    while (time_reached(now_ms, next_transition_ms_)) {
      const uint32_t transition_at = next_transition_ms_;

      switch (phase_) {
        case Phase::ChaseSingleLed:
          advance_chase_or_latch(transition_at);
          break;

        case Phase::LatchedOn:
          advance_to_next_segment(transition_at);
          break;
      }
    }
  }

  uint16_t segment_count() const { return segment_count_; }
  uint16_t current_segment() const { return current_segment_; }
  Phase phase() const { return phase_; }
  uint8_t chase_repeat_index() const { return chase_repeat_index_; }
  uint8_t current_led() const { return current_led_; }
  uint32_t next_transition_ms() const { return next_transition_ms_; }
  const DiagnosticStripTiming& timing() const { return timing_; }

  bool is_led_on(uint16_t segment_index, uint8_t led_in_segment) const {
    if (segment_index >= segment_count_) {
      return false;
    }
    if (led_in_segment >= kLedsPerSegment) {
      return false;
    }

    if (segment_index < current_segment_) {
      return true;
    }
    if (segment_index > current_segment_) {
      return false;
    }

    if (phase_ == Phase::LatchedOn) {
      return true;
    }
    return led_in_segment == current_led_;
  }

 private:
  static bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
  }

  void advance_chase_or_latch(uint32_t transition_at) {
    if (current_led_ + 1 < kLedsPerSegment) {
      ++current_led_;
      next_transition_ms_ = transition_at + timing_.step_ms;
      return;
    }

    // End of segment (LED 13). Either repeat or latch the full segment ON.
    if (chase_repeat_index_ + 1 < kChaseRepeatCount) {
      ++chase_repeat_index_;
      current_led_ = 0;
      next_transition_ms_ = transition_at + timing_.step_ms;
      return;
    }

    phase_ = Phase::LatchedOn;
    current_led_ = 0;
    next_transition_ms_ = transition_at + timing_.latched_hold_ms;
  }

  void advance_to_next_segment(uint32_t transition_at) {
    if (current_segment_ + 1 >= segment_count_) {
      reset(transition_at);
      return;
    }

    ++current_segment_;
    chase_repeat_index_ = 0;
    current_led_ = 0;
    phase_ = Phase::ChaseSingleLed;
    next_transition_ms_ = transition_at + timing_.step_ms;
  }

  uint16_t segment_count_ = 0;
  uint16_t current_segment_ = 0;
  uint8_t chase_repeat_index_ = 0;
  uint8_t current_led_ = 0;
  Phase phase_ = Phase::ChaseSingleLed;
  uint32_t next_transition_ms_ = 0;
  DiagnosticStripTiming timing_;
};

}  // namespace core
}  // namespace chromance
