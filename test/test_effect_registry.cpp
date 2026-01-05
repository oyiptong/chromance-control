#include <unity.h>

#include "core/effects/effect_registry.h"

using chromance::core::EffectFrame;
using chromance::core::EffectRegistry;
using chromance::core::IEffect;
using chromance::core::PixelsMap;
using chromance::core::Rgb;

namespace {

class DummyEffect final : public IEffect {
 public:
  explicit DummyEffect(const char* id) : id_(id) {}
  const char* id() const override { return id_; }
  void reset(uint32_t /*now_ms*/) override {}
  void render(const EffectFrame& /*frame*/,
              const PixelsMap& /*map*/,
              Rgb* /*out_rgb*/,
              size_t /*led_count*/) override {}

 private:
  const char* id_ = nullptr;
};

}  // namespace

void test_effect_registry_add_find_and_capacity() {
  EffectRegistry<2> reg;

  DummyEffect a("A");
  DummyEffect b("B");
  DummyEffect dup_a("A");

  TEST_ASSERT_TRUE(reg.add(&a));
  TEST_ASSERT_TRUE(reg.add(&b));
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(reg.count()));

  // Capacity exceeded.
  TEST_ASSERT_FALSE(reg.add(&dup_a));

  // Duplicate IDs rejected.
  EffectRegistry<3> reg2;
  TEST_ASSERT_TRUE(reg2.add(&a));
  TEST_ASSERT_FALSE(reg2.add(&dup_a));

  TEST_ASSERT_EQUAL_PTR(&a, reg2.find("A"));
  TEST_ASSERT_EQUAL_PTR(&b, reg.find("B"));
  TEST_ASSERT_NULL(reg.find("missing"));
  TEST_ASSERT_NULL(reg.find(nullptr));
}

