#include "webui_server.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <math.h>

#include "core/brightness.h"
#include "core/brightness_config.h"
#include "core/mapping/mapping_tables.h"
#include "generated/webui_assets.h"

namespace chromance {
namespace platform {

namespace {

static constexpr uint32_t kRenderGateHeadroomMs = 7;
static constexpr uint32_t kStaticSendCpuBudgetMs = 5;

static constexpr size_t kMaxHttpBodyBytes = 1024;
static constexpr size_t kMaxJsonBytes = 8192;

static constexpr uint32_t kRateWindowMs = 1000;
static constexpr uint8_t kMaxBrightnessPerSec = 4;
static constexpr uint8_t kMaxParamsPerSec = 8;

static size_t json_len_escaped(const char* s) {
  if (s == nullptr) return 0;
  size_t n = 0;
  for (const char* p = s; *p; ++p) {
    switch (*p) {
      case '\"':
      case '\\':
      case '\n':
      case '\r':
      case '\t':
        n += 2;
        break;
      default:
        n += 1;
        break;
    }
  }
  return n;
}

static void json_write_escaped(WiFiClient& client, const char* s) {
  if (s == nullptr) return;
  for (const char* p = s; *p; ++p) {
    switch (*p) {
      case '\"':
        client.print("\\\"");
        break;
      case '\\':
        client.print("\\\\");
        break;
      case '\n':
        client.print("\\n");
        break;
      case '\r':
        client.print("\\r");
        break;
      case '\t':
        client.print("\\t");
        break;
      default:
        client.write(static_cast<uint8_t>(*p));
        break;
    }
  }
}

static bool json_get_u32(const JsonDocument& doc, const char* key, uint32_t* out) {
  if (out == nullptr) return false;
  if (!doc[key].is<uint32_t>()) return false;
  *out = doc[key].as<uint32_t>();
  return true;
}

static bool json_get_string(const JsonDocument& doc, const char* key, String* out) {
  if (out == nullptr) return false;
  if (!doc[key].is<const char*>()) return false;
  *out = String(doc[key].as<const char*>());
  return out->length() > 0;
}

static uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t b = 0; b < 8; ++b) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

static void bytes_to_hex_lower(const uint8_t* bytes, size_t len, char* out_hex, size_t out_size) {
  static const char* kHex = "0123456789abcdef";
  if (out_hex == nullptr || out_size < (len * 2 + 1)) return;
  for (size_t i = 0; i < len; ++i) {
    out_hex[i * 2 + 0] = kHex[(bytes[i] >> 4) & 0xF];
    out_hex[i * 2 + 1] = kHex[(bytes[i] >> 0) & 0xF];
  }
  out_hex[len * 2] = '\0';
}

static const chromance::core::ParamDescriptor* find_param(const chromance::core::EffectConfigSchema& schema,
                                                          chromance::core::ParamId pid) {
  if (schema.params == nullptr || schema.param_count == 0 || pid.value == 0) return nullptr;
  for (uint8_t i = 0; i < schema.param_count; ++i) {
    if (schema.params[i].id.value == pid.value) return &schema.params[i];
  }
  return nullptr;
}

static const char* api_param_type_for(const chromance::core::ParamDescriptor& p) {
  using chromance::core::ParamType;
  switch (p.type) {
    case ParamType::Bool:
      return "bool";
    case ParamType::Enum:
      return "enum";
    case ParamType::ColorRgb:
      return "color";
    case ParamType::U8:
      return "int";
    case ParamType::I16:
    case ParamType::U16:
      return (p.scale > 1) ? "float" : "int";
    default:
      return "int";
  }
}

static void send_json_headers(WiFiClient& client, int http_status, size_t content_len) {
  const char* status_text = (http_status == 200)   ? "OK"
                           : (http_status == 400) ? "Bad Request"
                           : (http_status == 403) ? "Forbidden"
                           : (http_status == 404) ? "Not Found"
                           : (http_status == 409) ? "Conflict"
                           : (http_status == 429) ? "Too Many Requests"
                                                   : "Internal Server Error";
  client.printf(
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Cache-Control: no-store\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "\r\n",
      http_status, status_text, static_cast<unsigned>(content_len));
}

static void begin_chunked_json_response(WebServer& server, int http_status) {
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(http_status, "application/json; charset=utf-8", "");
}

class ChunkedJsonWriter {
 public:
  ChunkedJsonWriter(WebServer* server, bool send) : server_(server), send_(send) {}

  size_t bytes() const { return bytes_; }

  void write(const char* s) {
    if (s == nullptr) return;
    write(s, strlen(s));
  }

  void write(const char* s, size_t n) {
    if (s == nullptr || n == 0) return;
    bytes_ += n;
    if (!send_ || server_ == nullptr) return;
    while (n > 0) {
      const size_t space = sizeof(buf_) - used_;
      const size_t take = (n < space) ? n : space;
      memcpy(buf_ + used_, s, take);
      used_ += take;
      s += take;
      n -= take;
      if (used_ == sizeof(buf_)) flush();
    }
  }

  void write_u32(uint32_t v) {
    char tmp[16] = {};
    const int n = snprintf(tmp, sizeof(tmp), "%lu", static_cast<unsigned long>(v));
    if (n > 0) write(tmp, static_cast<size_t>(n));
  }

  void write_i32(int32_t v) {
    char tmp[16] = {};
    const int n = snprintf(tmp, sizeof(tmp), "%ld", static_cast<long>(v));
    if (n > 0) write(tmp, static_cast<size_t>(n));
  }

