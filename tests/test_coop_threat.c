// Tests for runtime/coop_threat.c (CU-15, CA-1): synthetic enemies and blows, no game and no network.
//   clang -Wall -Iruntime tests/test_coop_threat.c runtime/coop_threat.c -lm -o build/test_coop_threat
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "coop_threat.h"

#define S 1000000000ull  // a second, in ns

static int g_fail;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static const CoopThreatCfg k_still = {0, 1.5f, 2 * S};  // (no fading: exact sums for the pick tests)

static void set(CoopThreat *t, float a0, float a1) {
    coop_threat_reset(t);
    t->threat[0] = a0, t->threat[1] = a1;
}

static void test_defaults(void) {
    CoopThreatCfg c = coop_threat_cfg_default();
    CHECK(c.half_life_ns == 8 * S);
    CHECK(c.margin == 1.5f);
    CHECK(c.pending_ns == 2 * S);
    CoopThreat t;
    memset(&t, 0xab, sizeof t);
    coop_threat_reset(&t);
    for (int i = 0; i < COOP_THREAT_SLOTS; i++) CHECK(t.threat[i] == 0.0f);
    CHECK(!t.seen && !t.timed && t.npending == 0);
    puts("PASS: defaults (8 s, 1.5, 2 s) and reset");
}

static void test_clear_leader(void) {
    CoopThreat t;
    bool can[COOP_THREAT_SLOTS] = {true, true};
    int to = 99;
    // (owner 0 hurt it by 10, slot 1 by 15: 15 >= 1.5 x 10)
    set(&t, 10, 15);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_TO && to == 1);
    set(&t, 10, 14.9f);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    // (the owner leads clearly: he keeps it, whoever is nearer)
    set(&t, 30, 10);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_KEEP && to == 0);
    // (no one hurt it: today's rule)
    set(&t, 0, 0);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    // (a lone hitter leads clearly over nothing)
    set(&t, 0, 1);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_TO && to == 1);
    // (no owner, -1 or out of range like OWNER_NONE: to the leader if he may have it)
    set(&t, 0, 20);
    CHECK(coop_threat_pick(&t, &k_still, can, -1, &to) == COOP_THREAT_TO && to == 1);
    CHECK(coop_threat_pick(&t, &k_still, can, 0xff, &to) == COOP_THREAT_TO && to == 1);
    // (the third slot counts as the second: 20 vs 14 is no clear lead)
    can[2] = true;
    set(&t, 0, 20);
    t.threat[2] = 14;
    CHECK(coop_threat_pick(&t, &k_still, can, 2, &to) == COOP_THREAT_NEAREST);
    t.threat[2] = 13;
    CHECK(coop_threat_pick(&t, &k_still, can, 2, &to) == COOP_THREAT_TO && to == 1);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, NULL) == COOP_THREAT_TO);
    CHECK(coop_threat_pick(NULL, &k_still, can, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    puts("PASS: clear leader (1.5x), keep, no threat, no owner, third slot, NULL");
}

static void test_tie(void) {
    CoopThreat t;
    bool can[COOP_THREAT_SLOTS] = {true, true};
    int to;
    set(&t, 10, 10);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    CHECK(coop_threat_pick(&t, &k_still, can, 1, &to) == COOP_THREAT_NEAREST);
    // (a margin of 1, or a broken one, still gives no leader on a tie)
    CoopThreatCfg one = k_still, bad = k_still;
    one.margin = 1.0f, bad.margin = NAN;
    CHECK(coop_threat_pick(&t, &one, can, 0, &to) == COOP_THREAT_NEAREST);
    CHECK(coop_threat_pick(&t, &bad, can, 0, &to) == COOP_THREAT_NEAREST);
    set(&t, 10, 10.5f);
    CHECK(coop_threat_pick(&t, &one, can, 0, &to) == COOP_THREAT_TO && to == 1);
    CHECK(coop_threat_pick(&t, &bad, can, 0, &to) == COOP_THREAT_TO && to == 1);
    puts("PASS: tie (nearness decides, any margin)");
}

static void test_owner_cannot(void) {
    CoopThreat t;
    bool can[COOP_THREAT_SLOTS] = {false, true};
    int to;
    // (the owner leads but may not have it any more: today's "owner gone" rule decides)
    set(&t, 30, 10);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    // (he still counts as the second: 12 vs his 10 is no clear lead)
    set(&t, 10, 12);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_NEAREST);
    set(&t, 10, 15);
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_TO && to == 1);
    // (no one may have it)
    CHECK(coop_threat_pick(&t, &k_still, NULL, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    puts("PASS: owner who may no longer own it");
}

static void test_beyond_80m(void) {
    CoopThreat t;
    coop_threat_reset(&t);
    bool can[COOP_THREAT_SLOTS] = {true, false};  // (slot 1 shoots from 90 m)
    int to;
    coop_threat_life(&t, &k_still, 0, 1000, 0);
    for (int i = 0; i < 25; i++) coop_threat_blow(&t, &k_still, 1, 5, 1000 - 5 * i, (uint64_t)i * S / 2);
    CHECK(t.threat[1] == 125.0f);  // (his damage counts and grows)
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_NEAREST && to == -1);
    CHECK(coop_threat_pick(&t, &k_still, can, -1, &to) == COOP_THREAT_NEAREST);
    can[1] = true;  // (he comes within 80 m with it still high)
    CHECK(coop_threat_pick(&t, &k_still, can, 0, &to) == COOP_THREAT_TO && to == 1);
    puts("PASS: a player beyond 80 m gains threat but not the enemy");
}

