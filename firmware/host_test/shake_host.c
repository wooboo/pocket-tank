/* cc -std=c11 -Icommon firmware/host_test/shake_host.c common/tank.c common/advisor.c -lm -o /tmp/shake_host */
#include "tank.h"
#include "advisor.h"
#include "tank_events.h"
#include "../main/imu_gravity.h"
#include <assert.h>
#include <stdio.h>
static int spooks;
static void event(int ev,int fish,void *ud) { (void)fish;(void)ud; if(ev==TEV_SPOOK)spooks++; }
static bool sample(imu_shake_t *s,float x,float y,float z) {
    const float a[3]={x,y,z}; return imu_shake_sample(s,a,.04f);
}
int main(void) {
    imu_shake_t s={0};
    for(int i=0;i<250;i++) assert(!sample(&s,.003f*sinf(i),-1,0));
    /* Smooth rotation, including crossing the screen-flip orientation. */
    for(int i=0;i<500;i++) { float a=i*.025f; assert(!sample(&s,sinf(a),-cosf(a),0)); }
    s=(imu_shake_t){0}; sample(&s,0,-1,0);
    assert(!sample(&s,1,-1,0));
    for(int i=0;i<100;i++) assert(!sample(&s,0,-1,0));
    /* Constant acceleration and a saturated axis are not repeated shaking. */
    for(int i=0;i<100;i++) assert(!sample(&s,1,-1,0));
    for(int i=0;i<100;i++) assert(!sample(&s,2,-1,0));
    for(int axis=0;axis<3;axis++) {
        s=(imu_shake_t){0}; sample(&s,0,0,-1);
        int events=0;
        for(int i=0;i<125;i++) {
            float a[3]={0,0,-1}; a[axis]+=.8f*sinf(i*.04f*6.2831853f*3);
            events+=imu_shake_sample(&s,a,.04f);
        }
        assert(events==2); /* repeated stress during five seconds of shaking */
        for(int i=0;i<200;i++) assert(!sample(&s,0,0,-1));
    }
    tank_t t; tank_init(&t,123); tank_new_population(&t);
    tank_events_set(event,NULL);
    for(int i=0;i<t.n_fish;i++) { t.fish[i].stress=1; t.fish[i].trust=6; }
    t.tap_count=2;
    tank_shake(&t);
    assert(t.startled && t.shake_startled && spooks==1 && t.tap_count==0);
    for(int i=0;i<t.n_fish;i++) { assert(t.fish[i].stress==5.0f); assert(t.fish[i].trust==6); }
    for(int i=0;i<25;i++) tank_tick(&t,.04f,advisor_rules);
    assert(!t.startled && !t.shake_startled); /* brief scatter, lasting stress */
    for(int i=0;i<t.n_fish;i++) assert(t.fish[i].stress>4.0f);
    for(int i=0;i<225;i++) tank_tick(&t,.04f,advisor_rules);
    assert(!t.startled && !t.shake_startled);
    for(int i=0;i<t.n_fish;i++) assert(t.fish[i].stress<5.0f);
    for(int i=0;i<20;i++) tank_shake(&t);
    for(int i=0;i<t.n_fish;i++) assert(t.fish[i].stress==10);
    puts("PASS: gentle tilt/noise/bump rejected; sustained shake on all axes; cooldown; school stress, recovery and cap; trust preserved");
}