  void write_f64_4(double v) {
    char tmp[32] = {};
    const int n = snprintf(tmp, sizeof(tmp), "%.4f", v);
    if (n > 0) write(tmp, static_cast<size_t>(n));
  }

  void write_escaped(const char* s) {
    if (s == nullptr) return;
    for (const char* p = s; *p; ++p) {
      const char c = *p;
      switch (c) {
        case '\"':
          write("\\\"", 2);
          break;
        case '\\':
          write("\\\\", 2);
          break;
        case '\n':
          write("\\n", 2);
          break;
        case '\r':
          write("\\r", 2);
          break;
        case '\t':
          write("\\t", 2);
          break;
        default:
          write(&c, 1);
          break;
      }
    }
  }

  void flush() {
    if (!send_ || server_ == nullptr || used_ == 0) {
      used_ = 0;
      return;
    }
    server_->sendContent(buf_, used_);
    used_ = 0;
  }

  void end_chunked() {
    flush();
    if (send_ && server_ != nullptr) {
      server_->sendContent("");
    }
  }

 private:
  WebServer* server_ = nullptr;
  bool send_ = false;
  size_t bytes_ = 0;
  char buf_[512] = {};
  size_t used_ = 0;
};

}  // namespace

WebuiServer::WebuiServer(const char* firmware_version,
                         chromance::platform::RuntimeSettings* runtime_settings,
                         chromance::core::EffectParams* global_params,
                         chromance::core::EffectManager<32>* manager,
                         const chromance::core::EffectCatalog<32>* catalog)
    : firmware_version_(firmware_version),
      runtime_settings_(runtime_settings),
      global_params_(global_params),
      manager_(manager),
      catalog_(catalog) {}

void WebuiServer::begin() {
  prefs_.begin("chromance", false);
  init_confirm_token();
  validate_aliases_and_log();
  server_.onNotFound([this]() { dispatch(); });
  server_.begin();
}

bool WebuiServer::render_gate_allows(uint32_t now_ms, uint32_t next_render_deadline_ms) const {
  if (next_render_deadline_ms <= now_ms) return false;
  return (next_render_deadline_ms - now_ms) >= kRenderGateHeadroomMs;
}

void WebuiServer::handle(uint32_t now_ms, uint32_t next_render_deadline_ms) {
  if (!render_gate_allows(now_ms, next_render_deadline_ms)) {
    return;
  }
  server_.handleClient();
}

bool WebuiServer::take_pending_restart() {
  const bool v = pending_restart_;
  pending_restart_ = false;
  return v;
}

void WebuiServer::dispatch() {
  if (handle_api_routes()) return;
  if (handle_page_routes()) return;
  if (handle_static_asset_routes()) return;
  server_.send(404, "text/plain", "Not found");
}

bool WebuiServer::handle_page_routes() {
  if (server_.method() != HTTP_GET) return false;
  const String uri = server_.uri();

  if (uri == "/favicon.ico") {
    server_.send(204, "text/plain", "");
    return true;
  }
  if (uri == "/") return send_embedded_asset("/index.html", nullptr);
  if (uri == "/settings" || uri == "/settings/") return send_embedded_asset("/settings/index.html", nullptr);
  if (uri == "/settings/persistence" || uri == "/settings/persistence/")
    return send_embedded_asset("/settings/persistence/index.html", nullptr);

  if (uri.startsWith("/effects/")) {
    const String slug = uri.substring(String("/effects/").length());
    chromance::core::EffectId id;
    String canonical;
    bool is_alias = false;
    if (parse_effect_slug(slug, &id, &canonical, &is_alias) && is_alias) {
      server_.sendHeader("Location", String("/effects/") + canonical);
      server_.send(308, "text/plain", "Redirect");
      return true;
    }
    return send_embedded_asset("/effects/index.html", nullptr);
  }

  return false;
}

bool WebuiServer::handle_static_asset_routes() {
  if (server_.method() != HTTP_GET) return false;
  const String uri = server_.uri();
  return send_embedded_asset(uri.c_str(), nullptr);
}

bool WebuiServer::handle_api_routes() {
  const String uri = server_.uri();
  if (!uri.startsWith("/api/")) return false;

  if (server_.method() == HTTP_GET && uri == "/api/effects") {
    api_get_effects();
    return true;
  }
  if (server_.method() == HTTP_GET && uri.startsWith("/api/effects/")) {
    const String slug = uri.substring(String("/api/effects/").length());
    api_get_effect_detail(slug);
    return true;
  }
  if (server_.method() == HTTP_POST && uri.startsWith("/api/effects/") && uri.endsWith("/activate")) {
    const String base = uri.substring(0, uri.length() - String("/activate").length());
    const String slug = base.substring(String("/api/effects/").length());
    api_post_activate(slug);
    return true;
  }
  if (server_.method() == HTTP_POST && uri.startsWith("/api/effects/") && uri.endsWith("/restart")) {
    const String base = uri.substring(0, uri.length() - String("/restart").length());
    const String slug = base.substring(String("/api/effects/").length());
    api_post_restart(slug);
    return true;
  }
  if (server_.method() == HTTP_POST && uri.startsWith("/api/effects/") && uri.endsWith("/stage")) {
    const String base = uri.substring(0, uri.length() - String("/stage").length());
    const String slug = base.substring(String("/api/effects/").length());
    api_post_stage(slug);
    return true;
  }
  if (server_.method() == HTTP_POST && uri.startsWith("/api/effects/") && uri.endsWith("/params")) {
    const String base = uri.substring(0, uri.length() - String("/params").length());
    const String slug = base.substring(String("/api/effects/").length());
    api_post_params(slug);
    return true;
  }

  if (server_.method() == HTTP_GET && uri == "/api/settings") {
    api_get_settings();
    return true;
  }
  if (server_.method() == HTTP_POST && uri == "/api/settings/brightness") {
    api_post_brightness();
    return true;
  }
  if (server_.method() == HTTP_POST && uri == "/api/settings/reset") {
    api_post_reset();
    return true;
  }

  if (server_.method() == HTTP_GET && uri == "/api/settings/persistence/summary") {
    api_get_persistence_summary();
    return true;
  }
  if (server_.method() == HTTP_GET && uri.startsWith("/api/settings/persistence/effects/")) {
    const String slug = uri.substring(String("/api/settings/persistence/effects/").length());
    api_get_persistence_effect(slug);
    return true;
  }
  if (server_.method() == HTTP_DELETE && uri == "/api/settings/persistence") {
    api_delete_persistence_all();
    return true;
  }

  send_json_error(404, "not_found", "Unknown API route");
  return true;
}

bool WebuiServer::send_embedded_asset(const char* request_path, const char* content_type_override) {
  const WebuiAsset* asset = chromance_webui_find_asset(request_path);
  if (asset == nullptr) {
    return false;
  }

  WiFiClient client = server_.client();
  const char* ct = content_type_override ? content_type_override : asset->content_type;
  client.printf(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Encoding: gzip\r\n"
      "Cache-Control: %s\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "\r\n",
      ct, asset->cache_control, static_cast<unsigned>(asset->gz_len));

  const uint8_t* p = asset->gz_data;
  size_t remaining = asset->gz_len;
  uint32_t last_progress_ms = millis();

  if (asset->gz_len <= 1024) {
    (void)client.write(p, remaining);
    client.stop();
    return true;
  }

  while (remaining > 0) {
    const uint32_t now = millis();
    if ((now - last_progress_ms) > kStaticSendCpuBudgetMs) {
      // Fail fast if we cannot make progress (e.g., socket backpressure) within the budget window.
      client.stop();
      return true;
    }

    const size_t chunk = remaining > 512 ? 512 : remaining;
    const size_t wrote = client.write(p, chunk);
    if (wrote == 0) {
      delay(0);
      continue;
    }

    p += wrote;
    remaining -= wrote;
    last_progress_ms = now;
    delay(0);
  }

  client.stop();
  return true;
}

String WebuiServer::canonical_slug_for_id(chromance::core::EffectId id) const {
  char key[6] = {0};
  snprintf(key, sizeof(key), "e%04x", id.value);
  return String(key);
}

bool WebuiServer::alias_is_collided(const char* slug) const {
  if (slug == nullptr) return false;
  for (uint8_t i = 0; i < collided_alias_count_; ++i) {
    if (collided_aliases_[i] != nullptr && strcmp(collided_aliases_[i], slug) == 0) {
      return true;
    }
  }
  return false;
}

void WebuiServer::validate_aliases_and_log() {
  collided_alias_count_ = 0;
  if (catalog_ == nullptr) return;

  for (size_t i = 0; i < catalog_->count(); ++i) {
    const auto* d_i = catalog_->descriptor_at(i);
    if (d_i == nullptr || d_i->slug == nullptr) continue;
    const char* alias = d_i->slug;

    bool collides = false;

    for (size_t j = 0; j < catalog_->count(); ++j) {
      const auto* d_j = catalog_->descriptor_at(j);
      if (d_j == nullptr) continue;
      const String canonical = canonical_slug_for_id(d_j->id);
      if (strcmp(alias, canonical.c_str()) == 0) {
        collides = true;
        break;
      }
    }

    for (size_t j = 0; j < catalog_->count() && !collides; ++j) {
      if (i == j) continue;
      const auto* d_j = catalog_->descriptor_at(j);
      if (d_j == nullptr || d_j->slug == nullptr) continue;
      if (strcmp(alias, d_j->slug) == 0) {
        collides = true;
        break;
      }
    }

    if (!collides) continue;
    if (collided_alias_count_ < kMaxCollidedAliases) {
      collided_aliases_[collided_alias_count_++] = alias;
    }
  }

  if (collided_alias_count_ > 0) {
    Serial.print("WebUI alias collisions (ignored): ");
    for (uint8_t i = 0; i < collided_alias_count_; ++i) {
      if (i) Serial.print(", ");
      Serial.print(collided_aliases_[i] ? collided_aliases_[i] : "?");
    }
    Serial.println();
  }
}

bool WebuiServer::parse_effect_slug(const String& slug, chromance::core::EffectId* out_id,
                                    String* out_canonical_slug, bool* out_is_alias) {
  if (catalog_ == nullptr || out_id == nullptr || out_canonical_slug == nullptr || out_is_alias == nullptr) {
    return false;
  }
  *out_id = chromance::core::EffectId{0};
  *out_is_alias = false;
  *out_canonical_slug = "";

  // Canonical: eXXXX (hex)
  if (slug.length() == 5 && (slug[0] == 'e' || slug[0] == 'E')) {
    uint16_t v = 0;
    for (int i = 1; i < 5; ++i) {
      const char c = slug[i];
      uint8_t d = 0;
      if (c >= '0' && c <= '9') d = static_cast<uint8_t>(c - '0');
      else if (c >= 'a' && c <= 'f') d = static_cast<uint8_t>(10 + (c - 'a'));
      else if (c >= 'A' && c <= 'F') d = static_cast<uint8_t>(10 + (c - 'A'));
      else return false;
      v = static_cast<uint16_t>((v << 4) | d);
    }
    const auto id = chromance::core::EffectId{v};
    if (!id.valid() || catalog_->find_by_id(id) == nullptr) {
      return false;
    }
    *out_id = id;
    *out_canonical_slug = canonical_slug_for_id(id);
    *out_is_alias = false;
    return true;
  }

  // Alias: match EffectDescriptor.slug (unless collided)
  const String trimmed = slug;
  for (size_t i = 0; i < catalog_->count(); ++i) {
    const auto* d = catalog_->descriptor_at(i);
    if (d == nullptr || d->slug == nullptr) continue;
    if (alias_is_collided(d->slug)) continue;
    if (trimmed == String(d->slug)) {
      *out_id = d->id;
      *out_canonical_slug = canonical_slug_for_id(d->id);
      *out_is_alias = true;
      return true;
    }
  }

  return false;
}

bool WebuiServer::rate_limit_allow_brightness(uint32_t now_ms) {
  if (static_cast<int32_t>(now_ms - bright_window_start_ms_) >= static_cast<int32_t>(kRateWindowMs)) {
    bright_window_start_ms_ = now_ms;
    bright_count_in_window_ = 0;
  }
  if (bright_count_in_window_ >= kMaxBrightnessPerSec) {
    return false;
  }
  bright_count_in_window_++;
  return true;
}

bool WebuiServer::rate_limit_allow_params(uint32_t now_ms) {
  if (static_cast<int32_t>(now_ms - params_window_start_ms_) >= static_cast<int32_t>(kRateWindowMs)) {
    params_window_start_ms_ = now_ms;
    params_count_in_window_ = 0;
  }
  if (params_count_in_window_ >= kMaxParamsPerSec) {
    return false;
  }
  params_count_in_window_++;
  return true;
}

void WebuiServer::init_confirm_token() {
  // Per-boot, best-effort randomness (LAN-only UI; this is accidental-wipe protection, not auth).
  static const char* kHex = "0123456789abcdef";
  uint32_t x = esp_random();
  for (size_t i = 0; i < 16; ++i) {
    x = (x * 1664525UL) + 1013904223UL + static_cast<uint32_t>(micros());
    confirm_token_[i] = kHex[(x >> 28) & 0xF];
  }
  confirm_token_[16] = '\0';
}

bool WebuiServer::check_confirm_token_and_phrase(const String& body, const char* expected_phrase) {
  if (body.length() == 0 || body.length() > kMaxHttpBodyBytes) {
    send_json_error(400, "bad_request", "Invalid body");
    return false;
  }

  StaticJsonDocument<384> doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    send_json_error(400, "bad_request", "Invalid JSON");
    return false;
  }

