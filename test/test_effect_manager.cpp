#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include "core/effects/effect_manager.h"

using chromance::core::EffectCatalog;
using chromance::core::EffectConfigSchema;
using chromance::core::EffectDescriptor;
using chromance::core::EffectId;
using chromance::core::EffectManager;
using chromance::core::EffectParams;
using chromance::core::EventContext;
using chromance::core::IEffectV2;
using chromance::core::ISettingsStore;
using chromance::core::InputEvent;
using chromance::core::Key;
using chromance::core::ParamDescriptor;
using chromance::core::ParamId;
using chromance::core::ParamType;
using chromance::core::ParamValue;
using chromance::core::PixelsMap;
using chromance::core::RenderContext;
using chromance::core::Rgb;
using chromance::core::Signals;

namespace {

struct DummyConfig {
  uint8_t dot_count;
  uint8_t center_vertex;
  uint16_t speed_01;
  int16_t offset;
  uint8_t enabled;
  uint8_t mode;
  Rgb color;
};

static_assert(sizeof(DummyConfig) <= chromance::core::kMaxEffectConfigSize, "DummyConfig too large");

enum : uint16_t {
  kPidDotCount = 1,
  kPidCenterVertex = 2,
  kPidSpeed01 = 3,
  kPidOffset = 4,
  kPidEnabled = 5,
  kPidMode = 6,
  kPidColor = 7,
};

class DummyEffect final : public IEffectV2 {
 public:
  DummyEffect(EffectDescriptor d, const ParamDescriptor* params, uint8_t param_count)
      : d_(d), schema_{params, param_count} {}

  const EffectDescriptor& descriptor() const override { return d_; }
  const EffectConfigSchema* schema() const override { return &schema_; }

  void bind_config(const void* config_bytes, size_t config_size) override {
    bind_calls++;
    config_size_ = config_size;
    config_ = static_cast<const DummyConfig*>(config_bytes);
  }

  void start(const EventContext& ctx) override {
    start_calls++;
    last_start_ms = ctx.now_ms;
  }

  void stop(const EventContext& ctx) override {
    stop_calls++;
    last_stop_ms = ctx.now_ms;
  }

  void reset_runtime(const EventContext& ctx) override {
    reset_calls++;
    last_reset_ms = ctx.now_ms;
  }

  void on_event(const InputEvent& ev, const EventContext& ctx) override {
    event_calls++;
    last_event_key = ev.key;
    last_event_ms = ctx.now_ms;
  }

  void render(const RenderContext& ctx, Rgb* out, size_t n) override {
    render_calls++;
    last_render_ms = ctx.now_ms;
    last_render_dt_ms = ctx.dt_ms;
    last_render_map = ctx.map;
    last_render_brightness = ctx.global_params.brightness;
    last_render_has_bpm = ctx.signals.has_bpm;

    const uint8_t c = (config_ != nullptr) ? config_->dot_count : 0;
    for (size_t i = 0; i < n; ++i) {
      out[i] = Rgb{c, 0, 0};
    }
  }

  const DummyConfig* config() const { return config_; }
  size_t config_size() const { return config_size_; }

  uint32_t bind_calls = 0;
  uint32_t start_calls = 0;
  uint32_t stop_calls = 0;
  uint32_t reset_calls = 0;
  uint32_t event_calls = 0;
  uint32_t render_calls = 0;

  uint32_t last_start_ms = 0;
  uint32_t last_stop_ms = 0;
  uint32_t last_reset_ms = 0;
  Key last_event_key = Key::Esc;
  uint32_t last_event_ms = 0;
  uint32_t last_render_ms = 0;
  uint32_t last_render_dt_ms = 0;
  const PixelsMap* last_render_map = nullptr;
  uint8_t last_render_brightness = 0;
  bool last_render_has_bpm = false;

 private:
  EffectDescriptor d_{};
  EffectConfigSchema schema_{};
  const DummyConfig* config_ = nullptr;
  size_t config_size_ = 0;
};

class FakeSettingsStore final : public ISettingsStore {
 public:
  struct Entry {
    char key[16] = {};
    uint8_t bytes[chromance::core::kMaxEffectConfigSize] = {};
    size_t size = 0;
    bool used = false;
  };

  bool fail_reads = false;
  bool fail_writes = false;

  mutable uint32_t reads = 0;
  mutable uint32_t writes = 0;

  bool read_blob(const char* key, void* out, size_t out_size) const override {
    ++reads;
    if (fail_reads || key == nullptr || out == nullptr || out_size == 0) {
      return false;
    }
    for (size_t i = 0; i < kMaxEntries; ++i) {
      if (!entries[i].used) continue;
      if (strcmp(entries[i].key, key) != 0) continue;
      if (entries[i].size != out_size) return false;
      memcpy(out, entries[i].bytes, out_size);
      return true;
    }
    return false;
  }

