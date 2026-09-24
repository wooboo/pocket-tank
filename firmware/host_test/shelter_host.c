/* Link with tank, advisor, render, icons, progression, setup and sim/persist_port_sim. */
#include "tank.h"
#include "advisor.h"
#include "render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static tank_t tank;
static uint16_t base[TANK_W*TANK_H], normal[TANK_W*TANK_H], hidden[TANK_W*TANK_H], scene[TANK_W*TANK_H];
static uint32_t dirty[TANK_H*(TANK_W/32)];
static void populate(bool decor,bool bare) {
    tank_init(&tank,123); tank_new_population(&tank);
    while(tank_add_fish(&tank,0,1)>=0) {}
    if(decor) { tank.sd_unlocks|=SD_ITEM_CORAL|SD_ITEM_CLUSTER; tank_coral_place(&tank); tank_cluster_place(&tank); }
    for(int b=0;b<VEG_BEDS;b++) tank_veg_set(&tank,b,bare?VEG_NUB:.6f);
    tank.low_battery=true;
    for(int i=0;i<tank.n_fish;i++) { tank.fish[i].x=40+i*60; tank.fish[i].y=50; }
}
static void movement(bool decor,bool bare) {
    populate(decor,bare);
    for(int i=0;i<tank.n_fish;i++) {
        float x,y; tank_shelter_target(&tank,i,&x,&y);
        assert(x>=30 && x<=TANK_W-30 && y>=TANK_H*.5f && y<TANK_H);
    }
    for(int frame=0;frame<2000;frame++) tank_tick(&tank,.04f,advisor_rules);
    for(int i=0;i<tank.n_fish;i++) {
        assert(tank.fish[i].y>TANK_H*.5f);
        float x,y; tank_shelter_target(&tank,i,&x,&y);
        assert(tank_dist(x,y,tank.fish[i].x,tank.fish[i].y)<40);
    }
    tank.low_battery=false;
    for(int i=0;i<tank.n_fish;i++) assert(!tank_fish_sheltering(&tank,i));
    tank.low_battery=true; tank_shake(&tank);
    assert(!tank_fish_sheltering(&tank,0));
    for(int i=0;i<80;i++) tank_tick(&tank,.04f,advisor_rules);
    assert(tank_fish_sheltering(&tank,0));
    tank.stage_fish=0; assert(!tank_fish_sheltering(&tank,0));
}
static void ppm(const char *path,const uint16_t *fb) {
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n%d %d\n255\n",TANK_W,TANK_H);
    for(int i=0;i<TANK_W*TANK_H;i++) { unsigned char c[3]={(fb[i]>>11)*255/31,((fb[i]>>5)&63)*255/63,(fb[i]&31)*255/31}; fwrite(c,1,3,f); } fclose(f);
}
static void occlusion(bool coral,bool cached) {
    tank_init(&tank,123); tank_new_population(&tank); tank.n_fish=1;
    for(int b=0;b<VEG_BEDS;b++) tank_veg_set(&tank,b,VEG_NUB);
    if(coral) { tank.sd_unlocks|=SD_ITEM_CORAL; tank_coral_place(&tank); tank.coral_x=230; tank.coral_z=DECOR_Z_BACK; tank.coral_growth=1; tank.fish[0].x=230; tank.fish[0].y=TANK_H-42; }
    else { tank_veg_set(&tank,2,.7f); veg_shape_t shape; tank_veg_shape(&tank,2,2,&shape); float half; tank_veg_span(&shape,250,&tank.fish[0].x,&half); tank.fish[0].y=250; }
    tank.fish[0].size=1.5f; tank.fish[0].heading=0;
    render_set_scene_cache(cached?scene:NULL); render_set_dirty_mask(cached?dirty:NULL);
    for(int i=0;i<50;i++) render_tank(&tank,normal,TANK_W);
    tank.n_fish=0; render_tank(&tank,base,TANK_W); tank.n_fish=1;
    tank.low_battery=true; render_tank(&tank,hidden,TANK_W);
    int covered=0;
    for(int i=0;i<TANK_W*TANK_H;i++) covered+=normal[i]!=base[i] && hidden[i]==base[i];
    printf("occlusion %s %s: %d fish pixels covered\n",coral?"coral":"grass",cached?"cached":"plain",covered);
    assert(covered>10);
    tank.low_battery=false; render_tank(&tank,hidden,TANK_W);
    assert(memcmp(normal,hidden,sizeof normal)==0); /* normal depth restored */
}
int main(void) {
    tank_init(&tank,123);
    tank_update_battery(&tank,.41f,false,1); assert(!tank.low_battery);
    tank_update_battery(&tank,.40f,false,1); assert(tank.low_battery);
    tank_update_battery(&tank,.41f,false,29); assert(tank.low_battery);
    tank_update_battery(&tank,.39f,false,1); assert(tank.low_battery);
    tank_update_battery(&tank,.41f,false,29); assert(tank.low_battery);
    tank_update_battery(&tank,.41f,false,1); assert(!tank.low_battery);
    tank_update_battery(&tank,.10f,false,1); assert(tank.low_battery);
    tank_update_battery(&tank,.10f,true,1); assert(!tank.low_battery);

    movement(false,false); movement(true,false); movement(false,true);
    occlusion(false,false); occlusion(true,false); occlusion(false,true); occlusion(true,true);
    populate(true,false);
    for(int i=0;i<2000;i++) tank_tick(&tank,.04f,advisor_rules);
    render_tank(&tank,hidden,TANK_W); ppm("/tmp/low-battery-shelter.ppm",hidden);
    puts("PASS: shelter targets, descent, holding cover, empty-garden fallback, release, startle/setup exceptions, grass/coral occlusion with and without scene cache");
}
