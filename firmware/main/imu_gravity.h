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
/* Same quarter-turn convention as display_port and touch_port. */
static inline void imu_gravity_rotate(float ux, float uy, int q, float *x, float *y) {
    switch (q & 3) {
    case 1: *x = uy; *y = -ux; break;
    case 2: *x = -ux; *y = -uy; break;
    case 3: *x = -uy; *y = ux; break;
    default: *x = ux; *y = uy; break;
    }
}
/* High-pass acceleration drives transient water movement; unlike buoyancy,
 * it deliberately keeps shake impulses. The first sample seeds the baseline. */
typedef struct { float baseline, push; bool ready; } imu_water_t;
static inline void imu_water_sample(imu_water_t *w, float ax, float dt) {
    if (fabsf(ax) >= 1.95f) { w->push = 0; return; }
    if (!w->ready) { w->baseline = ax; w->ready = true; }
    w->baseline += (ax-w->baseline) * dt/(.35f+dt);
    w->push = ax-w->baseline;
    if (w->push > 1.5f) w->push = 1.5f;
    if (w->push < -1.5f) w->push = -1.5f;
}
/* Recognise sustained back-and-forth acceleration on any axis. A single
 * bump, steady tilt, or sensor saturation cannot startle the tank. */
typedef struct {
    float baseline[3], previous[3], elapsed, quiet, cooldown;
    int reversals;
    bool ready, active;
} imu_shake_t;
static inline bool imu_shake_sample(imu_shake_t *s, const float a[3], float dt) {
    if (dt <= 0 || dt > .1f) return false;
    s->cooldown = fmaxf(0, s->cooldown-dt);
    if (s->active) { s->elapsed += dt; s->quiet += dt; }
    if (s->elapsed > 1.5f || s->quiet > .4f) {
        s->active = false; s->reversals = 0; s->elapsed = s->quiet = 0;
    }
    for (int i=0;i<3;i++) if (fabsf(a[i]) >= 1.99f) return false;
    if (!s->ready) {
        for (int i=0;i<3;i++) s->baseline[i]=a[i];
        s->ready=true; return false;
    }
    float v[3], strength=0;
    for (int i=0;i<3;i++) {
        s->baseline[i] += (a[i]-s->baseline[i])*dt/(.35f+dt);
        v[i]=a[i]-s->baseline[i]; strength+=v[i]*v[i];
    }
    if (s->cooldown > 0 || strength < .45f*.45f) return false;
    s->quiet=0;
    if (!s->active) {
        s->active=true; s->elapsed=0; s->reversals=0;
        for (int i=0;i<3;i++) s->previous[i]=v[i];
    } else {
        float dot=0;
        for (int i=0;i<3;i++) dot+=v[i]*s->previous[i];
        if (dot < -.1f) {
            s->reversals++;
            for (int i=0;i<3;i++) s->previous[i]=v[i];
        }
    }
    if (s->reversals < 3 || s->elapsed < .35f) return false;
    s->cooldown=2; s->active=false; s->reversals=0; s->elapsed=s->quiet=0;
    return true;
}
#endif
