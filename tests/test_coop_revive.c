// Tests for runtime/coop_revive.c (CU-11, ADR-008 decision 15, ADR-011 decisions 33-35): the revive policy and the fallen
// state, with synthetic partners; no game and no network.
//   clang -Wall -Iruntime tests/test_coop_revive.c runtime/coop_revive.c -lm -o build/test_coop_revive
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "coop_revive.h"

#define S 1000000000ull  // a second, in ns

static int g_fail;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)

static CoopRevivePeer standing(void) {
    CoopRevivePeer p = {true, true, false, false, 0};
    return p;
}

static CoopReviveIn in_of(const CoopRevivePeer *peers, int n, uint64_t my_fell) {
    CoopReviveIn in = {true, my_fell, COOP_REVIVE_BOTH_WINDOW_DEFAULT_NS, peers, n};
    return in;
}

static void test_allowed(void) {
    CoopRevivePeer p[3];
    p[0] = standing();
    CoopReviveIn in = in_of(p, 1, 100 * S);
    CHECK(coop_revive_allowed(&in));  // (a partner on his feet: revived)
    // (the rule is off)
    in.rule_on = false;
    CHECK(!coop_revive_allowed(&in));
    in.rule_on = true;
    // (no partner at all, a bad input)
    in.npeers = 0;
    CHECK(!coop_revive_allowed(&in));
    in.npeers = -4;
    CHECK(!coop_revive_allowed(&in));
    in.npeers = 1;
    CHECK(!coop_revive_allowed(NULL));
    in.peers = NULL;
    CHECK(!coop_revive_allowed(&in));
    in.peers = p;
    // (the partner is fallen, loading, or not in the session)
    p[0].alive = false;
    CHECK(!coop_revive_allowed(&in));
    p[0] = standing(), p[0].loading = true;
    CHECK(!coop_revive_allowed(&in));
    p[0] = standing(), p[0].present = false;
    CHECK(!coop_revive_allowed(&in));
    puts("PASS: allowed only with the rule on and a partner standing");
}

// (nothing of distance, map or count is in the input: a partner standing far away or on another map is just standing)
static void test_far_and_often(void) {
    CoopRevivePeer p[1] = {standing()};
    CoopReviveIn in = in_of(p, 1, 0);
    // (revived again and again, 50 times: no limit of uses, and the same answer each time)
    for (int i = 0; i < 50; i++) {
        in.my_fell_ns = (uint64_t)(10 + i * 100) * S;
        CHECK(coop_revive_allowed(&in));
    }
    puts("PASS: no limit of distance, of map or of number of revivals");
}

static void test_both_fell(void) {
    CoopRevivePeer p[1];
    // (the partner fell 1 s before me and is still down: no one is left standing)
    p[0] = standing(), p[0].alive = false, p[0].fell = true, p[0].fell_ns = 99 * S;
    CoopReviveIn in = in_of(p, 1, 100 * S);
    CHECK(!coop_revive_allowed(&in));
    // (down at the same time, but already up again by the time it is asked: they still fell together)
    p[0].alive = true;
    CHECK(!coop_revive_allowed(&in));
    // (the window is inclusive: exactly 2 s apart is at once; 2 s and 1 ns is not)
    p[0].fell_ns = 98 * S;
    CHECK(!coop_revive_allowed(&in));
    p[0].fell_ns = 98 * S - 1;
    CHECK(coop_revive_allowed(&in));
    // (it counts the other way too: he fell 1.5 s after me)
    p[0].fell_ns = 100 * S + S / 2 * 3;
    CHECK(!coop_revive_allowed(&in));
    p[0].fell_ns = 102 * S + 1;
    CHECK(coop_revive_allowed(&in));
    // (he fell long ago, got up, and is standing: he can raise me)
    p[0].fell_ns = 10 * S;
    CHECK(coop_revive_allowed(&in));
    // (a partner that never fell counts by his flags alone, whatever his fell_ns says)
    p[0].fell = false, p[0].fell_ns = 100 * S;
    CHECK(coop_revive_allowed(&in));
    // (the window is a parameter: 0 is only the very same instant, a long one makes anything near at once)
    p[0].fell = true, p[0].fell_ns = 99 * S;
    in.both_window_ns = 0;
    CHECK(coop_revive_allowed(&in));
    p[0].fell_ns = 100 * S;
    CHECK(!coop_revive_allowed(&in));
    in.both_window_ns = 30 * S;
    p[0].fell_ns = 80 * S;
    CHECK(!coop_revive_allowed(&in));
    puts("PASS: both fallen at once is the game over of the normal game; the window is a parameter");
}

static void test_three_players(void) {
    CoopRevivePeer p[2] = {standing(), standing()};
    CoopReviveIn in = in_of(p, 2, 100 * S);
    // (one partner down, the other loading: no one)
    p[0].alive = false, p[1].loading = true;
    CHECK(!coop_revive_allowed(&in));
    // (one partner down at once with me, the other standing: revived)
    p[0].fell = true, p[0].fell_ns = 100 * S, p[1].loading = false;
    CHECK(coop_revive_allowed(&in));
    // (only the first MAX_PEERS partners are looked at)
    CoopRevivePeer many[12];
    for (int i = 0; i < 12; i++) many[i] = standing(), many[i].alive = i >= COOP_REVIVE_MAX_PEERS;
    CoopReviveIn big = in_of(many, 12, 0);
    CHECK(!coop_revive_allowed(&big));
    puts("PASS: three players, and at most COOP_REVIVE_MAX_PEERS partners looked at");
}

