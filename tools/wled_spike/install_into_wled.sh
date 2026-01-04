#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WLED_DIR="${ROOT_DIR}/tools/wled_spike/wled"

if [[ ! -d "${WLED_DIR}" ]]; then
  echo "Missing WLED clone at: ${WLED_DIR}"
  echo "Clone it with:"
  echo "  git clone --depth 1 https://github.com/Aircoookie/WLED.git tools/wled_spike/wled"
  exit 1
fi

cp "${ROOT_DIR}/tools/wled_spike/platformio_override.chromance_spike.ini" "${WLED_DIR}/platformio_override.ini"

mkdir -p "${WLED_DIR}/usermods/chromance_spike"
cp "${ROOT_DIR}/tools/wled_spike/usermods/chromance_spike/library.json" "${WLED_DIR}/usermods/chromance_spike/library.json"
cp "${ROOT_DIR}/tools/wled_spike/usermods/chromance_spike/chromance_spike.cpp" "${WLED_DIR}/usermods/chromance_spike/chromance_spike.cpp"

echo "Installed Chromance spike files into:"
echo "  ${WLED_DIR}"
echo ""
echo "Next:"
echo "  (cd tools/wled_spike/wled && pio run -e chromance_spike_featheresp32)"

