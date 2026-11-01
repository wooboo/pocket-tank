/* cc -std=c11 -Icommon firmware/host_test/food_host.c common/tank.c -lm -o /tmp/food_host */
#include "tank.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static tank_t tank;
static void seed(float ux,float uy,float x,float y) {
    tank_init(&tank,123);
    tank_set_water_motion(&tank,ux,uy,0,false);
    tank.food[0]=(food_t){.x=x,.y=y,.alive=true,.from_player=true};
}
int main(void) {
    for(int i=0;i<8;i++) {
        float a=i*3.14159265f/4,ux=cosf(a),uy=sinf(a);
        seed(ux,uy,200,180);
        tank_tick(&tank,.04f,NULL);
        food_t *p=&tank.food[0];
        assert(fabsf(-(p->x-200)*ux-(p->y-180)*uy-.32f)<.002f);
        for(int j=0;j<1000;j++) {
            tank_tick(&tank,.04f,NULL);
            assert(p->from_player);
            assert(p->alive || p->floor_s > FOOD_FLOOR_S);
            assert(p->x>=4 && p->x<=TANK_W-4 && p->y>=4 && p->y<=TANK_H-14);
        }
        for(int j=0;j<(int)((FOOD_LIFE_S + FOOD_FLOOR_S)/.04f)+1;j++) tank_tick(&tank,.04f,NULL);
        assert(!p->alive); /* upstream permits a late landing another FOOD_FLOOR_S on the floor */
    }
    const float dirs[][2]={{0,-1},{0,1},{-1,0},{1,0}};
    for(int i=0;i<4;i++) {
        float ux=dirs[i][0],uy=dirs[i][1];
        float x=ux<0?TANK_W-4:ux>0?4:200;
        float y=uy<0?TANK_H-14:uy>0?4:180;
        seed(ux,uy,x,y);
        for(int j=0;j<50;j++) tank_tick(&tank,.04f,NULL);
        assert(tank.food[0].x==x && tank.food[0].y==y); /* settled */
        tank_set_water_motion(&tank,-ux,-uy,0,true);
        tank_tick(&tank,.04f,NULL);
        assert((tank.food[0].x-x)*ux+(tank.food[0].y-y)*uy>.3f); /* releases */
    }
    seed(-.70710678f,-.70710678f,TANK_W-4,100);
    tank_tick(&tank,.04f,NULL);
    assert(tank.food[0].y>100); /* slides down a wall rather than freezing */
    puts("PASS: food sinks opposite bubbles in eight directions, stays inside glass, settles, releases on inversion, slides and decays");
}
