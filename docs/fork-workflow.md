# Working on this fork

`main` mirrors `mediacutlet/pocket-tank`'s `main`. Our working version is
`fork`: it includes tilt-aware bubbles and food, moving/cuttable grass,
shake reactions, low-battery shelter, and BLE money/fish gifts. Keeping a
feature here does not commit us to proposing it upstream.

Start each idea on a `feat/<idea>` branch from `fork`. Test it, then merge
it into `fork`. Keep unrelated ideas in separate commits. Shared integration
history is merged, not rebased or force-pushed.

## Updating upstream

Start with a clean working tree. Update the mirror by fast-forward only,
then integrate it on a temporary branch:

```sh
git fetch upstream
git switch main
git merge --ff-only upstream/main
git switch -c chore/update-upstream fork
git merge main
```

Resolve conflicts while preserving both upstream changes and our additions.
Run the simulator checks for all four boards, the fork regression tests,
and build the firmware before merging the update into `fork`:

```sh
make -C sim -j8 check-all
tools/check_fork.sh
# In an ESP-IDF environment, with the existing signing key configured:
# cd firmware && idf.py -B <checkout-specific-build-directory> build
git switch fork
git merge --ff-only chore/update-upstream
git push origin main fork
```

The simulator needs SDL2 and LVGL v9.2.2 at `sim/lvgl`. On an Apple Silicon
Mac with Homebrew's native SDL2, use
`make -C sim ARCH= EXTRA_CFLAGS=-I/opt/homebrew/include -j8 check-all`.
Without the model files, add `NOMODEL=1`.

## Proposing one feature upstream

Create `upstream/<idea>` from the freshly fetched `upstream/main`, and
cherry-pick only that feature's commits. Adapt it to work independently,
test it, and target the upstream repository in the PR. Do not open an
upstream PR from the combined `fork` branch.

## Initial integration: upstream v0.3.9.5

`fork` starts from `chore/update-from-upstream` (`7ed88a5`), which already
contains the tilt and BLE branches plus upstream v0.3.6. It merges upstream
`e223cfe` (v0.3.9.5). Earlier feature branches remain available as history;
they do not need to be merged again.

The upstream save prefix stays unchanged through byte 1951. Fork BLE
journals follow it at byte 1952. Loading recognizes both previous deployed
BLE layouts (journals at 1656 and 1912), preserving balances, fish identities,
family data and pending transfers. New saves also retain upstream's shelf,
volcano, locks, crab and board-layout data.

The optimized grass renderer uses the shared blade shape for drawing and
picking. Idle grass still uses upstream's baked loop; tilted or settling
grass draws live and returns to the loop after settling. The shared shape
uses the loop's seven-second period.

BOOT gestures coexist: double press opens MY TANKS; hold at least two seconds
and release opens/closes FRIENDS; hold five seconds and then tap keeps the
upstream reset/ADVANCED gesture. FRIENDS blocks tank switching, and setup,
MY TANKS, touch lock and confirmation pages block pairing.

Firmware builds do not flash the connected devices. Use the existing
archive-before-flash procedure in `tools/flash.sh` for hardware deployment.

Validation of this integration (2026-11-01): all 140 simulator selftests
passed (35 per board, including the model checks); all ten programs in
`tools/check_fork.sh` passed under AddressSanitizer/UndefinedBehaviorSanitizer;
the audio mixer test and Python syntax checks passed; the signed ESP-IDF 5.5
amoled18 firmware built successfully. Hardware was not flashed or tested in
this update. Linux container checks were unavailable because the local
Docker daemon was not running.