  bool write_blob(const char* key, const void* data, size_t size) override {
    ++writes;
    if (fail_writes || key == nullptr || data == nullptr || size == 0) {
      return false;
    }
    Entry* slot = nullptr;
    for (size_t i = 0; i < kMaxEntries; ++i) {
      if (entries[i].used && strcmp(entries[i].key, key) == 0) {
        slot = &entries[i];
        break;
      }
    }
    if (slot == nullptr) {
      for (size_t i = 0; i < kMaxEntries; ++i) {
        if (!entries[i].used) {
          slot = &entries[i];
          slot->used = true;
          strncpy(slot->key, key, sizeof(slot->key) - 1);
          break;
        }
      }
    }
    if (slot == nullptr) {
      return false;
    }
    if (size > sizeof(slot->bytes)) {
      return false;
    }
    slot->size = size;
    memcpy(slot->bytes, data, size);
    return true;
  }

  bool has_key(const char* key) const {
    for (size_t i = 0; i < kMaxEntries; ++i) {
      if (entries[i].used && strcmp(entries[i].key, key) == 0) {
        return true;
      }
    }
    return false;
  }

  size_t entry_size(const char* key) const {
    for (size_t i = 0; i < kMaxEntries; ++i) {
      if (entries[i].used && strcmp(entries[i].key, key) == 0) {
        return entries[i].size;
      }
    }
    return 0;
  }

 private:
  static constexpr size_t kMaxEntries = 8;
  mutable Entry entries[kMaxEntries] = {};
};

}  // namespace

void test_effect_manager_v2_init_persists_active_id_and_binds_configs() {
  FakeSettingsStore store;
  PixelsMap map;

  static const ParamDescriptor kParams[] = {
      {ParamId{kPidDotCount}, "dot_count", "Dot Count", ParamType::U8,
       static_cast<uint16_t>(offsetof(DummyConfig, dot_count)), 1, 0, 20, 1, 9, 1},
      {ParamId{kPidCenterVertex}, "center_vertex", "Center Vertex", ParamType::U8,
       static_cast<uint16_t>(offsetof(DummyConfig, center_vertex)), 1, 0, 255, 1, 12, 1},
      {ParamId{kPidSpeed01}, "speed_01", "Speed", ParamType::U16,
       static_cast<uint16_t>(offsetof(DummyConfig, speed_01)), 2, 0, 1000, 1, 100, 1},
      {ParamId{kPidOffset}, "offset", "Offset", ParamType::I16,
       static_cast<uint16_t>(offsetof(DummyConfig, offset)), 2, -100, 100, 1, 0, 1},
      {ParamId{kPidEnabled}, "enabled", "Enabled", ParamType::Bool,
       static_cast<uint16_t>(offsetof(DummyConfig, enabled)), 1, 0, 1, 1, 1, 1},
      {ParamId{kPidMode}, "mode", "Mode", ParamType::Enum,
       static_cast<uint16_t>(offsetof(DummyConfig, mode)), 1, 0, 3, 1, 0, 1},
      {ParamId{kPidColor}, "color", "Color", ParamType::ColorRgb,
       static_cast<uint16_t>(offsetof(DummyConfig, color)), 3, 0, 0xFFFFFF, 1, 0x112233, 1},
  };

  DummyEffect e1(EffectDescriptor{EffectId{1}, "e1", "E1", nullptr}, kParams,
                 static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0])));
  DummyEffect e2(EffectDescriptor{EffectId{2}, "e2", "E2", nullptr}, kParams,
                 static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0])));

  EffectCatalog<4> catalog;
  TEST_ASSERT_TRUE(catalog.add(e1.descriptor(), &e1));
  TEST_ASSERT_TRUE(catalog.add(e2.descriptor(), &e2));

  EffectManager<4> mgr;
  mgr.init(store, catalog, map, 1000);

  TEST_ASSERT_EQUAL_UINT32(2, e1.bind_calls + e2.bind_calls);
  TEST_ASSERT_NOT_NULL(e1.config());
  TEST_ASSERT_EQUAL_UINT32(chromance::core::kMaxEffectConfigSize, e1.config_size());

  // Active effect id is persisted immediately.
  TEST_ASSERT_TRUE(store.has_key("aeid"));
  TEST_ASSERT_EQUAL_UINT32(2, store.entry_size("aeid"));
  TEST_ASSERT_EQUAL_UINT16(1, mgr.active_id().value);
  TEST_ASSERT_EQUAL_UINT32(1, e1.start_calls);
  TEST_ASSERT_EQUAL_UINT32(0, e2.start_calls);

  // Defaults applied from schema.
  TEST_ASSERT_EQUAL_UINT8(9, e1.config()->dot_count);
  TEST_ASSERT_EQUAL_UINT8(12, e1.config()->center_vertex);
  TEST_ASSERT_EQUAL_UINT16(100, e1.config()->speed_01);
  TEST_ASSERT_EQUAL_INT16(0, e1.config()->offset);
  TEST_ASSERT_EQUAL_UINT8(1, e1.config()->enabled);
  TEST_ASSERT_EQUAL_UINT8(0, e1.config()->mode);
  TEST_ASSERT_EQUAL_UINT8(0x11, e1.config()->color.r);
  TEST_ASSERT_EQUAL_UINT8(0x22, e1.config()->color.g);
  TEST_ASSERT_EQUAL_UINT8(0x33, e1.config()->color.b);
}

