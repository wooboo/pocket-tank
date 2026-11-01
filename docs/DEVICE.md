# The device: what is in flight, and the rules for touching it

Read this before ANY action that resets the tank (flash, esptool, power-off).
Battery measurements run in parallel with feature work; the tank's own
battery log is the instrument, and a reset used to wipe it. On 2026-09-15
the first full night of deep sleep on battery - the measurement the whole
09-11 battery pass was waiting for - was lost when the morning's model flash
reset the chip before anyone read the log.

## Rules

1. **Flash only through `tools/flash.sh`** (`--model` for the model partition
   too). It runs `tools/preflight.py` first, which archives `batlog` + `state`
   to `docs/batlog/<date_time>.txt`, and refuses to flash without the archive.
   Commit the archive with the flash.
2. **Any morning after a night on battery: `tools/preflight.py` first**, before
   the director, before a flash, before anything. Then update the table below.
3. **The log itself now survives resets** (batlog.c: RTC_NOINIT + magic + crc;
   only a power-on / PMIC power-off clears it), so a forgotten preflight is no
   longer fatal - but the archive is still the record between sessions.
4. **Every session that starts a measurement writes it in the table below,
   with what would spoil it.** Every session that ends one closes the row.
   The table is the handoff between parallel workstreams; HANDOFF.md carries
   the narrative, this file carries the state.

## In flight on the device

