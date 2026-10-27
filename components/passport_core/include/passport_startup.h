#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool armed;
    int64_t deadline_us, ignore_until_us;
} passport_startup_t;
typedef enum { PASSPORT_STARTUP_WAIT, PASSPORT_STARTUP_CANCEL, PASSPORT_STARTUP_LAUNCH } passport_startup_action_t;

void passport_startup_init(passport_startup_t *state, bool enabled, int64_t now_us);
/* Pending input wins even if it is first observed at/after the deadline. */
passport_startup_action_t passport_startup_poll(passport_startup_t *state, int64_t now_us, bool input_pending);
/* Use callback event time, not dequeue time, so a delayed UI task cannot turn
 * a cancellation CLICK into an unintended manual app launch. */
bool passport_startup_consume_input(passport_startup_t *state, int64_t event_time_us);
