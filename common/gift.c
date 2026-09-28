#include "gift.h"
#include "tank.h"
#include <limits.h>
#include <string.h>
static int slot(gift_save_t *g, uint64_t peer) {
    for (int i=0;i<GIFT_PEERS;i++) if (g->peers[i].id==peer) return i;
    for (int i=0;i<GIFT_PEERS;i++) if (!g->peers[i].id) return i;
    return -1; /* never evict receipts: old messages must stay harmless */
}
static bool persist(tank_t *t, gift_save_t old, int32_t balance, gift_save_fn save) {
    if (save(t)) return true;
    t->gift=old; t->sd_balance=balance; return false;
}
bool gift_offer(tank_t *t, uint64_t peer, uint32_t amount, gift_save_fn save) {
    gift_save_t old=t->gift; int32_t bal=t->sd_balance;
    if (!peer || peer==old.identity || !old.identity || old.out_phase || !amount || amount>GIFT_MAX ||
        bal<0 || amount>(uint32_t)bal || old.next_seq==UINT32_MAX) return false;
    t->gift.out_peer=peer; t->gift.out_seq=++t->gift.next_seq;
    t->gift.out_amount=amount; t->gift.out_phase=GIFT_OFFER;
    t->sd_balance-=(int32_t)amount;
    return persist(t,old,bal,save);
}
bool gift_accept(tank_t *t, uint64_t peer, gift_msg_t m, gift_save_fn save) {
    gift_save_t old=t->gift; int32_t bal=t->sd_balance; int i=slot(&t->gift,peer);
    if (!peer || peer==old.identity || !old.identity || m.op!=GIFT_OFFER || !m.seq || !m.amount ||
        m.amount>GIFT_MAX || i<0 || m.seq<=old.peers[i].received || old.in_seq ||
        bal<0 || m.amount>(uint32_t)(INT32_MAX-bal)) return false;
    t->gift.peers[i].id=peer; t->gift.in_peer=peer; t->gift.in_seq=m.seq; t->gift.in_amount=m.amount;
    return persist(t,old,bal,save);
}
bool gift_cancel(tank_t *t, gift_save_fn save) {
    gift_save_t old=t->gift; int32_t bal=t->sd_balance;
    if (old.out_phase!=GIFT_OFFER || bal<0 || old.out_amount>(uint32_t)(INT32_MAX-bal)) return false;
    t->sd_balance+=(int32_t)old.out_amount; t->gift.out_phase=0;
    return persist(t,old,bal,save);
}
bool gift_pending(const tank_t *t,uint64_t peer,gift_msg_t *m) {
    if (!t->gift.out_phase || t->gift.out_peer!=peer) return false;
    *m=(gift_msg_t){t->gift.out_phase,t->gift.out_seq,t->gift.out_amount}; return true;
}
int gift_receive(tank_t *t,uint64_t peer,gift_msg_t m,gift_save_fn save) {
    gift_save_t old=t->gift; int32_t bal=t->sd_balance; int i=slot(&t->gift,peer);
    if (!peer || peer==old.identity || !m.seq || !m.amount || m.amount>GIFT_MAX) return 0;
    if (m.op==GIFT_OFFER || m.op==GIFT_COMMIT) {
        if (i<0) return GIFT_REJECT;
        if (m.seq<=old.peers[i].received) return GIFT_DONE;
        if (old.in_seq && (old.in_peer!=peer || old.in_seq!=m.seq || old.in_amount!=m.amount)) return GIFT_REJECT;
        if (m.op==GIFT_OFFER) return old.in_seq ? GIFT_ACCEPT : GIFT_OFFER;
        if (!old.in_seq || bal<0 || m.amount>(uint32_t)(INT32_MAX-bal)) return 0;
        t->sd_balance+=(int32_t)m.amount; t->gift.peers[i].id=peer;
        t->gift.peers[i].received=m.seq; t->gift.in_seq=0;
        return persist(t,old,bal,save) ? GIFT_DONE : 0;
    }
    if (old.out_phase && old.out_peer==peer && old.out_seq==m.seq && old.out_amount==m.amount) {
        if (m.op==GIFT_ACCEPT) {
            t->gift.out_phase=GIFT_COMMIT;
            return persist(t,old,bal,save) ? GIFT_COMMIT : 0;
        }
        if (m.op==GIFT_DONE && old.out_phase==GIFT_COMMIT) {
            t->gift.out_phase=0; persist(t,old,bal,save);
        }
        if (m.op==GIFT_REJECT && old.out_phase==GIFT_OFFER) gift_cancel(t,save);
    }
    return 0;
}
