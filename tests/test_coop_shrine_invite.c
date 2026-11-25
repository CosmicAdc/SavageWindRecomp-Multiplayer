// Tests for runtime/coop_shrine_invite.c (T465a): synthetic STATE and states, no game and no network.
//   clang -Wall -Iruntime tests/test_coop_shrine_invite.c runtime/coop_shrine_invite.c -lm -o build/test_coop_shrine_invite
#include <stdio.h>
#include <string.h>

#include "coop_shrine_invite.h"

#define S 1000000000ull   // a second, in ns

static int g_fail;
#define CHECK(c)                                                                \
    do {                                                                        \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)

static CoopSi si;
static uint64_t now;
static const CoopSiCtx FREE = {true, 0, -1};   // setting on, nothing blocks, not in a shrine

static void st(int slot, const char *map) { coop_si_state(&si, slot, map, strlen(map) + 1, now); }
static void st2(int slot, const char *map) { st(slot, map), st(slot, map); }
static void fresh(void) {
    coop_si_reset(&si);
    now = 100 * S;
}
// (a player known to be in the field)
static void field(int slot) { st(slot, "MainField/"); }

static CoopSiOut g_out;
static CoopSiAct poll_with(CoopSiCtx c) { return coop_si_poll(&si, &c, now, &g_out); }
static CoopSiAct poll(void) { return poll_with(FREE); }
static int drain_with(CoopSiCtx c) {   // polls until NONE; returns how many actions came
    int n = 0;
    while (coop_si_poll(&si, &c, now, &g_out) != COOP_SI_NONE && n < 50) n++;
    return n;
}

