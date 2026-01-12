#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <Preferences.h>
#include <stdint.h>

#include "core/effects/effect_catalog.h"
#include "core/effects/effect_manager.h"
#include "core/effects/effect_params.h"
#include "platform/settings.h"

namespace chromance {
namespace platform {

class WebuiServer {
 public:
  WebuiServer(const char* firmware_version,
              chromance::platform::RuntimeSettings* runtime_settings,
              chromance::core::EffectParams* global_params,
              chromance::core::EffectManager<32>* manager,
              const chromance::core::EffectCatalog<32>* catalog);

  void begin();

  // Called from the main loop when the render-loop gate allows web work.
  void handle(uint32_t now_ms, uint32_t next_render_deadline_ms);

  // When true, the main loop MUST reboot the device after the current iteration.
  bool take_pending_restart();

 private:
  void dispatch();

  // Route helpers
  bool handle_page_routes();
  bool handle_static_asset_routes();
  bool handle_api_routes();

  // Static assets
  bool send_embedded_asset(const char* request_path, const char* content_type_override);

  // API endpoints
  void api_get_effects();
  void api_get_effect_detail(const String& slug);
  void api_post_activate(const String& slug);
  void api_post_restart(const String& slug);
  void api_post_stage(const String& slug);
  void api_post_params(const String& slug);

  void api_get_settings();
  void api_post_brightness();
  void api_post_reset();

  void api_get_persistence_summary();
  void api_get_persistence_effect(const String& slug);
  void api_delete_persistence_all();

  // Utilities
  void validate_aliases_and_log();
  bool alias_is_collided(const char* slug) const;
  bool parse_effect_slug(const String& slug, chromance::core::EffectId* out_id, String* out_canonical_slug,
                         bool* out_is_alias);
  String canonical_slug_for_id(chromance::core::EffectId id) const;

  // Rate limiting (global)
  bool rate_limit_allow_brightness(uint32_t now_ms);
  bool rate_limit_allow_params(uint32_t now_ms);

  // Confirmation token (per boot)
  void init_confirm_token();
  bool check_confirm_token_and_phrase(const String& body, const char* expected_phrase);

  // Error responses
  void send_json_error(int http_status, const char* code, const char* message);
  void send_json_ok_bounded(const String& json_body);

  // Gate
  bool render_gate_allows(uint32_t now_ms, uint32_t next_render_deadline_ms) const;

 private:
  WebServer server_{80};

  const char* firmware_version_ = nullptr;
  chromance::platform::RuntimeSettings* runtime_settings_ = nullptr;
  chromance::core::EffectParams* global_params_ = nullptr;

  chromance::core::EffectManager<32>* manager_ = nullptr;
  const chromance::core::EffectCatalog<32>* catalog_ = nullptr;

  Preferences prefs_;

  char confirm_token_[17] = {0};  // 16 hex chars + NUL

  bool pending_restart_ = false;

  static constexpr size_t kMaxCollidedAliases = 8;
  const char* collided_aliases_[kMaxCollidedAliases] = {};
  uint8_t collided_alias_count_ = 0;

  // Global write rate limits
  uint32_t bright_window_start_ms_ = 0;
  uint8_t bright_count_in_window_ = 0;
  uint32_t params_window_start_ms_ = 0;
  uint8_t params_count_in_window_ = 0;
};

}  // namespace platform
}  // namespace chromance

