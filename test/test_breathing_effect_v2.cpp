#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include "core/effects/pattern_breathing_mode_v2.h"

using chromance::core::BreathingEffect;
using chromance::core::BreathingEffectV2;
using chromance::core::EffectDescriptor;
using chromance::core::EffectId;
using chromance::core::EventContext;
using chromance::core::InputEvent;
using chromance::core::Key;
using chromance::core::PixelsMap;
using chromance::core::StageId;

void test_breathing_effect_v2_stage_and_event_routing() {
  BreathingEffect legacy;
  const EffectDescriptor d{EffectId(7), "breathing", "Breathing", nullptr};
  BreathingEffectV2 v2(d, &legacy);

  alignas(4) uint8_t bytes[chromance::core::kMaxEffectConfigSize] = {};
  BreathingEffect::Config cfg;
  cfg.has_configured_center = true;
  cfg.configured_center_vertex_id = 12;
  cfg.num_dots = 5;
  memcpy(bytes, &cfg, sizeof(cfg));
  v2.bind_config(bytes, sizeof(bytes));

  PixelsMap map;
  EventContext ctx;
  ctx.now_ms = 100;
  ctx.map = &map;

  v2.start(ctx);
  TEST_ASSERT_FALSE(legacy.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BreathingEffect::Phase::Inhale),
                          static_cast<uint8_t>(legacy.phase()));

  TEST_ASSERT_EQUAL_UINT8(4, v2.stage_count());
  TEST_ASSERT_NOT_NULL(v2.stage_at(0));
  TEST_ASSERT_NOT_NULL(v2.stage_at(3));
  TEST_ASSERT_NULL(v2.stage_at(4));

  InputEvent ev;
  ev.now_ms = 110;
  ev.key = Key::N;
  v2.on_event(ev, ctx);
  TEST_ASSERT_TRUE(legacy.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BreathingEffect::Phase::Pause1),
                          static_cast<uint8_t>(legacy.phase()));
  TEST_ASSERT_EQUAL_UINT8(1, v2.current_stage().value);

  ev.key = Key::ShiftN;
  v2.on_event(ev, ctx);
  TEST_ASSERT_TRUE(legacy.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BreathingEffect::Phase::Inhale),
                          static_cast<uint8_t>(legacy.phase()));

  TEST_ASSERT_TRUE(v2.enter_stage(StageId(2), ctx));
  TEST_ASSERT_TRUE(legacy.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BreathingEffect::Phase::Exhale),
                          static_cast<uint8_t>(legacy.phase()));

  ev.key = Key::Esc;
  v2.on_event(ev, ctx);
  TEST_ASSERT_FALSE(legacy.manual_enabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BreathingEffect::Phase::Inhale),
                          static_cast<uint8_t>(legacy.phase()));
}