static void test_classify(void) {
    int n;
    char big[COOP_SI_MAP_CAP];
    CHECK(coop_si_classify("CDungeon/Dungeon038", 32, &n) == COOP_SI_MAP_SHRINE && n == 38);
    CHECK(coop_si_classify("CDungeon/Dungeon000", 32, &n) == COOP_SI_MAP_SHRINE && n == 0);
    CHECK(coop_si_classify("CDungeon/Dungeon135", 32, &n) == COOP_SI_MAP_SHRINE && n == 135);
    CHECK(coop_si_classify("CDungeon/Dungeon136", 32, &n) == COOP_SI_MAP_MALFORMED && n == -1);
    CHECK(coop_si_classify("CDungeon/Dungeon999", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/Dungeon5", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/Dungeon0380", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/Dungeon03a", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/Dungeon-38", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/../x", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("cdungeon/Dungeon038", 32, &n) == COOP_SI_MAP_OTHER);
    CHECK(coop_si_classify("MainFieldDungeon/RemainsWind", 32, &n) == COOP_SI_MAP_OTHER && n == -1);
    CHECK(coop_si_classify("MainField/", 32, &n) == COOP_SI_MAP_OTHER);
    CHECK(coop_si_classify("AocField/", 32, &n) == COOP_SI_MAP_OTHER);
    CHECK(coop_si_classify("", 32, &n) == COOP_SI_MAP_EMPTY);
    CHECK(coop_si_classify("CDungeon/Dungeon038\x01", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/Dungeon038\xff", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("MainField/\n", 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify(NULL, 32, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("MainField/", 0, &n) == COOP_SI_MAP_MALFORMED);
    CHECK(coop_si_classify("CDungeon/Dungeon038", 19, &n) == COOP_SI_MAP_MALFORMED);   // no NUL in range
    CHECK(coop_si_classify("CDungeon/Dungeon038", 20, &n) == COOP_SI_MAP_SHRINE);
    memset(big, 'A', sizeof big);   // no NUL at all
    CHECK(coop_si_classify(big, sizeof big, &n) == COOP_SI_MAP_MALFORMED);
    memcpy(big, "CDungeon/Dungeon038", 20);   // the text, then garbage after the NUL: only the text counts
    CHECK(coop_si_classify(big, sizeof big, &n) == COOP_SI_MAP_SHRINE && n == 38);
}

static void test_enter_debounce(void) {
    fresh();
    field(1);
    CHECK(poll() == COOP_SI_NONE);
    st(1, "CDungeon/Dungeon038");   // one STATE is not enough
    CHECK(poll() == COOP_SI_NONE);
    st(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    CHECK(g_out.slot == 1 && g_out.shrine == 38 && g_out.why == COOP_SI_WHY_NONE);
    CHECK(poll() == COOP_SI_NONE);   // once
    // a flicker: shrine, field, shrine is never two in a row
    fresh();
    field(1);
    st(1, "CDungeon/Dungeon038"), st(1, "MainField/"), st(1, "CDungeon/Dungeon038"), st(1, "MainField/");
    CHECK(drain_with(FREE) == 0);
    // from another map than the field, too
    fresh();
    st(1, "MainFieldDungeon/RemainsWind");
    st2(1, "CDungeon/Dungeon007");
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 7);
}

static void test_empty_ignored(void) {
    fresh();
    field(1);
    st(1, "CDungeon/Dungeon038");
    st(1, "");   // a stale pose in the middle
    st(1, "");
    CHECK(poll() == COOP_SI_NONE);
    st(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 38);
    // "" while he is in: no leave
    st(1, ""), st(1, ""), st(1, "");
    CHECK(poll() == COOP_SI_NONE);   // notice still open
    // "" before the first map: no baseline taken
    fresh();
    st(1, ""), st(1, "");
    st2(1, "CDungeon/Dungeon038");   // first real map: the baseline, so no invitation
    CHECK(drain_with(FREE) == 0);
}

static void test_malformed_ignored(void) {
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon999");
    st2(1, "CDungeon/../x");
    st2(1, "CDungeon/Dungeon5");
    st2(1, "MainFieldDungeon/RemainsWind");
    st2(1, "CDungeon/Dungeon038x");
    CHECK(drain_with(FREE) == 0);
    char junk[COOP_SI_MAP_CAP];
    memset(junk, 'C', sizeof junk);
    coop_si_state(&si, 1, junk, sizeof junk, now);
    coop_si_state(&si, 1, junk, sizeof junk, now);
    CHECK(drain_with(FREE) == 0);
    // malformed between two good STATE does not reset the count
    st(1, "CDungeon/Dungeon038");
    st(1, "CDungeon/Dungeon999");
    st(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 38);
    // slots out of range and NULL are harmless
    coop_si_state(&si, -1, "CDungeon/Dungeon038", 32, now);
    coop_si_state(&si, COOP_SI_SLOTS, "CDungeon/Dungeon038", 32, now);
    coop_si_state(&si, 2, NULL, 0, now);
    coop_si_leave(&si, -1), coop_si_leave(&si, COOP_SI_SLOTS);
    CHECK(drain_with(FREE) == 0);
}

static void test_already_inside(void) {
    fresh();
    st2(2, "CDungeon/Dungeon038");   // first seen inside: ADR-025 n.º 17
    CHECK(drain_with(FREE) == 0);
    st2(2, "MainField/");   // leaves, then enters again: now it is an entry
    CHECK(drain_with(FREE) == 0);
    st2(2, "CDungeon/Dungeon038");
    CHECK(drain_with(FREE) == 1 && g_out.act == COOP_SI_NONE);
    // a player who left the session and came back inside is "first seen" again
    fresh();
    field(2);
    coop_si_leave(&si, 2);
    st2(2, "CDungeon/Dungeon040");
    CHECK(drain_with(FREE) == 0);
}

static void test_repeat_window(void) {
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    now += 2 * S;
    st2(1, "MainField/");
    CHECK(poll() == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_LEFT);
    now += 2 * S;
    st2(1, "CDungeon/Dungeon038");   // 4 s after the first: not again
    CHECK(drain_with(FREE) == 0);
    st2(1, "MainField/");
    now += 6 * S;   // 10 s after the first entry
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 38);
    // another shrine at once is not a repeat
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    st2(1, "MainField/"), st2(1, "CDungeon/Dungeon039");
    CHECK(poll() == COOP_SI_CLOSE);   // the first one's notice (he left)
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 39);
    // 9.999 s is still a repeat; the same player on another slot is not
    fresh();
    field(1), field(2);
    st2(1, "CDungeon/Dungeon038");
    drain_with(FREE);
    coop_si_decline(&si);
    st2(1, "MainField/");
    now += 10 * S - 1;
    st2(1, "CDungeon/Dungeon038");
    CHECK(drain_with(FREE) == 0);
    st2(2, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW && g_out.slot == 2);
}

static void test_ten_in_a_row(void) {
    fresh();
    field(1);
    int shows = 0, open_max = 0;
    for (int i = 0; i < 10; i++) {   // ten different shrines, in and out, without a poll between some
        char m[COOP_SI_MAP_CAP];
        snprintf(m, sizeof m, "CDungeon/Dungeon%03d", 10 + i);
        st2(1, m);
        while (poll() != COOP_SI_NONE) {
            if (g_out.act == COOP_SI_SHOW) shows++;
            if (si.open) open_max = 1;
        }
        if (i % 2) st2(1, "MainField/"), drain_with(FREE);
        CHECK(!si.queued);   // from one player there is no queue
        now += 1 * S;
    }
    CHECK(shows == 10 && open_max == 1);
    CHECK(!si.open);   // the tenth left again
    // ten of the same shrine: one
    fresh();
    field(1);
    shows = 0;
    for (int i = 0; i < 10; i++) {
        st2(1, "CDungeon/Dungeon038");
        while (poll() != COOP_SI_NONE)
            if (g_out.act == COOP_SI_SHOW) shows++;
        st2(1, "MainField/");
        now += 500000000ull;
    }
    CHECK(shows == 1);
    // ten entries before any poll: one notice, the last
    fresh();
    field(1);
    for (int i = 0; i < 10; i++) {
        char m[COOP_SI_MAP_CAP];
        snprintf(m, sizeof m, "CDungeon/Dungeon%03d", 20 + i);
        st2(1, m);
        st2(1, "MainField/");
    }
    CHECK(drain_with(FREE) == 0);   // each one ended before the poll: nothing to show
    for (int i = 0; i < 10; i++) {
        char m[COOP_SI_MAP_CAP];
        snprintf(m, sizeof m, "CDungeon/Dungeon%03d", 40 + i);
        st2(1, m);
    }
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 49);
    CHECK(poll() == COOP_SI_NONE);
}

static void test_leave_closes(void) {
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    st(1, "MainField/");   // one STATE is not a leave
    CHECK(poll() == COOP_SI_NONE);
    st(1, "MainField/");
    CHECK(poll() == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_LEFT && g_out.slot == 1 && g_out.shrine == 38);
    CHECK(poll() == COOP_SI_NONE);
    CoopSiOut o;
    CHECK(coop_si_accept(&si, &FREE, now, &o) == COOP_SI_NONE && o.why == COOP_SI_WHY_NO_NOTICE);
    // he leaves the session
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    coop_si_leave(&si, 1);
    CHECK(poll() == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_LEFT);
    // quick out and in before the poll: the notice stays (he is in)
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    st(1, "MainField/");
    st(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_NONE && si.open);
}

static void test_accept_decline(void) {
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    CoopSiOut o;
    now += 5 * S;
    CHECK(coop_si_accept(&si, &FREE, now, &o) == COOP_SI_TRAVEL && o.slot == 1 && o.shrine == 38);
    CHECK(!si.open && poll() == COOP_SI_NONE);
    CHECK(coop_si_accept(&si, &FREE, now, &o) == COOP_SI_NONE);   // not twice
    // decline
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    CHECK(coop_si_decline(&si));
    CHECK(!coop_si_decline(&si));
    CHECK(poll() == COOP_SI_NONE);
    CHECK(coop_si_accept(&si, &FREE, now, &o) == COOP_SI_NONE);
    // null out is allowed
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(coop_si_poll(&si, &FREE, now, NULL) == COOP_SI_SHOW);
    CHECK(coop_si_accept(&si, &FREE, now, NULL) == COOP_SI_TRAVEL);
}

static void test_expiry(void) {
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    now += 30 * S - 1;
    CHECK(poll() == COOP_SI_NONE && si.open);
    now += 1;
    CHECK(poll() == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_EXPIRED);
    CHECK(poll() == COOP_SI_NONE);
    // accepting at 30 s sharp, before any poll, finds it expired
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    now += 30 * S;
    CoopSiOut o;
    CHECK(coop_si_accept(&si, &FREE, now, &o) == COOP_SI_CLOSE && o.why == COOP_SI_WHY_EXPIRED);
    // the clock going backwards never expires it
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    now -= 50 * S;
    CHECK(poll() == COOP_SI_NONE && si.open);
}

static void test_setting_off(void) {
    CoopSiCtx off = FREE;
    off.enabled = false;
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll_with(off) == COOP_SI_NONE);
    now += 1 * S;
    CHECK(poll() == COOP_SI_NONE);   // it was thrown away, not kept for later
    // turned off while open
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    CHECK(poll_with(off) == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_SETTING_OFF);
    CHECK(poll() == COOP_SI_NONE);
    // accepting with it off
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    CoopSiOut o;
    CHECK(coop_si_accept(&si, &off, now, &o) == COOP_SI_CLOSE && o.why == COOP_SI_WHY_SETTING_OFF);
    // the queue is dropped too
    fresh();
    field(1), field(2);
    st2(1, "CDungeon/Dungeon038"), st2(2, "CDungeon/Dungeon040");
    drain_with(FREE);
    CHECK(si.open && si.queued);
    drain_with(off);
    CHECK(!si.open && !si.queued);
    CHECK(poll() == COOP_SI_NONE);
}

