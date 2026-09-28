#include "tank.h"
#include "progression.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tank_t a,b,da,db;
static bool fail;
static bool save(tank_t *t) { if(fail) return false; if(t==&a) da=*t; else db=*t; return true; }
static void boot(void) { a=da; b=db; }
static void init(void) {
    tank_init(&a,1); tank_init(&b,2); tank_new_population(&a); tank_new_population(&b);
    a.gift.identity=10; b.gift.identity=20; tank_add_fish(&a,0,1);
    a.fish[2].age_s=12345; a.fish[2].ms_seen=3; a.sd_paid_fish[2]=7; a.fish[2].eaten=42;
    tank_set_name(&a,2,"bubbles"); save(&a); save(&b);
}
static gift_msg_t msg(uint8_t op) { gift_save_t *g=&a.fish_gift.journal; return (gift_msg_t){op,g->out_seq,g->out_amount}; }
int main(void) {
    init(); assert(!fish_gift_offer(&b,10,0,save));
    fail=true; assert(!fish_gift_offer(&a,20,2,save)); assert(a.n_fish==3 && !a.fish_gift.journal.out_phase); fail=false;
    assert(fish_gift_offer(&a,20,2,save)); assert(a.n_fish==2); boot();
    fish_passport_t p=a.fish_gift.outgoing; assert(p.age==12345 && p.eaten==42 && p.paid==7);
    assert(p.family.parents[0].tank==10 && p.family.parent_names[0][0]);
    assert(tank_add_fish(&a,0,1)==-1); /* refund slot stays reserved */
    fish_assembly_t assembly={0}; uint8_t bytes[FISH_FRAGMENTS*12]={0}; memcpy(bytes,&p,sizeof p);
    for(int i=FISH_FRAGMENTS-1;i>=0;i--) {
        fish_passport_t parsed;
        assert(!fish_gift_decode(&assembly,msg(GIFT_OFFER),&parsed));
        assert(fish_gift_fragment(&assembly,msg(1).seq,i,bytes+i*12));
    }
    fish_passport_t parsed; assert(fish_gift_decode(&assembly,msg(1),&parsed));
    assert(!fish_gift_fragment(&assembly,1,FISH_FRAGMENTS,bytes));
    assembly.bytes[13]^=1; assert(!fish_gift_decode(&assembly,msg(1),&parsed)); assembly.bytes[13]^=1;
    assert(fish_gift_receive(&b,10,msg(GIFT_COMMIT),save)==0); assert(b.n_fish==2);
    fail=true; assert(!fish_gift_accept(&b,10,msg(1),&p,save)); assert(!b.fish_gift.journal.in_seq); fail=false;
    assert(fish_gift_accept(&b,10,msg(1),&p,save)); boot(); assert(tank_add_fish(&b,0,1)==-1);
    assert(fish_gift_receive(&b,10,msg(1),save)==GIFT_ACCEPT);
    fail=true; assert(!fish_gift_receive(&a,20,msg(GIFT_ACCEPT),save)); assert(a.fish_gift.journal.out_phase==GIFT_OFFER); fail=false;
    assert(fish_gift_receive(&a,20,msg(GIFT_ACCEPT),save)==GIFT_COMMIT); boot();
    fail=true; assert(!fish_gift_receive(&b,10,msg(GIFT_COMMIT),save)); assert(b.n_fish==2 && b.fish_gift.journal.in_seq); fail=false;
    assert(fish_gift_receive(&b,10,msg(GIFT_COMMIT),save)==GIFT_DONE); boot();
    assert(b.n_fish==3 && b.fish[2].age_s==p.age && b.sd_paid_fish[2]==7);
    assert(!strcmp(b.fish[2].name,"bubbles") && b.fish[2].parent_a==-1);
    assert(!strcmp(b.fish[2].family.parent_names[0],p.family.parent_names[0]));
    assert(fish_gift_receive(&b,10,msg(GIFT_COMMIT),save)==GIFT_DONE); assert(b.n_fish==3);
    assert(fish_gift_receive(&a,20,msg(GIFT_REJECT),save)==0); assert(a.n_fish==2);
    fail=true; fish_gift_receive(&a,20,msg(GIFT_DONE),save); assert(a.fish_gift.journal.out_phase); fail=false;
    gift_msg_t old=msg(GIFT_COMMIT); fish_gift_receive(&a,20,msg(GIFT_DONE),save); boot();
    assert(!a.fish_gift.journal.out_phase);
    assert(fish_gift_offer(&b,10,2,save));
    gift_save_t *g=&b.fish_gift.journal; gift_msg_t back={1,g->out_seq,g->out_amount};
    assert(fish_gift_accept(&a,20,back,&b.fish_gift.outgoing,save));
    back.op=GIFT_ACCEPT; assert(fish_gift_receive(&b,10,back,save)==GIFT_COMMIT);
    back.op=GIFT_COMMIT; assert(fish_gift_receive(&a,20,back,save)==GIFT_DONE);
    back.op=GIFT_DONE; fish_gift_receive(&b,10,back,save); boot();
    assert(a.n_fish==3 && b.n_fish==2 && a.fish[2].parent_a>=0);
    assert(a.fish[2].family.id.serial==p.family.id.serial);
    assert(fish_gift_receive(&b,10,old,save)==GIFT_DONE); assert(b.n_fish==2); /* stale replay after return */
    assert(fish_gift_offer(&a,20,0,save));
    fail=true; fish_gift_receive(&a,20,msg(GIFT_REJECT),save); assert(a.n_fish==2); fail=false;
    fish_gift_receive(&a,20,msg(GIFT_REJECT),save); assert(a.n_fish==3);
    assert(a.fish[1].parent_a==2 || a.fish[1].parent_b==2); /* links after reordered parent returns */
    init(); while(b.n_fish<POP_CAP) assert(tank_add_fish(&b,0,1)>=0);
    assert(fish_gift_offer(&a,20,2,save)); assert(!fish_gift_accept(&b,10,msg(1),&a.fish_gift.outgoing,save));
    puts("fish: crash recovery, save rollback, fragments, consent, capacity, replay, paid history and family round trip passed");
}
