#ifndef POCKET_FRIENDS_H
#define POCKET_FRIENDS_H
#include "tank.h"
void friends_open(tank_t *t);
void friends_close(void);
bool friends_active(void);
void friends_tick(tank_t *t, int64_t now);
void friends_touch(tank_t *t, float x, float y, bool down);
void friends_render(const tank_t *t, uint16_t *fb, int stride);
void friends_command(tank_t *t, int argc, char **argv);
#endif
