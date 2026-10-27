#include "passport_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static passport_package_t package(const char *id)
{
    passport_package_t p = {.format=1, .abi=PASSPORT_ABI, .image_size=1000};
    memcpy(p.magic, PASSPORT_MAGIC, 8);
    snprintf(p.app_id, sizeof(p.app_id), "%s", id);
    strcpy(p.name, "Test"); strcpy(p.version, "1");
    return p;
}

int main(void)
{
    passport_registry_t r;
    passport_registry_init(&r);
    assert(passport_registry_valid(&r));
    assert(!passport_should_autostart(&r, true));
    passport_package_t a = package("org.passport.test-a");
    assert(passport_install_slot(&r, &a) == 0);
    assert(passport_install_prepare(&r, &a, 0));
    /* Reset after any failed image operation leaves it unlaunchable and owned. */
    passport_registry_t recovered = r;
    assert(recovered.slots[0].state != PASSPORT_READY);
    assert(passport_install_slot(&recovered, &a) == 0);
    assert(passport_install_finish(&r, 0));
    r.last_slot = 0; r.autostart = 1;
    assert(passport_should_autostart(&r, true));
    assert(!passport_should_autostart(&r, false));
    r.launch_pending = 1;
    assert(!passport_should_autostart(&r, true));
    r.launch_pending = 0;
    /* Same ID preserves data owner, unrelated ID cannot reuse removed slots. */
    assert(passport_remove(&r, 0));
    assert(r.last_slot == PASSPORT_NO_SLOT);
    passport_package_t b = package("org.passport.test-b"), c = package("org.passport.test-c");
    assert(passport_install_slot(&r, &b) == 1);
    assert(!passport_install_prepare(&r, &b, 0));
    assert(passport_install_prepare(&r, &b, 1));
    assert(passport_install_finish(&r, 1));
    assert(passport_install_prepare(&r, &c, 2));
    assert(passport_install_finish(&r, 2));
    passport_package_t d = package("org.passport.test-d");
    assert(passport_install_slot(&r, &d) == 3);
    assert(passport_install_slot(&r, &a) == 0);
    assert(passport_install_prepare(&r, &a, 0));
    assert(passport_install_finish(&r, 0));
    assert(passport_registry_valid(&r));
    r.slots[2].package = b;
    assert(!passport_registry_valid(&r));
    /* IDs are never shortened to fit NVS keys, Unicode/path syntax is rejected. */
    a = package("../../nvs"); assert(!passport_package_valid(&a));
    a = package("org.valid"); a.app_id[63] = 'x'; assert(!passport_package_valid(&a));
    a = package("org.valid"); a.image_size = PASSPORT_SLOT_SIZE+1; assert(!passport_package_valid(&a));
    a = package("org.valid"); a.reserved[20] = 1; assert(!passport_package_valid(&a));
    passport_registry_init(&r);
    a = package("community.raw"); a.abi = 0;
    assert(passport_package_valid(&a) && passport_install_prepare(&r,&a,0));
    assert(r.slots[0].package.abi==PASSPORT_ABI && passport_registry_valid(&r));
    r.slots[0].package.abi=0;assert(!passport_registry_valid(&r));
    for (unsigned slot = 0; slot < PASSPORT_SLOTS; ++slot) {
        passport_ticket_t t = passport_make_ticket(slot);
        assert(passport_consume_ticket(&t, true) == (int)slot);
        assert(passport_consume_ticket(&t, true) == -1);
        t = passport_make_ticket(slot);
        assert(passport_consume_ticket(&t, false) == -1);
        t = passport_make_ticket(slot); t.inverse ^= 1;
        assert(passport_consume_ticket(&t, true) == -1);
    }
    passport_ticket_t t = passport_make_ticket(PASSPORT_SLOTS);
    assert(passport_consume_ticket(&t, true) == -1);
    puts("Registry ownership, recovery states, auto start, and one-shot tickets: PASS");
}
