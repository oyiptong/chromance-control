#include <unity.h>

#include "core/brightness.h"

using chromance::core::brightness_step_down_10;
using chromance::core::brightness_step_up_10;
using chromance::core::clamp_percent_0_100;
using chromance::core::percent_to_u8_255;
using chromance::core::quantize_percent_to_10;

void test_brightness_clamp_percent() {
  TEST_ASSERT_EQUAL_UINT8(0, clamp_percent_0_100(0));
  TEST_ASSERT_EQUAL_UINT8(1, clamp_percent_0_100(1));
  TEST_ASSERT_EQUAL_UINT8(100, clamp_percent_0_100(100));
  TEST_ASSERT_EQUAL_UINT8(100, clamp_percent_0_100(101));
  TEST_ASSERT_EQUAL_UINT8(100, clamp_percent_0_100(255));
}

void test_brightness_quantize_to_10_rounding() {
  TEST_ASSERT_EQUAL_UINT8(0, quantize_percent_to_10(0));
  TEST_ASSERT_EQUAL_UINT8(0, quantize_percent_to_10(1));
  TEST_ASSERT_EQUAL_UINT8(0, quantize_percent_to_10(4));
  TEST_ASSERT_EQUAL_UINT8(10, quantize_percent_to_10(5));
  TEST_ASSERT_EQUAL_UINT8(10, quantize_percent_to_10(6));
  TEST_ASSERT_EQUAL_UINT8(10, quantize_percent_to_10(14));
  TEST_ASSERT_EQUAL_UINT8(20, quantize_percent_to_10(15));
  TEST_ASSERT_EQUAL_UINT8(70, quantize_percent_to_10(73));
  TEST_ASSERT_EQUAL_UINT8(100, quantize_percent_to_10(95));
  TEST_ASSERT_EQUAL_UINT8(100, quantize_percent_to_10(99));
  TEST_ASSERT_EQUAL_UINT8(100, quantize_percent_to_10(100));
  TEST_ASSERT_EQUAL_UINT8(100, quantize_percent_to_10(255));
}

void test_brightness_step_up_down_10() {
  // Stepping operates on quantized values.
  TEST_ASSERT_EQUAL_UINT8(10, brightness_step_up_10(0));
  TEST_ASSERT_EQUAL_UINT8(20, brightness_step_up_10(7));   // 7 -> 10 -> 20
  TEST_ASSERT_EQUAL_UINT8(100, brightness_step_up_10(100));
  TEST_ASSERT_EQUAL_UINT8(100, brightness_step_up_10(255));

  TEST_ASSERT_EQUAL_UINT8(0, brightness_step_down_10(0));
  TEST_ASSERT_EQUAL_UINT8(0, brightness_step_down_10(7));  // 7 -> 10 -> 0
  TEST_ASSERT_EQUAL_UINT8(90, brightness_step_down_10(100));
}

void test_brightness_percent_to_u8_255() {
  TEST_ASSERT_EQUAL_UINT8(0, percent_to_u8_255(0));
  TEST_ASSERT_EQUAL_UINT8(2, percent_to_u8_255(1));     // floor(2.55) = 2
  TEST_ASSERT_EQUAL_UINT8(127, percent_to_u8_255(50));  // floor(127.5) = 127
  TEST_ASSERT_EQUAL_UINT8(252, percent_to_u8_255(99));
  TEST_ASSERT_EQUAL_UINT8(255, percent_to_u8_255(100));
  TEST_ASSERT_EQUAL_UINT8(255, percent_to_u8_255(255));
}

