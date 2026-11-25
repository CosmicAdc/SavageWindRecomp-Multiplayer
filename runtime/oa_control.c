// Online play's actors, oa_control: the control commands (flag, setflag, event, actors, ...). What the modules share:
// runtime/online_actors_internal.h
#include "online_actors_internal.h"

// `actors <text>` (control): the live actors whose name has text in it, with where each is and how far from Link.
// The control thread asks, the frame loop answers (the actor list is the game's, read on its thread only).
#define FIND_MAX 40
static struct {
    volatile int state;  // 0 idle, 1 asked, 2 answered
    char text[48];
    int n;
    struct { char name[48]; float pos[3], dist; uint64_t actor; } hit[FIND_MAX];
} g_find;

// `flag <name>` (control): a bool game data flag's word as the game has it now (bit 0 its value), read on the frame
// loop like `actors`
// `setflag <name>` (developer mode): the same, set first, as the game's own setter does (else in memory); the others'
// games take it like any flag this player gets (their quests' ones, with what comes with them)
static struct { volatile int state; char name[64]; int found, set; uint32_t word; } g_flagq;

void oa_flag_query(Cpu *c) {
    if (__atomic_load_n(&g_flagq.state, __ATOMIC_ACQUIRE) != 1) return;
    if (!online_version_ok(g_game_version)) {  // (its setter is at a 1.9.0 address: answered, with nothing)
        g_flagq.found = 0, g_flagq.word = 0;
        __atomic_store_n(&g_flagq.state, 2, __ATOMIC_RELEASE);
        return;
    }
    uint32_t h = oa_crc32_str(g_flagq.name);
    uint32_t *v = oa_bool_flag(h);
    if (v && g_flagq.set && !(*v & 1) && !oa_set_bool_flag(c, h)) *v |= 1;
    g_flagq.found = v != NULL, g_flagq.word = v ? *v : 0;
    __atomic_store_n(&g_flagq.state, 2, __ATOMIC_RELEASE);
}

static void flag_cmd(int argc, char **argv, ControlReply *r, bool set) {
    if (argc != 2) { control_fail(r, set ? "Usage: setflag <bool flag name>" : "Usage: flag <bool flag name>"); return; }
    if (__atomic_load_n(&g_flagq.state, __ATOMIC_ACQUIRE) == 1) { control_fail(r, "A read is under way"); return; }
    g_flagq.set = set;
    snprintf(g_flagq.name, sizeof g_flagq.name, "%s", argv[1]);
    __atomic_store_n(&g_flagq.state, 1, __ATOMIC_RELEASE);
    uint64_t until = plat_now_ns() + 2000000000ull;
    while (__atomic_load_n(&g_flagq.state, __ATOMIC_ACQUIRE) != 2 && plat_now_ns() < until)
        plat_wait_until_ns(plat_now_ns() + 5000000ull);
    if (__atomic_load_n(&g_flagq.state, __ATOMIC_ACQUIRE) != 2) {
        __atomic_store_n(&g_flagq.state, 0, __ATOMIC_RELEASE);
        control_fail(r, "No answer from the game (no world, or online play off)");
        return;
    }
    if (!g_flagq.found) control_text(r, "%s: no such bool flag", g_flagq.name);
    else control_text(r, "%s = %u (word %#x)", g_flagq.name, g_flagq.word & 1, g_flagq.word);
    control_field_int(r, "found", g_flagq.found);
    control_field_int(r, "value", (int)(g_flagq.word & 1));
    control_field_int(r, "word", (int)g_flagq.word);
    __atomic_store_n(&g_flagq.state, 0, __ATOMIC_RELEASE);
}
static void cmd_flag(int argc, char **argv, ControlReply *r) { flag_cmd(argc, argv, r, false); }
static void cmd_setflag(int argc, char **argv, ControlReply *r) { flag_cmd(argc, argv, r, true); }
static void cmd_leave(int argc, char **argv, ControlReply *r) {  // (`leave`: the session left, as its button does)
    (void)argc, (void)argv;
    online_leave();
    control_text(r, "Left the online session");
}
static void cmd_bloodmoon(int argc, char **argv, ControlReply *r) {
    (void)argc, (void)argv;
    __atomic_store_n(&oa_g_fake_blood_moon, 1, __ATOMIC_RELEASE);
    control_text(r, "The world's shared flags (items, chests) are cleared here on the next check");
}

