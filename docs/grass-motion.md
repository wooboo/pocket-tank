# Grass responds to device motion

Grass leans toward the screen's gravity-up direction and swings opposite
sideways acceleration. A damped spring per bed makes the motion lag, overshoot
and settle; the sword plant is stiffer. Roots stay fixed. Taller blades bend
more than freshly trimmed stubble. A high-pass accelerometer signal supplies
shake impulses separately from the bubble direction's gravity filter.

Motion is transient. Growth, purchases, and the saved-game format are unchanged.
A display inversion transforms both bend and velocity. Wake re-seeds the
acceleration baseline; missing sensor samples stop supplying shake forces.
Integration is substepped and bounded, including on slow frames.

## Trimming

`tank_veg_shape` builds one segmented spine, and `tank_veg_span` gives its
pixel-row silhouette. The renderer, gesture arming, and cutting use this
geometry. Scissors intersect the actual blade width, including diagonal
strokes and the existing fingertip reach beyond each endpoint. A stroke at
a bent-away blade's former position does not cut it. Cuts choose the lowest
crossed row and snap the remaining tip down to the next whole segment
(3.2 pixels), except for the protected minimum stubble. Sweeping the sand
just below the roots still mows to stubble.

Bend depends on absolute height above the root, so removing a tip does not
move the surviving stem. Fish cover/slowdown regions remain the existing bed
regions; motion does not change their gameplay rules. Decoration occlusion
continues to be handled by the renderer as before.

## Verification

From the repository root:

```sh
cc -std=c11 -Icommon firmware/host_test/grass_host.c common/tank.c -lm -o /tmp/grass_host
/tmp/grass_host
cc -std=c11 -Icommon firmware/host_test/tilt_host.c common/tank.c -lm -o /tmp/tilt_host
/tmp/tilt_host
```

The grass regression exercises settling, inversion, repeated shocks, anchored
roots, cuts across bent grass and sword leaves both ways and diagonally,
misses at old stem positions, unchanged lower-spine geometry, and floor mowing.
Both host regressions were also run with address and undefined-behavior
sanitizers. The existing simulator `--selftest-tend` checks gardening and
save round trips; its precision expectation now follows the visible blade
silhouettes instead of assuming straight stems.

Defining `GRASS_RENDER` for the grass test and linking render.c, icons.c,
progression.c, setup.c and sim/persist_port_sim.c writes upright/left/right
PPM previews under /tmp. Device `imu 3` logs the water impulse and bed-0 bend.

## Fish react to excessive shaking

A separate three-axis detector subtracts a slow acceleration baseline and
requires impulses above 0.45 g with at least three opposing directions over
at least 350 ms. The sequence expires after 1.5 seconds or 400 ms without a
strong impulse. Ordinary smooth tilt, a single bump and saturated sensor
readings do not qualify. A qualifying event has a two-second cooldown.

The whole school briefly flinches for 0.8 seconds with softer steering and
28–42 px/s target speed, and gains 4 stress points (capped at 10).
Continued shaking can raise stress again after the cooldown without
keeping fish in a prolonged flee reflex. Stress remains part of the model's normal input
and calms through the existing recovery rules. Trust is unchanged. Shake
history is transient and resets on wake; the existing save format persists
fish stress as before.

`firmware/host_test/shake_host.c` tests all three axes, quiet/tilt/bump rejection,
rate limiting, school stress, recovery, the stress cap and unchanged trust.
Compile with common/tank.c and common/advisor.c, like the other host tests.