| started | what | spoiled by | status |
|---|---|---|---|
| 2026-09-15 | Deep-sleep current, first full night on battery (BOOT sleep, unplugged, read `batlog` at wake) | any reset before the read (now survives), a mid-night charge, waking it | **CLOSED 2026-09-16: the cell died overnight.** Morning archive docs/batlog/2026-09-16_0710.txt: the RTC log was wiped (a true power-off = the PMIC cut at the empty cell), first row 0% / 3232 mV. The 09-15 daytime windows had already put deep sleep at ~15 gauge-mA (2:22-2:37 15.2, 3:48-4:10 15.9) vs 4.7 for the old light-sleep drowse - deep sleep is WORSE than drowse. Cause (ESP-IDF sleep_gpio.c): digital pads are only isolated in deep sleep when `gpio_deep_sleep_hold_en()` was called, and the firmware never calls it; Espressif's own comment there: un-isolated, "the bottom current of deep sleep will be higher than light sleep". Fix + a fresh night: see HANDOFF 2026-09-16. Bedtime SoC unknown (the log died with the cell) - the sleep row should also go to NVS. |
| 2026-09-16 | **First night on POWER-OFF** (PWR key short press, unplugged - the power-off comes 20 min after the press now -, a HELD PWR press in the morning, then `tools/preflight.py`: the `bed` row from NVS + the boot row give the night's mA; expect the gauge not to move) | a USB plug-in (it powers the PMIC on), a press | **CLOSED 2026-09-17: the power-off works.** Archives docs/batlog/2026-09-17_0739.txt + _0746.txt: `bed` 63% / 3826 mV at the power-off (~23:18, 20 min after the sleep press), the tank read 62% a minute after the morning boot (07:38, on USB by then) and the 5-min boot row 64% (charging). 8 h 28 min for ~1% = ~2 mAh: at or under ~0.25 mA, the gauge's 1% step (2 mAh) being the floor of what it can resolve - consistent with the datasheet's 40 uA, vs ~15 gauge-mA for the un-isolated deep sleep that killed the cell and 4.7 for the old drowse. The tank lived the night (grass to 1.00, 68 algae cells, two fish begging). Caveat: the boot row lands 5 min after boot, so a tank plugged in at wake is already charging when it is written - an immediate boot row would make the number clean. The evening is what drains the cell: 79% on the charger at 20:29 -> 63% at bedtime. |
| 2026-09-15 | Awake draw with 4 fish + the boredom re-asks (09-14 batlog: 72-120 mA awake at 60% brightness, 96% -> 52% in an hour) | brightness changes, charging mid-window | PARKED 2026-09-17 by Strato: "battery is at a manageable level, we may open this back up at a later point. but what we are trying to solve for has been achieved, long term sleep mode." Nothing is in flight; to reopen: measure a clean hour on battery, then look at ask cadence with 4-5 fish |

## How to resume the battery work (everything needed is committed)

1. **Deep-sleep night.** Wake the tank, unplug it, short-press BOOT, leave it
   overnight. Morning: plug in, `tools/preflight.py` FIRST. The archive shows
   the `sleep` and `wake` rows and the "asleep: N h, X mAh -> Y mA" line; the
   gauge's %-scale runs ~0.6x (docs/HANDOFF.md, the battery pass), so compare
   nights, don't trust the absolute. Target: the ESP32-S3 deep-sleep floor
   with touch/codec rails still up on VCC3V3 - expect single-digit mA or
   better; if it is not, `pmic` on the director dumps the rails.
2. **Awake draw, 4 fish.** Unplug at a known SoC, leave the tank awake and
   untouched for an hour at 60% brightness, preflight. Compare with 09-14's
   72-120 mA. The lever is LLM duty: the boot log's "asks / decisions" line
   (`t=...s 4 fish goals: ... | asks N decisions M`); with 4 fish the core is
   ~84% busy (13.6 decisions/min x 3.7 s). Knobs: ADVISOR_MIN_INTERVAL /
   ADVISOR_IDLE_CEILING and the state_signature bands in common/tank.h /
   tank.c (the 09-11 pass cut asks 55% by calming the signature; the boredom
   band of 2026-09-14 added ~2 asks/min/fish while idle).
3. Record results in this table and in docs/stats.md; close the row.

## Firmware on the tank right now

- App: everything through the 09-15 night (sand dollars, the shop, the
  snail, audio, settings, the light) PLUS 2026-09-16: the PWR key is the one
  sleep/wake key, sleep = 20 min grace (was 90 s; 2026-09-16 evening) then PMIC power-off, cold boot lives
  the absence, deep sleep (bench / no-PMIC) holds + isolates its pads,
  bedtime batlog row in NVS, director `poweroff` (flashed 2026-09-16
  morning; archive docs/batlog/2026-09-16_*.txt), the GLASS gate + the
  snail's card (midday), and the CASTLE - shop item 3, BEHIND / IN FRONT
  of the grass (flashed 2026-09-16 13:17; archive docs/batlog/2026-09-16_1317.txt),
  and the SNAIL drawn procedurally, crawling (flashed 2026-09-16 14:37;
  archive docs/batlog/2026-09-16_1437.txt); and everything through
  2026-09-24 - the frond ceilings, the coral, the fry SPAWNING (a staged
  fry is born on its own, no light-on; flashed 2026-09-24 11:04, archive
  docs/batlog/2026-09-24_1104.txt), the reef cluster + SELL, and the
  battery page with the color-blind-safe pill (bolt + sweep; flashed
  2026-09-24 13:15, archive docs/batlog/2026-09-24_1315.txt). The battery
  page's history lives in NVS namespace "bat" (fresh at that flash).
- Model partition: v4m (model_q4_v4m.bin, flashed 2026-09-15 ~06:00).
- Save: mem + lira (juveniles, 0.7 h old, 20 sand dollars) at the 2026-09-24 13:15 flash - the morning's lori / mira / bolt / sol / kelp were already gone in that preflight archive (wiped between 11:16 and 13:15).

## BLE gifting development — 2026-09-28

Device `/dev/cu.usbmodem311401` received the BOOT-triggered Friends build from
this worktree (ESP-IDF 5.5). Preflight archives are the dated per-port files
under `docs/batlog/`; NVS backups are under
`~/.cache/pocket-tank/backups/`. All five fish (dolt, zori, chip, gira, help)
and the 802-dollar balance survived the update. The model was not flashed.

On this board, discovery and repeated full Bluetooth shutdown/restart work.
Internal free heap was 42,687 bytes during discovery and 76,579 after close,
identical over the two measured cycles. A five-fish tank continued rendering
at about 20 fps with discovery/menu open, with model decisions around 3.7 s.
These are USB bench observations, not battery-life measurements.

The initial build ran out of display DMA memory because caches and the newly
linked Bluetooth code exhausted internal SRAM. Render caches now use PSRAM;
NimBLE host allocations use PSRAM too, and display flush checks initialization.
The corrected build boots and runs without that assertion.

The owner confirmed `/dev/cu.usbmodem311101` is the same board and authorized
installing Pocket Tank. They subsequently requested skipping the backup; the
in-progress full read was cancelled and the incomplete file removed. The new
board received the model and app with `--install-new --no-backup`. Later app
updates on both devices also used `--no-backup`; preflight logs were retained.

Two-device testing passed: matching-code Secure Connections, requiring both
confirmations, cancellation, gifts in both directions, decline/refund, and
resuming a pending offer after disconnect and re-pairing. The first run
returned balances to 802/0. Before the final run they were 682/120; that run
returned them to exactly 682/120, with no pending journals and radios off.
Both first-run setup flows were already complete by the final test.

The initial hardware test exposed a duplicate termination: calling
`ble_gap_terminate` just before `nimble_port_stop` caused NimBLE's shutdown
counter to underflow and report 255 connections. Shutdown now lets
`nimble_port_stop` own termination; error paths use the same shutdown. The
final hardware harness rejects that log signature and passed without it.
Evidence: `docs/ble/2026-09-28-hardware.txt`. No new battery measurement is in
flight.

## Combined tilt + BLE deployment — 2026-09-28

Rebased BLE onto `feat/tilt-bubbles` (including main's portable flashing changes)
and flashed application commit `996a1dc` to both `/dev/cu.usbmodem311401` and
`/dev/cu.usbmodem311101`. Used `--no-backup --no-build`, as requested; model
partitions and NVS were not rewritten. Preflight archives:
`2026-09-28_144330_cu.usbmodem311401.txt` and
`2026-09-28_144431_cu.usbmodem311101.txt`. The second device's initial preflight
received an incomplete response and correctly prevented flashing; the retry
archived its state successfully before proceeding.

Both flash operations verified successfully. The two-device BLE harness passed
mutual code confirmation, cancellation, gifts both ways, decline/refund,
reconnect recovery and clean shutdown. Starting/ending balances were 154/44,
with no pending transfers and both radios off. Tilt/water-motion diagnostics
and low-battery shelter status were checked on the deployed devices. Evidence:
`docs/ble/2026-09-28-rebased-hardware.txt`. Before deployment, all seven host
regressions (tilt, grass, food, shake, shelter, gifts, persistence) and the
combined firmware build passed. No new battery measurement is in flight.

## Fish gifting deployment — 2026-09-28

Both boards now run `889d2eb` (fish gifts on top of the combined tilt + BLE
branch). Flashed through `tools/flash.sh --no-backup --no-build`; archives:
`docs/batlog/2026-09-28_151242_cu.usbmodem311401.txt` and
`docs/batlog/2026-09-28_151242_cu.usbmodem311101.txt`. No binary backup, model
replacement, NVS erase or staging/restore was performed. No battery experiment
was active.

`tools/test_fish_hardware.py` verified the founding-pair restriction, decline
and return, interrupted offer/re-pair/accept, preserved name/colours/age/paid
history, and the return trip with the same identity and parent names. `help`
returned to the original tank: dolt, zori, chip, gira, help; the second tank
still has nori and sol. Balances at this test were 156 / 81 and were unchanged.
Evidence: `docs/ble/2026-09-28-fish-hardware.txt`. The test closes both radios.

The existing two-device money regression also passed on this firmware: both
confirmations required, cancellation, gifts both ways, decline/refund and
reconnect recovery; final balances remained 156 / 81 and both radios stopped.

## Upstream 0.3.6 + existing BLE integration — 2026-10-18

Merged public upstream `b407974` with `t3code/plan-ble-multiplayer-trading`
(`dbcf5df`), preserving money/fish gifts, BOOT pairing, tilt-driven bubbles,
food and grass, shake response, and the 15% shelter. Upstream Wi-Fi updates,
new animals, shop and decoration management remain available.

The September BLE saves appended journals at byte 1656; upstream 0.3.6 uses
that area for its own additions. The merged build keeps upstream's frozen
prefix, moves BLE journals to byte 1912, and migrates both legacy BLE lengths.
Merged saves are 3152 bytes, below the 4000-byte NVS budget. Fish ages retain
upstream's double precision and travel with their fish when slots shift.

Before any device update, archived both tanks under `docs/batlog/2026-10-18_*`.
Read and hashed each device's first 0x290000 flash bytes (bootloader, table,
app and NVS); separate NVS copies and the SHA-256 manifest are in
`~/.cache/pocket-tank/backups/2026-10-18/`, keyed by USB serial
`90:70:69:FE:4E:C4` and `90:70:69:FE:4C:F8`. The AI model is preserved.
The local OTA signing key is also copied to `~/.cache/pocket-tank/keys/`.

Validation before flashing: all 12 simulator selftests on all four boards
(`CPATH=/opt/homebrew/include make -C sim -j8 ARCH= check-all`), protocol and
persistence tests under ASan/UBSan, tilt/grass/food/shake/shelter host tests,
and a signed ESP-IDF 5.5 firmware build. Real-device save fixtures in
`tests/fixtures/` verify all eight fish, identities, family, balances and a
second load after saving in the merged format. Upstream fixtures still load.
Hardware deployment and BLE verification are recorded below after flashing.

### Deployment and hardware verification

Both USB devices now report **v0.3.6 alpha, build `899fe45`**, running and
booting `ota_0`. All flash writes passed esptool hash verification; NVS and
model partitions were not written. The first board received a second app
flash after fixing stale build metadata in linked worktrees; its first
merged save loaded successfully again on that second boot.

Both physical BLE harnesses passed: matching-code confirmation on both
screens, cancellation, money gifts in both directions, decline/refund and
reconnect recovery; fish decline, interrupted offer/reconnect/accept and a
return trip with stable identity, family, name, colours, age and paid history.
Final checks matched all eight original fish against the pre-update rosters.
Both radios are off, both money and fish journals have no pending transfers,
and both devices acknowledged `save`. Final balances are 728 / 902, exactly
the transfer tests' baselines (keeper activity before flashing changed the
balances from the initial backup). Evidence is under
`docs/ble/2026-10-18-upstream-036-*`; the validation file contains the checks
and deployed firmware SHA-256. No battery experiment was started.