// `event <flow> [entry]` (control, developer mode): an event started on Link, as an EventTag starts its own
// (start_event), on the frame loop; for tests that need a story step without playing up to it
static struct { volatile int state; char flow[64], entry[64]; int ok; } g_eventq;

void oa_event_query(Cpu *c, uint64_t pinfo) {
    if (__atomic_load_n(&g_eventq.state, __ATOMIC_ACQUIRE) != 1) return;
    uint64_t player = pinfo ? *(uint64_t *)(uintptr_t)(pinfo + 0x60) : 0;
    // (the event manager and its calls are at 1.9.0 addresses: on another version, answered as not started)
    g_eventq.ok = online_version_ok(g_game_version) && VALID(player) &&
                  oa_start_event(c, player, g_eventq.flow, g_eventq.entry);
    __atomic_store_n(&g_eventq.state, 2, __ATOMIC_RELEASE);
}

static void cmd_event(int argc, char **argv, ControlReply *r) {
    if (argc < 2 || argc > 3) { control_fail(r, "Usage: event <flow> [entry]"); return; }
    if (__atomic_load_n(&g_eventq.state, __ATOMIC_ACQUIRE) == 1) { control_fail(r, "An event start is under way"); return; }
    snprintf(g_eventq.flow, sizeof g_eventq.flow, "%s", argv[1]);
    snprintf(g_eventq.entry, sizeof g_eventq.entry, "%s", argc == 3 ? argv[2] : argv[1]);
    __atomic_store_n(&g_eventq.state, 1, __ATOMIC_RELEASE);
    uint64_t until = plat_now_ns() + 2000000000ull;
    while (__atomic_load_n(&g_eventq.state, __ATOMIC_ACQUIRE) != 2 && plat_now_ns() < until)
        plat_wait_until_ns(plat_now_ns() + 5000000ull);
    if (__atomic_load_n(&g_eventq.state, __ATOMIC_ACQUIRE) != 2) {
        __atomic_store_n(&g_eventq.state, 0, __ATOMIC_RELEASE);
        control_fail(r, "No answer from the game (no world, or online play off)");
        return;
    }
    control_text(r, "%s<%s>: %s", g_eventq.flow, g_eventq.entry, g_eventq.ok ? "started" : "not started");
    control_field_int(r, "started", g_eventq.ok);
    __atomic_store_n(&g_eventq.state, 0, __ATOMIC_RELEASE);
}

void oa_find_actors(uint64_t pinfo) {
    if (__atomic_load_n(&g_find.state, __ATOMIC_ACQUIRE) != 1) return;
    g_find.n = 0;
    uint64_t player = pinfo ? *(uint64_t *)(uintptr_t)(pinfo + 0x60) : 0;
    if (VALID(player)) {
        const float *pm = (const float *)(uintptr_t)(player + 0x398);
        int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        for (int k = 0; k < na && g_find.n < FIND_MAX; k++) {
            char name[48];
            oa_actor_name_of(oa_g_actors[k], name);
            if (!strstr(name, g_find.text)) continue;
            const float *m = (const float *)(uintptr_t)(oa_g_actors[k] + 0x398);
            float dx = m[3] - pm[3], dy = m[7] - pm[7], dz = m[11] - pm[11];
            snprintf(g_find.hit[g_find.n].name, sizeof g_find.hit[0].name, "%s", name);
            g_find.hit[g_find.n].pos[0] = m[3], g_find.hit[g_find.n].pos[1] = m[7], g_find.hit[g_find.n].pos[2] = m[11];
            g_find.hit[g_find.n].dist = sqrtf(dx * dx + dy * dy + dz * dz);
            g_find.hit[g_find.n].actor = oa_g_actors[k];
            g_find.n++;
        }
    }
    __atomic_store_n(&g_find.state, 2, __ATOMIC_RELEASE);
}

