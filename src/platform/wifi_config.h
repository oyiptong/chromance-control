#pragma once

// Preferred: set build flags from environment variables via platformio.ini:
// WIFI_SSID='...' WIFI_PASSWORD='...' pio run -e diagnostic

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
#if defined(__has_include)
#if __has_include(<wifi_secrets.h>)
#include <wifi_secrets.h>
#endif
#endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "chromance-control"
#endif

