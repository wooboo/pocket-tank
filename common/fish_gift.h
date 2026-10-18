/* Fish passports and a durable escrow. Wire v1 uses little-endian IEEE floats. */
#ifndef POCKET_FISH_GIFT_H
#define POCKET_FISH_GIFT_H
#include "gift.h"
typedef struct { uint64_t tank, serial; } fish_id_t;
typedef struct {
    fish_id_t id, parents[2];
    char parent_names[2][8];
    uint32_t parent_body[2], parent_accent[2];
} fish_family_t;
typedef struct {
    fish_family_t family;
    char name[8];
    uint32_t preset, stage, color, fin, accent, eaten, eaten_player, ms_bits, ms_seen, paid;
    float age, size, base_size, turn_rate, trust, bold, sociable, lazy, bold0, sociable0;
    float hunger, energy, stress, curiosity, drift_acc, rest_dx, rest_dy;
    uint32_t reserved;
} fish_passport_t;
/* Append-only save tail; independent receipt stream from money gifts. */
typedef struct {
    gift_save_t journal;
    fish_passport_t outgoing, incoming;
    uint64_t next_fish;
    fish_family_t family[6];
} fish_gift_save_t;
#define FISH_PACKET_BYTES 200
#define FISH_FRAGMENTS ((FISH_PACKET_BYTES+11)/12)
typedef struct { uint32_t seq, mask; uint8_t bytes[FISH_FRAGMENTS*12]; } fish_assembly_t;
bool fish_gift_fragment(fish_assembly_t *a, uint32_t seq, unsigned index, const uint8_t data[12]);
bool fish_gift_decode(const fish_assembly_t *a, gift_msg_t m, fish_passport_t *p);
uint32_t fish_gift_hash(const fish_passport_t *p);
bool fish_gift_offer(struct tank *t,uint64_t peer,int idx,gift_save_fn save);
bool fish_gift_accept(struct tank *t,uint64_t peer,gift_msg_t m,const fish_passport_t *p,gift_save_fn save);
int fish_gift_receive(struct tank *t,uint64_t peer,gift_msg_t m,gift_save_fn save);
void fish_gift_identities(struct tank *t);
#endif
