#include "tank.h"
#include "progression.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char blob[8192];
static size_t blob_len;
static bool fail_save;
bool persist_port_load(void *buf,size_t max,size_t *got) {
    if(!blob_len || blob_len>max) return false;
    memcpy(buf,blob,blob_len); *got=blob_len; return true;
}
bool persist_port_save(const void *buf,size_t len) {
    if(fail_save) return false;
    assert(len<=sizeof blob); memcpy(blob,buf,len); blob_len=len; return true;
}
bool persist_port_erase(void) { blob_len=0; return true; }
int64_t clock_port_now_unix(void) { return 0; }
const char *version_port_string(void) { return "test"; }
int main(void) {
    tank_t a,b; tank_init(&a,123); progression_boot(&a);
    a.sd_balance=802; a.sd_earned=1412; a.gift.identity=42;
    assert(gift_offer(&a,99,20,progression_save));
    tank_init(&b,456); progression_boot(&b);
    assert(b.sd_balance==782 && b.sd_earned==1412 && b.gift.out_phase==GIFT_OFFER && b.gift.out_amount==20);
    assert(b.gift.identity==42 && b.gift.out_peer==99);
    assert(!strcmp(a.fish[0].name,b.fish[0].name));
    /* Pre-BLE saves ended immediately before the appended journal. */
    blob_len-=sizeof(gift_save_t);
    tank_init(&b,789); progression_boot(&b);
    assert(b.sd_balance==782 && b.sd_earned==1412 && b.gift.identity==0 && !b.gift.out_phase);
    fail_save=true; assert(!progression_save(&b));
    puts("progression: gift journal round-trip, legacy save migration and failed save passed");
}
