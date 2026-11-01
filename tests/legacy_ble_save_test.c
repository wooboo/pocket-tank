/* Real saves from both devices before merging upstream 0.3.6 with BLE. */
#include "tank.h"
#include "progression.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char blob[8192];
static size_t blob_len;
static tank_t a, b;
bool persist_port_load(void *buf, size_t max, size_t *got) {
    assert(blob_len <= max); memcpy(buf, blob, blob_len); *got = blob_len; return true;
}
bool persist_port_save(const void *buf, size_t len) {
    assert(len <= sizeof blob); memcpy(blob, buf, len); blob_len = len; return true;
}
bool persist_port_erase(void) { return false; }
int64_t clock_port_now_unix(void) { return 0; }
const char *version_port_string(void) { return "migration-test"; }
static void check(const char *path, const char *names[4], int balance) {
    FILE *f = fopen(path, "rb"); assert(f);
    blob_len = fread(blob, 1, sizeof blob, f); fclose(f); assert(blob_len == 2896);
    gift_save_t gift; fish_gift_save_t fish_gift;
    memcpy(&gift, blob + 1656, sizeof gift);
    memcpy(&fish_gift, blob + 1656 + sizeof gift, sizeof fish_gift);
    tank_init(&a, 1); progression_boot(&a);
    assert(a.n_fish == 4 && a.sd_balance == balance);
    assert(!memcmp(&a.gift, &gift, sizeof gift));
    assert(!memcmp(&a.fish_gift, &fish_gift, sizeof fish_gift));
    assert(a.shrimp_n == 0 && !a.autofeed_off && !a.tools_locked);
    for (int i = 0; i < 4; i++) {
        assert(!strcmp(a.fish[i].name, names[i]));
        assert(a.fish[i].age_s > 100 * 3600);
        assert(a.fish[i].family.id.tank && a.fish[i].family.id.serial);
    }
    assert(progression_save(&a)); assert(blob_len > 2896 && blob_len <= 4000);
    tank_init(&b, 2); progression_boot(&b);
    assert(b.sd_balance == balance && b.n_fish == a.n_fish);
    assert(!memcmp(&a.gift, &b.gift, sizeof gift));
    assert(!memcmp(&a.fish_gift, &b.fish_gift, sizeof fish_gift));
    for (int i = 0; i < 4; i++) {
        assert(!strcmp(a.fish[i].name, b.fish[i].name));
        assert(a.fish[i].age_s == b.fish[i].age_s);
        assert(a.fish[i].color == b.fish[i].color && a.fish[i].accent == b.fish[i].accent);
        assert(a.sd_paid_fish[i] == b.sd_paid_fish[i]);
    }
    /* The deployed 0.3.6 fork moved the journals to 1912; its padding
     * at 1908 now belongs to upstream's shelf and must not become flags. */
    unsigned char merged[8192]; memcpy(merged, blob, blob_len);
    const size_t journals = sizeof gift + sizeof fish_gift;
    memcpy(blob + 1912, merged + 1952, journals);
    memset(blob + 1908, 0xa5, 4); /* old structure padding is not sd_shelved */
    blob_len = 1912 + journals;
    tank_init(&b, 3); progression_boot(&b);
    assert(b.n_fish == a.n_fish && b.sd_balance == balance);
    assert(!memcmp(&b.gift, &gift, sizeof gift));
    assert(!memcmp(&b.fish_gift, &fish_gift, sizeof fish_gift));
    assert(b.sd_shelved == 0 && b.sd_locked == 0 && b.cr_found == 0);
    assert(progression_save(&b));
    tank_init(&a, 4); progression_boot(&a);
    assert(!memcmp(&a.gift, &gift, sizeof gift));
    assert(!memcmp(&a.fish_gift, &fish_gift, sizeof fish_gift));
    printf("PASS: %s, four fish, %d dollars, identities/journals and merged-save round trip\n", path, balance);
}
int main(void) {
    const char *first[] = {"zori", "chip", "dolt", "nori"};
    const char *second[] = {"sol", "gira", "help", "misa"};
    check("tests/fixtures/2026-10-18-ble-4E-C4.sav", first, 624);
    check("tests/fixtures/2026-10-18-ble-4C-F8.sav", second, 823);
}