## Upstream 0.3.9.5 fork deployment — 2026-11-01

Both USB tanks were updated from v0.3.6 (`899fe45`) to the integrated fork,
**v0.3.9.5 alpha, build `3fe8521`**, running and booting `ota_0`.
The signed amoled18 image SHA-256 is
`bae0f643d9bce01bb369081e79693e140fe8cc1555a2d50e3b4c0dda7ed7e1ca`.
Every flash used `tools/flash.sh` and passed all four esptool hash checks.
The model and NVS partitions were not flashed.

Before updating, captured the battery logs, state, passport rosters and saved
both tanks. Original bootloader/table/app/NVS images, separate NVS copies,
post-update NVS copies and their SHA-256 manifest are kept in
`~/.cache/pocket-tank/backups/2026-11-01-0395-vsrz86te/`. The September/October
BLE save migration retained all eight fish, identities, parent names, colours,
paid history, age progression and balances:

- `/dev/cu.usbmodem311101`, USB `90:70:69:FE:4E:C4`: zori, chip, dolt, nori;
  **280 sand dollars**.
- `/dev/cu.usbmodem31201`, USB `90:70:69:FE:4C:F8`: sol, gira, help, misa;
  **391 sand dollars**.

Hardware validation found two resource problems that simulator tests cannot
exercise. The integrated renderer's tasks left only ~19 KB of internal heap
with an 11.5 KB largest block: controller initialization returned
`ESP_ERR_NO_MEM`; moving just the bake stack still failed the controller's
30 KB contiguous EMI allocation. Commit `08c48a4` moves the bake stack to
PSRAM and reserves controller memory before advisor allocations, reclaiming
it for the radio and protecting the EMI block for reopening. Four controller
open/close cycles verified the lifecycle. The initial reservation also exposed
an unchecked advisor-task creation failure (zero AI decisions); `3fe8521`
moves that stack to PSRAM too and checks the creation result.

