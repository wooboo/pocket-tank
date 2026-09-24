# Low-battery shelter

At 25% or less while not charging, fish swim into the lower half of the tank and hover near available cover. Coral and
reef-cluster refuges are assigned first, then grass/plant beds with enough
height. If everything has been trimmed to stubble and no coral is owned,
the school rests low near the reef instead. Targets follow relocated
corals and the current bent grass geometry.

Sheltering fish render behind grass and corals, including decorations
normally placed behind fish. Normal rendering order returns when shelter
ends. This is a temporary presentation/steering behavior; it does not
replace the model's chosen goal, change hunger or stress, or change the save
format. It does not reduce model inference or promise extra battery life.
Startles temporarily take precedence; fish in the setup preview retain
their presentation behavior. Fish can still eat food they encounter.

Charging clears shelter on the next battery poll. When running on battery,
readings above 25% must persist for a 30-second recovery period. Missing
readings retain the previous known shelter state. The existing notice and
battery pill still trigger at 10%. The mode is recomputed after boot rather
than saved.

## Checks

The `firmware/host_test/shelter_host.c` host test exercises descent and
holding cover, coral/grass targets, the bare-garden fallback, release,
startle/setup exceptions, and pixel occlusion with and without the scene
cache. Link it with common/tank.c, common/advisor.c, common/render.c,
common/icons.c, common/progression.c, common/setup.c and
sim/persist_port_sim.c. It also writes /tmp/low-battery-shelter.ppm.

On hardware, `battery 25` temporarily stages an unplugged low battery;
`state` reports `low-battery shelter: active`. `battery real` restores the
actual gauge and charging input. These test overrides never persist.

Prior device verification at the original 40% threshold on 2026-09-24: staged 40% activated shelter; all five fish
reached y=284..332 (screen midpoint 184). At staged 41% the mode initially
held, then cleared after the recovery period. Restoring `battery real` left
shelter off. Host tests now cover the 25/26% boundary and immediate release
on charging.