static void test_hard_blocks(void) {
    static const struct {
        uint32_t f;
        CoopSiWhy why;
    } hard[] = {
        {COOP_SI_F_DEAD, COOP_SI_WHY_DEAD},
        {COOP_SI_F_OTHER_MAP, COOP_SI_WHY_OTHER_MAP},
        {COOP_SI_F_CHALLENGE, COOP_SI_WHY_CHALLENGE},
    };
    for (size_t i = 0; i < sizeof hard / sizeof *hard; i++) {
        CoopSiCtx c = FREE;
        c.flags = hard[i].f;
        // does not open
        fresh();
        field(1);
        st2(1, "CDungeon/Dungeon038");
        CHECK(poll_with(c) == COOP_SI_NONE && !si.open);
        CHECK(poll() == COOP_SI_NONE);   // and is not kept
        // closes when it arises while open
        fresh();
        field(1);
        st2(1, "CDungeon/Dungeon038");
        CHECK(poll() == COOP_SI_SHOW);
        CHECK(poll_with(c) == COOP_SI_CLOSE && g_out.why == hard[i].why);
        CHECK(poll() == COOP_SI_NONE);
        // accepting with it
        fresh();
        field(1);
        st2(1, "CDungeon/Dungeon038");
        poll();
        CoopSiOut o;
        CHECK(coop_si_accept(&si, &c, now, &o) == COOP_SI_CLOSE && o.why == hard[i].why);
    }
    // the same shrine: we are inside it (a shrine is another map too, but the reason is the shrine)
    CoopSiCtx same = {true, COOP_SI_F_OTHER_MAP, 38};
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll_with(same) == COOP_SI_NONE && !si.open);
    // inside another shrine: the other map
    CoopSiCtx other = {true, COOP_SI_F_OTHER_MAP, 12};
    st2(1, "MainField/"), now += 20 * S, st2(1, "CDungeon/Dungeon038");
    CHECK(poll_with(other) == COOP_SI_NONE && !si.open);
    // own_shrine alone (flag missing) still stops the same shrine
    CoopSiCtx only = {true, 0, 38};
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll_with(only) == COOP_SI_NONE && !si.open);
    // we walk into his shrine while the notice is open
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    CHECK(poll_with(only) == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_SAME_SHRINE);
}

