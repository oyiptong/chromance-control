#include <unity.h>

#include "core/layout.h"

using chromance::core::kDiagnosticBrightness;
using chromance::core::kLedsPerSegment;
using chromance::core::kStripConfigs;
using chromance::core::kStripCount;
using chromance::core::kTotalSegments;
using chromance::core::kStrip0Segments;
using chromance::core::kStrip1Segments;
using chromance::core::kStrip2Segments;
using chromance::core::kStrip3Segments;

void test_layout_constants() {
  TEST_ASSERT_EQUAL_UINT8(4, kStripCount);
  TEST_ASSERT_EQUAL_UINT16(40, kTotalSegments);
  TEST_ASSERT_EQUAL_UINT8(14, kLedsPerSegment);
  TEST_ASSERT_EQUAL_UINT8(64, kDiagnosticBrightness);

  TEST_ASSERT_EQUAL_UINT8(11, kStrip0Segments);
  TEST_ASSERT_EQUAL_UINT8(12, kStrip1Segments);
  TEST_ASSERT_EQUAL_UINT8(6, kStrip2Segments);
  TEST_ASSERT_EQUAL_UINT8(11, kStrip3Segments);

  TEST_ASSERT_EQUAL_UINT8(kStrip0Segments, kStripConfigs[0].segment_count);
  TEST_ASSERT_EQUAL_UINT8(kStrip1Segments, kStripConfigs[1].segment_count);
  TEST_ASSERT_EQUAL_UINT8(kStrip2Segments, kStripConfigs[2].segment_count);
  TEST_ASSERT_EQUAL_UINT8(kStrip3Segments, kStripConfigs[3].segment_count);

  TEST_ASSERT_EQUAL_UINT16(40, kStrip0Segments + kStrip1Segments + kStrip2Segments + kStrip3Segments);
}