The final physical regression, `tools/test_friends_hardware.py --pair-only
--require-llm`, passed: the same pairing code on both devices, confirmation on
both sides, cancellation, reopening, secure connection, clean shutdown and
unchanged balances. AI decisions advanced on both tanks from `[20, 14]` to
`[23, 16]` after pairing. No money or fish were transferred by this check.

Both final saves were acknowledged, NVS was read back, and both devices were
restarted to verify the migrated save loads again. Final checks compare the
full passport rosters against the pre-update capture, require the exact build,
no pending transfers, working AI decisions and successful saving. No battery
experiment was started. Console evidence is in
`docs/ble/2026-11-01-upstream-0395-*`; the `after-4E-C4` / `after-4C-F8` files
are the final post-restart checks, and `pairing-ai-final-hardware` records the
combined BLE/AI regression. Earlier diagnostic traces remain as evidence of
the hardware failures and their fixes.

## Upstream 0.3.9.6 and shared shelter depth — 2026-11-01

Both physical tanks now run signed **v0.3.9.6 alpha, build `4f7d63f`**,
from `ota_0`, with `ota_0` selected for the next boot. Image SHA-256:
`fd531ef7ea7d5b0a80bd43bdccc3e0272e99c87c1747528125730b1540d4f6e6`.
The integration includes upstream `7ffac89` while retaining BLE/AI memory
reservations and both PSRAM task stacks. The new upstream grass loop is on
by default, with its pacing, held heights, memory planning and flush-window
bake scheduling.

