#include <stddef.h>
#include <stdint.h>

#include <unity.h>

#include "core/effects/legacy_effect_adapter.h"

using chromance::core::EffectDescriptor;
using chromance::core::EffectFrame;
using chromance::core::EffectId;
using chromance::core::EventContext;
using chromance::core::IEffect;
using chromance::core::LegacyEffectAdapter;
using chromance::core::PixelsMap;
using chromance::core::RenderContext;
using chromance::core::Rgb;
using chromance::core::Signals;

namespace {

class DummyLegacyEffect final : public IEffect {
 public:
  const char* id() const override { return "DummyLegacy"; }

  void reset(uint32_t now_ms) override {
    reset_calls++;
    last_reset_ms = now_ms;
  }

  void render(const EffectFrame& frame, const PixelsMap& map, Rgb* out_rgb, size_t led_count) override {
    (void)map;
    render_calls++;
    last_frame = frame;
    if (out_rgb != nullptr && led_count > 0) {
      for (size_t i = 0; i < led_count; ++i) {
        out_rgb[i] = Rgb{frame.params.brightness, 0, 0};
      }
    }
  }

  uint32_t reset_calls = 0;
  uint32_t render_calls = 0;
  uint32_t last_reset_ms = 0;
  EffectFrame last_frame{};
};

}  // namespace

void test_legacy_effect_adapter_calls_reset_and_passes_frame() {
  DummyLegacyEffect legacy;
  const EffectDescriptor d{EffectId{1}, "dummy", "DummyLegacy", nullptr};
  LegacyEffectAdapter a(d, &legacy);

  PixelsMap map;
  EventContext ev;
  ev.now_ms = 123;
  ev.map = &map;
  a.start(ev);
  TEST_ASSERT_EQUAL_UINT32(1, legacy.reset_calls);
  TEST_ASSERT_EQUAL_UINT32(123, legacy.last_reset_ms);

  ev.now_ms = 456;
  a.reset_runtime(ev);
  TEST_ASSERT_EQUAL_UINT32(2, legacy.reset_calls);
  TEST_ASSERT_EQUAL_UINT32(456, legacy.last_reset_ms);

  RenderContext rc;
  rc.now_ms = 1000;
  rc.dt_ms = 16;
  rc.map = &map;
  rc.global_params.brightness = 7;
  rc.signals.has_bpm = true;
  rc.signals.bpm = 120.0f;

  Rgb out[4] = {};
  a.render(rc, out, 4);
  TEST_ASSERT_EQUAL_UINT32(1, legacy.render_calls);
  TEST_ASSERT_EQUAL_UINT32(1000, legacy.last_frame.now_ms);
  TEST_ASSERT_EQUAL_UINT32(16, legacy.last_frame.dt_ms);
  TEST_ASSERT_EQUAL_UINT8(7, legacy.last_frame.params.brightness);
  TEST_ASSERT_TRUE(legacy.last_frame.signals.has_bpm);

  TEST_ASSERT_EQUAL_UINT8(7, out[0].r);
  TEST_ASSERT_EQUAL_UINT8(7, out[3].r);
}

void test_legacy_effect_adapter_null_map_blanks_and_does_not_call_render() {
  DummyLegacyEffect legacy;
  const EffectDescriptor d{EffectId{1}, "dummy", "DummyLegacy", nullptr};
  LegacyEffectAdapter a(d, &legacy);

  RenderContext rc;
  rc.now_ms = 1;
  rc.dt_ms = 1;
  rc.map = nullptr;
  rc.global_params.brightness = 99;

  Rgb out[2] = {Rgb{1, 2, 3}, Rgb{4, 5, 6}};
  a.render(rc, out, 2);

  TEST_ASSERT_EQUAL_UINT32(0, legacy.render_calls);
  TEST_ASSERT_EQUAL_UINT8(0, out[0].r);
  TEST_ASSERT_EQUAL_UINT8(0, out[0].g);
  TEST_ASSERT_EQUAL_UINT8(0, out[0].b);
  TEST_ASSERT_EQUAL_UINT8(0, out[1].r);
  TEST_ASSERT_EQUAL_UINT8(0, out[1].g);
  TEST_ASSERT_EQUAL_UINT8(0, out[1].b);
}

