// Tests for runtime/coop_pvp.c (CU-12; T433): synthetic players and blows, no game and no network.
//   clang -Wall -Iruntime tests/test_coop_pvp.c runtime/coop_pvp.c -lm -o build/test_coop_pvp
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "coop_pvp.h"

#define MS 1000000ull     // a millisecond, in ns
#define S 1000000000ull   // a second, in ns
#define TWO 0x3u          // slots 0 and 1 present
#define THREE 0x7u        // slots 0, 1 and 2

static int g_fail;
#define CHECK(c)                                                                \
    do {                                                                        \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) <= 1e-4)

#define CTL(who, op) coop_pvp_ctl(&st, (who), COOP_PVP_##op, 0)
#define R(r) COOP_PVP_CTL_##r
#define W(w) COOP_PVP_HIT_##w

// (every player present proposes or accepts: 0 proposes, the others accept)
static void duel(CoopPvp *st, uint32_t present) {
    coop_pvp_reset(st);
    coop_pvp_present(st, present);
    coop_pvp_ctl(st, 0, COOP_PVP_PROPOSE, 0);
    for (int i = 1; i < COOP_PVP_SLOTS; i++)
        if (present & (1u << i)) coop_pvp_ctl(st, i, COOP_PVP_ACCEPT, 0);
}

// (slot a hits slot v standing 1.5 m in front of him, he facing +z, with a weapon of reach 3 m and power 20)
static CoopPvpHit blow(int a, int v, uint16_t seq, uint64_t now_ns) {
    CoopPvpHit h;
    memset(&h, 0, sizeof h);
    h.attacker = a, h.victim = v, h.seq = seq;
    h.to[2] = 1.5f;
    h.reach = 3.0f, h.power = 20;
    h.attacker_alive = h.victim_alive = true;
    h.same_map = true;
    h.now_ns = now_ns;
    return h;
}

// (the same with a blow number newer than any before, a second after the last one: no repeat, no rate)
static uint16_t g_seq;
static CoopPvpHit fresh(int a, int v) {
    g_seq++;
    return blow(a, v, g_seq, (uint64_t)g_seq * S);
}

static CoopPvpWhy why_of(CoopPvp *st, CoopPvpHit h) {
    CoopPvpVerdict v;
    coop_pvp_validate(st, &h, &v);
    return v.why;
}

static void test_reset(void) {
    CoopPvp st;
    memset(&st, 0xab, sizeof st);
    coop_pvp_reset(&st);
    CHECK(st.phase == COOP_PVP_OFF && st.by == -1);
    CHECK(st.allowed && !st.no_duel_zone);  // (the host's rule on by default, as T409 proposes)
    CHECK(st.present == 0 && st.accepted == 0);
    for (int i = 0; i < COOP_PVP_SLOTS; i++) CHECK(!st.atk[i].seen && st.atk[i].ntaken == 0 && st.atk[i].next == 0);
    // (presence beyond the slots is dropped)
    coop_pvp_present(&st, 0xffffffffu);
    CHECK(st.present == 0xffu);
    puts("PASS: reset (off, allowed, no zone, no one) and presence mask");
}

