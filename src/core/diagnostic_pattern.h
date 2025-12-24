#pragma once

#include <stdint.h>

#include "diagnostic_strip_sm.h"
#include "layout.h"

namespace chromance {
namespace core {

class IDiagnosticRenderer {
 public:
  virtual ~IDiagnosticRenderer() {}
  virtual void set_segment(uint8_t strip_index,
                           uint16_t segment_index,
                           Rgb color,
                           bool on) = 0;
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
        renderer.set_segment(strip, seg, cfg.diagnostic_color, strip_sms_[strip].is_segment_on(seg));
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

