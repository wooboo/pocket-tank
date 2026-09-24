/* Host regression: real bubble physics plus the sensor's gravity filter.
 * cc -std=c11 -Icommon firmware/host_test/tilt_host.c common/tank.c -lm -o /tmp/tilt_host
 * /tmp/tilt_host
 */
#include "tank.h"
#include "../main/imu_gravity.h"
#include <assert.h>
#include <stdio.h>

static tank_t tank;
static void close_to(float a, float b) { assert(fabsf(a - b) < 0.002f); }

static void heading(float ax, float ay, float az, float x, float y) {
    imu_gravity_t g = {0, -1};
    for (int i = 0; i < 100; ++i) imu_gravity_sample(&g, ax, ay, az, 0.04f);
    close_to(g.x, x); close_to(g.y, y);
    float sx, sy;
    imu_gravity_screen(&g, true, &sx, &sy);
    close_to(sx, -x); close_to(sy, -y);
    /* Flat, freefall, shake and a railed sensor must hold the last heading. */
    const float invalid[][3] = {{.01f,.01f,1}, {0,0,0}, {1.5f,1.5f,0}, {2,0,0}};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        imu_gravity_sample(&g, invalid[i][0], invalid[i][1], invalid[i][2], .04f);
    close_to(g.x, x); close_to(g.y, y);
}

static void bubbles(float x, float y) {
    tank_init(&tank, 1234);
    close_to(tank.bubble_up_x, 0); close_to(tank.bubble_up_y, -1);
    tank.bubble_up_x = x; tank.bubble_up_y = y;
    bubble_t *b = &tank.bubble[0];
    b->x = 200; b->y = 180;
    float speed = b->vy;
    tank_tick(&tank, .04f, NULL);
    /* Wobble must not change rise speed along the buoyancy direction. */
    close_to((b->x - 200)*x + (b->y - 180)*y, speed*.04f);
    /* Every exit edge recycles the column to the aerator, not offscreen. */
    const float exits[][2] = {{-50,180}, {TANK_W+50,180}, {200,-50}, {200,TANK_H+50}};
    for (unsigned i = 0; i < sizeof exits / sizeof exits[0]; ++i) {
        b->x = exits[i][0]; b->y = exits[i][1];
        tank_tick(&tank, .04f, NULL);
        assert(fabsf(b->x - tank.bubble_x) <= 10);
        close_to(b->y, TANK_H-17);
    }
    /* Long runs catch leaks beyond the glass, including ambient respawns. */
    for (int step = 0; step < 6000; ++step) {
        tank_tick(&tank, .04f, NULL);
        for (int i = 0; i < MAX_BUBBLE; ++i) {
            b = &tank.bubble[i];
            assert(isfinite(b->x) && isfinite(b->y));
            assert(b->x >= -6 && b->x <= TANK_W+6);
            assert(b->y >= -6 && b->y <= TANK_H+6);
        }
    }
}

int main(void) {
    heading(0,-1,0,0,-1);
    heading(1,0,0,1,0);
    heading(-1,0,0,-1,0);
    heading(0,1,0,0,1);
    /* Measured right-edge-up pose: +X, -Y, slightly reclined. */
    float len = sqrtf(10752.0f*10752 + 11602.0f*11602);
    heading(10752.0f/16384, -11602.0f/16384, -4077.0f/16384,
            10752/len, -11602/len);
    imu_gravity_t g = {0,-1};
    imu_gravity_sample(&g,1,0,0,.04f);
    assert(g.x > 0 && g.x < .5f && g.y < -.8f); /* smooth, not a jump */
    for (int i = 0; i < 8; ++i) {
        float angle = i * 3.14159265f / 4;
        bubbles(cosf(angle), sinf(angle));
    }
    puts("PASS: tilt filtering, calibrated axes, inversion, invalid samples, all bubble exit edges and eight rise directions");
}
