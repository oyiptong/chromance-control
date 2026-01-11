#include <unity.h>

#include "core/effects/effect_catalog.h"

using chromance::core::EffectCatalog;
using chromance::core::EffectDescriptor;
using chromance::core::EffectId;
using chromance::core::EffectParams;
using chromance::core::EffectConfigSchema;
using chromance::core::IEffectV2;
using chromance::core::EventContext;
using chromance::core::RenderContext;
using chromance::core::Rgb;

namespace {

class DummyEffect final : public IEffectV2 {
 public:
  explicit DummyEffect(EffectDescriptor d) : d_(d) {}

  const EffectDescriptor& descriptor() const override { return d_; }
  const EffectConfigSchema* schema() const override { return nullptr; }

  void start(const EventContext&) override { ++start_calls; }
  void reset_runtime(const EventContext&) override { ++reset_calls; }
  void render(const RenderContext&, Rgb*, size_t) override { ++render_calls; }

  uint32_t start_calls = 0;
  uint32_t reset_calls = 0;
  uint32_t render_calls = 0;

 private:
  EffectDescriptor d_{};
};

}  // namespace

void test_effect_catalog_v2_add_find_and_capacity() {
  EffectCatalog<2> catalog;

  DummyEffect e1(EffectDescriptor{EffectId{1}, "index_walk", "Index Walk", nullptr});
  DummyEffect e2(EffectDescriptor{EffectId{2}, "breathing", "Breathing", nullptr});
  DummyEffect dup_id(EffectDescriptor{EffectId{1}, "other", "Other", nullptr});
  DummyEffect dup_slug(EffectDescriptor{EffectId{3}, "breathing", "Breathing 2", nullptr});

  TEST_ASSERT_TRUE(catalog.add(e1.descriptor(), &e1));
  TEST_ASSERT_TRUE(catalog.add(e2.descriptor(), &e2));
  TEST_ASSERT_FALSE(catalog.add(dup_id.descriptor(), &dup_id));
  TEST_ASSERT_FALSE(catalog.add(dup_slug.descriptor(), &dup_slug));

  TEST_ASSERT_EQUAL_UINT32(2, catalog.count());
  TEST_ASSERT_EQUAL_PTR(&e1, catalog.find_by_id(EffectId{1}));
  TEST_ASSERT_EQUAL_PTR(&e2, catalog.find_by_slug("breathing"));

  TEST_ASSERT_NOT_NULL(catalog.descriptor_by_slug("index_walk"));
  TEST_ASSERT_EQUAL_UINT16(2, catalog.descriptor_by_slug("breathing")->id.value);
}

