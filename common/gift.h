/* Durable, transport-independent sand-dollar gifts. All calls on tank thread. */
#ifndef POCKET_GIFT_H
#define POCKET_GIFT_H
#include <stdbool.h>
#include <stdint.h>
#define GIFT_PEERS 8
#define GIFT_MAX 1000000
/* Fixed-size save tail; never reorder these fields. No transient radio state. */
typedef struct { uint64_t id; uint32_t received, reserved; } gift_peer_t;
typedef struct {
    uint64_t identity, out_peer, in_peer;
    uint32_t next_seq, out_seq, out_amount, out_phase;
    uint32_t in_seq, in_amount;
    gift_peer_t peers[GIFT_PEERS];
} gift_save_t;
enum { GIFT_OFFER=1, GIFT_ACCEPT, GIFT_COMMIT, GIFT_DONE, GIFT_REJECT };
typedef struct { uint8_t op; uint32_t seq, amount; } gift_msg_t;
struct tank;
/* Save callback must atomically persist both balance and journal. */
typedef bool (*gift_save_fn)(struct tank *);
bool gift_offer(struct tank *t, uint64_t peer, uint32_t amount, gift_save_fn save);
bool gift_accept(struct tank *t, uint64_t peer, gift_msg_t offer, gift_save_fn save);
bool gift_cancel(struct tank *t, gift_save_fn save); /* only before commit */
/* Returns response opcode, or 0. Incoming offers requiring consent return OFFER. */
int gift_receive(struct tank *t, uint64_t peer, gift_msg_t msg, gift_save_fn save);
bool gift_pending(const struct tank *t, uint64_t peer, gift_msg_t *msg);
#endif