  String token;
  String phrase;
  if (!json_get_string(doc, "confirmToken", &token) || !json_get_string(doc, "confirmPhrase", &phrase)) {
    send_json_error(400, "bad_request", "Missing confirm fields");
    return false;
  }

  if (token != String(confirm_token_)) {
    send_json_error(403, "bad_request", "Bad token");
    return false;
  }
  if (expected_phrase == nullptr || phrase != String(expected_phrase)) {
    send_json_error(403, "bad_request", "Bad confirm phrase");
    return false;
  }
  return true;
}

void WebuiServer::send_json_error(int http_status, const char* code, const char* message) {
  String resp;
  resp.reserve(256);
  resp += "{\"ok\":false,\"error\":{\"code\":\"";
  resp += code ? code : "internal";
  resp += "\",\"message\":\"";
  resp += message ? message : "error";
  resp += "\"}}";
  server_.send(http_status, "application/json; charset=utf-8", resp);
}

void WebuiServer::send_json_ok_bounded(const String& json_body) {
  if (json_body.length() > kMaxJsonBytes) {
    send_json_error(500, "response_too_large", "Response too large");
    return;
  }
  server_.send(200, "application/json; charset=utf-8", json_body);
}

void WebuiServer::api_get_effects() {
  if (catalog_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing catalog");
    return;
  }

  const auto emit = [&](ChunkedJsonWriter& w) {
    const String active_canonical = canonical_slug_for_id(manager_->active_id());

    w.write("{\"ok\":true,\"data\":{\"effects\":[");
    bool first = true;
    for (size_t i = 0; i < catalog_->count(); ++i) {
      const auto* d = catalog_->descriptor_at(i);
      if (d == nullptr) continue;
      if (!first) w.write(",");
      first = false;

      const String canonical = canonical_slug_for_id(d->id);

      w.write("{\"id\":");
      w.write_u32(d->id.value);
      w.write(",\"canonicalSlug\":\"");
      w.write_escaped(canonical.c_str());
      w.write("\",\"displayName\":\"");
      w.write_escaped(d->display_name ? d->display_name : "");
      w.write("\"");
      if (d->description) {
        w.write(",\"description\":\"");
        w.write_escaped(d->description);
        w.write("\"");
      }
      w.write("}");
    }
    w.write("],\"activeId\":");
    w.write_u32(manager_->active_id().value);
    w.write(",\"activeCanonicalSlug\":\"");
    w.write_escaped(active_canonical.c_str());
    w.write("\"}}");
  };

  ChunkedJsonWriter measure(nullptr, false);
  emit(measure);
  if (measure.bytes() > kMaxJsonBytes) {
    send_json_error(500, "response_too_large", "Response too large");
    return;
  }

  begin_chunked_json_response(server_, 200);
  ChunkedJsonWriter out(&server_, true);
  emit(out);
  out.end_chunked();
}

