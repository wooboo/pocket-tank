/* cc -std=c11 -Icommon firmware/host_test/grass_host.c common/tank.c -lm -o /tmp/grass_host */
#include "tank.h"
#include "../main/imu_gravity.h"
#include <assert.h>
#include <stdio.h>
#ifdef GRASS_RENDER
#include "render.h"
#endif
static tank_t tank;
static void near(float a, float b) { assert(fabsf(a-b)<.005f); }
static void garden(int bed, float bend) {
    tank_init(&tank,123);
    tank.sd_unlocks |= SD_ITEM_PLANT;
    tank_plant_place(&tank);
    for(int b=0;b<VEG_BEDS_MAX;b++)
        for(int i=0;i<VEG_FRONDS_MAX;i++) tank.veg_h[b][i]=VEG_NUB;
    tank.veg_h[bed][0]=.85f;
    tank.veg_bend[bed]=bend;
}
static void trimming(int bed, float bend, bool reverse, bool diagonal) {
    garden(bed,bend);
    veg_shape_t before, after; tank_veg_shape(&tank,bed,0,&before);
    float cx, half, base; tank_veg_span(&before,100,&cx,&half);
    tank_veg_frond(&tank,bed,0,&base);
    near(before.x[0],base); /* root stays attached */
    assert(fabsf(cx-base)>30);
    /* Touch the obsolete straight-stem position: visible blade is elsewhere. */
    tank_touch_drag(&tank,base-9,100);
    tank_touch_drag(&tank,base+9,100);
    near(tank.veg_h[bed][0],.85f);
    garden(bed,bend);
    float sign=reverse?-1:1, dy=diagonal?3:0;
    tank_veg_shape(&tank,bed,0,&after);
    float start_center,start_half; tank_veg_span(&after,(int)(100-dy),&start_center,&start_half);
    tank_touch_drag(&tank,start_center-sign*10,100-dy);
    assert(tank.slash_armed);
    float end_center,end_half; tank_veg_span(&after,(int)(100+dy),&end_center,&end_half);
    tank_touch_drag(&tank,end_center+sign*22,100+dy);
    assert(tank.veg_h[bed][0]<.85f && tank.veg_h[bed][0]>=VEG_NUB);
    tank_veg_shape(&tank,bed,0,&after);
    assert(after.top>=93 && after.top<=110);
    /* Cutting leaves the remaining stem in the same place. */
    for(int i=0;i<after.segs;i++) near(before.x[i],after.x[i]);
    /* Nubs and all the other blades were out of the stroke. */
    for(int b=0;b<VEG_BEDS_MAX;b++) for(int i=0;i<VEG_FRONDS_MAX;i++)
        if(b!=bed || i!=0) near(tank.veg_h[b][i],VEG_NUB);
}
static void dynamics(void) {
    tank_init(&tank,123);
    tank_set_water_motion(&tank,.7f,-.714f,0,false);
    for(int i=0;i<500;i++) tank_tick(&tank,.02f,NULL);
    near(tank.veg_bend[0],.56f); near(tank.veg_bend[3],.315f);
    tank_set_water_motion(&tank,-.7f,.714f,0,true);
    near(tank.veg_bend[0],-.56f);
    tank_init(&tank,123);
    imu_water_t w={0}; imu_water_sample(&w,0,.04f); near(w.push,0);
    imu_water_sample(&w,1,.04f); assert(w.push>.8f);
    for(int i=0;i<5;i++) { tank_set_water_motion(&tank,0,-1,w.push,false); tank_tick(&tank,.04f,NULL); }
    assert(tank.veg_bend[0]<-.1f); /* grass lags behind a rightward shake */
    for(int i=0;i<400;i++) { imu_water_sample(&w,1,.04f); tank_set_water_motion(&tank,0,-1,w.push,false); tank_tick(&tank,.04f,NULL); }
    near(w.push,0); near(tank.veg_bend[0],0); near(tank.veg_velocity[0],0);
    /* Bound repeated shocks and long frame times; settling must still work. */
    for(int i=0;i<500;i++) {
        tank_set_water_motion(&tank,i%2?1:-1,0,i%2?10:-10,false);
        tank_tick(&tank,.25f,NULL);
        for(int b=0;b<VEG_BEDS_MAX;b++) assert(isfinite(tank.veg_bend[b]) && fabsf(tank.veg_bend[b])<=1.201f);
    }
    tank_set_water_motion(&tank,0,-1,0,false);
    for(int i=0;i<400;i++) tank_tick(&tank,.04f,NULL);
    near(tank.veg_bend[0],0);
}
#ifdef GRASS_RENDER
static void snapshot(float bend, const char *path) {
    static uint16_t fb[TANK_W*TANK_H];
    tank_init(&tank,123);
    for(int b=0;b<VEG_BEDS;b++) {
        tank.veg_bend[b]=bend;
        for(int i=0;i<VEG_FRONDS_MAX;i++) tank.veg_h[b][i]=.65f;
    }
    render_tank(&tank,fb,TANK_W);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n%d %d\n255\n",TANK_W,TANK_H);
    for(int i=0;i<TANK_W*TANK_H;i++) {
        unsigned char rgb[3]={(fb[i]>>11)*255/31,((fb[i]>>5)&63)*255/63,(fb[i]&31)*255/31};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
#endif
int main(void) {
    dynamics();
    for(int bed=2;bed<=3;bed++) for(int sign=-1;sign<=1;sign+=2)
        for(int reverse=0;reverse<2;reverse++) for(int diagonal=0;diagonal<2;diagonal++)
            trimming(bed,sign*1.1f,reverse,diagonal);
    garden(2,1.1f);
    float root; tank_veg_frond(&tank,2,0,&root);
    tank_touch_drag(&tank,root-8,TANK_H-8);
    tank_touch_drag(&tank,TANK_W-1,TANK_H-8);
    near(tank.veg_h[2][0],VEG_NUB); /* floor sweep mows to protected stubble */
#ifdef GRASS_RENDER
    snapshot(0,"/tmp/grass-upright.ppm");
    snapshot(.9f,"/tmp/grass-right.ppm");
    snapshot(-.9f,"/tmp/grass-left.ppm");
#endif
    puts("PASS: anchored roots, tilt, shake lag/settling, flip, bounded motion, bent grass/sword cuts both ways and diagonally, straight-position misses");
}