static void test_defaults(void) {
    CoopReviveCfg c = coop_revive_cfg_default();
    CHECK(c.countdown_ns == 30 * S && c.hold_ns == 3 * S && c.range_m == 2.0f && c.timeout_hearts == 3);
    CHECK(coop_revive_hearts(COOP_REVIVE_EV_TIMED_OUT, NULL) == 3);
    CHECK(coop_revive_hearts(COOP_REVIVE_EV_RESCUED, NULL) == 0);  // (the game's own fairy)
    CHECK(coop_revive_hearts(COOP_REVIVE_EV_NONE, NULL) == 0);
    CoopReviveCfg two = c;
    two.timeout_hearts = 2;
    CHECK(coop_revive_hearts(COOP_REVIVE_EV_TIMED_OUT, &two) == 2);
    puts("PASS: defaults 30 s, 3 s, 2 m, 3 hearts");
}

static void test_helper(void) {
    CHECK(coop_revive_helper_ok(NULL, 1.0f, true));
    CHECK(coop_revive_helper_ok(NULL, 0.0f, true));
    CHECK(coop_revive_helper_ok(NULL, 1.99f, true));
    CHECK(!coop_revive_helper_ok(NULL, 2.0f, true));  // (less than 2 m)
    CHECK(!coop_revive_helper_ok(NULL, 1.0f, false));
    CHECK(!coop_revive_helper_ok(NULL, -1.0f, true));
    CHECK(!coop_revive_helper_ok(NULL, NAN, true));
    CHECK(!coop_revive_helper_ok(NULL, INFINITY, true));
    CoopReviveCfg c = coop_revive_cfg_default();
    c.range_m = 5.0f;
    CHECK(coop_revive_helper_ok(&c, 4.0f, true) && !coop_revive_helper_ok(&c, 5.0f, true));
    puts("PASS: a partner's button counts only held, within 2 m");
}

static void test_rescue(void) {
    CoopReviveMachine m;
    coop_revive_reset(&m);
    CHECK(m.state == COOP_REVIVE_UP);
    CHECK(coop_revive_tick(&m, NULL, 5 * S, true) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_UP);  // (UP: nothing)
    coop_revive_fall(&m, 10 * S);
    CHECK(m.state == COOP_REVIVE_DOWN);
    coop_revive_fall(&m, 20 * S);  // (cannot fall twice: the countdown is from the first fall)
    CHECK(m.down_ns == 10 * S);
    CHECK(coop_revive_tick(&m, NULL, 11 * S, false) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_DOWN);
    CHECK(coop_revive_remaining_ns(&m, NULL, 11 * S) == 29 * S);
    // (a partner holds from 12 s: REVIVING, no progress yet)
    CHECK(coop_revive_tick(&m, NULL, 12 * S, true) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_REVIVING);
    CHECK(coop_revive_progress(&m, NULL) == 0.0f);
    CHECK(coop_revive_tick(&m, NULL, 13 * S + S / 2, true) == COOP_REVIVE_EV_NONE);
    CHECK(coop_revive_progress(&m, NULL) > 0.49f && coop_revive_progress(&m, NULL) < 0.51f);
    CHECK(coop_revive_tick(&m, NULL, 15 * S - 1, true) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_REVIVING);
    CHECK(coop_revive_tick(&m, NULL, 15 * S, true) == COOP_REVIVE_EV_RESCUED);  // (3 s held)
    CHECK(m.state == COOP_REVIVE_UP && coop_revive_progress(&m, NULL) == 0.0f && coop_revive_remaining_ns(&m, NULL, 15 * S) == 0);
    // (by time, not frames: the same 3 s at 30, 60, 90 and 120 FPS, to the tick)
    for (int fps = 30; fps <= 120; fps += 30) {
        coop_revive_reset(&m);
        coop_revive_fall(&m, 0);
        CHECK(coop_revive_tick(&m, NULL, 1 * S, true) == COOP_REVIVE_EV_NONE);
        int rescued_at = -1;
        for (int f = 1; f <= 5 * fps && rescued_at < 0; f++)
            if (coop_revive_tick(&m, NULL, 1 * S + (uint64_t)f * S / (uint64_t)fps, true) == COOP_REVIVE_EV_RESCUED) rescued_at = f;
        CHECK(rescued_at == 3 * fps);
    }
    puts("PASS: rescue after 3 s held, by time at 30/60/90/120 FPS");
}

