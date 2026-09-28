#!/bin/bash
# Archive the selected device; updates preserve NVS. --install-new replaces foreign firmware.
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD="${POCKET_TANK_BUILD:-${BUILD:-$HOME/.cache/pocket-tank/ble-fw-build}}"
PORT="${PORT:-}"; MODEL=0; NO_BUILD=0; INSTALL_NEW=0; NO_BACKUP=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --model) MODEL=1; shift ;;
    --install-new) INSTALL_NEW=1; MODEL=1; shift ;;
    --no-build) NO_BUILD=1; shift ;;
    --no-backup) NO_BACKUP=1; shift ;;
    *) echo "Usage: $0 --port PATH [--model] [--no-build] [--install-new] [--no-backup]"; exit 1 ;;
  esac
done
if [ -z "$PORT" ]; then
  shopt -s nullglob
  ports=(/dev/cu.usbmodem*)
  [ "${#ports[@]}" -le 1 ] || { echo "flash: multiple devices; specify --port or PORT"; exit 1; }
  PORT=${ports[0]:-}
fi
[ -n "$PORT" ] || { echo "flash: no device; specify --port or PORT"; exit 1; }
export PORT
: "${IDF_PATH:=$HOME/esp/esp-idf}"
. "$IDF_PATH/export.sh" > /dev/null 2>&1 || { echo "flash: set IDF_PATH to your ESP-IDF checkout"; exit 1; }
if [ "$INSTALL_NEW" -eq 1 ]; then
  python tools/preflight.py --port "$PORT" --foreign-firmware
else
  python tools/preflight.py --port "$PORT"
fi
# Resolve build failures before any flash read/reset or new-install erase.
if [ "$NO_BUILD" -eq 0 ]; then
  (cd firmware && idf.py -B "$BUILD" build)
fi
BACKUP_DIR="${POCKET_TANK_BACKUPS:-$HOME/.cache/pocket-tank/backups}"
mkdir -p "$BACKUP_DIR"
BACKUP="$BACKUP_DIR/$(date +%Y%m%d-%H%M%S)-$(basename "$PORT")-nvs.bin"
if [ "$INSTALL_NEW" -eq 1 ]; then
  if [ "$NO_BACKUP" -eq 0 ]; then
    BACKUP="${BACKUP%-nvs.bin}-full.bin"
    python -m esptool --chip esp32s3 -p "$PORT" -b 460800 read_flash 0 ALL "$BACKUP"
    python -m esptool --chip esp32s3 -p "$PORT" -b 460800 verify_flash 0 "$BACKUP"
    shasum -a 256 "$BACKUP" > "$BACKUP.sha256"
    echo "flash: verified full firmware backup: $BACKUP"
  fi
  python -m esptool --chip esp32s3 -p "$PORT" erase_region 0x9000 0x6000
elif [ "$NO_BACKUP" -eq 0 ]; then
  python -m esptool --chip esp32s3 -p "$PORT" read_flash 0x9000 0x6000 "$BACKUP"
fi
cd firmware
if [ "$MODEL" -eq 1 ]; then
  python -m esptool --chip esp32s3 -p "$PORT" -b 460800 write_flash 0x290000 ../model/out/model_q4.bin
fi
idf.py -B "$BUILD" -p "$PORT" flash
echo "flash: complete; pre-flash archive is in docs/batlog"
