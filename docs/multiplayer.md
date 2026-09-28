# Nearby gifts over BLE

Hold BOOT for two seconds on **both awake tanks**, within 30 seconds of each
other. Both show the same six-digit Bluetooth confirmation code. Check the
codes and tap CONNECT on each. In FRIENDS, choose a number of sand dollars
with minus/plus, tap GIVE, and have the recipient tap ACCEPT or DECLINE.
CLOSE disconnects and shuts Bluetooth down. An inactive connected session
closes after two minutes; initial discovery expires after 30 seconds.

BOOT + a new touch **before** the two-second hold still opens the existing
reset prompt. While Friends is open, that reset chord is suppressed. Holding
BOOT again restarts discovery. First-run setup and reset confirmation must
be finished or dismissed before pairing. PWR still sleeps/powers off the tank.

Friends supports sand dollars (steps of 10 on screen) and individual fish
(see Fish gifts below). Swaps and upgrade gifts remain future work. Dollar
gifts change the spendable balance, not lifetime care earnings.

## Transfers and recovery

Pressing GIVE reserves the amount by subtracting it from the available
balance and saving an outgoing offer. The recipient's ACCEPT is saved before
it is transmitted. The sender then saves its irreversible commitment. The
recipient saves the credit and its receipt together, then acknowledges it.
All money changes and protocol records share the tank's single NVS save blob.
Save failures roll back the in-memory change and never acknowledge completion.

A dropped connection or restart can leave GIFT PENDING. Hold BOOT on those
same tanks and confirm the codes again: they retry the saved transaction.
Do not reset a tank or restore an old firmware/save while a gift is pending.
There is no automatic timeout refund: the other device might already have
received the gift. A declined, uncommitted offer refunds the sender.

Transaction sequences and permanent receipt records prevent replay credits.
A tank remembers up to eight sending tank identities; receipts are never
silently evicted. A factory-reset tank gets a new random identity. This is a
local game protocol, not protection against modified firmware or restored
flash backups. A lost/reset peer can leave an unresolved gift pending.

## Implementation

- `common/gift.c`: transport-independent transaction state machine.
- `common/gift.h`: fixed-layout appended save journal, eight receipt slots.
- `common/progression.c`: backward-compatible appended journal, checked save.
- `common/friends_ui.c`: screens rendered with existing tank primitives.
- `firmware/main/friends.c`: on-demand NimBLE lifecycle, discovery, Secure
  Connections numeric comparison, authenticated GATT writes and UI actions.

Both devices advertise the Pocket Tank service and scan. Comparing Bluetooth
addresses elects one central, avoiding two simultaneous connection attempts.
BLE callbacks enqueue bounded events; only the tank thread changes game state.
The custom protocol uses explicit little-endian 20-byte messages, fitting the
minimum ATT MTU, rather than transmitting C structs. Version 1 consists of
version/opcode/reserved (four bytes), sequence (four), amount (four), and
sender tank identity (eight). Opcode zero introduces the tank after encryption.
The other opcodes are OFFER, ACCEPT, COMMIT, DONE and REJECT.

Render caches and NimBLE host allocations use PSRAM to leave internal RAM for
the display DMA and controller. The stack is initialized on demand and fully
stopped/deinitialized on close or timeout. Saves with the journal should not
be opened using old firmware: the old loader rejects saves larger than its
known structure. Keep the per-device NVS backup before updating.

## Development and checks

Build with ESP-IDF 5.5 (the local verified toolchain). Defaults enable NimBLE,
Secure Connections and external host memory. For an existing build config,
apply the new Bluetooth settings or generate a fresh sdkconfig from defaults.
Use `tools/flash.sh --port /dev/cu.usbmodem...` through bash with `IDF_PATH`
set to the ESP-IDF checkout. The script archives battery/state output and
backs up NVS before flashing; it requires an explicit port. It refuses a
non-Pocket-Tank device rather than silently overwriting other firmware. After
explicit authorization, `--install-new` archives raw serial output, reads and
verifies a full flash backup (with a SHA-256 sidecar), initializes the tank NVS
partition, and installs both model and app. The backup path is printed.
`--no-backup` skips binary backups when the owner explicitly chooses that;
the short preflight archive is still recorded.

The serial director supports:

```
friends open
friends                    # page, matching code, peer, balance, journal status
friends confirm            # equivalent to tapping CONNECT
friends give 10
friends accept
friends decline
friends close
```

