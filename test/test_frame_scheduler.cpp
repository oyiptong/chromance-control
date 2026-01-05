#include <unity.h>

#include "core/effects/frame_scheduler.h"

using chromance::core::FrameScheduler;

void test_frame_scheduler_uncapped() {
  FrameScheduler s(0);
  s.reset(100);

  TEST_ASSERT_TRUE(s.should_render(100));
  TEST_ASSERT_EQUAL_UINT32(0, s.dt_ms());

  TEST_ASSERT_TRUE(s.should_render(105));
  TEST_ASSERT_EQUAL_UINT32(5, s.dt_ms());

  TEST_ASSERT_TRUE(s.should_render(250));
  TEST_ASSERT_EQUAL_UINT32(145, s.dt_ms());
}

void test_frame_scheduler_50fps_fixed_interval() {
  FrameScheduler s(50);  // 20ms
  s.reset(0);

  TEST_ASSERT_TRUE(s.should_render(0));
  TEST_ASSERT_EQUAL_UINT32(0, s.dt_ms());

  TEST_ASSERT_FALSE(s.should_render(19));

  TEST_ASSERT_TRUE(s.should_render(20));
  TEST_ASSERT_EQUAL_UINT32(20, s.dt_ms());

  TEST_ASSERT_TRUE(s.should_render(40));
  TEST_ASSERT_EQUAL_UINT32(20, s.dt_ms());
}

void test_frame_scheduler_60fps_deterministic_rounding() {
  FrameScheduler s(60);
  s.reset(0);

  // First frame at t=0.
  TEST_ASSERT_TRUE(s.should_render(0));
  TEST_ASSERT_EQUAL_UINT32(0, s.dt_ms());

  // Expect a deterministic 16/17/17/16... pattern when sampling at boundaries.
  const uint32_t t1 = s.next_frame_ms();  // 16
  TEST_ASSERT_TRUE(s.should_render(t1));
  TEST_ASSERT_EQUAL_UINT32(16, s.dt_ms());

  const uint32_t t2 = s.next_frame_ms();  // 33
  TEST_ASSERT_TRUE(s.should_render(t2));
  TEST_ASSERT_EQUAL_UINT32(17, s.dt_ms());

  const uint32_t t3 = s.next_frame_ms();  // 50
  TEST_ASSERT_TRUE(s.should_render(t3));
  TEST_ASSERT_EQUAL_UINT32(17, s.dt_ms());

  const uint32_t t4 = s.next_frame_ms();  // 66
  TEST_ASSERT_TRUE(s.should_render(t4));
  TEST_ASSERT_EQUAL_UINT32(16, s.dt_ms());
}

