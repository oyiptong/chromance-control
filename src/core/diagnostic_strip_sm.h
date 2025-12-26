#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

struct SegmentDiagnosticTiming {
  uint32_t flash_on_ms;
  uint32_t flash_off_ms;
  uint32_t latched_hold_ms;

  constexpr SegmentDiagnosticTiming(uint32_t flash_on_ms_,
                                    uint32_t flash_off_ms_,
                                    uint32_t latched_hold_ms_)
      : flash_on_ms(flash_on_ms_),
        flash_off_ms(flash_off_ms_),
        latched_hold_ms(latched_hold_ms_) {}

  constexpr SegmentDiagnosticTiming() : flash_on_ms(150), flash_off_ms(150), latched_hold_ms(100) {}
};

class DiagnosticStripStateMachine {
 public:
  enum class Phase : uint8_t {
    FlashOn,
    FlashOff,
    LatchedOn,
    DoneFullOn,
  };

  explicit DiagnosticStripStateMachine(uint16_t segment_count,
                                       SegmentDiagnosticTiming timing = SegmentDiagnosticTiming())
      : segment_count_(segment_count), timing_(timing) {
    reset(0);
  }

  void reset(uint32_t now_ms) {
    current_segment_ = 0;
    flashes_completed_ = 0;
    phase_ = Phase::FlashOff;
    next_transition_ms_ = now_ms + timing_.flash_off_ms;
  }

  void tick(uint32_t now_ms) {
    if (segment_count_ == 0) {
      return;
    }

    while (time_reached(now_ms, next_transition_ms_)) {
      const uint32_t transition_at = next_transition_ms_;

      switch (phase_) {
        case Phase::FlashOff:
          if (flashes_completed_ < required_flash_count(current_segment_)) {
            phase_ = Phase::FlashOn;
            next_transition_ms_ = transition_at + timing_.flash_on_ms;
          } else {
            phase_ = Phase::LatchedOn;
            next_transition_ms_ = transition_at + timing_.latched_hold_ms;
          }
          break;

        case Phase::FlashOn:
          phase_ = Phase::FlashOff;
          ++flashes_completed_;
          next_transition_ms_ = transition_at + timing_.flash_off_ms;
          break;

        case Phase::LatchedOn:
          advance_to_next_segment(transition_at);
          break;

        case Phase::DoneFullOn:
          return;
      }
    }
  }

  uint16_t segment_count() const { return segment_count_; }
  uint16_t current_segment() const { return current_segment_; }
  Phase phase() const { return phase_; }
  uint8_t flashes_completed() const { return flashes_completed_; }
  uint32_t next_transition_ms() const { return next_transition_ms_; }
  const SegmentDiagnosticTiming& timing() const { return timing_; }

  bool is_done() const { return phase_ == Phase::DoneFullOn; }

  uint8_t required_flash_count(uint16_t segment_index) const {
    // Segment order is 1-based: segment 0 flashes 1 time, segment 1 flashes 2 times, etc.
    return static_cast<uint8_t>(segment_index + 1);
  }

  bool is_segment_on(uint16_t segment_index) const {
    if (segment_index >= segment_count_) {
      return false;
    }
    if (phase_ == Phase::DoneFullOn) {
      return true;
    }

    if (segment_index < current_segment_) {
      return true;
    }
    if (segment_index > current_segment_) {
      return false;
    }

    return phase_ == Phase::FlashOn || phase_ == Phase::LatchedOn;
  }

 private:
  static bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
  }

  void advance_to_next_segment(uint32_t transition_at) {
    if (current_segment_ + 1 >= segment_count_) {
      phase_ = Phase::DoneFullOn;
      next_transition_ms_ = transition_at;
      return;
    }

    ++current_segment_;
    flashes_completed_ = 0;
    phase_ = Phase::FlashOff;
    next_transition_ms_ = transition_at + timing_.flash_off_ms;
  }

  uint16_t segment_count_ = 0;
  uint16_t current_segment_ = 0;
  uint8_t flashes_completed_ = 0;
  Phase phase_ = Phase::FlashOff;
  uint32_t next_transition_ms_ = 0;
  SegmentDiagnosticTiming timing_;
};

}  // namespace core
}  // namespace chromance