static void test_agreement(void) {
    CoopPvp st;
    coop_pvp_reset(&st);
    // (alone, there is no one to propose to; someone not in the session proposes nothing)
    coop_pvp_present(&st, 0x1u);
    CHECK(CTL(0, PROPOSE) == R(ALONE) && st.phase == COOP_PVP_OFF);
    CHECK(CTL(1, PROPOSE) == R(ABSENT) && st.phase == COOP_PVP_OFF);
    coop_pvp_present(&st, TWO);
    CHECK(CTL(1, ACCEPT) == R(NO_PROPOSAL) && st.phase == COOP_PVP_OFF);
    // (anyone proposes, P4-6's default; it takes the other one)
    CHECK(CTL(0, PROPOSE) == R(OK) && st.phase == COOP_PVP_PROPOSED && st.by == 0 && st.accepted == 0x1u);
    CHECK(CTL(1, PROPOSE) == R(BUSY) && st.by == 0);
    CHECK(CTL(0, ACCEPT) == R(OWN) && st.phase == COOP_PVP_PROPOSED);
    CHECK(CTL(1, ACCEPT) == R(OK) && st.phase == COOP_PVP_ON && st.accepted == TWO);
    CHECK(CTL(1, ACCEPT) == R(ALREADY) && st.phase == COOP_PVP_ON);
    CHECK(CTL(0, ACCEPT) == R(ALREADY));
    CHECK(CTL(0, PROPOSE) == R(BUSY) && st.phase == COOP_PVP_ON);
    // (either turns it off, at once, without asking; off already, it stays off)
    CHECK(CTL(1, TURN_OFF) == R(OK) && st.phase == COOP_PVP_OFF && st.by == -1 && st.accepted == 0);
    CHECK(CTL(1, TURN_OFF) == R(OK) && st.phase == COOP_PVP_OFF);
    // (the other proposes too; declining is turning it off)
    CHECK(CTL(1, PROPOSE) == R(OK) && st.by == 1);
    CHECK(CTL(0, TURN_OFF) == R(OK) && st.phase == COOP_PVP_OFF);
    // (a slot not in the session accepts nothing, but turning off works from any slot)
    CHECK(CTL(0, PROPOSE) == R(OK));
    CHECK(CTL(5, ACCEPT) == R(ABSENT) && st.phase == COOP_PVP_PROPOSED);
    CHECK(CTL(5, TURN_OFF) == R(OK) && st.phase == COOP_PVP_OFF);
    // (slots out of range, unknown ops, no state)
    CHECK(CTL(-1, TURN_OFF) == R(BAD) && CTL(8, PROPOSE) == R(BAD));
    CHECK(coop_pvp_ctl(&st, 0, (CoopPvpOp)7, 0) == R(BAD) && st.phase == COOP_PVP_OFF);
    CHECK(coop_pvp_ctl(NULL, 0, COOP_PVP_PROPOSE, 0) == R(BAD));
    puts("PASS: agreement (propose, accept by the other, own, again, busy, turn off by anyone, decline, absent)");
}

static void test_timeout(void) {
    CoopPvp st;
    coop_pvp_reset(&st);
    coop_pvp_present(&st, TWO);
    CHECK(coop_pvp_ctl(&st, 0, COOP_PVP_PROPOSE, 100 * S) == R(OK));
    CHECK(!coop_pvp_tick(&st, 100 * S + COOP_PVP_ASK_NS - 1) && st.phase == COOP_PVP_PROPOSED);
    CHECK(!coop_pvp_tick(&st, 50 * S) && st.phase == COOP_PVP_PROPOSED);  // (a clock gone back drops nothing)
    // (CA-8: no answer in 20 s, still off)
    CHECK(coop_pvp_tick(&st, 100 * S + COOP_PVP_ASK_NS) && st.phase == COOP_PVP_OFF);
    CHECK(!coop_pvp_tick(&st, 1000 * S));
    // (a duel on never times out)
    duel(&st, TWO);
    CHECK(!coop_pvp_tick(&st, 1000 * S) && st.phase == COOP_PVP_ON);
    CHECK(!coop_pvp_tick(NULL, 0));
    puts("PASS: a proposal unanswered for 20 s is dropped; a duel is not");
}