void WebuiServer::api_get_effect_detail(const String& slug) {
  if (catalog_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing catalog");
    return;
  }

  chromance::core::EffectId id;
  String canonical;
  bool is_alias = false;
  if (!parse_effect_slug(slug, &id, &canonical, &is_alias)) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }

  const auto* d = catalog_->descriptor_by_id(id);
  auto* e = catalog_->find_by_id(id);
  if (d == nullptr || e == nullptr) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }

  const chromance::core::EffectConfigSchema* schema = e->schema();
  const bool has_schema = (schema != nullptr && schema->params != nullptr && schema->param_count > 0);

  const bool include_alias = (d->slug != nullptr && !alias_is_collided(d->slug));

  const auto emit = [&](ChunkedJsonWriter& w) {
    w.write("{\"ok\":true,\"data\":{");
    w.write("\"id\":");
    w.write_u32(id.value);
    w.write(",\"canonicalSlug\":\"");
    w.write_escaped(canonical.c_str());
    w.write("\"");

    w.write(",\"aliasSlugs\":[");
    if (include_alias) {
      w.write("\"");
      w.write_escaped(d->slug);
      w.write("\"");
    }
    w.write("]");

    w.write(",\"displayName\":\"");
    w.write_escaped(d->display_name ? d->display_name : "");
    w.write("\"");
    if (d->description) {
      w.write(",\"description\":\"");
      w.write_escaped(d->description);
      w.write("\"");
    }

    w.write(",\"supportsRestart\":true");

    if (has_schema) {
      w.write(",\"schema\":{\"params\":[");
      for (uint8_t i = 0; i < schema->param_count; ++i) {
        const auto& p = schema->params[i];
        if (i) w.write(",");
        w.write("{\"id\":");
        w.write_u32(p.id.value);
        w.write(",\"name\":\"");
        w.write_escaped(p.name ? p.name : "");
        w.write("\",\"displayName\":\"");
        w.write_escaped(p.display_name ? p.display_name : "");
        w.write("\",\"type\":\"");
        w.write_escaped(api_param_type_for(p));
        w.write("\"");

        if (p.scale > 1) {
          w.write(",\"scale\":");
          w.write_u32(p.scale);
        }

        const bool is_float =
            (p.scale > 1) && (p.type == chromance::core::ParamType::I16 || p.type == chromance::core::ParamType::U16);
        if (!is_float) {
          w.write(",\"min\":");
          w.write_i32(p.min);
          w.write(",\"max\":");
          w.write_i32(p.max);
          w.write(",\"step\":");
          w.write_i32(p.step);
          w.write(",\"def\":");
          w.write_i32(p.def);
        } else {
          const double scale = static_cast<double>(p.scale);
          w.write(",\"min\":");
          w.write_f64_4(static_cast<double>(p.min) / scale);
          w.write(",\"max\":");
          w.write_f64_4(static_cast<double>(p.max) / scale);
          w.write(",\"step\":");
          w.write_f64_4(static_cast<double>(p.step) / scale);
          w.write(",\"def\":");
          w.write_f64_4(static_cast<double>(p.def) / scale);
        }
        w.write("}");
      }
      w.write("]}");

      w.write(",\"values\":{");
      for (uint8_t i = 0; i < schema->param_count; ++i) {
        const auto& p = schema->params[i];
        if (i) w.write(",");
        w.write("\"");
        w.write_escaped(p.name ? p.name : "");
        w.write("\":");

        chromance::core::ParamValue v;
        if (!manager_->get_param(id, p.id, &v)) {
          w.write_i32(p.def);
          continue;
        }

        if (p.type == chromance::core::ParamType::Bool) {
          w.write((v.v.b || v.v.u8 != 0) ? "true" : "false");
        } else if (p.type == chromance::core::ParamType::ColorRgb) {
          const uint32_t packed = (static_cast<uint32_t>(v.v.color_rgb.r) << 16) |
                                  (static_cast<uint32_t>(v.v.color_rgb.g) << 8) |
                                  (static_cast<uint32_t>(v.v.color_rgb.b) << 0);
          w.write_u32(packed);
        } else if (p.type == chromance::core::ParamType::Enum) {
          w.write_u32(v.v.e);
        } else if (p.type == chromance::core::ParamType::U8) {
          w.write_u32(v.v.u8);
        } else if (p.type == chromance::core::ParamType::U16) {
          if (p.scale > 1) {
            w.write_f64_4(static_cast<double>(v.v.u16) / static_cast<double>(p.scale));
          } else {
            w.write_u32(v.v.u16);
          }
        } else if (p.type == chromance::core::ParamType::I16) {
          if (p.scale > 1) {
            w.write_f64_4(static_cast<double>(v.v.i16) / static_cast<double>(p.scale));
          } else {
            w.write_i32(v.v.i16);
          }
        } else {
          w.write_i32(p.def);
        }
      }
      w.write("}");
    }

    if (e->stage_count() > 0) {
      w.write(",\"stages\":{\"items\":[");
      const uint8_t n = e->stage_count();
      bool first_stage = true;
      for (uint8_t i = 0; i < n; ++i) {
        const auto* st = e->stage_at(i);
        if (st == nullptr || st->name == nullptr || st->display_name == nullptr) continue;
        if (!first_stage) w.write(",");
        first_stage = false;
        w.write("{\"id\":");
        w.write_u32(st->id.value);
        w.write(",\"name\":\"");
        w.write_escaped(st->name);
        w.write("\",\"displayName\":\"");
        w.write_escaped(st->display_name);
        w.write("\"}");
      }
      w.write("],\"currentId\":");
      w.write_u32(e->current_stage().value);
      w.write("}");
    }

    w.write("}}");
  };

  ChunkedJsonWriter measure(nullptr, false);
  emit(measure);
  if (measure.bytes() > kMaxJsonBytes) {
    send_json_error(500, "response_too_large", "Response too large");
    return;
  }

  begin_chunked_json_response(server_, 200);
  ChunkedJsonWriter out(&server_, true);
  emit(out);
  out.end_chunked();
}