static void test_half_life(void) {
    const CoopThreatCfg *def = NULL;  // (the defaults: 8 s)
    CoopThreat t;
    coop_threat_reset(&t);
    coop_threat_blow(&t, def, 1, 100, 1000, 1 * S);
    CHECK(t.threat[1] == 100.0f);
    coop_threat_decay(&t, def, 9 * S);
    CHECK(NEAR(t.threat[1], 50.0, 0.5));  // (half, within 1 %)
    coop_threat_decay(&t, def, 17 * S);
    CHECK(NEAR(t.threat[1], 25.0, 0.25));
    // (by time, not frames: decayed every frame at 30, 60, 90 or 120 FPS for 8 s, the same half)
    for (int fps = 30; fps <= 120; fps += 30) {
        coop_threat_reset(&t);
        coop_threat_blow(&t, def, 0, 100, 1000, 0);
        for (int f = 1; f <= 8 * fps; f++) coop_threat_decay(&t, def, (uint64_t)f * S / (uint64_t)fps);
        CHECK(NEAR(t.threat[0], 50.0, 0.5));
    }
    // (a clock that goes back, or the same time twice, changes nothing)
    coop_threat_reset(&t);
    coop_threat_blow(&t, def, 0, 100, 1000, 10 * S);
    coop_threat_decay(&t, def, 5 * S);
    coop_threat_decay(&t, def, 10 * S);
    CHECK(t.threat[0] == 100.0f);
    // (blows fade from their own time: 100 at 0 s and 100 at 8 s give 150 at 8 s)
    coop_threat_reset(&t);
    coop_threat_blow(&t, def, 0, 100, 1000, 0);
    coop_threat_blow(&t, def, 0, 100, 900, 8 * S);
    CHECK(NEAR(t.threat[0], 150.0, 1.5));
    // (once below half a point it is 0: nearness decides again)
    coop_threat_reset(&t);
    coop_threat_blow(&t, def, 1, 10, 1000, 0);
    coop_threat_decay(&t, def, 40 * S);
    CHECK(t.threat[1] == 0.0f);
    bool can[COOP_THREAT_SLOTS] = {true, true};
    CHECK(coop_threat_pick(&t, def, can, 0, NULL) == COOP_THREAT_NEAREST);
    // (a half life of 0: no fading)
    coop_threat_reset(&t);
    coop_threat_blow(&t, &k_still, 1, 10, 1000, 0);
    coop_threat_decay(&t, &k_still, 1000 * S);
    CHECK(t.threat[1] == 10.0f);
    puts("PASS: half life of 8 s, by time at 30/60/90/120 FPS");
}