static void test_three(void) {
    CoopPvp st;
    // (more than two: it takes every player present)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, THREE);
    CHECK(CTL(0, PROPOSE) == R(OK));
    CHECK(CTL(1, ACCEPT) == R(OK) && st.phase == COOP_PVP_PROPOSED);
    CHECK(CTL(2, ACCEPT) == R(OK) && st.phase == COOP_PVP_ON && st.accepted == THREE);
    // (the one who had not accepted leaves: those left all did)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, THREE);
    CTL(0, PROPOSE);
    CTL(1, ACCEPT);
    coop_pvp_present(&st, TWO);
    CHECK(st.phase == COOP_PVP_ON && st.accepted == TWO);
    // (the proposer leaves: his proposal goes with him)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, THREE);
    CTL(0, PROPOSE);
    CTL(1, ACCEPT);
    coop_pvp_present(&st, 0x6u);
    CHECK(st.phase == COOP_PVP_OFF && st.by == -1);
    // (a third comes in during a duel: he is not in it, neither his blows nor theirs on him count, until he accepts)
    duel(&st, TWO);
    coop_pvp_present(&st, THREE);
    CHECK(st.phase == COOP_PVP_ON && st.accepted == TWO);
    CHECK(why_of(&st, fresh(2, 0)) == W(NOT_IN_DUEL));
    CHECK(why_of(&st, fresh(0, 2)) == W(NOT_IN_DUEL));
    CHECK(why_of(&st, fresh(0, 1)) == W(OK));
    CHECK(CTL(2, ACCEPT) == R(OK) && st.phase == COOP_PVP_ON && st.accepted == THREE);
    CHECK(why_of(&st, fresh(2, 0)) == W(OK));
    // (one of three leaves: the other two go on; then one more: off)
    coop_pvp_present(&st, TWO);
    CHECK(st.phase == COOP_PVP_ON && st.accepted == TWO);
    coop_pvp_present(&st, 0x1u);
    CHECK(st.phase == COOP_PVP_OFF);
    puts("PASS: three players (all must accept, leaving, a late comer joins by accepting)");
}

static void test_forbidden(void) {
    CoopPvp st;
    CoopPvpVerdict v;
    // (the host forbids it during a duel: it ends, and no blow does damage)
    duel(&st, TWO);
    CHECK(coop_pvp_set_allowed(&st, false) && st.phase == COOP_PVP_OFF && !st.allowed);
    CoopPvpHit h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.damage == 0 && v.why == W(OFF));
    // (proposing and accepting are refused)
    CHECK(CTL(0, PROPOSE) == R(FORBIDDEN) && st.phase == COOP_PVP_OFF);
    CHECK(CTL(1, ACCEPT) == R(FORBIDDEN));
    // (even a duel set on behind the setter's back does no damage)
    st.phase = COOP_PVP_ON, st.accepted = TWO;
    h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(FORBIDDEN));
    // (a pending proposal ends too; forbidding again, or allowing, ends nothing and opens nothing)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, TWO);
    CTL(0, PROPOSE);
    CHECK(coop_pvp_set_allowed(&st, false) && st.phase == COOP_PVP_OFF);
    CHECK(!coop_pvp_set_allowed(&st, false));
    CHECK(!coop_pvp_set_allowed(&st, true) && st.phase == COOP_PVP_OFF && st.allowed);
    CHECK(CTL(0, PROPOSE) == R(OK));
    CHECK(!coop_pvp_set_allowed(NULL, false));
    puts("PASS: PvP forbidden by the host (no proposing or accepting, a duel ends, no damage)");
}

static void test_death(void) {
    CoopPvp st;
    // (the victim dies in a duel: off; nothing else of the state changes, no item is anywhere here)
    duel(&st, TWO);
    CHECK(why_of(&st, fresh(0, 1)) == W(OK));
    CoopPvp before = st;
    CHECK(coop_pvp_on_death(&st, 1) && st.phase == COOP_PVP_OFF && st.by == -1 && st.accepted == 0);
    CHECK(st.allowed == before.allowed && st.no_duel_zone == before.no_duel_zone && st.present == before.present);
    CHECK(!memcmp(st.atk, before.atk, sizeof st.atk));
    CHECK(why_of(&st, fresh(0, 1)) == W(OFF));
    CHECK(!coop_pvp_on_death(&st, 1));  // (already off)
    // (after the revive they may duel again; the attacker's death ends it as well)
    CHECK(CTL(1, PROPOSE) == R(OK) && CTL(0, ACCEPT) == R(OK) && st.phase == COOP_PVP_ON);
    CHECK(coop_pvp_on_death(&st, 0) && st.phase == COOP_PVP_OFF);
    // (someone not in the duel dies: it goes on)
    duel(&st, TWO);
    coop_pvp_present(&st, THREE);
    CHECK(!coop_pvp_on_death(&st, 2) && st.phase == COOP_PVP_ON);
    CHECK(!coop_pvp_on_death(&st, -1) && !coop_pvp_on_death(&st, 8) && !coop_pvp_on_death(NULL, 0));
    // (a proposal is no duel)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, TWO);
    CTL(0, PROPOSE);
    CHECK(!coop_pvp_on_death(&st, 0) && st.phase == COOP_PVP_PROPOSED);
    puts("PASS: death in a duel (off, nothing else touched, can duel again)");
}