Low-battery shelter now selects BEHIND grass/piece depths in the shared swim
depth renderer for that frame. It does not overwrite the fish's selected
depths. Only the no-cache/no-depth-buffer fallback keeps the old painter pass.
Grass shapes accept the loop's held segment count; during tilt and settling
both the loop and held heights are bypassed so live blades and cuts agree.

Validation: 148 simulator selftests across all four board worlds; ten fork
regression programs under ASan/UBSan; audio checks; Python compilation; and
signed ESP-IDF 5.5 firmware. Shelter tests exercise depth scratch and compare
shelter pixels with ordinary BEHIND swimming, check depth restoration, and
retain coverage for the no-cache fallback.

On both USB tanks, pairing/cancellation/reopening passed and AI decisions
advanced `[36, 20]` -> `[38, 22]`. At a staged 15%, all eight fish moved below
the midpoint (observed y 219..332). Crops of actual displayed frames are in
`docs/media/2026-11-01-0396-shelter-4E-C4.png` and `...-4C-F8.png`; they show
fish covered by grass/corals. The real gauges were restored (100%, FULL),
and shelter ended after its existing 30-second recovery hysteresis.

The final restart/load checks retained all passport fields, names, colours,
paid history and progressing ages. The rosters remain zori/chip/dolt/nori
and sol/gira/help/misa; balances remain **280 / 391**. Both radios are closed,
there are no pending transfers and both tanks acknowledged save. Battery
logs and console evidence are under `docs/batlog/2026-11-01_135*` and
`docs/ble/2026-11-01-upstream-0396-*`.

Previous signed `3fe8521` firmware/bootloader/table images, each tank's
pre/post-update NVS and SHA-256 manifest are kept outside Git in
`~/.cache/pocket-tank/backups/2026-11-01-0396-ld3338wk/`. The model and NVS
partitions were not flashed. No battery experiment was started.
