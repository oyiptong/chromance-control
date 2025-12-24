#include <unity.h>

void test_layout_constants();

void test_segment_start_led_normal_orientation();
void test_segment_start_led_reversed_orientation();
void test_segments_non_overlapping_and_in_bounds();

void test_strip_sm_flash_count_and_latch();
void test_strip_sm_restart_clears_previous_segments();
void test_pattern_render_sends_segment_states();

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_layout_constants);

  RUN_TEST(test_segment_start_led_normal_orientation);
  RUN_TEST(test_segment_start_led_reversed_orientation);
  RUN_TEST(test_segments_non_overlapping_and_in_bounds);

  RUN_TEST(test_strip_sm_flash_count_and_latch);
  RUN_TEST(test_strip_sm_restart_clears_previous_segments);
  RUN_TEST(test_pattern_render_sends_segment_states);

  return UNITY_END();
}