static void test_zone(void) {
    CoopPvp st;
    CoopPvpVerdict v;
    // (in a shrine or a cutscene: no proposing, no accepting)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, TWO);
    coop_pvp_set_zone(&st, true);
    CHECK(CTL(0, PROPOSE) == R(ZONE) && st.phase == COOP_PVP_OFF);
    coop_pvp_set_zone(&st, false);
    CHECK(CTL(0, PROPOSE) == R(OK));
    coop_pvp_set_zone(&st, true);
    CHECK(CTL(1, ACCEPT) == R(ZONE) && st.phase == COOP_PVP_PROPOSED);
    // (a duel opened outside stays on, but no blow does damage in there)
    duel(&st, TWO);
    coop_pvp_set_zone(&st, true);
    CHECK(st.phase == COOP_PVP_ON);
    CoopPvpHit h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(ZONE));
    coop_pvp_set_zone(&st, false);
    h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, &h, &v) == 20 && v.why == W(OK));
    // (turning off still works there)
    coop_pvp_set_zone(&st, true);
    CHECK(CTL(1, TURN_OFF) == R(OK) && st.phase == COOP_PVP_OFF);
    coop_pvp_set_zone(NULL, true);
    puts("PASS: no-duel zone (no proposing or accepting, no damage, turning off works)");
}

static void test_rejections(void) {
    CoopPvp st;
    CoopPvpVerdict v;
    CoopPvpHit h;
    duel(&st, TWO);
    h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, &h, &v) == 20 && v.damage == 20 && v.why == W(OK));
    // (bad input)
    CHECK(why_of(&st, fresh(-1, 1)) == W(BAD));
    CHECK(why_of(&st, fresh(0, 8)) == W(BAD));
    CHECK(why_of(&st, fresh(1, 1)) == W(BAD));
    h = fresh(0, 1), h.from[0] = NAN;
    CHECK(why_of(&st, h) == W(BAD));
    h = fresh(0, 1), h.to[1] = INFINITY;
    CHECK(why_of(&st, h) == W(BAD));
    h = fresh(0, 1), h.attacker_yaw = NAN;
    CHECK(why_of(&st, h) == W(BAD));
    h = fresh(0, 1), h.reach = -INFINITY;
    CHECK(why_of(&st, h) == W(BAD));
    h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, NULL, &v) == 0 && v.why == W(BAD));
    CHECK(coop_pvp_validate(NULL, &h, &v) == 0 && v.why == W(BAD));
    // (dead or down, either)
    h = fresh(0, 1), h.attacker_alive = false;
    CHECK(why_of(&st, h) == W(DEAD));
    h = fresh(0, 1), h.victim_alive = false;
    CHECK(why_of(&st, h) == W(DEAD));
    // (another map)
    h = fresh(0, 1), h.same_map = false;
    CHECK(why_of(&st, h) == W(MAP));
    // (loading, either)
    h = fresh(0, 1), h.attacker_loading = true;
    CHECK(why_of(&st, h) == W(LOADING));
    h = fresh(0, 1), h.victim_loading = true;
    CHECK(why_of(&st, h) == W(LOADING));
    // (distance: reach 3 + 2 m, in 3D; CA-5: 100 m)
    h = fresh(0, 1), h.to[2] = 5.0f;
    CHECK(why_of(&st, h) == W(OK));
    h = fresh(0, 1), h.to[2] = 5.01f;
    CHECK(why_of(&st, h) == W(FAR));
    h = fresh(0, 1), h.to[1] = 3.0f, h.to[2] = 4.0f;
    CHECK(why_of(&st, h) == W(OK));
    h = fresh(0, 1), h.to[1] = 3.0f, h.to[2] = 4.1f;
    CHECK(why_of(&st, h) == W(FAR));
    h = fresh(0, 1), h.to[2] = 100.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(FAR));
    CHECK(!strcmp(coop_pvp_why_name(v.why), "distance"));
    // (a reach out of bounds: no more than 4 m, no less than 0)
    h = fresh(0, 1), h.reach = 100.0f, h.to[2] = 6.0f;
    CHECK(why_of(&st, h) == W(OK));
    h = fresh(0, 1), h.reach = 100.0f, h.to[2] = 6.5f;
    CHECK(why_of(&st, h) == W(FAR));
    h = fresh(0, 1), h.reach = -5.0f, h.to[2] = 2.0f;
    CHECK(why_of(&st, h) == W(OK));
    h = fresh(0, 1), h.reach = -5.0f, h.to[2] = 2.5f;
    CHECK(why_of(&st, h) == W(FAR));
    // (no weapon)
    h = fresh(0, 1), h.power = 0;
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(WEAPON));
    // (no duel)
    CTL(0, TURN_OFF);
    h = fresh(0, 1);
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(OFF));
    // (a rejected blow says no damage and no push)
    CHECK(v.damage == 0 && v.push[0] == 0.0f && v.push[1] == 0.0f);
    puts("PASS: each rejection (bad, dead, map, loading, distance, reach bounds, weapon, off)");
}

