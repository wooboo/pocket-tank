#include "fish_gift.h"
#include "tank.h"
#include "progression.h"
#include <string.h>
#include <limits.h>
_Static_assert(sizeof(fish_passport_t)==FISH_PACKET_BYTES,"passport wire/save layout");
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Fish wire v1 requires little endian
#endif
static bool same(fish_id_t a,fish_id_t b) { return a.tank==b.tank && a.serial==b.serial; }
static int slot(const gift_save_t *g,uint64_t peer) {
    for(int i=0;i<GIFT_PEERS;i++) if(g->peers[i].id==peer) return i;
    for(int i=0;i<GIFT_PEERS;i++) if(!g->peers[i].id) return i;
    return -1;
}
uint32_t fish_gift_hash(const fish_passport_t *p) {
    const uint8_t *b=(const uint8_t *)p; uint32_t crc=~0u;
    for(unsigned i=0;i<sizeof *p;i++) { crc^=b[i]; for(int j=0;j<8;j++) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1))); }
    return ~crc;
}
static bool number(float f,float lo,float hi) {
    uint32_t bits; memcpy(&bits,&f,4); /* finite even with firmware -ffast-math */
    return (bits&0x7f800000u)!=0x7f800000u && f>=lo && f<=hi;
}
static bool valid(const fish_passport_t *p) {
    if(!p->family.id.tank || !p->family.id.serial || !p->name[0] || p->name[7] ||
       p->family.parent_names[0][7] || p->family.parent_names[1][7] || p->reserved ||
       p->preset>=(unsigned)tank_roster_count() || p->stage>STAGE_ELDER ||
       p->color>0xffffff || p->fin>0xffffff || p->accent>0xffffff ||
       p->eaten>INT32_MAX || p->eaten_player>p->eaten) return false;
    for(int i=0;i<7 && p->name[i];i++) if((unsigned char)p->name[i]<32 || (unsigned char)p->name[i]>126) return false;
    return number(p->age,0,1e10f) && number(p->size,.01f,10) && number(p->base_size,.01f,10) &&
        number(p->turn_rate,0,100) && number(p->trust,0,10) && number(p->bold,0,1) &&
        number(p->sociable,0,1) && number(p->lazy,0,1) && number(p->bold0,0,1) && number(p->sociable0,0,1) &&
        number(p->hunger,0,10) && number(p->energy,0,10) && number(p->stress,0,10) && number(p->curiosity,0,10) &&
        number(p->drift_acc,0,1e10f) && number(p->rest_dx,-1000,1000) && number(p->rest_dy,-1000,1000);
}
bool fish_gift_fragment(fish_assembly_t *a,uint32_t seq,unsigned index,const uint8_t data[12]) {
    if(!seq || index>=FISH_FRAGMENTS) return false;
    if(a->seq!=seq) { memset(a,0,sizeof *a); a->seq=seq; }
    memcpy(a->bytes+index*12,data,12); a->mask|=1u<<index; return true;
}
bool fish_gift_decode(const fish_assembly_t *a,gift_msg_t m,fish_passport_t *p) {
    if(a->seq!=m.seq || a->mask!=((1u<<FISH_FRAGMENTS)-1)) return false;
    memcpy(p,a->bytes,sizeof *p); return fish_gift_hash(p)==m.amount && valid(p);
}
void fish_gift_identities(tank_t *t) {
    if(!t->gift.identity) return;
    for(int i=0;i<t->n_fish;i++) if(!t->fish[i].family.id.tank && t->fish_gift.next_fish<UINT64_MAX)
        t->fish[i].family.id=(fish_id_t){t->gift.identity,++t->fish_gift.next_fish};
    for(int i=0;i<t->n_fish;i++) {
        fish_t *f=&t->fish[i]; int parents[]={f->parent_a,f->parent_b};
        for(int j=0;j<2;j++) if(parents[j]>=0 && parents[j]<t->n_fish) {
            const fish_t *p=&t->fish[parents[j]];
            f->family.parents[j]=p->family.id; memcpy(f->family.parent_names[j],p->name,8);
            f->family.parent_body[j]=p->color; f->family.parent_accent[j]=p->accent;
        }
    }
}
static fish_passport_t passport(const tank_t *t,int i) {
    const fish_t *f=&t->fish[i];
    fish_passport_t p={.family=f->family,.paid=t->sd_paid_fish[i],.age=f->age_s};
    memcpy(p.name,f->name,8);
#define COPY(x) p.x=f->x
    COPY(preset); COPY(stage); COPY(color); COPY(fin); COPY(accent); COPY(eaten); COPY(eaten_player);
    COPY(ms_bits); COPY(ms_seen); COPY(size); COPY(base_size); COPY(turn_rate); COPY(trust); COPY(bold);
    COPY(sociable); COPY(lazy); COPY(bold0); COPY(sociable0); COPY(hunger); COPY(energy); COPY(stress);
    COPY(curiosity); COPY(drift_acc); COPY(rest_dx); COPY(rest_dy);
#undef COPY
    return p;
}
static void relink(tank_t *t) {
    for(int i=0;i<t->n_fish;i++) {
        fish_t *f=&t->fish[i]; f->parent_a=f->parent_b=-1;
        for(int j=0;j<t->n_fish;j++) if(j!=i) {
            if(same(f->family.parents[0],t->fish[j].family.id)) f->parent_a=j;
            if(same(f->family.parents[1],t->fish[j].family.id)) f->parent_b=j;
        }
    }
    t->roster_generation++;
}
static void append(tank_t *t,const fish_passport_t *p) {
    int i=t->n_fish; tank_make_fish(t,i,p->preset,p->sociable,p->bold,p->stage);
    fish_t *f=&t->fish[i]; f->family=p->family; f->age_s=p->age; t->sd_paid_fish[i]=p->paid;
    memcpy(f->name,p->name,8);
#define COPY(x) f->x=p->x
    COPY(color); COPY(fin); COPY(accent); COPY(eaten); COPY(eaten_player); COPY(ms_bits); COPY(ms_seen);
    COPY(size); COPY(base_size); COPY(turn_rate); COPY(trust); COPY(lazy); COPY(bold0); COPY(sociable0);
    COPY(hunger); COPY(energy); COPY(stress); COPY(curiosity); COPY(drift_acc); COPY(rest_dx); COPY(rest_dy);
#undef COPY
    t->n_fish++; relink(t);
}
/* Only fields changed by this module. Failed NVS commits restore the live tank. */
typedef struct {
    fish_gift_save_t gift; fish_t fish[N_FISH_MAX]; uint32_t paid[N_FISH_MAX],rng,generation; int n;
} before_t;
static before_t before(const tank_t *t) {
    before_t b={.gift=t->fish_gift,.rng=t->rng,.generation=t->roster_generation,.n=t->n_fish};
    memcpy(b.fish,t->fish,sizeof b.fish); memcpy(b.paid,t->sd_paid_fish,sizeof b.paid); return b;
}
static bool persist(tank_t *t,const before_t *b,gift_save_fn save) {
    if(save && save(t)) { t->courting=false; t->spawning=false; t->court_a=t->court_b=-1; t->court_active=0; return true; }
    t->fish_gift=b->gift; t->rng=b->rng; t->n_fish=b->n; t->roster_generation=b->generation;
    memcpy(t->fish,b->fish,sizeof b->fish); memcpy(t->sd_paid_fish,b->paid,sizeof b->paid); return false;
}
bool fish_gift_offer(tank_t *t,uint64_t peer,int idx,gift_save_fn save) {
    gift_save_t *g=&t->fish_gift.journal;
    if(!peer || peer==t->gift.identity || !t->gift.identity || idx<0 || idx>=t->n_fish || t->n_fish<=2 ||
       g->out_phase || g->in_seq || g->next_seq==UINT32_MAX || t->gift.out_phase || t->gift.in_seq) return false;
    before_t b=before(t); fish_gift_identities(t);
    fish_passport_t p=passport(t,idx);
    if(!valid(&p)) { persist(t,&b,NULL); return false; }
    g->out_peer=peer; g->out_seq=++g->next_seq; g->out_amount=fish_gift_hash(&p); g->out_phase=GIFT_OFFER;
    t->fish_gift.outgoing=p;
    for(int i=idx;i<t->n_fish-1;i++) { t->fish[i]=t->fish[i+1]; t->sd_paid_fish[i]=t->sd_paid_fish[i+1]; }
    t->n_fish--; t->sd_paid_fish[t->n_fish]=0; relink(t);
    return persist(t,&b,save);
}
bool fish_gift_accept(tank_t *t,uint64_t peer,gift_msg_t m,const fish_passport_t *p,gift_save_fn save) {
    gift_save_t *g=&t->fish_gift.journal; int i=slot(g,peer);
    if(!peer || peer==t->gift.identity || !t->gift.identity || m.op!=GIFT_OFFER || !m.seq ||
       !valid(p) || m.amount!=fish_gift_hash(p) || i<0 || m.seq<=g->peers[i].received ||
       g->in_seq || g->out_phase || t->gift.out_phase || t->gift.in_seq || t->n_fish>=POP_CAP) return false;
    for(int j=0;j<t->n_fish;j++) if(same(t->fish[j].family.id,p->family.id)) return false;
    before_t b=before(t); fish_gift_identities(t);
    g->peers[i].id=peer; g->in_peer=peer; g->in_seq=m.seq; g->in_amount=m.amount; t->fish_gift.incoming=*p;
    return persist(t,&b,save);
}
int fish_gift_receive(tank_t *t,uint64_t peer,gift_msg_t m,gift_save_fn save) {
    gift_save_t *g=&t->fish_gift.journal; int i=slot(g,peer);
    if(!peer || peer==t->gift.identity || !m.seq) return 0;
    if(m.op==GIFT_OFFER || m.op==GIFT_COMMIT) {
        if(i<0) return GIFT_REJECT;
        if(m.seq<=g->peers[i].received) return GIFT_DONE;
        if(g->out_phase || (g->in_seq && (g->in_peer!=peer || g->in_seq!=m.seq || g->in_amount!=m.amount))) return GIFT_REJECT;
        if(m.op==GIFT_OFFER) {
            if(t->gift.out_phase || t->gift.in_seq) return GIFT_REJECT;
            return g->in_seq ? GIFT_ACCEPT : GIFT_OFFER;
        }
        if(!g->in_seq || t->n_fish>=POP_CAP) return 0;
        before_t b=before(t); append(t,&t->fish_gift.incoming);
        g->peers[i].received=m.seq; g->in_seq=0;
        return persist(t,&b,save) ? GIFT_DONE : 0;
    }
    if(!g->out_phase || g->out_peer!=peer || g->out_seq!=m.seq || g->out_amount!=m.amount) return 0;
    before_t b=before(t);
    if(m.op==GIFT_ACCEPT) { g->out_phase=GIFT_COMMIT; return persist(t,&b,save) ? GIFT_COMMIT : 0; }
    if(m.op==GIFT_DONE && g->out_phase==GIFT_COMMIT) { g->out_phase=0; persist(t,&b,save); }
    if(m.op==GIFT_REJECT && g->out_phase==GIFT_OFFER && t->n_fish<N_FISH_MAX) {
        append(t,&t->fish_gift.outgoing); g->out_phase=0; persist(t,&b,save);
    }
    return 0;
}