static void test_pending(void) {
    CoopThreat t;
    coop_threat_reset(&t);
    // (slot 1 hits for 5: the 5 the life then loses are his, not the owner's)
    coop_threat_life(&t, &k_still, 0, 100, 0);
    coop_threat_blow(&t, &k_still, 1, 5, 100, S / 10);
    coop_threat_life(&t, &k_still, 0, 95, S / 2);
    CHECK(t.threat[0] == 0.0f && t.threat[1] == 5.0f && t.npending == 0);
    // (a drop bigger than the pending: the rest is the owner's)
    coop_threat_blow(&t, &k_still, 1, 5, 95, 1 * S);
    coop_threat_life(&t, &k_still, 0, 87, 1 * S + S / 5);
    CHECK(t.threat[0] == 3.0f && t.threat[1] == 10.0f && t.npending == 0);
    // (expired: a HIT whose drop is not seen within 2 s explains nothing after)
    coop_threat_blow(&t, &k_still, 1, 5, 87, 2 * S);
    coop_threat_life(&t, &k_still, 0, 82, 4 * S);  // (exactly 2 s: expired)
    CHECK(t.threat[0] == 8.0f && t.threat[1] == 15.0f && t.npending == 0);
    coop_threat_blow(&t, &k_still, 1, 5, 82, 5 * S);
    coop_threat_life(&t, &k_still, 0, 77, 7 * S - 1);  // (just before: still his)
    CHECK(t.threat[0] == 8.0f && t.threat[1] == 20.0f);
    // (oldest first: two blows, then a drop the size of the first, then the second expires)
    coop_threat_blow(&t, &k_still, 1, 4, 77, 10 * S);
    coop_threat_blow(&t, &k_still, 2, 6, 77, 11 * S);
    CHECK(t.npending == 2);
    coop_threat_life(&t, &k_still, 0, 73, 11 * S + S / 2);
    CHECK(t.npending == 1 && t.pending[0].damage == 6);
    coop_threat_life(&t, &k_still, 0, 67, 13 * S);  // (11 s + 2 s: the 6 expired)
    CHECK(t.threat[0] == 14.0f && t.npending == 0);
    // (with no owner the rest is no one's)
    coop_threat_life(&t, &k_still, -1, 60, 14 * S);
    float sum = 0;
    for (int i = 0; i < COOP_THREAT_SLOTS; i++) sum += t.threat[i];
    CHECK(sum == 14.0f + 24.0f + 6.0f);  // (owner 0; slot 1: 20 + 4; slot 2: 6)
    // (more blows than room: they go into the newest, none is lost)
    coop_threat_reset(&t);
    coop_threat_life(&t, &k_still, 0, 1000, 0);
    for (int i = 0; i < COOP_THREAT_PENDING + 3; i++) coop_threat_blow(&t, &k_still, 1, 2, 1000, (uint64_t)i * S / 100);
    CHECK(t.npending == COOP_THREAT_PENDING);
    coop_threat_life(&t, &k_still, 0, 1000 - 2 * (COOP_THREAT_PENDING + 3), S);
    CHECK(t.threat[0] == 0.0f && t.npending == 0);
    puts("PASS: pending HIT damage, its 2 s expiry, oldest first, and a full pending");
}

static void test_clamped(void) {
    CoopThreat t;
    coop_threat_reset(&t);
    // (a blow is worth no more than the life it has left)
    coop_threat_blow(&t, &k_still, 1, 50, 10, 0);
    CHECK(t.threat[1] == 10.0f);
    // (nor than what is left once the pending blows take theirs: a stream of HITs before the life drops)
    for (int i = 0; i < 100; i++) coop_threat_blow(&t, &k_still, 1, 50, 10, (uint64_t)i * S / 1000);
    CHECK(t.threat[1] == 10.0f);
    coop_threat_blow(&t, &k_still, 2, 50, 10, S / 2);
    CHECK(t.threat[2] == 0.0f);
    // (negative, nothing, a dead enemy, a bad slot)
    coop_threat_reset(&t);
    coop_threat_blow(&t, &k_still, 1, -40, 100, 0);
    coop_threat_blow(&t, &k_still, 1, 0, 100, 0);
    coop_threat_blow(&t, &k_still, 1, 5, 0, 0);
    coop_threat_blow(&t, &k_still, 1, 5, -1, 0);
    coop_threat_blow(&t, &k_still, -1, 5, 100, 0);
    coop_threat_blow(&t, &k_still, COOP_THREAT_SLOTS, 5, 100, 0);
    coop_threat_blow(&t, &k_still, 1, INT32_MIN, 100, 0);
    for (int i = 0; i < COOP_THREAT_SLOTS; i++) CHECK(t.threat[i] == 0.0f);
    CHECK(t.npending == 0);
    // (the biggest blows still sum to a finite, bounded threat)
    coop_threat_reset(&t);
    for (int i = 0; i < 1000; i++) coop_threat_blow(&t, &k_still, 3, INT32_MAX, INT32_MAX, (uint64_t)i * 3 * S);
    CHECK(isfinite(t.threat[3]) && t.threat[3] <= COOP_THREAT_MAX && t.threat[3] > 0);
    puts("PASS: damage clamped to the life left, never negative, bounded");
}