void WebuiServer::api_post_activate(const String& slug) {
  if (catalog_ == nullptr || manager_ == nullptr || runtime_settings_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }
  const uint32_t now_ms = millis();

  chromance::core::EffectId id;
  String canonical;
  bool is_alias = false;
  if (!parse_effect_slug(slug, &id, &canonical, &is_alias)) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }

  if (!manager_->set_active(id, now_ms)) {
    send_json_error(409, "busy", "Failed to activate effect");
    return;
  }

  runtime_settings_->set_mode(static_cast<uint8_t>(id.value));

  String resp;
  resp.reserve(96);
  resp += "{\"ok\":true,\"data\":{\"activeId\":";
  resp += String(static_cast<unsigned>(id.value));
  resp += ",\"activeCanonicalSlug\":\"";
  resp += canonical;
  resp += "\"}}";
  send_json_ok_bounded(resp);
}

void WebuiServer::api_post_restart(const String& slug) {
  if (catalog_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }
  chromance::core::EffectId id;
  String canonical;
  bool is_alias = false;
  if (!parse_effect_slug(slug, &id, &canonical, &is_alias)) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }
  if (id.value != manager_->active_id().value) {
    // Restart is only meaningful for the active effect.
    send_json_error(409, "busy", "Effect not active");
    return;
  }
  manager_->restart_active(millis());
  send_json_ok_bounded("{\"ok\":true,\"data\":{}}");
}

