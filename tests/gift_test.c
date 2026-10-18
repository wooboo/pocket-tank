#include "tank.h"
#include "gift.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tank_t a,b,da,db;
static bool fail_save;
static bool save(tank_t *t) {
    if(fail_save) return false;
    if(t==&a) da=*t; else { assert(t==&b); db=*t; } return true;
}
static void fresh(void) {
    memset(&a,0,sizeof a); memset(&b,0,sizeof b);
    a.gift.identity=11; b.gift.identity=22; a.sd_balance=100; b.sd_balance=25;
    fail_save=false; save(&a); save(&b);
}
static gift_msg_t pending(void) { gift_msg_t m; assert(gift_pending(&a,22,&m)); return m; }
static void reboot(void) { a=da; b=db; }
static void transfer(int cut) {
    fresh(); assert(gift_offer(&a,22,10,save)); if(cut==0) reboot();
    gift_msg_t m=pending(); assert(gift_receive(&b,11,m,save)==GIFT_OFFER);
    assert(gift_accept(&b,11,m,save)); if(cut==1) reboot();
    assert(gift_receive(&b,11,m,save)==GIFT_ACCEPT);
    m.op=GIFT_ACCEPT; assert(gift_receive(&a,22,m,save)==GIFT_COMMIT); if(cut==2) reboot();
    m=pending(); assert(m.op==GIFT_COMMIT);
    assert(gift_receive(&b,11,m,save)==GIFT_DONE); if(cut==3) reboot();
    assert(gift_receive(&b,11,m,save)==GIFT_DONE); assert(b.sd_balance==35);
    m.op=GIFT_DONE; gift_receive(&a,22,m,save); if(cut==4) reboot();
    assert(a.sd_balance==90 && b.sd_balance==35 && !a.gift.out_phase && !b.gift.in_seq);
    assert(a.sd_earned==0 && b.sd_earned==0);
    /* delayed replay after another transaction */
    gift_msg_t old=m; old.op=GIFT_COMMIT;
    assert(gift_offer(&a,22,10,save)); m=pending(); assert(gift_accept(&b,11,m,save));
    m.op=GIFT_ACCEPT; assert(gift_receive(&a,22,m,save)==GIFT_COMMIT);
    m.op=GIFT_COMMIT; assert(gift_receive(&b,11,m,save)==GIFT_DONE);
    assert(gift_receive(&b,11,old,save)==GIFT_DONE); assert(b.sd_balance==45);
}
int main(void) {
    for(int cut=0;cut<5;cut++) transfer(cut);
    fresh(); fail_save=true; assert(!gift_offer(&a,22,10,save)); assert(a.sd_balance==100 && !a.gift.out_phase);
    fail_save=false; assert(gift_offer(&a,22,10,save)); gift_msg_t m=pending();
    fail_save=true; assert(!gift_accept(&b,11,m,save)); assert(!b.gift.in_seq);
    fail_save=false; assert(gift_accept(&b,11,m,save)); m.op=GIFT_ACCEPT;
    fail_save=true; assert(!gift_receive(&a,22,m,save)); assert(a.gift.out_phase==GIFT_OFFER);
    fail_save=false; assert(gift_receive(&a,22,m,save)==GIFT_COMMIT); m.op=GIFT_COMMIT;
    fail_save=true; assert(!gift_receive(&b,11,m,save)); assert(b.sd_balance==25 && b.gift.in_seq);
    fail_save=false; assert(gift_receive(&b,11,m,save)==GIFT_DONE); m.op=GIFT_DONE;
    fail_save=true; gift_receive(&a,22,m,save); assert(a.gift.out_phase==GIFT_COMMIT);
    fail_save=false; gift_receive(&a,22,m,save); assert(!a.gift.out_phase);
    fresh(); assert(!gift_offer(&a,22,101,save)); assert(!gift_offer(&a,11,10,save)); assert(!gift_offer(&a,22,0,save));
    assert(gift_offer(&a,22,10,save)); m=pending(); m.op=GIFT_COMMIT;
    assert(!gift_receive(&b,11,m,save)); assert(b.sd_balance==25); /* no unsolicited credit */
    m.op=GIFT_REJECT; gift_receive(&a,22,m,save); assert(a.sd_balance==100 && !a.gift.out_phase);
    fresh(); for(int i=0;i<GIFT_PEERS;i++) b.gift.peers[i].id=100+i;
    m=(gift_msg_t){GIFT_OFFER,1,10}; assert(!gift_accept(&b,11,m,save));
    puts("gift: crash boundaries, save failures, replays, consent, limits and refunds passed");
}