static void test_damage(void) {
    CoopPvp st;
    CoopPvpHit h;
    duel(&st, TWO);
    // (the damage is the power the host found for the equipped weapon, 120 at most, never negative; CA-4, CA-7)
    static const struct { int power, damage; } k[] = {
        {1, 1}, {20, 20}, {119, 119}, {120, 120}, {121, 120}, {30000, 120}, {INT_MAX, 120},
        {-1, 0}, {-30000, 0}, {INT_MIN, 0},
    };
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) {
        h = fresh(0, 1), h.power = k[i].power;
        int d = coop_pvp_validate(&st, &h, NULL);
        CHECK(d == k[i].damage);
        CHECK(d >= 0 && d <= COOP_PVP_DAMAGE_MAX);
    }
    puts("PASS: damage = min(power, 120), never negative");
}

static void test_repeat(void) {
    CoopPvp st;
    CoopPvpHit h;
    duel(&st, TWO);
    CHECK(why_of(&st, blow(0, 1, 10, 1 * S)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 10, 2 * S)) == W(REPEAT));  // (the same blow again)
    CHECK(why_of(&st, blow(0, 1, 9, 3 * S)) == W(REPEAT));   // (an older one)
    CHECK(why_of(&st, blow(0, 1, 0, 4 * S)) == W(REPEAT));   // (none)
    CHECK(why_of(&st, blow(1, 0, 10, 5 * S)) == W(OK));      // (each attacker numbers his own)
    // (a blow turned down is used up all the same: sent again later, it still does nothing)
    h = blow(0, 1, 11, 6 * S), h.same_map = false;
    CHECK(why_of(&st, h) == W(MAP));
    CHECK(why_of(&st, blow(0, 1, 11, 7 * S)) == W(REPEAT));
    // (numbers wrap around 16 bits)
    CHECK(why_of(&st, blow(0, 1, 30000, 8 * S)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 60000, 9 * S)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 65535, 10 * S)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 1, 11 * S)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 65535, 12 * S)) == W(REPEAT));
    // (a player who left and came back numbers his blows anew; one who stayed does not)
    CHECK(why_of(&st, blow(1, 0, 1, 13 * S)) == W(REPEAT));
    coop_pvp_present(&st, 0x1u);
    CHECK(st.phase == COOP_PVP_OFF);  // (CA-10: he left, the duel is off)
    coop_pvp_present(&st, TWO);
    CHECK(st.phase == COOP_PVP_OFF);
    CTL(1, PROPOSE);
    CTL(0, ACCEPT);
    CHECK(why_of(&st, blow(1, 0, 1, 14 * S)) == W(OK));
    puts("PASS: blow numbers (repeat, older, 0, used up when turned down, wrap, anew after leaving)");
}