void WebuiServer::api_post_stage(const String& slug) {
  if (catalog_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }

  const String body = server_.arg("plain");
  if (body.length() == 0 || body.length() > kMaxHttpBodyBytes) {
    send_json_error(400, "bad_request", "Invalid body");
    return;
  }

  StaticJsonDocument<256> doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    send_json_error(400, "bad_request", "Invalid JSON");
    return;
  }

  uint32_t stage_id = 0;
  if (!json_get_u32(doc, "id", &stage_id) || stage_id > 255) {
    send_json_error(400, "bad_request", "Invalid stage id");
    return;
  }

  chromance::core::EffectId id;
  String canonical;
  bool is_alias = false;
  if (!parse_effect_slug(slug, &id, &canonical, &is_alias)) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }

  if (id.value != manager_->active_id().value) {
    send_json_error(409, "busy", "Effect not active");
    return;
  }

  if (!manager_->enter_active_stage(chromance::core::StageId(static_cast<uint8_t>(stage_id)), millis())) {
    send_json_error(400, "bad_request", "Stage not supported");
    return;
  }

  String resp;
  resp.reserve(64);
  resp += "{\"ok\":true,\"data\":{\"currentId\":";
  resp += String(stage_id);
  resp += "}}";
  send_json_ok_bounded(resp);
}