void test_effect_manager_v2_set_get_param_and_persistence_debounce() {
  FakeSettingsStore store;
  PixelsMap map;

  static const ParamDescriptor kParams[] = {
      {ParamId{kPidDotCount}, "dot_count", "Dot Count", ParamType::U8,
       static_cast<uint16_t>(offsetof(DummyConfig, dot_count)), 1, 0, 20, 1, 9, 1},
  };

  DummyEffect e1(EffectDescriptor{EffectId{1}, "e1", "E1", nullptr}, kParams,
                 static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0])));
  EffectCatalog<2> catalog;
  TEST_ASSERT_TRUE(catalog.add(e1.descriptor(), &e1));

  EffectManager<2> mgr;
  mgr.init(store, catalog, map, 0);

  ParamValue v;
  v.type = ParamType::U8;
  v.v.u8 = 12;
  TEST_ASSERT_TRUE(mgr.set_param(EffectId{1}, ParamId{kPidDotCount}, v));
  TEST_ASSERT_EQUAL_UINT8(12, e1.config()->dot_count);

  ParamValue got;
  TEST_ASSERT_TRUE(mgr.get_param(EffectId{1}, ParamId{kPidDotCount}, &got));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ParamType::U8), static_cast<uint8_t>(got.type));
  TEST_ASSERT_EQUAL_UINT8(12, got.v.u8);

  // Debounced persistence: no config blob key until debounce interval has elapsed.
  TEST_ASSERT_FALSE(store.has_key("e0001"));
  mgr.tick(499, 1, Signals{});
  TEST_ASSERT_FALSE(store.has_key("e0001"));
  mgr.tick(500, 1, Signals{});
  TEST_ASSERT_TRUE(store.has_key("e0001"));
  TEST_ASSERT_EQUAL_UINT32(chromance::core::kMaxEffectConfigSize, store.entry_size("e0001"));
}

void test_effect_manager_v2_set_active_calls_stop_start_and_events_render_flow() {
  FakeSettingsStore store;
  PixelsMap map;

  static const ParamDescriptor kParams[] = {
      {ParamId{kPidDotCount}, "dot_count", "Dot Count", ParamType::U8,
       static_cast<uint16_t>(offsetof(DummyConfig, dot_count)), 1, 0, 20, 1, 9, 1},
  };

  DummyEffect e1(EffectDescriptor{EffectId{1}, "e1", "E1", nullptr}, kParams,
                 static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0])));
  DummyEffect e2(EffectDescriptor{EffectId{2}, "e2", "E2", nullptr}, kParams,
                 static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0])));

  EffectCatalog<4> catalog;
  TEST_ASSERT_TRUE(catalog.add(e1.descriptor(), &e1));
  TEST_ASSERT_TRUE(catalog.add(e2.descriptor(), &e2));

  EffectManager<4> mgr;
  mgr.init(store, catalog, map, 100);

  TEST_ASSERT_TRUE(mgr.set_active(EffectId{2}, 200));
  TEST_ASSERT_EQUAL_UINT32(1, e1.stop_calls);
  TEST_ASSERT_EQUAL_UINT32(1, e2.start_calls);

  const InputEvent ev{Key::N, 250};
  mgr.on_event(ev, 250);
  TEST_ASSERT_EQUAL_UINT32(1, e2.event_calls);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint8_t>(Key::N), static_cast<uint8_t>(e2.last_event_key));

  EffectParams gp;
  gp.brightness = 7;
  mgr.set_global_params(gp);
  Signals sig;
  sig.has_bpm = true;
  mgr.tick(300, 16, sig);

  Rgb out[4] = {};
  mgr.render(out, 4);
  TEST_ASSERT_EQUAL_UINT32(1, e2.render_calls);
  TEST_ASSERT_EQUAL_UINT32(300, e2.last_render_ms);
  TEST_ASSERT_EQUAL_UINT32(16, e2.last_render_dt_ms);
  TEST_ASSERT_EQUAL_UINT8(7, e2.last_render_brightness);
  TEST_ASSERT_TRUE(e2.last_render_has_bpm);
}