static void cmd_actors(int argc, char **argv, ControlReply *r) {
    if (argc != 2) { control_fail(r, "Usage: actors <part of an actor name>"); return; }
    if (__atomic_load_n(&g_find.state, __ATOMIC_ACQUIRE) == 1) { control_fail(r, "A search is under way"); return; }
    snprintf(g_find.text, sizeof g_find.text, "%s", argv[1]);
    __atomic_store_n(&g_find.state, 1, __ATOMIC_RELEASE);
    uint64_t until = plat_now_ns() + 2000000000ull;
    while (__atomic_load_n(&g_find.state, __ATOMIC_ACQUIRE) != 2 && plat_now_ns() < until)
        plat_wait_until_ns(plat_now_ns() + 5000000ull);
    if (__atomic_load_n(&g_find.state, __ATOMIC_ACQUIRE) != 2) {
        __atomic_store_n(&g_find.state, 0, __ATOMIC_RELEASE);
        control_fail(r, "No answer from the game (no world, or online play off)");
        return;
    }
    // (the lines for the console, which prints them; the same in one field for scripts)
    char list[FIND_MAX * 112] = "";
    size_t at = 0;
    for (int k = 0; k < g_find.n; k++) {
        control_text(r, "%s at %.1f %.1f %.1f (%.0f m) actor %llx", g_find.hit[k].name, g_find.hit[k].pos[0],
                     g_find.hit[k].pos[1], g_find.hit[k].pos[2], g_find.hit[k].dist, (unsigned long long)g_find.hit[k].actor);
        if (at < sizeof list)
            at += (size_t)snprintf(list + at, sizeof list - at, "%s%s %.1f %.1f %.1f %.0fm %llx", k ? "; " : "",
                                   g_find.hit[k].name, g_find.hit[k].pos[0], g_find.hit[k].pos[1], g_find.hit[k].pos[2],
                                   g_find.hit[k].dist, (unsigned long long)g_find.hit[k].actor);
    }
    control_field_str(r, "actors", list);
    control_text(r, "%d actor%s%s", g_find.n, g_find.n == 1 ? "" : "s", g_find.n == FIND_MAX ? " (first ones only)" : "");
    control_field_int(r, "count", g_find.n);
    __atomic_store_n(&g_find.state, 0, __ATOMIC_RELEASE);
}

void online_actors_control_init(void) {
    static const ControlCmd cmds[] = {
        {"actors", "<part of an actor name>", "The live actors with this in their name: where, how far from Link",
         cmd_actors, CONTROL_PLAYER},
        {"flag", "<bool flag name>", "A game data bool flag's value and word, as the game has it now", cmd_flag,
         CONTROL_PLAYER},
        {"event", "<flow> [entry]", "Start an event on Link (entry: the flow's name if left out)", cmd_event, 0},
        {"setflag", "<bool flag name>", "Set a game data bool flag (as the game's setter does), then show it", cmd_setflag, 0},
        {"leave", "", "Leave (or end) the online session, as its button does", cmd_leave, 0},
        {"bloodmoon", "", "Clear the world's shared flags (items, chests) here, as a Blood Moon does", cmd_bloodmoon, 0},
    };
    control_register(cmds, (int)(sizeof cmds / sizeof *cmds));
}

// rt_online_frame, step 1: the control commands' answers (`actors`, `flag`, `event`), on the game's thread
void oa_control_frame(Cpu *c, uint64_t pinfo) {
    oa_find_actors(pinfo);
    oa_flag_query(c);
    oa_event_query(c, pinfo);
}

