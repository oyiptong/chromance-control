#include <unity.h>

#include "core/layout.h"
#include "core/mapping/mapping_tables.h"
#include "core/strip_layout.h"

using chromance::core::MappingTables;

void test_mapping_tables_dimensions_and_counts() {
  const uint16_t w = MappingTables::width();
  const uint16_t h = MappingTables::height();
  const uint16_t n = MappingTables::led_count();

  TEST_ASSERT_TRUE(w > 0);
  TEST_ASSERT_TRUE(h > 0);
  TEST_ASSERT_TRUE(n > 0);

  const bool bench = MappingTables::is_bench_subset();
  const uint16_t expected = bench
                                ? chromance::core::kStrip0Segments * chromance::core::kLedsPerSegment
                                : chromance::core::kTotalSegments * chromance::core::kLedsPerSegment;
  TEST_ASSERT_EQUAL_UINT16(expected, n);
}

void test_mapping_tables_global_indices_are_consistent() {
  const uint16_t n = MappingTables::led_count();
  const uint8_t* g2s = MappingTables::global_to_strip();
  const uint16_t* g2l = MappingTables::global_to_local();
  const uint8_t* g2seg = MappingTables::global_to_seg();
  const uint8_t* g2k = MappingTables::global_to_seg_k();
  const uint8_t* g2dir = MappingTables::global_to_dir();
  const int16_t* px = MappingTables::pixel_x();
  const int16_t* py = MappingTables::pixel_y();

  for (uint16_t i = 0; i < n; ++i) {
    const uint8_t strip = g2s[i];
    TEST_ASSERT_TRUE(strip < chromance::core::kStripCount);

    const uint16_t local = g2l[i];
    TEST_ASSERT_TRUE(local < chromance::core::strip_led_count(chromance::core::kStripConfigs[strip]));

    const uint8_t seg = g2seg[i];
    TEST_ASSERT_TRUE(seg >= 1);
    TEST_ASSERT_TRUE(seg <= chromance::core::kTotalSegments);

    const uint8_t k = g2k[i];
    TEST_ASSERT_TRUE(k < chromance::core::kLedsPerSegment);

    const uint8_t dir = g2dir[i];
    TEST_ASSERT_TRUE(dir == 0 || dir == 1);

    TEST_ASSERT_TRUE(px[i] >= 0);
    TEST_ASSERT_TRUE(py[i] >= 0);
    TEST_ASSERT_TRUE(px[i] < static_cast<int16_t>(MappingTables::width()));
    TEST_ASSERT_TRUE(py[i] < static_cast<int16_t>(MappingTables::height()));
  }
}