static void test_life_rise(void) {
    CoopThreat t;
    coop_threat_reset(&t);
    // (the first life seen only sets it)
    coop_threat_life(&t, &k_still, 0, 100, 0);
    CHECK(t.seen && t.life == 100 && t.threat[0] == 0.0f);
    // (a rise counts for no one; the next drop is from the risen life)
    coop_threat_life(&t, &k_still, 0, 120, S);
    CHECK(t.threat[0] == 0.0f && t.life == 120);
    coop_threat_life(&t, &k_still, 0, 110, 2 * S);
    CHECK(t.threat[0] == 10.0f);
    // (a negative life, gone from that game, is not seen)
    coop_threat_life(&t, &k_still, 0, -1, 3 * S);
    CHECK(t.life == 110 && t.threat[0] == 10.0f);
    // (the owner out of range is no one)
    coop_threat_life(&t, &k_still, COOP_THREAT_SLOTS, 100, 4 * S);
    CHECK(t.life == 100 && t.threat[0] == 10.0f);
    puts("PASS: a life rise is ignored; first and negative lives");
}

static void test_forget(void) {
    CoopThreat t;
    bool can[COOP_THREAT_SLOTS] = {true, true};
    int to;
    // (slot 1 led, then died: his threat is 0, nearness decides; on reviving he starts from 0)
    set(&t, 10, 40);
    coop_threat_forget_slot(&t, 1);
    CHECK(t.threat[1] == 0.0f && t.threat[0] == 10.0f);
    CHECK(coop_threat_pick(&t, &k_still, can, 1, &to) == COOP_THREAT_TO && to == 0);
    coop_threat_forget_slot(&t, -1);
    coop_threat_forget_slot(&t, COOP_THREAT_SLOTS);
    coop_threat_forget_slot(NULL, 0);
    CHECK(t.threat[0] == 10.0f);
    puts("PASS: forget a slot (dead, loading, other map, disconnected)");
}

// CU-15's main flow, end to end: A (slot 0) owns it and fights it; B (slot 1) shoots it from 25 m.
static void test_flow(void) {
    const CoopThreatCfg *def = NULL;
    CoopThreat t;
    coop_threat_reset(&t);
    bool can[COOP_THREAT_SLOTS] = {true, true};
    int life = 300, to, owner = 0;
    coop_threat_life(&t, def, owner, life, 0);
    // (A 3 a second, B 1 a second: A keeps it)
    for (int s = 1; s <= 30; s++) {
        uint64_t now = (uint64_t)s * S;
        coop_threat_blow(&t, def, 1, 1, life, now);
        life -= 1 + 3;
        coop_threat_life(&t, def, owner, life, now + S / 10);
        CHECK(coop_threat_pick(&t, def, can, owner, &to) != COOP_THREAT_TO);
    }
    CHECK(t.threat[0] > 2.5f * t.threat[1]);
    // (then B 6 a second, A 1: B leads clearly within some seconds and takes it)
    int handed = -1;
    for (int s = 31; s <= 60 && handed < 0; s++) {
        uint64_t now = (uint64_t)s * S;
        coop_threat_blow(&t, def, 1, 6, life, now);
        life -= 6 + 1;
        coop_threat_life(&t, def, owner, life, now + S / 10);
        if (coop_threat_pick(&t, def, can, owner, &to) == COOP_THREAT_TO) handed = s, owner = to;
    }
    CHECK(owner == 1 && handed > 31 && handed <= 42);
    // (now B owns it and A's blows come as HITs: B keeps it while he does more)
    for (int s = handed + 1; s <= handed + 10; s++) {
        uint64_t now = (uint64_t)s * S;
        coop_threat_blow(&t, def, 0, 1, life, now);
        life -= 1 + 6;
        coop_threat_life(&t, def, owner, life, now + S / 10);
        CHECK(coop_threat_pick(&t, def, can, owner, &to) == COOP_THREAT_KEEP && to == 1);
    }
    // (no one hits it: the threat dies out and nearness decides again)
    coop_threat_decay(&t, def, (uint64_t)(handed + 10 + 120) * S);
    CHECK(coop_threat_pick(&t, def, can, owner, &to) == COOP_THREAT_NEAREST);
    puts("PASS: CU-15 main flow (melee owner, archer takes it, keeps it, fades)");
}

int main(void) {
    test_defaults();
    test_clear_leader();
    test_tie();
    test_owner_cannot();
    test_beyond_80m();
    test_half_life();
    test_pending();
    test_clamped();
    test_life_rise();
    test_forget();
    test_flow();
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    puts("PASS: coop_threat");
    return 0;
}
