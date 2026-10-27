#include "passport_startup.h"

void passport_startup_init(passport_startup_t *s, bool enabled, int64_t now)
{
    *s = (passport_startup_t){.armed=enabled, .deadline_us=now+5000000};
}

static void cancel(passport_startup_t *s, int64_t now)
{
    s->armed = false;
    s->ignore_until_us = now + 600000;
}

passport_startup_action_t passport_startup_poll(passport_startup_t *s, int64_t now, bool pending)
{
    if (!s->armed) return PASSPORT_STARTUP_WAIT;
    if (pending) { cancel(s, now); return PASSPORT_STARTUP_CANCEL; }
    if (now >= s->deadline_us) { s->armed=false; return PASSPORT_STARTUP_LAUNCH; }
    return PASSPORT_STARTUP_WAIT;
}

bool passport_startup_consume_input(passport_startup_t *s, int64_t now)
{
    if (s->armed || now < s->ignore_until_us) { cancel(s, now); return true; }
    return false;
}
