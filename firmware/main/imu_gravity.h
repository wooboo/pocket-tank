/* Gravity heading in the unflipped landscape framebuffer. Kept separate
 * from I2C so filtering and coordinate transforms can be tested on a host. */
#ifndef IMU_GRAVITY_H
#define IMU_GRAVITY_H
#include <stdbool.h>
#include <math.h>

typedef struct { float x, y; } imu_gravity_t;

static inline void imu_gravity_sample(imu_gravity_t *g, float ax, float ay,
                                      float az, float dt) {
    /* Accelerometer specific force points up at rest. Calibrated on the board:
     * raised right edge gives +X; upright gives -Y. Samples are expressed in g. */
    float magnitude2 = ax*ax + ay*ay + az*az;
    float plane2 = ax*ax + ay*ay;
    /* Reject shakes, freefall, and nearly flat poses. Keep the last heading
     * when gravity points through the glass rather than along it. */
    if (magnitude2 < 0.36f || magnitude2 > 1.96f || plane2 < 0.0225f) return;
    float len = sqrtf(plane2), x = ax / len, y = ay / len;
    float angle = atan2f(g->x*y - g->y*x, g->x*x + g->y*y);
    angle *= dt / (0.18f + dt);
    float c = cosf(angle), s = sinf(angle), old_x = g->x;
    g->x = c*old_x - s*g->y;
    g->y = s*old_x + c*g->y;
}

static inline void imu_gravity_screen(const imu_gravity_t *g, bool inverted,
                                      float *x, float *y) {
    *x = inverted ? -g->x : g->x;
    *y = inverted ? -g->y : g->y;
}
#endif