static void test_burst(void) {
    CoopPvp st;
    CoopPvpHit h;
    int taken = 0;
    // (10 blows in a second: 4 taken)
    duel(&st, TWO);
    for (int i = 0; i < 10; i++) {
        h = blow(0, 1, (uint16_t)(i + 1), (uint64_t)i * 100 * MS);
        CoopPvpWhy w = why_of(&st, h);
        taken += w == W(OK);
        CHECK(i < 4 ? w == W(OK) : w == W(RATE));
    }
    CHECK(taken == 4);
    // (the other attacker has his own count)
    CHECK(why_of(&st, blow(1, 0, 1, 900 * MS)) == W(OK));
    // (a second after the first one taken, one more; then a second after the second one, and so on)
    CHECK(why_of(&st, blow(0, 1, 11, 1000 * MS)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 12, 1050 * MS)) == W(RATE));
    CHECK(why_of(&st, blow(0, 1, 13, 1100 * MS)) == W(OK));
    // (CA-6: 50 blows in a second, 4 taken)
    duel(&st, TWO);
    taken = 0;
    for (int i = 0; i < 50; i++) taken += why_of(&st, blow(0, 1, (uint16_t)(i + 1), (uint64_t)i * 20 * MS)) == W(OK);
    CHECK(taken == 4);
    // (blows turned down for another reason do not count)
    duel(&st, TWO);
    for (int i = 0; i < 6; i++) {
        h = blow(0, 1, (uint16_t)(i + 1), (uint64_t)i * 10 * MS), h.to[2] = 50.0f;
        CHECK(why_of(&st, h) == W(FAR));
    }
    taken = 0;
    for (int i = 0; i < 4; i++)
        taken += why_of(&st, blow(0, 1, (uint16_t)(i + 7), (60 + (uint64_t)i * 10) * MS)) == W(OK);
    CHECK(taken == 4);
    // (a clock gone back counts as within the second)
    duel(&st, TWO);
    for (int i = 0; i < 4; i++) CHECK(why_of(&st, blow(0, 1, (uint16_t)(i + 1), 10 * S + (uint64_t)i * MS)) == W(OK));
    CHECK(why_of(&st, blow(0, 1, 5, 5 * S)) == W(RATE));
    puts("PASS: rate (10 and 50 blows in a second: 4 taken; per attacker; sliding second; rejected ones free)");
}

static void test_push(void) {
    CoopPvp st;
    CoopPvpVerdict v;
    CoopPvpHit h;
    const float pi = 3.14159265f;
    duel(&st, TWO);
    // (apart, in front: away from the attacker, as the host sees them)
    h = fresh(0, 1), h.to[0] = 1.0f, h.to[2] = 1.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 20);
    CHECK(NEAR(v.push[0], sqrt(0.5)) && NEAR(v.push[1], sqrt(0.5)));
    h = fresh(0, 1), h.to[2] = 2.0f;
    coop_pvp_validate(&st, &h, &v);
    CHECK(NEAR(v.push[0], 0) && NEAR(v.push[1], 1));
    // (facing +x, yaw pi/2: the victim 2 m along x is pushed along +x)
    h = fresh(0, 1), h.attacker_yaw = pi / 2, h.to[0] = 2.0f, h.to[2] = 0.0f;
    coop_pvp_validate(&st, &h, &v);
    CHECK(NEAR(v.push[0], 1) && NEAR(v.push[1], 0));
    // (N5: right on him, 0.3 m to his side: along his facing, never towards him)
    h = fresh(0, 1), h.to[0] = 0.3f, h.to[2] = 0.0f;
    coop_pvp_validate(&st, &h, &v);
    CHECK(NEAR(v.push[0], 0) && NEAR(v.push[1], 1));
    // (N5: stale positions put the victim behind the swing: along the facing, not back past the attacker)
    h = fresh(0, 1), h.to[2] = -1.5f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 20);
    CHECK(NEAR(v.push[0], 0) && NEAR(v.push[1], 1));
    h = fresh(0, 1), h.attacker_yaw = pi, h.to[2] = -2.0f;
    coop_pvp_validate(&st, &h, &v);
    CHECK(NEAR(v.push[0], 0) && NEAR(v.push[1], -1));
    // (positions far from the origin: still a unit)
    h = fresh(0, 1), h.from[0] = 4000.0f, h.from[2] = -2500.0f, h.to[0] = 4001.5f, h.to[2] = -2500.0f;
    h.attacker_yaw = pi / 2;
    coop_pvp_validate(&st, &h, &v);
    CHECK(NEAR(v.push[0], 1) && NEAR(v.push[1], 0));
    // (a bomb: away from where it went off, whichever way the thrower faces; right on it, his facing)
    h = fresh(0, 1), h.bomb = true, h.reach = 4.0f, h.power = 8, h.to[0] = -2.0f, h.to[2] = 0.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 8);
    CHECK(NEAR(v.push[0], -1) && NEAR(v.push[1], 0));
    h = fresh(0, 1), h.bomb = true, h.reach = 4.0f, h.power = 8, h.to[2] = 0.0f;
    coop_pvp_validate(&st, &h, &v);
    CHECK(NEAR(v.push[0], 0) && NEAR(v.push[1], 1));
    // (a bomb farther than its blast + 2 m)
    h = fresh(0, 1), h.bomb = true, h.reach = 4.0f, h.power = 8, h.to[2] = 6.5f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(FAR));
    puts("PASS: push (away from the attacker; along his facing when on him or behind him: N5; bombs)");
}