void WebuiServer::api_post_params(const String& slug) {
  if (catalog_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }
  const uint32_t now_ms = millis();
  if (!rate_limit_allow_params(now_ms)) {
    send_json_error(429, "rate_limited", "Too many requests");
    return;
  }

  const String body = server_.arg("plain");
  if (body.length() == 0 || body.length() > kMaxHttpBodyBytes) {
    send_json_error(400, "bad_request", "Invalid body");
    return;
  }

  StaticJsonDocument<kMaxHttpBodyBytes> doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    send_json_error(400, "bad_request", "Invalid JSON");
    return;
  }

  chromance::core::EffectId id;
  String canonical;
  bool is_alias = false;
  if (!parse_effect_slug(slug, &id, &canonical, &is_alias)) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }

  auto* e = catalog_->find_by_id(id);
  const auto* schema = e ? e->schema() : nullptr;
  if (schema == nullptr || schema->params == nullptr || schema->param_count == 0) {
    send_json_error(400, "bad_request", "Effect has no params");
    return;
  }

  JsonArray items = doc["items"].as<JsonArray>();
  if (items.isNull() || items.size() == 0) {
    send_json_error(400, "bad_request", "Missing items");
    return;
  }

  uint8_t applied = 0;
  for (JsonVariant v : items) {
    if (!v.is<JsonObject>()) continue;
    JsonObject o = v.as<JsonObject>();

    uint32_t pid_u32 = 0;
    if (!o["id"].is<uint32_t>() || !o["id"].as<uint32_t>()) continue;
    pid_u32 = o["id"].as<uint32_t>();
    if (pid_u32 == 0 || pid_u32 > 0xFFFF) continue;
    const auto pid = chromance::core::ParamId(static_cast<uint16_t>(pid_u32));

    const auto* d = find_param(*schema, pid);
    if (d == nullptr || d->name == nullptr) continue;

    chromance::core::ParamValue pv;
    pv.type = d->type;

    // Enforce step in raw units (after scaling if needed).
    auto step_ok = [&](int32_t raw) -> bool {
      if (d->step <= 0) return false;
      const int32_t delta = raw - d->min;
      if (delta < 0) return false;
      return (delta % d->step) == 0;
    };

    if (d->type == chromance::core::ParamType::Bool) {
      if (o["value"].is<bool>()) {
        pv.v.b = o["value"].as<bool>();
      } else if (o["value"].is<uint32_t>()) {
        pv.type = chromance::core::ParamType::U8;
        pv.v.u8 = o["value"].as<uint32_t>() ? 1 : 0;
      } else {
        continue;
      }
      if (!manager_->set_param(id, pid, pv)) continue;
      applied++;
      continue;
    }

    if (d->type == chromance::core::ParamType::Enum || d->type == chromance::core::ParamType::U8) {
      if (!o["value"].is<uint32_t>()) continue;
      const uint32_t x = o["value"].as<uint32_t>();
      if (x > 255) continue;
      pv.type = chromance::core::ParamType::U8;
      pv.v.u8 = static_cast<uint8_t>(x);
      if (!step_ok(static_cast<int32_t>(pv.v.u8))) continue;
      if (!manager_->set_param(id, pid, pv)) continue;
      applied++;
      continue;
    }

    if (d->type == chromance::core::ParamType::U16) {
      if (d->scale > 1) {
        if (!o["value"].is<double>()) continue;
        const double ui = o["value"].as<double>();
        const int32_t raw = static_cast<int32_t>(lround(ui * static_cast<double>(d->scale)));
        if (!step_ok(raw)) continue;
        pv.v.u16 = static_cast<uint16_t>(raw);
      } else {
        if (!o["value"].is<uint32_t>()) continue;
        const uint32_t x = o["value"].as<uint32_t>();
        if (x > 65535) continue;
        if (!step_ok(static_cast<int32_t>(x))) continue;
        pv.v.u16 = static_cast<uint16_t>(x);
      }
      if (!manager_->set_param(id, pid, pv)) continue;
      applied++;
      continue;
    }

    if (d->type == chromance::core::ParamType::I16) {
      if (d->scale > 1) {
        if (!o["value"].is<double>()) continue;
        const double ui = o["value"].as<double>();
        const int32_t raw = static_cast<int32_t>(lround(ui * static_cast<double>(d->scale)));
        if (!step_ok(raw)) continue;
        pv.v.i16 = static_cast<int16_t>(raw);
      } else {
        if (!o["value"].is<int32_t>()) continue;
        const int32_t x = o["value"].as<int32_t>();
        if (!step_ok(x)) continue;
        pv.v.i16 = static_cast<int16_t>(x);
      }
      if (!manager_->set_param(id, pid, pv)) continue;
      applied++;
      continue;
    }

    if (d->type == chromance::core::ParamType::ColorRgb) {
      if (!o["value"].is<uint32_t>()) continue;
      const uint32_t packed = o["value"].as<uint32_t>();
      pv.v.color_rgb.r = static_cast<uint8_t>((packed >> 16) & 0xFF);
      pv.v.color_rgb.g = static_cast<uint8_t>((packed >> 8) & 0xFF);
      pv.v.color_rgb.b = static_cast<uint8_t>((packed >> 0) & 0xFF);
      if (!manager_->set_param(id, pid, pv)) continue;
      applied++;
      continue;
    }
  }

  String resp;
  resp.reserve(96);
  resp += "{\"ok\":true,\"data\":{\"applied\":";
  resp += String(applied);
  resp += ",\"canonicalSlug\":\"";
  resp += canonical;
  resp += "\"}}";
  send_json_ok_bounded(resp);
}

void WebuiServer::api_get_settings() {
  if (runtime_settings_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }
  const uint8_t soft = runtime_settings_->brightness_percent();
  const uint8_t ceiling = chromance::core::kHardwareBrightnessCeilingPercent;
  const uint8_t effective = chromance::core::soft_percent_to_hw_percent(soft, ceiling);
  const String active = canonical_slug_for_id(manager_->active_id());

  String resp;
  resp.reserve(256);
  resp += "{\"ok\":true,\"data\":{";
  resp += "\"firmwareVersion\":\"";
  resp += firmware_version_ ? firmware_version_ : "unknown";
  resp += "\",\"mappingVersion\":\"";
  resp += chromance::core::MappingTables::mapping_version();
  resp += "\",\"activeId\":";
  resp += String(static_cast<unsigned>(manager_->active_id().value));
  resp += ",\"activeCanonicalSlug\":\"";
  resp += active;
  resp += "\",\"brightness\":{";
  resp += "\"softPct\":";
  resp += String(static_cast<unsigned>(soft));
  resp += ",\"hwCeilingPct\":";
  resp += String(static_cast<unsigned>(ceiling));
  resp += ",\"effectivePct\":";
  resp += String(static_cast<unsigned>(effective));
  resp += "}}}";
  send_json_ok_bounded(resp);
}

void WebuiServer::api_post_brightness() {
  if (runtime_settings_ == nullptr || global_params_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }

  const uint32_t now_ms = millis();
  if (!rate_limit_allow_brightness(now_ms)) {
    send_json_error(429, "rate_limited", "Too many requests");
    return;
  }

  const String body = server_.arg("plain");
  if (body.length() == 0 || body.length() > kMaxHttpBodyBytes) {
    send_json_error(400, "bad_request", "Invalid body");
    return;
  }

  StaticJsonDocument<256> doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    send_json_error(400, "bad_request", "Invalid JSON");
    return;
  }

  uint32_t soft_pct = 0;
  if (!json_get_u32(doc, "softPct", &soft_pct) || soft_pct > 100) {
    send_json_error(400, "bad_request", "Invalid softPct");
    return;
  }

  const uint8_t q = chromance::core::clamp_percent_0_100(static_cast<uint8_t>(soft_pct));
  runtime_settings_->set_brightness_percent(q);
  global_params_->brightness = chromance::core::soft_percent_to_u8_255(
      runtime_settings_->brightness_percent(), chromance::core::kHardwareBrightnessCeilingPercent);
  manager_->set_global_params(*global_params_);

  const uint8_t ceiling = chromance::core::kHardwareBrightnessCeilingPercent;
  const uint8_t effective =
      chromance::core::soft_percent_to_hw_percent(runtime_settings_->brightness_percent(), ceiling);

  String resp;
  resp.reserve(128);
  resp += "{\"ok\":true,\"data\":{";
  resp += "\"softPct\":";
  resp += String(static_cast<unsigned>(runtime_settings_->brightness_percent()));
  resp += ",\"hwCeilingPct\":";
  resp += String(static_cast<unsigned>(ceiling));
  resp += ",\"effectivePct\":";
  resp += String(static_cast<unsigned>(effective));
  resp += "}}";
  send_json_ok_bounded(resp);
}