static void test_partner_leaves(void) {
    CoopReviveMachine m;
    coop_revive_reset(&m);
    coop_revive_fall(&m, 0);
    coop_revive_tick(&m, NULL, 1 * S, true);
    coop_revive_tick(&m, NULL, 3 * S, true);  // (2 s of 3)
    CHECK(m.state == COOP_REVIVE_REVIVING);
    // (he moves away or lets go: the progress is lost, it is DOWN, the countdown goes on from the fall)
    CHECK(coop_revive_tick(&m, NULL, 4 * S, false) == COOP_REVIVE_EV_NONE);
    CHECK(m.state == COOP_REVIVE_DOWN && m.progress_ns == 0 && m.down_ns == 0);
    CHECK(coop_revive_remaining_ns(&m, NULL, 4 * S) == 26 * S);
    // (he comes back: it starts from 0, so 3 s more, not 1)
    CHECK(coop_revive_tick(&m, NULL, 5 * S, true) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_REVIVING);
    CHECK(coop_revive_tick(&m, NULL, 7 * S, true) == COOP_REVIVE_EV_NONE);
    CHECK(coop_revive_tick(&m, NULL, 8 * S, true) == COOP_REVIVE_EV_RESCUED);
    puts("PASS: a partner who leaves half way loses the progress, the countdown goes on");
}

static void test_timeout(void) {
    CoopReviveMachine m;
    CoopReviveCfg c = coop_revive_cfg_default();
    coop_revive_reset(&m);
    coop_revive_fall(&m, 10 * S);
    CHECK(coop_revive_tick(&m, NULL, 40 * S - 1, false) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_DOWN);
    CHECK(coop_revive_remaining_ns(&m, NULL, 40 * S - 1) == 1);
    // (30 s: revived on the spot with 3 hearts, no game over by time: DOWN -> UP)
    CoopReviveEvent ev = coop_revive_tick(&m, NULL, 40 * S, false);
    CHECK(ev == COOP_REVIVE_EV_TIMED_OUT && m.state == COOP_REVIVE_UP && coop_revive_hearts(ev, &c) == 3);
    CHECK(coop_revive_remaining_ns(&m, NULL, 40 * S) == 0);
    // (and he can fall again later, with a new countdown)
    coop_revive_fall(&m, 100 * S);
    CHECK(coop_revive_tick(&m, NULL, 129 * S, false) == COOP_REVIVE_EV_NONE);
    CHECK(coop_revive_tick(&m, NULL, 130 * S, false) == COOP_REVIVE_EV_TIMED_OUT);
    // (the time runs out while a partner is still holding, 1 s short of the rescue: revived by time)
    coop_revive_reset(&m);
    coop_revive_fall(&m, 0);
    coop_revive_tick(&m, NULL, 27 * S, true);
    CHECK(coop_revive_tick(&m, NULL, 30 * S, true) == COOP_REVIVE_EV_RESCUED);  // (3 s held at the very tick: the rescue wins)
    coop_revive_reset(&m);
    coop_revive_fall(&m, 0);
    coop_revive_tick(&m, NULL, 28 * S, true);
    CHECK(coop_revive_tick(&m, NULL, 30 * S, true) == COOP_REVIVE_EV_TIMED_OUT);  // (2 s held: the time wins)
    // (a clock before the fall, or going back, leaves things as they are)
    coop_revive_reset(&m);
    coop_revive_fall(&m, 50 * S);
    CHECK(coop_revive_tick(&m, NULL, 10 * S, false) == COOP_REVIVE_EV_NONE && m.state == COOP_REVIVE_DOWN);
    CHECK(coop_revive_remaining_ns(&m, NULL, 10 * S) == 30 * S);
    coop_revive_tick(&m, NULL, 51 * S, true);
    coop_revive_tick(&m, NULL, 52 * S, true);
    CHECK(coop_revive_tick(&m, NULL, 40 * S, true) == COOP_REVIVE_EV_NONE && m.progress_ns == 1 * S);
    // (a config of its own, and NULL machines)
    c.countdown_ns = 5 * S;
    c.hold_ns = 1 * S;
    coop_revive_reset(&m);
    coop_revive_fall(&m, 0);
    CHECK(coop_revive_tick(&m, &c, 5 * S, false) == COOP_REVIVE_EV_TIMED_OUT);
    coop_revive_reset(&m);
    coop_revive_fall(&m, 0);
    coop_revive_tick(&m, &c, 1 * S, true);
    CHECK(coop_revive_tick(&m, &c, 2 * S, true) == COOP_REVIVE_EV_RESCUED);
    coop_revive_reset(NULL);
    coop_revive_fall(NULL, 0);
    CHECK(coop_revive_tick(NULL, NULL, 0, true) == COOP_REVIVE_EV_NONE);
    CHECK(coop_revive_remaining_ns(NULL, NULL, 0) == 0 && coop_revive_progress(NULL, NULL) == 0.0f);
    puts("PASS: timeout revives on the spot with 3 hearts, rescue and time at the same tick, clock going back");
}

int main(void) {
    test_allowed();
    test_far_and_often();
    test_both_fell();
    test_three_players();
    test_defaults();
    test_helper();
    test_rescue();
    test_partner_leaves();
    test_timeout();
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    puts("PASS: coop_revive");
    return 0;
}