static void test_flow(void) {
    CoopPvp st;
    CoopPvpVerdict v;
    CoopPvpHit h;
    // (CU-12 main flow, as the host sees it: A slot 0 (the host), B slot 1)
    coop_pvp_reset(&st);
    coop_pvp_present(&st, TWO);
    h = fresh(0, 1), h.to[2] = 1.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(OFF));  // (CA-1: no agreement)
    CHECK(CTL(0, PROPOSE) == R(OK));
    h = fresh(0, 1), h.to[2] = 1.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 0 && v.why == W(OFF));  // (CA-2: only A has it)
    CHECK(CTL(1, ACCEPT) == R(OK));
    h = fresh(0, 1), h.to[2] = 1.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 20 && v.why == W(OK));  // (CA-3: B takes its defence off P)
    h = fresh(1, 0), h.from[2] = 1.0f, h.to[2] = 0.0f, h.attacker_yaw = 3.14159265f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 20 && NEAR(v.push[1], -1));  // (and back)
    CHECK(CTL(1, TURN_OFF) == R(OK));
    h = fresh(0, 1), h.to[2] = 1.0f;
    CHECK(coop_pvp_validate(&st, &h, &v) == 0);  // (CA-9: B ended it)
    puts("PASS: CU-12 main flow (no agreement, one side, both, ended)");
}

static void test_names(void) {
    CHECK(!strcmp(coop_pvp_why_name(W(OK)), "ok"));
    CHECK(!strcmp(coop_pvp_why_name(W(FAR)), "distance"));
    CHECK(!strcmp(coop_pvp_why_name(W(RATE)), "rate"));
    CHECK(!strcmp(coop_pvp_why_name(W(ZONE)), "no-duel zone"));
    CHECK(!strcmp(coop_pvp_why_name((CoopPvpWhy)99), "?"));
    CHECK(!strcmp(coop_pvp_why_name((CoopPvpWhy)-1), "?"));
    for (int i = W(OK); i <= W(RATE); i++) CHECK(strcmp(coop_pvp_why_name((CoopPvpWhy)i), "?") != 0);
    coop_pvp_reset(NULL);
    coop_pvp_present(NULL, TWO);
    puts("PASS: reason names, NULL state");
}

int main(void) {
    test_reset();
    test_agreement();
    test_timeout();
    test_three();
    test_forbidden();
    test_death();
    test_zone();
    test_rejections();
    test_damage();
    test_repeat();
    test_burst();
    test_push();
    test_flow();
    test_names();
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    puts("PASS: coop_pvp");
    return 0;
}