void WebuiServer::api_post_reset() {
  const String body = server_.arg("plain");
  if (!check_confirm_token_and_phrase(body, "RESET")) {
    return;
  }
  send_json_ok_bounded("{\"ok\":true,\"data\":{}}");
  pending_restart_ = true;
}

void WebuiServer::api_get_persistence_summary() {
  if (catalog_ == nullptr || runtime_settings_ == nullptr || manager_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }

  const uint8_t mode = runtime_settings_->mode();
  const uint8_t bright = runtime_settings_->brightness_percent();
  const uint16_t aeid = manager_->active_id().value;

  const auto emit = [&](ChunkedJsonWriter& w) {
    w.write("{\"ok\":true,\"data\":{");
    w.write("\"globals\":{\"aeid\":");
    w.write_u32(aeid);
    w.write(",\"bright_pct\":");
    w.write_u32(bright);
    w.write(",\"mode\":");
    w.write_u32(mode);
    w.write("},\"effects\":[");

    bool first = true;
    for (size_t i = 0; i < catalog_->count(); ++i) {
      const auto* d = catalog_->descriptor_at(i);
      if (d == nullptr) continue;
      if (!first) w.write(",");
      first = false;
      const String canonical = canonical_slug_for_id(d->id);
      const bool present =
          prefs_.isKey(canonical.c_str()) && (prefs_.getBytesLength(canonical.c_str()) == chromance::core::kMaxEffectConfigSize);
      w.write("{\"id\":");
      w.write_u32(d->id.value);
      w.write(",\"canonicalSlug\":\"");
      w.write_escaped(canonical.c_str());
      w.write("\",\"present\":");
      w.write(present ? "true" : "false");
      w.write("}");
    }

    w.write("],\"confirmToken\":\"");
    w.write_escaped(confirm_token_);
    w.write("\"}}");
  };

  ChunkedJsonWriter measure(nullptr, false);
  emit(measure);
  if (measure.bytes() > kMaxJsonBytes) {
    send_json_error(500, "response_too_large", "Response too large");
    return;
  }

  begin_chunked_json_response(server_, 200);
  ChunkedJsonWriter out(&server_, true);
  emit(out);
  out.end_chunked();
}

void WebuiServer::api_get_persistence_effect(const String& slug) {
  if (catalog_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }

  chromance::core::EffectId id;
  String canonical;
  bool is_alias = false;
  if (!parse_effect_slug(slug, &id, &canonical, &is_alias)) {
    send_json_error(404, "not_found", "Unknown effect");
    return;
  }

  uint8_t blob[chromance::core::kMaxEffectConfigSize] = {};
  const bool present =
      prefs_.isKey(canonical.c_str()) && (prefs_.getBytesLength(canonical.c_str()) == sizeof(blob)) &&
      (prefs_.getBytes(canonical.c_str(), blob, sizeof(blob)) == sizeof(blob));

  char hex[(chromance::core::kMaxEffectConfigSize * 2) + 1] = {};
  if (present) {
    bytes_to_hex_lower(blob, sizeof(blob), hex, sizeof(hex));
  }
  const uint16_t crc = present ? crc16_ccitt(blob, sizeof(blob)) : 0;

  String resp;
  resp.reserve(512);
  resp += "{\"ok\":true,\"data\":{";
  resp += "\"id\":";
  resp += String(static_cast<unsigned>(id.value));
  resp += ",\"canonicalSlug\":\"";
  resp += canonical;
  resp += "\",\"present\":";
  resp += present ? "true" : "false";
  if (present) {
    resp += ",\"blobHex\":\"";
    resp += String(hex);
    resp += "\",\"blobVersion\":";
    resp += String(static_cast<unsigned>(blob[0]));
    resp += ",\"crc16\":";
    resp += String(static_cast<unsigned>(crc));
  }
  resp += "}}";
  send_json_ok_bounded(resp);
}

void WebuiServer::api_delete_persistence_all() {
  if (catalog_ == nullptr) {
    send_json_error(500, "internal", "Missing state");
    return;
  }
  const String body = server_.arg("plain");
  if (!check_confirm_token_and_phrase(body, "DELETE")) {
    return;
  }

  (void)prefs_.remove("aeid");
  (void)prefs_.remove("bright_pct");
  (void)prefs_.remove("mode");

  // Remove per-effect keys derived from catalog ids (lowercase), plus legacy uppercase variants.
  for (size_t i = 0; i < catalog_->count(); ++i) {
    const auto* d = catalog_->descriptor_at(i);
    if (d == nullptr) continue;

    const String key_lower = canonical_slug_for_id(d->id);
    (void)prefs_.remove(key_lower.c_str());

    char key_upper[6] = {0};
    snprintf(key_upper, sizeof(key_upper), "e%04X", d->id.value);
    (void)prefs_.remove(key_upper);
  }

  send_json_ok_bounded("{\"ok\":true,\"data\":{}}");
}

}  // namespace platform
}  // namespace chromance