static void test_soft_blocks(void) {
    static const struct {
        uint32_t f;
        CoopSiWhy why;
    } soft[] = {
        {COOP_SI_F_PAUSED, COOP_SI_WHY_PAUSED},     {COOP_SI_F_SCENE, COOP_SI_WHY_SCENE},
        {COOP_SI_F_LOADING, COOP_SI_WHY_LOADING},   {COOP_SI_F_MOUNTED, COOP_SI_WHY_MOUNTED},
        {COOP_SI_F_GLIDING, COOP_SI_WHY_GLIDING},   {COOP_SI_F_SWIMMING, COOP_SI_WHY_SWIMMING},
        {COOP_SI_F_CLIMBING, COOP_SI_WHY_CLIMBING},
    };
    for (size_t i = 0; i < sizeof soft / sizeof *soft; i++) {
        CoopSiCtx c = FREE;
        c.flags = soft[i].f;
        CHECK(coop_si_soft_block(&c) == soft[i].why);
        // the notice opens and stays; accepting does nothing, then works
        fresh();
        field(1);
        st2(1, "CDungeon/Dungeon038");
        CHECK(poll_with(c) == COOP_SI_SHOW);
        CHECK(poll_with(c) == COOP_SI_NONE && si.open);
        CoopSiOut o;
        now += 10 * S;
        CHECK(coop_si_accept(&si, &c, now, &o) == COOP_SI_NONE && o.why == soft[i].why && o.shrine == 38);
        CHECK(si.open);
        CHECK(coop_si_accept(&si, &FREE, now, &o) == COOP_SI_TRAVEL && o.shrine == 38);
        // it still expires under the block
        fresh();
        field(1);
        st2(1, "CDungeon/Dungeon038");
        poll_with(c);
        now += 30 * S;
        CHECK(poll_with(c) == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_EXPIRED);
    }
    CHECK(coop_si_soft_block(&FREE) == COOP_SI_WHY_NONE);
    // a hard block outranks a soft one
    CoopSiCtx both = FREE;
    both.flags = COOP_SI_F_PAUSED | COOP_SI_F_DEAD;
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll_with(both) == COOP_SI_NONE && !si.open);
    // unknown flag bits do nothing
    CoopSiCtx junk = FREE;
    junk.flags = 0xfffff000u;
    CHECK(coop_si_soft_block(&junk) == COOP_SI_WHY_NONE);
}

