#pragma once

#include <stdint.h>

#if defined(CHROMANCE_BENCH_MODE) && CHROMANCE_BENCH_MODE
#include "generated/chromance_mapping_bench.h"
#else
#include "generated/chromance_mapping_full.h"
#endif

namespace chromance {
namespace core {

struct MappingTables {
  static constexpr const char* mapping_version() { return mapping::MAPPING_VERSION; }
  static constexpr bool is_bench_subset() { return mapping::IS_BENCH_SUBSET; }

  static constexpr uint16_t led_count() { return mapping::LED_COUNT; }
  static constexpr uint16_t width() { return mapping::WIDTH; }
  static constexpr uint16_t height() { return mapping::HEIGHT; }

  static constexpr const int16_t* pixel_x() { return mapping::pixel_x; }
  static constexpr const int16_t* pixel_y() { return mapping::pixel_y; }
  static constexpr const uint8_t* global_to_strip() { return mapping::global_to_strip; }
  static constexpr const uint16_t* global_to_local() { return mapping::global_to_local; }
  static constexpr const uint8_t* global_to_seg() { return mapping::global_to_seg; }
  static constexpr const uint8_t* global_to_seg_k() { return mapping::global_to_seg_k; }
  static constexpr const uint8_t* global_to_dir() { return mapping::global_to_dir; }  // 0=a_to_b, 1=b_to_a
};

}  // namespace core
}  // namespace chromance
