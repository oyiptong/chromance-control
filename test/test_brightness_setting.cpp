#include <string.h>

#include <unity.h>

#include "core/settings/brightness_setting.h"

using chromance::core::BrightnessSetting;
using chromance::core::IKeyValueStore;

namespace {

class FakeStore final : public IKeyValueStore {
 public:
  bool has_key = false;
  uint8_t stored = 0;

  mutable uint32_t reads = 0;
  mutable uint32_t writes = 0;
  mutable char last_key[32] = {};
  mutable uint8_t last_value = 0;

  bool read_u8(const char* key, uint8_t* out) const override {
    ++reads;
    if (key == nullptr || out == nullptr || !has_key) {
      return false;
    }
    strncpy(last_key, key, sizeof(last_key) - 1);
    *out = stored;
    return true;
  }

  bool write_u8(const char* key, uint8_t value) override {
    ++writes;
    if (key == nullptr) {
      return false;
    }
    strncpy(last_key, key, sizeof(last_key) - 1);
    last_value = value;
    has_key = true;
    stored = value;
    return true;
  }
};

}  // namespace

void test_brightness_setting_begin_default_and_persists_quantized() {
  FakeStore store;
  BrightnessSetting s;

  s.begin(store, "bright_pct", 100);
  TEST_ASSERT_EQUAL_UINT8(100, s.percent());
  TEST_ASSERT_EQUAL_UINT32(1, store.reads);
  TEST_ASSERT_EQUAL_UINT32(1, store.writes);
  TEST_ASSERT_EQUAL_STRING("bright_pct", store.last_key);
  TEST_ASSERT_EQUAL_UINT8(100, store.last_value);
}

void test_brightness_setting_begin_reads_existing_and_writes_back_quantized() {
  FakeStore store;
  store.has_key = true;
  store.stored = 73;  // should round to 70

  BrightnessSetting s;
  s.begin(store, "bright_pct", 100);
  TEST_ASSERT_EQUAL_UINT8(70, s.percent());
  TEST_ASSERT_EQUAL_UINT32(1, store.reads);
  TEST_ASSERT_EQUAL_UINT32(1, store.writes);
  TEST_ASSERT_EQUAL_UINT8(70, store.stored);
}

void test_brightness_setting_set_persists_quantized_and_clamped() {
  FakeStore store;
  BrightnessSetting s;
  s.begin(store, "bright_pct", 100);

  s.set_percent(store, "bright_pct", 99);
  TEST_ASSERT_EQUAL_UINT8(100, s.percent());
  TEST_ASSERT_EQUAL_UINT8(100, store.stored);

  s.set_percent(store, "bright_pct", 0);
  TEST_ASSERT_EQUAL_UINT8(0, s.percent());
  TEST_ASSERT_EQUAL_UINT8(0, store.stored);

  s.set_percent(store, "bright_pct", 250);
  TEST_ASSERT_EQUAL_UINT8(100, s.percent());
  TEST_ASSERT_EQUAL_UINT8(100, store.stored);
}

void test_brightness_setting_null_key_does_not_touch_store() {
  FakeStore store;
  BrightnessSetting s;

  s.begin(store, nullptr, 55);
  TEST_ASSERT_EQUAL_UINT8(60, s.percent());
  TEST_ASSERT_EQUAL_UINT32(0, store.reads);
  TEST_ASSERT_EQUAL_UINT32(0, store.writes);
}