Use these commands for bench testing only after checking both codes. Debug
staging is blocked during a session, and a tank with a parked save cannot
trade. The model partition is not modified by normal app flashing.

Host tests (no SDL or radio hardware required):

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Icommon \
  tests/gift_test.c common/gift.c -o /tmp/pocket-gift-test
/tmp/pocket-gift-test
cc -std=c11 -O1 -g -fsanitize=address,undefined -Icommon \
  tests/progression_gift_test.c common/gift.c common/tank.c \
  common/progression.c common/advisor.c -lm -o /tmp/pocket-progression-gift-test
/tmp/pocket-progression-gift-test
```

`tests/friends_snapshot.c` renders the four screens through the real renderer
into PPMs for visual inspection. It takes an output prefix as its argument.
On macOS it links with `common/friends_ui.c common/render.c common/icons.c`,
`-Icommon -O2 -ffunction-sections -fdata-sections -Wl,-dead_strip -lm`.

For an authorized two-device bench test, `tools/test_friends_hardware.py`
compares the actual matching codes, confirms both, and exercises gifts in both
directions, decline/refund, and reconnect recovery. Pass `--a PORT` (at least
10 dollars), `--b PORT`, and `--log PATH`. It returns the transferred dollars
to their starting tanks and closes both radio sessions. It temporarily drops
setup panels; an unfinished first-run setup is still owed at the next boot.

## Hardware verification — 2026-09-28

Both connected Waveshare boards ran the final firmware. The bench harness
passed matching codes, confirmation required on both ends, cancelled pairing,
gifts both ways, declined gift refunds, pending-offer recovery after reconnect,
and clean full Bluetooth shutdown. The final run preserved its starting
balances of 682 and 120. There were no pending transfers at the end. See
`docs/ble/2026-09-28-hardware.txt`. Power-loss boundaries remain covered by the
host persistence tests; the hardware recovery test used a BLE disconnect.

## Fish gifts (2026-09-28)

In Friends, tap **FISH**, choose a fish with the arrows, and **GIVE FISH**.
The other tank sees its portrait, name and age and chooses **ACCEPT** or
**DECLINE**. At least two fish must stay with the sender; the receiver must
have space under its normal population cap (five on hardware). Finish a
new fry's welcome before giving a fish. Dollars remain available through
**DOLLARS**. Both devices need this firmware for fish gifts.

The sender atomically saves the fish in escrow and removes it from the live
roster. Declining returns it. Closing Friends does not cancel a transfer:
reopen Friends on these same two tanks to finish it. There is deliberately no
timeout refund after acceptance. A pending fish neither ages nor feeds until
it arrives or returns. New births wait while either fish journal is pending,
reserving room for arrival or return. A returned fish occupies the last slot.

Passports preserve name, preset, colours, age, stage, size, personality,
trust, meals, milestones and their seen/paid ledgers. Stable origin/serial IDs
preserve parent relationships across tanks; absent parents retain their names
and colours. Local parent links reconnect when a family reunites. Fish roster
changes invalidate old asynchronous advisor results.

The existing money save layout and receipt stream are unchanged. A new
append-only save tail holds fish escrow, accepted incoming passport, its own
sequence/receipt ledger (eight senders, never evicted), identity counter and
family records. Save failures roll back the entire affected live roster.
BLE HELLO amount bit 0 advertises fish support; older peers still trade money.
Fish control opcodes are 17–21 (money protocol opcode + 16), with seq and CRC32
in the existing 20-byte envelope. Opcode 32 carries a uint16 fragment index,
uint32 sequence and 12 passport bytes. The 200-byte little-endian v1 passport
uses IEEE binary32; complete rounds retry, and consent requires a complete,
validated passport matching the offered CRC. Accepted passports are durable
before ACCEPT; COMMIT, delivery and DONE remain idempotent across restarts.

Host verification: `tests/fish_gift_test.c` covers save failures at each stage,
reboots, duplicate/stale messages, out-of-order fragments, corrupt payload,
capacity, reserved slots, round trips, family links and paid reward history.
`tests/progression_gift_test.c` exercises the real save serializer and legacy
migration. Serial `friends fish INDEX` offers a fish; `friends roster` reports
identity/history for hardware verification. Never restore staged/debug saves
around a transfer: doing so could replay ownership.
