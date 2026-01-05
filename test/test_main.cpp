#include <unity.h>

void test_layout_constants();

void test_segment_start_led_normal_orientation();
void test_segment_start_led_reversed_orientation();
void test_segments_non_overlapping_and_in_bounds();

void test_strip_sm_segment_order_flash_counts();
void test_strip_sm_done_after_last_segment();
void test_pattern_phase_sequence_and_restart();

void test_frame_scheduler_uncapped();
void test_frame_scheduler_50fps_fixed_interval();
void test_frame_scheduler_60fps_deterministic_rounding();

void test_effect_registry_add_find_and_capacity();

void test_brightness_clamp_percent();
void test_brightness_quantize_to_10_rounding();
void test_brightness_step_up_down_10();
void test_brightness_percent_to_u8_255();

void test_brightness_setting_begin_default_and_persists_quantized();
void test_brightness_setting_begin_reads_existing_and_writes_back_quantized();
void test_brightness_setting_set_persists_quantized_and_clamped();
void test_brightness_setting_null_key_does_not_touch_store();

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

  RUN_TEST(test_frame_scheduler_uncapped);
  RUN_TEST(test_frame_scheduler_50fps_fixed_interval);
  RUN_TEST(test_frame_scheduler_60fps_deterministic_rounding);

  RUN_TEST(test_effect_registry_add_find_and_capacity);

  RUN_TEST(test_brightness_clamp_percent);
  RUN_TEST(test_brightness_quantize_to_10_rounding);
  RUN_TEST(test_brightness_step_up_down_10);
  RUN_TEST(test_brightness_percent_to_u8_255);

  RUN_TEST(test_brightness_setting_begin_default_and_persists_quantized);
  RUN_TEST(test_brightness_setting_begin_reads_existing_and_writes_back_quantized);
  RUN_TEST(test_brightness_setting_set_persists_quantized_and_clamped);
  RUN_TEST(test_brightness_setting_null_key_does_not_touch_store);

  return UNITY_END();
}
