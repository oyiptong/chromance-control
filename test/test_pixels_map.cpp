#include <vector>

#include <unity.h>

#include "core/mapping/mapping_tables.h"
#include "core/mapping/pixels_map.h"

using chromance::core::MappingTables;
using chromance::core::PixelsMap;

void test_pixels_map_coords_in_bounds() {
  PixelsMap map;
  const uint16_t w = map.width();
  const uint16_t h = map.height();
  const size_t n = map.led_count();

  TEST_ASSERT_TRUE(w > 0);
  TEST_ASSERT_TRUE(h > 0);
  TEST_ASSERT_TRUE(n > 0);

  for (uint16_t i = 0; i < n; ++i) {
    const auto c = map.coord(i);
    TEST_ASSERT_TRUE(c.x >= 0);
    TEST_ASSERT_TRUE(c.y >= 0);
    TEST_ASSERT_TRUE(c.x < static_cast<int16_t>(w));
    TEST_ASSERT_TRUE(c.y < static_cast<int16_t>(h));
  }
}

void test_pixels_map_center_in_bounds() {
  PixelsMap map;
  const auto c = map.center();
  TEST_ASSERT_TRUE(c.x >= 0);
  TEST_ASSERT_TRUE(c.y >= 0);
  TEST_ASSERT_TRUE(c.x < static_cast<int16_t>(map.width()));
  TEST_ASSERT_TRUE(c.y < static_cast<int16_t>(map.height()));
}

void test_pixels_map_scan_order_is_sorted_and_permutation() {
  PixelsMap map;
  const size_t n = map.led_count();
  std::vector<uint16_t> order(n);
  map.build_scan_order(order.data(), order.size());

  std::vector<bool> seen(n, false);
  for (size_t i = 0; i < n; ++i) {
    const uint16_t idx = order[i];
    TEST_ASSERT_TRUE(idx < n);
    TEST_ASSERT_FALSE(seen[idx]);
    seen[idx] = true;
  }

  const int16_t* px = MappingTables::pixel_x();
  const int16_t* py = MappingTables::pixel_y();
  for (size_t i = 1; i < n; ++i) {
    const uint16_t a = order[i - 1];
    const uint16_t b = order[i];
    if (py[a] != py[b]) {
      TEST_ASSERT_TRUE(py[a] < py[b]);
    } else if (px[a] != px[b]) {
      TEST_ASSERT_TRUE(px[a] < px[b]);
    } else {
      TEST_ASSERT_TRUE(a < b);
    }
  }
}

