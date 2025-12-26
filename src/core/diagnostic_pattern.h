#pragma once

#include <stdint.h>

#include "diagnostic_strip_sm.h"
#include "layout.h"

namespace chromance {
namespace core {

class IDiagnosticRenderer {
 public:
  virtual ~IDiagnosticRenderer() {}
  virtual void set_segment_all(uint8_t strip_index,
                               uint16_t segment_index,
                               Rgb color,
                               bool on) = 0;
  virtual void set_segment_single_led(uint8_t strip_index,
                                      uint16_t segment_index,
                                      uint8_t led_in_segment,
                                      Rgb color) = 0;
};

class DiagnosticPattern {
 public:
  struct Timing {
    uint32_t all_off_hold_ms;
    uint32_t all_on_hold_ms;
    uint32_t all_flash_on_ms;
    uint32_t all_flash_off_ms;
    uint8_t all_flash_cycles;
    SegmentDiagnosticTiming segment;

    constexpr Timing()
        : all_off_hold_ms(250),
          all_on_hold_ms(5000),
          all_flash_on_ms(150),
          all_flash_off_ms(150),
          all_flash_cycles(10),
          segment() {}
  };

  enum class Phase : uint8_t {
    AllOffHold,
    AllOnHold,
    AllFlashOff,
    AllFlashOn,
    SegmentDiagnostics,
  };

  explicit DiagnosticPattern(Timing timing = Timing())
      : timing_(timing),
        strip_sms_{DiagnosticStripStateMachine(kStripConfigs[0].segment_count, timing_.segment),
                   DiagnosticStripStateMachine(kStripConfigs[1].segment_count, timing_.segment),
                   DiagnosticStripStateMachine(kStripConfigs[2].segment_count, timing_.segment),
                   DiagnosticStripStateMachine(kStripConfigs[3].segment_count, timing_.segment)} {
    static_assert(kStripCount == 4, "update constructor if kStripCount changes");
    reset(0);
  }

  void reset(uint32_t now_ms) {
    phase_ = Phase::AllOffHold;
    flash_cycles_completed_ = 0;
    next_transition_ms_ = now_ms + timing_.all_off_hold_ms;
  }

  void tick(uint32_t now_ms) {
    while (phase_ != Phase::SegmentDiagnostics && time_reached(now_ms, next_transition_ms_)) {
      const uint32_t transition_at = next_transition_ms_;

      switch (phase_) {
        case Phase::AllOffHold:
          phase_ = Phase::AllOnHold;
          next_transition_ms_ = transition_at + timing_.all_on_hold_ms;
          break;

        case Phase::AllOnHold:
          phase_ = Phase::AllFlashOff;
          flash_cycles_completed_ = 0;
          next_transition_ms_ = transition_at + timing_.all_flash_off_ms;
          break;

        case Phase::AllFlashOff:
          if (flash_cycles_completed_ >= timing_.all_flash_cycles) {
            begin_segment_diagnostics(transition_at);
          } else {
            phase_ = Phase::AllFlashOn;
            next_transition_ms_ = transition_at + timing_.all_flash_on_ms;
          }
          break;

        case Phase::AllFlashOn:
          phase_ = Phase::AllFlashOff;
          ++flash_cycles_completed_;
          next_transition_ms_ = transition_at + timing_.all_flash_off_ms;
          break;

        case Phase::SegmentDiagnostics:
          break;
      }
    }

    if (phase_ != Phase::SegmentDiagnostics) {
      return;
    }

    bool all_done = true;
    for (uint8_t i = 0; i < kStripCount; ++i) {
      strip_sms_[i].tick(now_ms);
      all_done = all_done && strip_sms_[i].is_done();
    }

    if (all_done) {
      reset(now_ms);
    }
  }

  void render(IDiagnosticRenderer& renderer) const {
    const bool all_on = phase_ == Phase::AllOnHold || phase_ == Phase::AllFlashOn;
    const bool all_off = phase_ == Phase::AllOffHold || phase_ == Phase::AllFlashOff;

    for (uint8_t strip = 0; strip < kStripCount; ++strip) {
      const StripConfig& cfg = kStripConfigs[strip];

      for (uint16_t seg = 0; seg < cfg.segment_count; ++seg) {
        if (all_on) {
          renderer.set_segment_all(strip, seg, cfg.diagnostic_color, true);
        } else if (all_off) {
          renderer.set_segment_all(strip, seg, cfg.diagnostic_color, false);
        } else {
          renderer.set_segment_all(strip,
                                   seg,
                                   cfg.diagnostic_color,
                                   strip_sms_[strip].is_segment_on(seg));
        }
      }
    }
  }

  Phase phase() const { return phase_; }
  uint32_t next_transition_ms() const { return next_transition_ms_; }

  const DiagnosticStripStateMachine& strip_sm(uint8_t strip_index) const {
    return strip_sms_[strip_index];
  }

 private:
  static bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
  }

  void begin_segment_diagnostics(uint32_t now_ms) {
    phase_ = Phase::SegmentDiagnostics;
    for (uint8_t i = 0; i < kStripCount; ++i) {
      strip_sms_[i].reset(now_ms);
    }
  }

  Timing timing_;
  Phase phase_ = Phase::AllOffHold;
  uint32_t next_transition_ms_ = 0;
  uint8_t flash_cycles_completed_ = 0;
  DiagnosticStripStateMachine strip_sms_[kStripCount];
};

}  // namespace core
}  // namespace chromance
