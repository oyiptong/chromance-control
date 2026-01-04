#include "wled.h"

// Milestone 1 spike: report multi-bus indexing and overlay bus boundary markers.
//
// Intended usage:
// - Configure 4 APA102/DotStar busses in WLED (LED prefs), with lengths:
//     bus0=154, bus1=168, bus2=84, bus3=154  (total 560)
// - Set bus "start index" to be contiguous:
//     bus0 start=0, bus1 start=154, bus2 start=322, bus3 start=406
// - Wire pins to match Chromance:
//     Strip1 DATA=23 CLK=22
//     Strip2 DATA=17 CLK=16
//     Strip3 DATA=33 CLK=27
//     Strip4 DATA=14 CLK=32
//
// This usermod:
// - Prints bus start/len and inferred contiguous model to Serial at boot.
// - Overlays distinct colors on each bus's first pixel (by bus start index).

namespace {

constexpr uint32_t kColors[] = {
    0xFF0000u,  // Red
    0x00FF00u,  // Green
    0x0000FFu,  // Blue
    0xFF00FFu,  // Magenta
};

constexpr uint16_t kChromanceStarts[] = {0, 154, 322, 406};

bool find_bus_for_index(uint16_t global_index, size_t* out_bus, uint16_t* out_local) {
  const size_t bus_count = BusManager::getNumBusses();
  for (size_t i = 0; i < bus_count; ++i) {
    const Bus* bus = BusManager::getBus(i);
    if (bus == nullptr) continue;
    const uint16_t start = bus->getStart();
    const uint16_t len = bus->getLength();
    if (global_index >= start && global_index < static_cast<uint16_t>(start + len)) {
      if (out_bus) *out_bus = i;
      if (out_local) *out_local = static_cast<uint16_t>(global_index - start);
      return true;
    }
  }
  return false;
}

void print_bus_info() {
  const size_t bus_count = BusManager::getNumBusses();
  const uint16_t total = BusManager::getTotalLength(false);
  Serial.printf("ChromanceSpike: busses=%u total_len=%u\n", static_cast<unsigned>(bus_count), total);

  bool contiguous = true;
  uint16_t expected_next = 0;
  for (size_t i = 0; i < bus_count; ++i) {
    const Bus* bus = BusManager::getBus(i);
    if (bus == nullptr) continue;
    const uint16_t start = bus->getStart();
    const uint16_t len = bus->getLength();
    Serial.printf("  bus%u: start=%u len=%u pins=%u\n",
                  static_cast<unsigned>(i),
                  start,
                  len,
                  static_cast<unsigned>(bus->getPins()));
    if (start != expected_next) contiguous = false;
    expected_next = static_cast<uint16_t>(start + len);
  }

  Serial.printf("ChromanceSpike: contiguous_index_space=%s\n", contiguous ? "true" : "false");

  for (size_t i = 0; i < (sizeof(kChromanceStarts) / sizeof(kChromanceStarts[0])); ++i) {
    const uint16_t idx = kChromanceStarts[i];
    size_t bus = 0;
    uint16_t local = 0;
    if (find_bus_for_index(idx, &bus, &local)) {
      Serial.printf("ChromanceSpike: idx=%u -> bus%u local=%u\n",
                    idx,
                    static_cast<unsigned>(bus),
                    local);
    } else {
      Serial.printf("ChromanceSpike: idx=%u -> (no bus)\n", idx);
    }
  }
}

void print_net_info() {
  const wl_status_t st = WiFi.status();
  const wifi_mode_t mode = WiFi.getMode();

  Serial.printf("ChromanceSpike: wifi_mode=%u wifi_status=%u WLED_CONNECTED=%s apActive=%s apSSID=%s\n",
                static_cast<unsigned>(mode),
                static_cast<unsigned>(st),
                WLED_CONNECTED ? "true" : "false",
                apActive ? "true" : "false",
                apSSID);

  const IPAddress sta_ip = WiFi.localIP();
  const IPAddress ap_ip = WiFi.softAPIP();
  Serial.printf("ChromanceSpike: sta_ip=%u.%u.%u.%u ap_ip=%u.%u.%u.%u\n",
                sta_ip[0], sta_ip[1], sta_ip[2], sta_ip[3],
                ap_ip[0], ap_ip[1], ap_ip[2], ap_ip[3]);
}

}  // namespace

class ChromanceSpikeUsermod final : public Usermod {
 public:
  void setup() override {
    print_bus_info();
    print_net_info();
    last_print_ms_ = millis();
  }

  void loop() override {
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - last_print_ms_) < 2000) {
      return;
    }
    last_print_ms_ = now;
    print_bus_info();
    print_net_info();
  }

  void handleOverlayDraw() override {
    const size_t bus_count = BusManager::getNumBusses();
    for (size_t i = 0; i < bus_count && i < (sizeof(kColors) / sizeof(kColors[0])); ++i) {
      const Bus* bus = BusManager::getBus(i);
      if (bus == nullptr) continue;
      BusManager::setPixelColor(bus->getStart(), kColors[i]);
    }

    // Also mark the expected Chromance bus boundary indices in white at (idx+1) so the bus-start
    // colored markers remain visible when start indices are configured contiguously.
    for (size_t i = 0; i < (sizeof(kChromanceStarts) / sizeof(kChromanceStarts[0])); ++i) {
      BusManager::setPixelColor(static_cast<uint16_t>(kChromanceStarts[i] + 1), 0xFFFFFFu);
    }
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }

 private:
  uint32_t last_print_ms_ = 0;
};

ChromanceSpikeUsermod chromance_spike;
REGISTER_USERMOD(chromance_spike);