static void test_two_players(void) {
    fresh();
    field(1), field(2), field(3);
    st2(1, "CDungeon/Dungeon038");
    st2(2, "CDungeon/Dungeon040");
    st2(3, "CDungeon/Dungeon041");
    CHECK(poll() == COOP_SI_SHOW && g_out.slot == 1);   // by slot order
    CHECK(poll() == COOP_SI_NONE);                      // 2 queued, 3 dropped
    CHECK(si.open && si.slot == 1 && si.queued && si.qslot == 2);
    // answering opens the queued one
    coop_si_decline(&si);
    CHECK(poll() == COOP_SI_SHOW && g_out.slot == 2 && g_out.shrine == 40);
    CHECK(poll() == COOP_SI_NONE && !si.queued);
    // 30 s run from when it opened, not from when it was queued
    now += 29 * S;
    CHECK(poll() == COOP_SI_NONE);
    now += 1 * S;
    CHECK(poll() == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_EXPIRED);
    // a queued one whose player leaves is dropped
    fresh();
    field(1), field(2);
    st2(1, "CDungeon/Dungeon038"), st2(2, "CDungeon/Dungeon040");
    poll(), poll();
    CHECK(si.queued);
    st2(2, "MainField/");
    coop_si_decline(&si);
    CHECK(poll() == COOP_SI_NONE && !si.open);
    // the open one's player goes into another shrine: the old notice closes (he left it), the new one opens
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    CHECK(poll() == COOP_SI_SHOW);
    st2(1, "CDungeon/Dungeon039");
    CHECK(poll() == COOP_SI_CLOSE && g_out.why == COOP_SI_WHY_LEFT && g_out.shrine == 38);
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 39 && si.slot == 1);
    CHECK(poll() == COOP_SI_NONE);
    // a second entry of a queued player replaces the queue
    fresh();
    field(1), field(2);
    st2(1, "CDungeon/Dungeon038"), st2(2, "CDungeon/Dungeon040");
    poll(), poll();
    st2(2, "CDungeon/Dungeon041");
    poll();
    CHECK(si.queued && si.qslot == 2 && si.qshrine == 41);
    // a queued one that we cannot join any more is dropped
    CoopSiCtx dead = FREE;
    dead.flags = COOP_SI_F_DEAD;
    coop_si_decline(&si);
    CHECK(poll_with(dead) == COOP_SI_NONE && !si.open && !si.queued);
}

static void test_session_lifecycle(void) {
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    coop_si_reset(&si);
    CHECK(!si.open && poll() == COOP_SI_NONE);
    // after a reset the first map is a baseline again
    st2(1, "CDungeon/Dungeon038");
    CHECK(drain_with(FREE) == 0);
    // a slot reused by another player
    fresh();
    field(1);
    st2(1, "CDungeon/Dungeon038");
    poll();
    coop_si_leave(&si, 1);
    st2(1, "MainField/");   // the new one's first map
    st2(1, "CDungeon/Dungeon038");   // not a repeat of the old one's: the memory went with him
    CHECK(poll() == COOP_SI_SHOW && g_out.shrine == 38);
}

int main(void) {
    test_classify();
    test_enter_debounce();
    test_empty_ignored();
    test_malformed_ignored();
    test_already_inside();
    test_repeat_window();
    test_ten_in_a_row();
    test_leave_closes();
    test_accept_decline();
    test_expiry();
    test_setting_off();
    test_hard_blocks();
    test_soft_blocks();
    test_two_players();
    test_session_lifecycle();
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    printf("test_coop_shrine_invite: all checks passed\n");
    return 0;
}
