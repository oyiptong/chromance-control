#include <algorithm>
#include <vector>

#include <unity.h>

#include "core/layout.h"
#include "core/strip_layout.h"

using chromance::core::Rgb;
using chromance::core::StripConfig;
using chromance::core::kLedsPerSegment;

void test_segment_start_led_normal_orientation() {
  const StripConfig strip{3, false, 0, 0, Rgb{0, 0, 0}};
  TEST_ASSERT_EQUAL_UINT16(0, chromance::core::segment_start_led(strip, 0));
  TEST_ASSERT_EQUAL_UINT16(14, chromance::core::segment_start_led(strip, 1));
  TEST_ASSERT_EQUAL_UINT16(28, chromance::core::segment_start_led(strip, 2));
}

void test_segment_start_led_reversed_orientation() {
  const StripConfig strip{3, true, 0, 0, Rgb{0, 0, 0}};
  TEST_ASSERT_EQUAL_UINT16(28, chromance::core::segment_start_led(strip, 0));
  TEST_ASSERT_EQUAL_UINT16(14, chromance::core::segment_start_led(strip, 1));
  TEST_ASSERT_EQUAL_UINT16(0, chromance::core::segment_start_led(strip, 2));
}

void test_segments_non_overlapping_and_in_bounds() {
  const StripConfig strip{6, true, 0, 0, Rgb{0, 0, 0}};
  const uint16_t total_leds = chromance::core::strip_led_count(strip);

  std::vector<uint16_t> starts;
  starts.reserve(strip.segment_count);
  for (uint16_t seg = 0; seg < strip.segment_count; ++seg) {
    const uint16_t start = chromance::core::segment_start_led(strip, seg);
    TEST_ASSERT_TRUE(start < total_leds);
    TEST_ASSERT_TRUE(start + kLedsPerSegment <= total_leds);
    starts.push_back(start);
  }

  std::sort(starts.begin(), starts.end());
  for (size_t i = 1; i < starts.size(); ++i) {
    TEST_ASSERT_EQUAL_UINT16(starts[i - 1] + kLedsPerSegment, starts[i]);
  }
}
