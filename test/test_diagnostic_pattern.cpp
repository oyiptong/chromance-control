#include <string.h>

#include <unity.h>

#include "core/diagnostic_pattern.h"

using chromance::core::DiagnosticStripStateMachine;
using chromance::core::DiagnosticStripTiming;
using chromance::core::IDiagnosticRenderer;
using chromance::core::Rgb;

namespace {

struct FakeRenderer final : public IDiagnosticRenderer {
  enum class SegmentMode : uint8_t { Off, AllOn, SingleLed };

  SegmentMode mode[chromance::core::kStripCount][12];
  uint8_t single_led[chromance::core::kStripCount][12];
  Rgb color[chromance::core::kStripCount];

  FakeRenderer() { clear(); }

  void clear() {
    memset(mode, 0, sizeof(mode));
    memset(single_led, 0, sizeof(single_led));
  }

  void set_segment_all(uint8_t strip_index,
                       uint16_t segment_index,
                       Rgb c,
                       bool is_on) override {
    if (strip_index >= chromance::core::kStripCount || segment_index >= 12) {
      return;
    }
    color[strip_index] = c;
    mode[strip_index][segment_index] = is_on ? SegmentMode::AllOn : SegmentMode::Off;
  }

  void set_segment_single_led(uint8_t strip_index,
                              uint16_t segment_index,
                              uint8_t led_in_segment,
                              Rgb c) override {
    if (strip_index >= chromance::core::kStripCount || segment_index >= 12) {
      return;
    }
    color[strip_index] = c;
    mode[strip_index][segment_index] = SegmentMode::SingleLed;
    single_led[strip_index][segment_index] = led_in_segment;
  }
};

void tick_to_next_transition(DiagnosticStripStateMachine& sm, uint32_t& now_ms) {
  now_ms = sm.next_transition_ms();
  sm.tick(now_ms);
}

}  // namespace

void test_strip_sm_chase_count_and_latch() {
  const DiagnosticStripTiming timing(3, 7);
  DiagnosticStripStateMachine sm(2, timing);
  sm.reset(0);

  uint32_t t = 0;

  TEST_ASSERT_EQUAL_UINT16(0, sm.current_segment());
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::ChaseSingleLed, sm.phase());
  TEST_ASSERT_EQUAL_UINT8(0, sm.chase_repeat_index());
  TEST_ASSERT_EQUAL_UINT8(0, sm.current_led());
  TEST_ASSERT_TRUE(sm.is_led_on(0, 0));
  TEST_ASSERT_FALSE(sm.is_led_on(0, 1));
  TEST_ASSERT_FALSE(sm.is_led_on(1, 0));

  // Advance to LED 13 on the first pass.
  for (uint8_t expected_led = 1; expected_led < chromance::core::kLedsPerSegment; ++expected_led) {
    tick_to_next_transition(sm, t);
    TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::ChaseSingleLed, sm.phase());
    TEST_ASSERT_EQUAL_UINT8(0, sm.chase_repeat_index());
    TEST_ASSERT_EQUAL_UINT8(expected_led, sm.current_led());
  }

  // Wrap to LED 0 on the second pass.
  tick_to_next_transition(sm, t);
  TEST_ASSERT_EQUAL_UINT8(1, sm.chase_repeat_index());
  TEST_ASSERT_EQUAL_UINT8(0, sm.current_led());

  // Run to completion of all repeats; should latch full segment ON.
  while (sm.phase() != DiagnosticStripStateMachine::Phase::LatchedOn) {
    tick_to_next_transition(sm, t);
  }
  TEST_ASSERT_EQUAL_UINT16(0, sm.current_segment());
  TEST_ASSERT_EQUAL_UINT8(DiagnosticStripStateMachine::kChaseRepeatCount - 1, sm.chase_repeat_index());
  for (uint8_t led = 0; led < chromance::core::kLedsPerSegment; ++led) {
    TEST_ASSERT_TRUE(sm.is_led_on(0, led));
  }

  // After hold, advance to segment 1 (segment 0 stays ON; segment 1 chase starts).
  tick_to_next_transition(sm, t);
  TEST_ASSERT_EQUAL_UINT16(1, sm.current_segment());
  for (uint8_t led = 0; led < chromance::core::kLedsPerSegment; ++led) {
    TEST_ASSERT_TRUE(sm.is_led_on(0, led));
  }
  TEST_ASSERT_TRUE(sm.is_led_on(1, 0));
  TEST_ASSERT_FALSE(sm.is_led_on(1, 1));
}

void test_strip_sm_restart_clears_previous_segments() {
  const DiagnosticStripTiming timing(3, 7);
  DiagnosticStripStateMachine sm(2, timing);
  sm.reset(0);

  uint32_t t = 0;

  // Drive until we're flashing segment 1.
  while (sm.current_segment() == 0) {
    tick_to_next_transition(sm, t);
  }
  TEST_ASSERT_EQUAL_UINT16(1, sm.current_segment());

  // Drive until we wrap back to segment 0.
  while (sm.current_segment() != 0 || sm.chase_repeat_index() != 0 || sm.current_led() != 0) {
    tick_to_next_transition(sm, t);
  }

  // On restart, prior segments are cleared and the chase starts at LED 0 of segment 0.
  for (uint8_t led = 0; led < chromance::core::kLedsPerSegment; ++led) {
    TEST_ASSERT_EQUAL(led == 0, sm.is_led_on(0, led));
  }
  for (uint8_t led = 0; led < chromance::core::kLedsPerSegment; ++led) {
    TEST_ASSERT_FALSE(sm.is_led_on(1, led));
  }
  TEST_ASSERT_EQUAL(DiagnosticStripStateMachine::Phase::ChaseSingleLed, sm.phase());
}

void test_pattern_render_sends_segment_states() {
  chromance::core::DiagnosticPattern pattern;
  pattern.reset(0);

  FakeRenderer renderer;
  renderer.clear();
  pattern.render(renderer);

  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(FakeRenderer::SegmentMode::SingleLed),
                      static_cast<uint8_t>(renderer.mode[strip][0]));
    TEST_ASSERT_EQUAL_UINT8(0, renderer.single_led[strip][0]);
  }

  // Advance time to first transition (all strips share the same default timing).
  const uint32_t t = pattern.strip_sm(0).next_transition_ms();
  pattern.tick(t);

  renderer.clear();
  pattern.render(renderer);

  for (uint8_t strip = 0; strip < chromance::core::kStripCount; ++strip) {
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(FakeRenderer::SegmentMode::SingleLed),
                      static_cast<uint8_t>(renderer.mode[strip][0]));
    TEST_ASSERT_EQUAL_UINT8(1, renderer.single_led[strip][0]);
  }
}
