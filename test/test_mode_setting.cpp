#include <string.h>

#include <unity.h>

#include "core/settings/mode_setting.h"

using chromance::core::IKeyValueStore;
using chromance::core::ModeSetting;

namespace {

class FakeStore final : public IKeyValueStore {
 public:
  bool has_key = false;
  uint8_t stored = 0;
  mutable uint32_t reads = 0;
  mutable uint32_t writes = 0;

  bool read_u8(const char* /*key*/, uint8_t* out) const override {
    ++reads;
    if (!has_key || out == nullptr) return false;
    *out = stored;
    return true;
  }

  bool write_u8(const char* /*key*/, uint8_t value) override {
    ++writes;
    has_key = true;
    stored = value;
    return true;
  }
};

}  // namespace

void test_mode_setting_sanitizes_values() {
  TEST_ASSERT_EQUAL_UINT8(1, ModeSetting::sanitize(0));
  TEST_ASSERT_EQUAL_UINT8(1, ModeSetting::sanitize(1));
  TEST_ASSERT_EQUAL_UINT8(5, ModeSetting::sanitize(5));
  TEST_ASSERT_EQUAL_UINT8(6, ModeSetting::sanitize(6));
  TEST_ASSERT_EQUAL_UINT8(1, ModeSetting::sanitize(7));
  TEST_ASSERT_EQUAL_UINT8(1, ModeSetting::sanitize(255));
}

void test_mode_setting_begin_reads_and_writes_back_sanitized() {
  FakeStore store;
  store.has_key = true;
  store.stored = 9;

  ModeSetting s;
  s.begin(store, "mode", 3);
  TEST_ASSERT_EQUAL_UINT8(1, s.mode());  // 9 -> sanitized to 1
  TEST_ASSERT_EQUAL_UINT8(1, store.stored);
  TEST_ASSERT_EQUAL_UINT32(1, store.reads);
  TEST_ASSERT_EQUAL_UINT32(1, store.writes);
}

void test_mode_setting_begin_uses_default_when_missing() {
  FakeStore store;
  ModeSetting s;
  s.begin(store, "mode", 4);
  TEST_ASSERT_EQUAL_UINT8(4, s.mode());
  TEST_ASSERT_EQUAL_UINT32(1, store.reads);
  TEST_ASSERT_EQUAL_UINT32(1, store.writes);
}

void test_mode_setting_set_mode_persists_sanitized() {
  FakeStore store;
  ModeSetting s;
  s.begin(store, "mode", 1);
  s.set_mode(store, "mode", 5);
  TEST_ASSERT_EQUAL_UINT8(5, s.mode());
  TEST_ASSERT_EQUAL_UINT8(5, store.stored);

  s.set_mode(store, "mode", 77);
  TEST_ASSERT_EQUAL_UINT8(1, s.mode());
  TEST_ASSERT_EQUAL_UINT8(1, store.stored);
}
