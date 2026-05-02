#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/
#include "input/input.h"

/* -------------------------------------------------------------------------
 * Minimal simulation harness – mirrors the gui.c state machine exactly.
 * ------------------------------------------------------------------------- */

#define SIM_MAX_EVENTS 64

typedef struct {
    InputType events[SIM_MAX_EVENTS];
    int       count;
} EventLog;

typedef struct {
    bool     held_down;
    uint64_t held_time;
    bool     long_fired;
} SimKeyState;

static EventLog g_log;
static SimKeyState g_key;

static void log_event(InputType t) {
    assert(g_log.count < SIM_MAX_EVENTS);
    g_log.events[g_log.count++] = t;
}

/* Called once when a physical key is pressed */
static void sim_key_down(void) {
    if(g_key.held_down) return; /* ignore SDL key-repeat */
    g_key.held_down  = true;
    g_key.held_time  = 0;
    g_key.long_fired = false;
    log_event(InputTypePress);
}

/* Called on every 1 ms tick while any key is held (mirrors input_loop) */
static void sim_tick(void) {
    if(!g_key.held_down) return;

    if(g_key.held_time % INPUT_PRESS_TICKS == 0 && g_key.held_time != 0) {
        uint32_t presses = (uint32_t)(g_key.held_time / INPUT_PRESS_TICKS);
        if(presses == INPUT_LONG_PRESS_COUNTS) {
            g_key.long_fired = true;
            log_event(InputTypeLong);
        } else if(presses > INPUT_LONG_PRESS_COUNTS) {
            log_event(InputTypeRepeat);
        }
        /* presses < INPUT_LONG_PRESS_COUNTS: still in short window, do nothing */
    }
    g_key.held_time++;
}

/* Called when the physical key is released */
static void sim_key_up(void) {
    if(!g_key.held_down) return;
    g_key.held_down = false;

    if(!g_key.long_fired) {
        log_event(InputTypeShort);
    }
    g_key.long_fired = false;
    g_key.held_time  = 0;
    log_event(InputTypeRelease);
}

/* Helper: tick n times */
static void tick_n(uint64_t n) {
    for(uint64_t i = 0; i < n; i++) sim_tick();
}

static void reset(void) {
    memset(&g_log, 0, sizeof(g_log));
    memset(&g_key, 0, sizeof(g_key));
}

/* -------------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------------- */

static void test_short_press(void) {
    reset();
    sim_key_down();
    tick_n(INPUT_PRESS_TICKS - 1); /* just before long-press threshold */
    sim_key_up();

    assert(g_log.count == 3);
    assert(g_log.events[0] == InputTypePress);
    assert(g_log.events[1] == InputTypeShort);
    assert(g_log.events[2] == InputTypeRelease);
    printf("[PASS] test_short_press\n");
}

static void test_immediate_release(void) {
    reset();
    sim_key_down();
    sim_key_up(); /* released with zero ticks elapsed */

    assert(g_log.count == 3);
    assert(g_log.events[0] == InputTypePress);
    assert(g_log.events[1] == InputTypeShort);
    assert(g_log.events[2] == InputTypeRelease);
    printf("[PASS] test_immediate_release\n");
}

static void test_long_press(void) {
    reset();
    sim_key_down();
    /* +1: the check fires when held_time == threshold, which requires that
     * many prior increments, so we need one extra tick call. */
    tick_n((uint64_t)INPUT_LONG_PRESS_COUNTS * INPUT_PRESS_TICKS + 1);
    sim_key_up();

    assert(g_log.count == 3);
    assert(g_log.events[0] == InputTypePress);
    assert(g_log.events[1] == InputTypeLong);
    assert(g_log.events[2] == InputTypeRelease);
    printf("[PASS] test_long_press\n");
}

static void test_long_then_repeat(void) {
    reset();
    sim_key_down();
    /* +1: same off-by-one reasoning as test_long_press */
    tick_n((uint64_t)(INPUT_LONG_PRESS_COUNTS + 1) * INPUT_PRESS_TICKS + 1);
    sim_key_up();

    assert(g_log.count == 4);
    assert(g_log.events[0] == InputTypePress);
    assert(g_log.events[1] == InputTypeLong);
    assert(g_log.events[2] == InputTypeRepeat);
    assert(g_log.events[3] == InputTypeRelease);
    printf("[PASS] test_long_then_repeat\n");
}

static void test_multiple_repeats(void) {
    const int extra_repeats = 3;
    reset();
    sim_key_down();
    tick_n((uint64_t)(INPUT_LONG_PRESS_COUNTS + extra_repeats) * INPUT_PRESS_TICKS + 1);
    sim_key_up();

    /* events: Press, Long, Repeat×extra_repeats, Release */
    int expected = 1 + 1 + extra_repeats + 1;
    assert(g_log.count == expected);
    assert(g_log.events[0] == InputTypePress);
    assert(g_log.events[1] == InputTypeLong);
    for(int i = 0; i < extra_repeats; i++) {
        assert(g_log.events[2 + i] == InputTypeRepeat);
    }
    assert(g_log.events[expected - 1] == InputTypeRelease);
    printf("[PASS] test_multiple_repeats\n");
}

static void test_no_short_after_long(void) {
    reset();
    sim_key_down();
    tick_n((uint64_t)INPUT_LONG_PRESS_COUNTS * INPUT_PRESS_TICKS + 1 + 5);
    sim_key_up();

    for(int i = 0; i < g_log.count; i++) {
        assert(g_log.events[i] != InputTypeShort);
    }
    printf("[PASS] test_no_short_after_long\n");
}

static void test_two_short_presses(void) {
    reset();
    for(int press = 0; press < 2; press++) {
        sim_key_down();
        tick_n(10);
        sim_key_up();
    }

    assert(g_log.count == 6);
    /* first press */
    assert(g_log.events[0] == InputTypePress);
    assert(g_log.events[1] == InputTypeShort);
    assert(g_log.events[2] == InputTypeRelease);
    /* second press */
    assert(g_log.events[3] == InputTypePress);
    assert(g_log.events[4] == InputTypeShort);
    assert(g_log.events[5] == InputTypeRelease);
    printf("[PASS] test_two_short_presses\n");
}

/* -------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */
int main(void) {
    printf("=== input events tests ===\n");
    test_short_press();
    test_immediate_release();
    test_long_press();
    test_long_then_repeat();
    test_multiple_repeats();
    test_no_short_after_long();
    test_two_short_presses();
    printf("All input event tests passed.\n");
    return 0;
}
