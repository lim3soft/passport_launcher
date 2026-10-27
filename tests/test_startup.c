#include "passport_startup.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    passport_startup_t s;
    passport_startup_init(&s,false,0);
    assert(passport_startup_poll(&s,6000000,false)==PASSPORT_STARTUP_WAIT);
    assert(!passport_startup_consume_input(&s,6000000));
    passport_startup_init(&s,true,1000000);
    assert(passport_startup_poll(&s,5999999,false)==PASSPORT_STARTUP_WAIT);
    assert(passport_startup_poll(&s,6000000,true)==PASSPORT_STARTUP_CANCEL);
    assert(!s.armed && passport_startup_poll(&s,6000001,false)==PASSPORT_STARTUP_WAIT);
    assert(passport_startup_consume_input(&s,6100000)); /* press */
    assert(passport_startup_consume_input(&s,6200000)); /* release */
    assert(passport_startup_consume_input(&s,6400000)); /* delayed click */
    assert(!passport_startup_consume_input(&s,7000000));
    passport_startup_init(&s,true,0);
    assert(passport_startup_poll(&s,5200000,true)==PASSPORT_STARTUP_CANCEL); /* task was delayed */
    passport_startup_init(&s,true,0);
    assert(passport_startup_poll(&s,5000000,false)==PASSPORT_STARTUP_LAUNCH);
    assert(passport_startup_poll(&s,5000001,false)==PASSPORT_STARTUP_WAIT);
    assert(!passport_startup_consume_input(&s,5000002));
    passport_startup_init(&s,true,0);
    assert(passport_startup_consume_input(&s,1000000)); /* input while queue wait completed */
    assert(!s.armed);
    passport_startup_init(&s,true,0);
    assert(passport_startup_poll(&s,5100000,true)==PASSPORT_STARTUP_CANCEL);
    /* These old callbacks may be dequeued much later by a delayed UI task. */
    assert(passport_startup_consume_input(&s,4900000));
    assert(passport_startup_consume_input(&s,5000000));
    assert(passport_startup_consume_input(&s,5200000));
    assert(!passport_startup_consume_input(&s,6000000)); /* genuinely new input */
    puts("Startup control: queued input wins at deadline, delayed task, gesture suppression, one launch: PASS");
}
