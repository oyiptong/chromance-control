#include <string.h>

#include <unity.h>

#include "core/diagnostic_pattern.h"

using chromance::core::DiagnosticStripStateMachine;
using chromance::core::DiagnosticStripTiming;
using chromance::core::IDiagnosticRenderer;
using chromance::core::Rgb;

namespace {

struct FakeRenderer final : public IDiagnosticRenderer {
  bool on[chromance::core::kStripCount][12];
  Rgb color[chromance::core::kStripCount];

  FakeRenderer() { clear(); }

  void clear() { memset(on, 0, sizeof(on)); }

  void set_segment(uint8_t strip_index, uint16_t segment_index, Rgb c, bool is_on) override {
    if (strip_index >= chromance::core::kStripCount || segment_index >= 12) {
      return;
    }
    color[strip_index] = c;
    on[strip_index][segment_index] = is_on;
  }
};

void tick_to_next_transition(DiagnosticStripStateMachine& sm, uint32_t& now_ms) {
  now_ms = sm.next_transition_ms();
  sm.tick(now_ms);
}

}  // namespace

void test_strip_sm_flash_count_and_latch() {
  const DiagnosticStripTiming timing(3, 5, 7);
  DiagnosticStripStateMachine sm(2, timing);
  sm.reset(0);

  uint32_t t = 0;
  TEST_ASSERT_FALSE(sm.is_segment_on(0));
  TEST_ASSERT_FALSE(sm.is_segment_on(1));

  // Enter first ON.
  tick_to_next_transition(sm, t);
  TEST_ASSERT_TRUE(sm.is_segment_on(0));
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOn, sm.phase());
  TEST_ASSERT_EQUAL_UINT8(0, sm.flashes_completed());

  // Run through 5 ON->OFF transitions.
  for (uint8_t i = 1; i <= DiagnosticStripStateMachine::kFlashCount; ++i) {
    tick_to_next_transition(sm, t);  // FlashOn -> FlashOff
    TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOff, sm.phase());
    TEST_ASSERT_FALSE(sm.is_segment_on(0));
    TEST_ASSERT_EQUAL_UINT8(i, sm.flashes_completed());

    if (i < DiagnosticStripStateMachine::kFlashCount) {
      tick_to_next_transition(sm, t);  // FlashOff -> FlashOn
      TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOn, sm.phase());
      TEST_ASSERT_TRUE(sm.is_segment_on(0));
    }
  }

  // After final OFF duration, segment should latch ON.
  tick_to_next_transition(sm, t);  // FlashOff -> LatchedOn
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::LatchedOn, sm.phase());
  TEST_ASSERT_TRUE(sm.is_segment_on(0));
  TEST_ASSERT_EQUAL_UINT16(0, sm.current_segment());

  // After hold, advance to segment 1 (segment 0 stays ON).
  tick_to_next_transition(sm, t);  // LatchedOn -> segment 1 FlashOff
  TEST_ASSERT_EQUAL_UINT16(1, sm.current_segment());
  TEST_ASSERT_TRUE(sm.is_segment_on(0));
  TEST_ASSERT_FALSE(sm.is_segment_on(1));
}

void test_strip_sm_restart_clears_previous_segments() {
  const DiagnosticStripTiming timing(3, 5, 7);
  DiagnosticStripStateMachine sm(2, timing);
  sm.reset(0);

  uint32_t t = 0;

  // Drive until we're flashing segment 1.
  while (sm.current_segment() == 0) {
    tick_to_next_transition(sm, t);
  }
  TEST_ASSERT_EQUAL_UINT16(1, sm.current_segment());

  // Drive until we wrap back to segment 0.
  while (sm.current_segment() != 0 || sm.flashes_completed() != 0) {
    tick_to_next_transition(sm, t);
  }

  TEST_ASSERT_FALSE(sm.is_segment_on(0));
  TEST_ASSERT_FALSE(sm.is_segment_on(1));
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::FlashOff, sm.phase());
}

void test_pattern_render_sends_segment_states() {
  chromance::core::DiagnosticPattern pattern;
  pattern.reset(0);

  FakeRenderer renderer;
  renderer.clear();
  pattern.render(renderer);

  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_FALSE(renderer.on[strip][0]);
  }

  // Advance time to first transition (all strips share the same default timing).
  const uint32_t t = pattern.strip_sm(0).next_transition_ms();
  pattern.tick(t);

  renderer.clear();
  pattern.render(renderer);

  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_TRUE(renderer.on[strip][0]);
  }
}
