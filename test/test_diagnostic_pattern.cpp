#include <string.h>

#include <unity.h>

#include "core/diagnostic_pattern.h"

using chromance::core::DiagnosticStripStateMachine;
using chromance::core::DiagnosticPattern;
using chromance::core::IDiagnosticRenderer;
using chromance::core::Rgb;
using chromance::core::SegmentDiagnosticTiming;

namespace {

struct FakeRenderer final : public IDiagnosticRenderer {
  bool on[chromance::core::kStripCount][12];
  Rgb color[chromance::core::kStripCount]{};

  FakeRenderer() { clear(); }

  void clear() {
    memset(on, 0, sizeof(on));
  }

  void set_segment_all(uint8_t strip_index,
                       uint16_t segment_index,
                       Rgb c,
                       bool is_on) override {
    if (strip_index >= chromance::core::kStripCount || segment_index >= 12) {
      return;
    }
    color[strip_index] = c;
    on[strip_index][segment_index] = is_on;
  }

  void set_segment_single_led(uint8_t strip_index,
                              uint16_t segment_index,
                              uint8_t led_in_segment,
                              Rgb c) override {
    if (strip_index >= chromance::core::kStripCount || segment_index >= 12) {
      return;
    }
    (void)led_in_segment;
    color[strip_index] = c;
  }
};

void tick_to_next_transition(DiagnosticStripStateMachine& sm, uint32_t& now_ms) {
  now_ms = sm.next_transition_ms();
  sm.tick(now_ms);
}

}  // namespace

void test_strip_sm_segment_order_flash_counts() {
  const SegmentDiagnosticTiming timing(3, 5, 7);
  DiagnosticStripStateMachine sm(3, timing);
  sm.reset(0);

  uint32_t t = 0;

  TEST_ASSERT_EQUAL_UINT16(0, sm.current_segment());
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOff, sm.phase());
  TEST_ASSERT_FALSE(sm.is_segment_on(0));

  // Segment 0: flashes 1 time, then latches on.
  tick_to_next_transition(sm, t);
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOn, sm.phase());
  TEST_ASSERT_TRUE(sm.is_segment_on(0));

  tick_to_next_transition(sm, t);
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOff, sm.phase());
  TEST_ASSERT_EQUAL_UINT8(1, sm.flashes_completed());
  TEST_ASSERT_FALSE(sm.is_segment_on(0));

  tick_to_next_transition(sm, t);
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::LatchedOn, sm.phase());
  TEST_ASSERT_TRUE(sm.is_segment_on(0));

  tick_to_next_transition(sm, t);
  TEST_ASSERT_EQUAL_UINT16(1, sm.current_segment());
  TEST_ASSERT_TRUE(sm.is_segment_on(0));
  TEST_ASSERT_FALSE(sm.is_segment_on(1));

  // Segment 1: requires 2 flashes; after 1 flash it should not latch.
  tick_to_next_transition(sm, t);  // FlashOff -> FlashOn
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOn, sm.phase());
  tick_to_next_transition(sm, t);  // FlashOn -> FlashOff (flash 1 complete)
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOff, sm.phase());
  TEST_ASSERT_EQUAL_UINT8(1, sm.flashes_completed());

  tick_to_next_transition(sm, t);  // FlashOff -> FlashOn (should not latch yet)
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOn, sm.phase());
  TEST_ASSERT_EQUAL_UINT16(1, sm.current_segment());
}

void test_strip_sm_done_after_last_segment() {
  const SegmentDiagnosticTiming timing(3, 5, 7);
  DiagnosticStripStateMachine sm(1, timing);
  sm.reset(0);

  uint32_t t = 0;
  tick_to_next_transition(sm, t);  // FlashOff -> FlashOn
  tick_to_next_transition(sm, t);  // FlashOn -> FlashOff (flash 1 complete)
  tick_to_next_transition(sm, t);  // FlashOff -> LatchedOn
  TEST_ASSERT_FALSE(sm.is_done());
  tick_to_next_transition(sm, t);  // LatchedOn -> DoneFullOn
  TEST_ASSERT_TRUE(sm.is_done());
  TEST_ASSERT_TRUE(sm.is_segment_on(0));
}

void test_pattern_phase_sequence_and_restart() {
  DiagnosticPattern::Timing timing;
  timing.all_off_hold_ms = 1;
  timing.all_on_hold_ms = 2;
  timing.all_flash_off_ms = 1;
  timing.all_flash_on_ms = 1;
  timing.all_flash_cycles = 2;
  timing.segment = SegmentDiagnosticTiming(1, 1, 1);

  DiagnosticPattern pattern(timing);
  pattern.reset(0);

  FakeRenderer renderer;
  renderer.clear();
  pattern.render(renderer);

  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_FALSE(renderer.on[strip][0]);
  }

  pattern.tick(1);  // -> AllOnHold
  renderer.clear();
  pattern.render(renderer);
  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_TRUE(renderer.on[strip][0]);
  }

  pattern.tick(3);  // -> AllFlashOff
  renderer.clear();
  pattern.render(renderer);
  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_FALSE(renderer.on[strip][0]);
  }

  pattern.tick(4);  // -> AllFlashOn
  renderer.clear();
  pattern.render(renderer);
  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_TRUE(renderer.on[strip][0]);
  }

  pattern.tick(5);  // -> AllFlashOff (cycle 1 complete)
  pattern.tick(6);  // -> AllFlashOn
  pattern.tick(7);  // -> AllFlashOff (cycle 2 complete)
  pattern.tick(8);  // -> SegmentDiagnostics (resets strip SMs)

  TEST_ASSERT_EQUAL(static_cast<uint8_t>(DiagnosticPattern::Phase::SegmentDiagnostics),
                    static_cast<uint8_t>(pattern.phase()));

  renderer.clear();
  pattern.render(renderer);
  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_FALSE(renderer.on[strip][0]);
  }

  // Fast-forward until all strips complete their segments; pattern should restart to all-off hold.
  pattern.tick(10000);
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(DiagnosticPattern::Phase::AllOffHold),
                    static_cast<uint8_t>(pattern.phase()));
  renderer.clear();
  pattern.render(renderer);
  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_FALSE(renderer.on[strip][0]);
  }
}
