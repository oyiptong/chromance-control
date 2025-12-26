#include <unity.h>

void test_layout_constants();

void test_segment_start_led_normal_orientation();
void test_segment_start_led_reversed_orientation();
void test_segments_non_overlapping_and_in_bounds();

void test_strip_sm_segment_order_flash_counts();
void test_strip_sm_done_after_last_segment();
void test_pattern_phase_sequence_and_restart();

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_layout_constants);

  RUN_TEST(test_segment_start_led_normal_orientation);
  RUN_TEST(test_segment_start_led_reversed_orientation);
  RUN_TEST(test_segments_non_overlapping_and_in_bounds);

  RUN_TEST(test_strip_sm_segment_order_flash_counts);
  RUN_TEST(test_strip_sm_done_after_last_segment);
  RUN_TEST(test_pattern_phase_sequence_and_restart);

  return UNITY_END();
}
