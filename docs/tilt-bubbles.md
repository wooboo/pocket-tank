# Tilt-aware bubbles

Bubbles rise along the accelerometer's estimate of up, projected onto the
screen. This is a visual effect: the aerator, fish bubble-visit targets,
decorations and water remain in tank coordinates. Food sinks opposite the
bubble direction at its original speed, with perpendicular drift while
moving. Pellets slide along the glass, settle at the downhill boundary and
resume moving when tilt changes; their lifetime and feeding rules remain
unchanged. Grass additionally
responds to motion as described in [grass-motion.md](grass-motion.md). No model or save
format changes are needed.

The QMI8658 runs with its existing 31.25 Hz accelerometer configuration and
gyroscope off. The tank polls it at most once per 32 ms; motion detection
and screen inversion retain their separate 250 ms cadence and thresholds.
Heading changes are smoothed with a 180 ms time constant. Nearly flat
poses (less than 0.15 g in the screen plane) and implausible magnitudes
hold the last direction. Missing or stale sensor samples fall back to
screen-up. The 180-degree display inversion is applied after filtering.

The board was calibrated on 2026-09-24: with its screen facing the keeper
and the right edge raised, readings were approximately [10752,-11602,-4077].
Thus positive sensor X maps right and negative sensor Y maps up in the
normal framebuffer orientation.

Column bubbles respawn at the floor aerator after leaving any edge.
Ambient bubbles enter from the opposite edge, weighted by the direction
of motion. Wobble stays perpendicular to the rise direction.

## Verification

Run the isolated host regression from the repository root:

```sh
cc -std=c11 -Icommon firmware/host_test/tilt_host.c common/tank.c -lm -o /tmp/tilt_host
/tmp/tilt_host
```

It exercises the measured tilt pose, smoothing, inverted coordinates,
flat/freefall/shake rejection, all exit edges and four-minute runs in
eight directions. The test also passes with address/undefined sanitizers.

On hardware, `imu 3` reports both raw acceleration and the bubble-up vector.
Visually check right/left tilt, the automatic upside-down screen flip,
and laying the board flat. Fish continue visiting their original landmark;
following the tilted stream is a separate gameplay change.

`firmware/host_test/food_host.c` verifies eight sinking directions, boundary
containment, settling, wall sliding, release on inversion and unchanged decay.
