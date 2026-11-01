/* Tilt must render live grass even after upstream's idle loop has been baked. */
#include "tank.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static tank_t tank;
static uint32_t dirty[RENDER_DIRTY_WORDS];
static uint16_t scene[TANK_W*TANK_H], idle[TANK_W*TANK_H];
static uint16_t bent[TANK_W*TANK_H], live[TANK_W*TANK_H];
static bool loop_parts(void) {
    const uint16_t *sc, *band;
    size_t top, off, bytes;
    unsigned epoch;
    return render_grass_loop_parts(tank.clock, &sc, &top, &band, &off, &bytes, &epoch);
}
int main(void) {
    tank_init(&tank, 77);
    tank.n_fish = 0; tank.night = false;
    for (int b = 0; b < VEG_BEDS; b++) tank_veg_set(&tank, b, .7f);
    render_set_scene_cache(scene);
    render_set_dirty_mask(dirty);
    render_set_grass_loop(malloc, free, 8u << 20);
    render_grass_loop_lock(64); render_grass_loop_on(true);
    for (int i = 0; i < 400; i++) {
        render_tank(&tank, idle, TANK_W);
        render_grass_loop_bake_more(&tank, 4);
        if (render_grass_loop_n() && render_grass_loop_ready() == render_grass_loop_n()) break;
    }
    assert(render_grass_loop_n() > 0);
    assert(render_grass_loop_ready() == render_grass_loop_n());
    assert(loop_parts());
    for (int b = 0; b < VEG_BEDS; b++) tank.veg_bend[b] = .7f;
    render_tank(&tank, bent, TANK_W);
    assert(!loop_parts());
    assert(render_grass_loop_bake_more(&tank, 4) == 0);
    assert(memcmp(idle, bent, sizeof bent));
    render_grass_loop_bypass(true);
    render_tank(&tank, live, TANK_W);
    assert(!memcmp(bent, live, sizeof live));
    render_grass_loop_bypass(false);
    for (int b = 0; b < VEG_BEDS; b++) tank.veg_bend[b] = 0;
    tank.veg_velocity[0] = .1f;
    render_tank(&tank, live, TANK_W);
    assert(!loop_parts());
    tank.veg_velocity[0] = 0;
    render_tank(&tank, idle, TANK_W);
    assert(loop_parts());
    render_grass_loop_on(false);
    puts("PASS: baked grass bypassed during tilt and settling, live pixels match, idle loop resumes");
}