// rt_online_frame, step 10: the tests by environment variable (their own state kept in each)
void oa_control_tests_frame(Cpu *c, uint64_t pinfo, const float link[3]) {
    if (getenv("SAVAGEWIND_ONLINE_ENEMY_TEST")) {  // (test: each enemy's online actor made in turn, next to Link)
        static MapLine *m;
        static int n = -1, next, tick;
        static Piece p;
        if (n < 0) n = oa_map_load("enemies.txt", &m);
        if (tick++ % 20 == 0 && next < n) {
            if (p.actor || p.pending) oa_delete_piece(c, &p);
            LOG("[online] enemy test %d: %s\n", next, m[next].val);
            oa_piece_update(c, 0, &p, m[next].val, "test");
            next++;
        } else oa_piece_update(c, 0, &p, p.name, "test");
        if (p.actor) {
            float mm[12] = {1, 0, 0, link[0] + 4, 0, 1, 0, link[1], 0, 0, 1, link[2]};
            oa_call(c, FN_SET_MTX, p.actor, (uint64_t)(uintptr_t)mm, 0, 0, 0, 0);
        }
    }
    if (getenv("SAVAGEWIND_ONLINE_LOOK_LOG")) {  // (test: the actors near whose +0x710 / +0x518 change)
        static uint64_t seen_a[64];
        static uint32_t seen_v[64];
        static int tick;
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        if (VALID(pl0) && tick++ % 3 == 0) {
            int na = oa_all_actors(*(uint64_t *)(uintptr_t)(pl0 + 0x68), oa_g_actors, MAX_ACTORS);
            const float *pm = (const float *)(uintptr_t)(pl0 + 0x398);
            for (int k = 0; k < na; k++) {
                uint64_t a = oa_g_actors[k];
                const float *m = (const float *)(uintptr_t)(a + 0x398);
                if ((m[3] - pm[3]) * (m[3] - pm[3]) + (m[11] - pm[11]) * (m[11] - pm[11]) > 30 * 30) continue;
                uint32_t v = *(uint32_t *)(uintptr_t)(a + 0x710);
                int q = 0;
                while (q < 64 && seen_a[q] != a) q++;
                if (q == 64) { for (q = 0; q < 64 && seen_a[q]; q++) ; if (q == 64) continue; seen_a[q] = a, seen_v[q] = v; continue; }
                if (v != seen_v[q]) {
                    char n[48];
                    oa_actor_name_of(a, n);
                    LOG("[online] look %s: +0x710 %x -> %x\n", n, seen_v[q], v);
                    seen_v[q] = v;
                }
            }
        }
    }
    {   // (test: SAVAGEWIND_ONLINE_DIE_TEST=<s>: the local Link dies that many seconds in)
        static int t;
        const char *dt = getenv("SAVAGEWIND_ONLINE_DIE_TEST");
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        if (dt && VALID(pl0) && ++t == atoi(dt) * 30) *(int32_t *)(uintptr_t)(pl0 + ENEMY_LIFE) = 0, LOG("[online] die test\n");
    }
    if (getenv("SAVAGEWIND_ONLINE_LIFE_LOG")) {  // (test: the local Link's life, as it changes)
        static int32_t last = -12345;
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        int32_t v = VALID(pl0) ? *(int32_t *)(uintptr_t)(pl0 + ENEMY_LIFE) : -1;
        if (v != last) LOG("[online] our life %d\n", v), last = v;
    }
    {   // (test: SAVAGEWIND_ONLINE_KNOCK_TEST=<s>: a heavy blow's knock on the local Link every that many seconds)
        static int t;
        const char *kt = getenv("SAVAGEWIND_ONLINE_KNOCK_TEST");
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        if (kt && VALID(pl0) && ++t % (atoi(kt) * 30) == 0) {
            const float *pm = (const float *)(uintptr_t)(pl0 + 0x398);
            float at[3] = {pm[3] + 1, pm[7] + 0.3f, pm[11]};
            const char *kn = getenv("SAVAGEWIND_ONLINE_KNOCK");
            oa_boom_start(c, kn ? kn : "OnlineShock", at);
            LOG("[online] knock test\n");
        }
    }
}
