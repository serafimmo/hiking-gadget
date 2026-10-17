#!/usr/bin/env bash
# Hiking Gadget v2 build entry point (SPEC §10.5): fw | probe | host | test | render | golden | check | lint | all
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -d /dev/shm/arduino-esp32 ] || /mnt/wd/arduino-esp32/ramcache.sh          # tmpfs vanishes after a reboot
export TMPDIR=/dev/shm/arduino-esp32/tmp; mkdir -p "$TMPDIR"
A="/mnt/wd/arduino-esp32/bin/arduino-cli --config-file /mnt/wd/arduino-esp32/arduino-cli.yaml"
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc"                                  # USBMode default = hwcdc [V-src boards.txt]
BP="/dev/shm/arduino-esp32/build/${HG_BUILDER:-hg2-main}"                  # one build path per parallel builder
case "${1:-all}" in
  fw)     $A compile --fqbn "$FQBN" --build-path "$BP" --warnings all "$ROOT" 2>&1 | grep -v 'Error initializing instance\|403' ;;
  probe)  $A compile --fqbn "$FQBN" --build-path "$BP-probe" --warnings all "$ROOT/tools/probe/api_probe" ;;
  host|test|sanitize|render|golden|golden-update|check|headers|audit|docs) make -C "$ROOT" -f tools/host/Makefile "${1/host/all}" ;;
  lint)   "$ROOT/tools/lint.sh" ;;
  all)    "$0" lint && "$0" check && "$0" fw ;;
  *)      echo "usage: $0 [fw|probe|host|test|sanitize|render|golden|golden-update|check|headers|audit|docs|lint|all]" >&2; exit 2 ;;
esac
