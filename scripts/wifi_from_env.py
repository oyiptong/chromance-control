import os

Import("env")


def _as_c_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


wifi_ssid = os.getenv("WIFI_SSID")
wifi_password = os.getenv("WIFI_PASSWORD")
ota_hostname = os.getenv("OTA_HOSTNAME")

cpp_defines = []
if wifi_ssid and wifi_password:
    cpp_defines.append(("WIFI_SSID", _as_c_string(wifi_ssid)))
    cpp_defines.append(("WIFI_PASSWORD", _as_c_string(wifi_password)))
if ota_hostname:
    cpp_defines.append(("OTA_HOSTNAME", _as_c_string(ota_hostname)))

if cpp_defines:
    env.Append(CPPDEFINES=cpp_defines)
