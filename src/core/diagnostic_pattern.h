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
  DiagnosticPattern()
      : strip_sms_{DiagnosticStripStateMachine(kStripConfigs[0].segment_count),
                   DiagnosticStripStateMachine(kStripConfigs[1].segment_count),
                   DiagnosticStripStateMachine(kStripConfigs[2].segment_count),
                   DiagnosticStripStateMachine(kStripConfigs[3].segment_count)} {
    static_assert(kStripCount == 4, "update constructor if kStripCount changes");
  }

  void reset(uint32_t now_ms) {
    for (uint8_t i = 0; i < kStripCount; ++i) {
      strip_sms_[i].reset(now_ms);
    }
  }

  void tick(uint32_t now_ms) {
    for (uint8_t i = 0; i < kStripCount; ++i) {
      strip_sms_[i].tick(now_ms);
    }
  }

  void render(IDiagnosticRenderer& renderer) const {
    for (uint8_t strip = 0; strip < kStripCount; ++strip) {
      const StripConfig& cfg = kStripConfigs[strip];
      for (uint16_t seg = 0; seg < cfg.segment_count; ++seg) {
        const auto& sm = strip_sms_[strip];
        if (seg < sm.current_segment()) {
          renderer.set_segment_all(strip, seg, cfg.diagnostic_color, true);
        } else if (seg > sm.current_segment()) {
          renderer.set_segment_all(strip, seg, cfg.diagnostic_color, false);
        } else if (sm.phase() == DiagnosticStripStateMachine::Phase::LatchedOn) {
          renderer.set_segment_all(strip, seg, cfg.diagnostic_color, true);
        } else {
          renderer.set_segment_single_led(strip, seg, sm.current_led(), cfg.diagnostic_color);
        }
      }
    }
  }

  const DiagnosticStripStateMachine& strip_sm(uint8_t strip_index) const {
    return strip_sms_[strip_index];
  }

 private:
  DiagnosticStripStateMachine strip_sms_[kStripCount];
};

}  // namespace core
}  // namespace chromance
