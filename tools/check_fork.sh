#!/usr/bin/env bash
# The fork's transfer, save migration and motion regression tests (no device).
set -euo pipefail
cd "$(dirname "$0")/.."
pt_test_dir=$(mktemp -d "${TMPDIR:-/tmp}/pocket-tank-tests.XXXXXX")
trap 'rm -rf "$pt_test_dir"' EXIT
pt_cc=${CC:-cc}
pt_flags=(-std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -Icommon)
run_test() {
  local name=$1
  shift
  echo "RUN $name"
  "$pt_cc" "${pt_flags[@]}" "$@" -lm -o "$pt_test_dir/$name"
  "$pt_test_dir/$name"
}
pt_core=(common/tank.c common/advisor.c)
pt_save=(common/fish_gift.c common/gift.c "${pt_core[@]}" common/progression.c)
run_test gift tests/gift_test.c common/gift.c
run_test fish-gift tests/fish_gift_test.c common/fish_gift.c "${pt_core[@]}"
run_test progression-gift tests/progression_gift_test.c "${pt_save[@]}"
run_test legacy-ble-save tests/legacy_ble_save_test.c "${pt_save[@]}"
for name in tilt grass food; do
  run_test "$name" "firmware/host_test/${name}_host.c" common/tank.c
done
run_test shake firmware/host_test/shake_host.c "${pt_core[@]}"
run_test shelter firmware/host_test/shelter_host.c "${pt_save[@]}" \
  common/render.c common/icons.c common/setup.c common/advanced.c common/shelf.c \
  common/boot_show.c common/battery.c common/update.c sim/persist_port_sim.c sim/net_port_sim.c
run_test grass-motion-cache tests/grass_motion_cache_test.c "${pt_save[@]}" \
  common/render.c common/icons.c common/setup.c common/advanced.c common/shelf.c \
  common/boot_show.c common/battery.c common/update.c sim/persist_port_sim.c sim/net_port_sim.c
