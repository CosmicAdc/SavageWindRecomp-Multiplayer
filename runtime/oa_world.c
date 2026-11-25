// Online play's actors, oa_world: the world's objects: moved, broken, trees cut and their trunks; the level's
// mechanisms. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"

static uint64_t g_break_test_t;            // (when SAVAGEWIND_ONLINE_BREAK_TEST cut its tree)

// ---- The world's objects (protocol 35): the map's objects with a body (a HashId of the map; boulders, crates,
// barrels, carts, a shrine's balls...) moving within 20 m of the local Link - pushed, rolling, carried, blown, hit -
// go with his state (up to ONLINE_OBJS, the nearest), where they are; the others' games move theirs, the same map
// object, there through its body (FN_BODY_TO, as Magnesis), so a boulder he pushes rolls in every game. One moving
// in two players' worlds goes as the first one's in the session. Not enemies, players, items nor what a level's
// own motion moves (lifts, gears, doors, water: each game moves those itself)
#define OBJ_NEAR 20.0f
#define OBJ_FAR 100.0f  // (one followed while it moves, as far: launched out of Stasis, it flew past 40 m)
#define OBJ_SEEN 64
static struct {
    uint64_t a; Watch w; uint32_t hash; float last[3]; int still; bool trunk;
    float his_m[12]; int his_hold;  // (his last place of it, still taken there OBJ_HOLD frames after his last)
    uint64_t quiet;                 // (not ours to send before: it settling as it was taken to his isn't our move)
    bool sending;                   // (sent as ours last time: it may go on so past OBJ_NEAR, launched)
    uint64_t send_t, his_seen_t, his_start;  // (since we send it; his stream last seen, and since when)
} g_obj_seen[OBJ_SEEN];
#define OBJ_HOLD 90
#define OBJ_QUIET_NS 700000000ull
#define OBJ_TIE_NS 300000000ull   // (both started it this close: the lower player number's)
#define TRUNK_QUIET_NS 8000000000ull  // (a trunk made here for his cut: its fall his)
// (a cut tree's trunk has no HashId of its own: made by each game as the tree is cut, ours and the others' here are
// known by the tree's, breaks_update; trunks_near lists those alive near p)
static int trunks_near(const float p[3], float r, uint64_t *a, uint32_t *h, int max);
static uint64_t trunk_made_t(uint64_t a);  // (one made here for another's cut: when; 0 none)

static bool obj_kind(const char *n) {
    if (!strncmp(n, "Enemy_", 6) || !strncmp(n, "Npc", 3) || !strncmp(n, "Online", 6) || !strncmp(n, "Item_", 5) ||
        !strncmp(n, "Weapon_", 7) || !strncmp(n, "Animal_", 7) || !strncmp(n, "GameRom", 7) || !strcmp(n, "PlayerObj"))
        return false;
    static const char *const fixed[] = {"Lift", "Elevator", "Gear", "Door", "Shutter", "Water", "Fence", "Gate", "Bridge",
                                        "Rotate", "Slide", "Move", "Floor", "Wall", "Propeller", "Windmill"};
    for (size_t k = 0; k < sizeof fixed / sizeof *fixed; k++)
        if (strstr(n, fixed[k])) return false;
    return true;
}

// One the others move that we don't follow yet - farther than OBJ_NEAR from our Link, he 25 m off as he cut a tree and
// pushed its trunk: its moves went unheard here, it lay as it fell here - found now: a trunk by its tree's HashId, else
// the map object of that HashId (the actors looked through at most twice a second)
static void obj_find(uint64_t player, uint32_t hash, bool trunk) {
    for (int s = 0; s < OBJ_SEEN; s++)
        if (g_obj_seen[s].a && g_obj_seen[s].hash == hash && g_obj_seen[s].trunk == trunk && oa_alive(g_obj_seen[s].a, g_obj_seen[s].w))
            return;
    uint64_t a = 0;
    if (trunk) {
        uint64_t ta[16];
        uint32_t th[16];
        float far_p[3] = {0, 0, 0};
        int n = trunks_near(far_p, 1e9f, ta, th, 16);
        for (int k = 0; k < n && !a; k++)
            if (th[k] == hash) a = ta[k];
    } else {
        static uint64_t last;
        uint64_t now = plat_now_ns();
        if (now - last < 500000000ull) return;
        last = now;
        int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        for (int k = 0; k < na && !a; k++)
            if (*(uint32_t *)(uintptr_t)(oa_g_actors[k] + ACTOR_HASH_ID) == hash && oa_has_body(oa_g_actors[k])) a = oa_g_actors[k];
    }
    if (!a) return;
    int free_s = -1;
    for (int s = 0; s < OBJ_SEEN && free_s < 0; s++)
        if (!g_obj_seen[s].a || !oa_alive(g_obj_seen[s].a, g_obj_seen[s].w)) free_s = s;
    if (free_s < 0) return;
    const float *m = (const float *)(uintptr_t)(a + 0x398);
    memset(&g_obj_seen[free_s], 0, sizeof g_obj_seen[free_s]);
    g_obj_seen[free_s].a = a, g_obj_seen[free_s].w = oa_watch(a), g_obj_seen[free_s].hash = hash, g_obj_seen[free_s].trunk = trunk;
    g_obj_seen[free_s].last[0] = m[3], g_obj_seen[free_s].last[1] = m[7], g_obj_seen[free_s].last[2] = m[11];
    g_obj_seen[free_s].still = 1000;
    g_obj_seen[free_s].quiet = plat_now_ns() + OBJ_QUIET_NS;
    if (trunk && trunk_made_t(a) && trunk_made_t(a) + TRUNK_QUIET_NS > g_obj_seen[free_s].quiet)
        g_obj_seen[free_s].quiet = trunk_made_t(a) + TRUNK_QUIET_NS;
}

void oa_objs_update(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me) {
    static int tick;
    // ours moving: looked for among the actors twice a second, followed each 3 frames
    if (tick % 30 == 0) {
        int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        for (int k = 0; k < na; k++) {
            uint64_t a = oa_g_actors[k];
            uint32_t hash = *(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID);
            if (!hash || !oa_has_body(a)) continue;
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float dx = m[3] - link[0], dy = m[7] - link[1], dz = m[11] - link[2];
            if (dx * dx + dy * dy + dz * dz > OBJ_NEAR * OBJ_NEAR) continue;
            int f = -1, free_s = -1;
            for (int s = 0; s < OBJ_SEEN && f < 0; s++) {
                if (g_obj_seen[s].a == a) f = s;
                else if (free_s < 0 && (!g_obj_seen[s].a || !oa_alive(g_obj_seen[s].a, g_obj_seen[s].w))) free_s = s;
            }
            if (f >= 0 || free_s < 0) continue;
            char n[48];
            oa_actor_name_of(a, n);
            if (!obj_kind(n)) continue;
            g_obj_seen[free_s].a = a, g_obj_seen[free_s].w = oa_watch(a), g_obj_seen[free_s].hash = hash;
            g_obj_seen[free_s].trunk = false;
            g_obj_seen[free_s].last[0] = m[3], g_obj_seen[free_s].last[1] = m[7], g_obj_seen[free_s].last[2] = m[11];
            g_obj_seen[free_s].still = 1000;
        }
        uint64_t ta[16];
        uint32_t th[16];
        int nt0 = trunks_near(link, OBJ_NEAR, ta, th, 16);
        for (int k = 0; k < nt0; k++) {
            int f = -1, free_s = -1;
            for (int s = 0; s < OBJ_SEEN && f < 0; s++) {
                if (g_obj_seen[s].a == ta[k]) f = s;
                else if (free_s < 0 && (!g_obj_seen[s].a || !oa_alive(g_obj_seen[s].a, g_obj_seen[s].w))) free_s = s;
            }
            if (f >= 0 || free_s < 0) continue;
            const float *m = (const float *)(uintptr_t)(ta[k] + 0x398);
            g_obj_seen[free_s].a = ta[k], g_obj_seen[free_s].w = oa_watch(ta[k]), g_obj_seen[free_s].hash = th[k];
            g_obj_seen[free_s].trunk = true;
            g_obj_seen[free_s].last[0] = m[3], g_obj_seen[free_s].last[1] = m[7], g_obj_seen[free_s].last[2] = m[11];
            g_obj_seen[free_s].still = 1000;
            if (trunk_made_t(ta[k])) g_obj_seen[free_s].quiet = trunk_made_t(ta[k]) + TRUNK_QUIET_NS;  // (its fall his)
            else g_obj_seen[free_s].still = 0;  // (ours, of our cut: where it fell sent a second, the others' put there)
        }
    }
    {   // (test: SAVAGEWIND_ONLINE_OBJ_TEST=<s>: that many seconds in - 1000 + <s>: after the break test's cut - the nearest
        // object we follow is lifted 3 m and held there for 3 s, as Magnesis would)
        static const char *ot;
        static uint64_t t0;
        static int held = -1;
        if (!ot) ot = getenv("SAVAGEWIND_ONLINE_OBJ_TEST") ? getenv("SAVAGEWIND_ONLINE_OBJ_TEST") : "";
        bool after_cut = atof(ot) >= 1000;
        if (*ot && !t0 && !after_cut) t0 = plat_now_ns();
        if (after_cut && !t0 && g_break_test_t) t0 = g_break_test_t;
        double el = *ot && t0 ? (plat_now_ns() - t0) / 1e9 - (atof(ot) - (after_cut ? 1000 : 0)) : -1;
        if (el >= 0 && el < 3) {
            if (held < 0) {
                float bd = 1e30f;
                for (int s = 0; s < OBJ_SEEN; s++) {
                    if (!g_obj_seen[s].a || !oa_alive(g_obj_seen[s].a, g_obj_seen[s].w)) continue;
                    char tn[48];
                    oa_actor_name_of(g_obj_seen[s].a, tn);
                    if (!strstr(tn, "Kibako") && !strstr(tn, "Barrel") && !strstr(tn, "Box") && !strstr(tn, "Ball") &&
                        !strstr(tn, "_Trunk"))
                        continue;
                    const float *m = (const float *)(uintptr_t)(g_obj_seen[s].a + 0x398);
                    float dx = m[3] - link[0], dz = m[11] - link[2], d = dx * dx + dz * dz;
                    if (d < bd) bd = d, held = s;
                }
                if (held >= 0) {
                    char n[48];
                    oa_actor_name_of(g_obj_seen[held].a, n);
                    LOG("[online] object test: lifting %s %u\n", n, g_obj_seen[held].hash);
                } else {
                    static bool told;
                    if (!told) told = true, LOG("[online] object test: nothing to lift near\n");
                }
            }
            if (held >= 0 && oa_alive(g_obj_seen[held].a, g_obj_seen[held].w)) {
                static float to[12];
                if (to[0] == 0) memcpy(to, (const float *)(uintptr_t)(g_obj_seen[held].a + 0x398), sizeof to), to[7] += 3;
                if (*(uint8_t *)(uintptr_t)(g_obj_seen[held].a + 0x64) != 1)  // (at rest a while: asleep)
                    oa_call(c, FN_ACTOR_WAKE, g_obj_seen[held].a, 0, 0, 0, 0, 0);
                uint64_t body = *(uint64_t *)(uintptr_t)(g_obj_seen[held].a + 0x190);
                if (VALID(body)) oa_call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)to, 0, 0, 0, 0);
            }
        }
    }
    // (the others' moving ones: moved here each frame - through its body, to be where his is the next step - and not
    // sent as ours)
    static uint32_t theirs[ONLINE_MAX * ONLINE_OBJS];
    static bool theirs_trunk[ONLINE_MAX * ONLINE_OBJS];
    int nt = 0;
    for (int i = 0; i < ONLINE_MAX; i++) {
        if (i == me || !pl[i].used || !pl[i].pose.valid || (pl[i].flags & ONLINE_F_LOADING) || !oa_same_map(&pl[i])) continue;
        for (int q = 0; q < ONLINE_OBJS; q++) {
            const OnlineObj *o = &pl[i].pose.objs[q];
            if (!o->hash) continue;
            bool finite = true;
            for (int z = 0; z < 12; z++) finite &= isfinite(o->m[z]);
            if (!finite) continue;
            bool trunk = (pl[i].pose.objs_trunk >> q) & 1;
            obj_find(player, o->hash, trunk);  // (ours of it followed, however far from our Link: else it went unmoved)
            // (whose it is, as in one world: the game that set it moving - a push, a blow, a bomb, a throw, however far
            // its player - sends it until it rests, and the others follow it; one we are sending ourselves is ours,
            // his ignored, unless he started it first (both within OBJ_TIE_NS: the lower player number's). One he
            // just let go of - Magnesis, Stasis struck and released - is his, as it flies)
            uint64_t now_t = plat_now_ns();
            int es = -1;
            for (int s = 0; s < OBJ_SEEN && es < 0; s++)
                if (g_obj_seen[s].a && g_obj_seen[s].hash == o->hash && g_obj_seen[s].trunk == trunk &&
                    oa_alive(g_obj_seen[s].a, g_obj_seen[s].w))
                    es = s;
            bool thrown = es >= 0 && g_obj_seen[es].a == oa_rune_released(i) && now_t - oa_rune_released_t(i) < 4000000000ull;
            bool ours = false;
            if (es >= 0) {
                if (now_t - g_obj_seen[es].his_seen_t > 500000000ull) g_obj_seen[es].his_start = now_t;  // (his began)
                g_obj_seen[es].his_seen_t = now_t;
                if (!thrown && g_obj_seen[es].sending)
                    ours = g_obj_seen[es].send_t + OBJ_TIE_NS < g_obj_seen[es].his_start ||
                           (g_obj_seen[es].send_t < g_obj_seen[es].his_start + OBJ_TIE_NS && me < i);
            }
            if (ours) continue;
            theirs_trunk[nt] = trunk, theirs[nt++] = o->hash;
            for (int s = 0; s < OBJ_SEEN; s++) {
                if (g_obj_seen[s].hash != o->hash || g_obj_seen[s].trunk != trunk || !oa_alive(g_obj_seen[s].a, g_obj_seen[s].w))
                    continue;
                memcpy(g_obj_seen[s].his_m, o->m, sizeof g_obj_seen[s].his_m);
                g_obj_seen[s].his_hold = OBJ_HOLD + 1;
                break;
            }
        }
    }
    // (his place of each, each frame: through its body, which gets there over a few steps, so still after his last -
    // he stopped sending it as it came to rest - until it is there too: else it stopped short of where his lay)
    for (int s = 0; s < OBJ_SEEN; s++) {
        if (g_obj_seen[s].his_hold <= 0) continue;
        if (!g_obj_seen[s].a || !oa_alive(g_obj_seen[s].a, g_obj_seen[s].w)) { g_obj_seen[s].his_hold = 0; continue; }
        g_obj_seen[s].his_hold--;
        g_obj_seen[s].quiet = plat_now_ns() + OBJ_QUIET_NS;
        // (asleep - at rest a while, the game updates it no more - its body took nothing: woken first)
        if (*(uint8_t *)(uintptr_t)(g_obj_seen[s].a + 0x64) != 1) oa_call(c, FN_ACTOR_WAKE, g_obj_seen[s].a, 0, 0, 0, 0, 0);
        uint64_t body = *(uint64_t *)(uintptr_t)(g_obj_seen[s].a + 0x190);
        const float *hm = g_obj_seen[s].his_m;
        if (!g_obj_seen[s].his_hold) {  // (the last: still apart - caught on a stump its body went round - put there)
            const float *am = (const float *)(uintptr_t)(g_obj_seen[s].a + 0x398);
            float dx = am[3] - hm[3], dy = am[7] - hm[7], dz = am[11] - hm[11];
            if (dx * dx + dy * dy + dz * dz > 0.5f * 0.5f) {
                oa_call(c, FN_SET_MTX, g_obj_seen[s].a, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
                static int told;
                if (told++ < 8) LOG("[online] an object of another's %.1f m off his at rest: put there\n", sqrtf(dx * dx + dy * dy + dz * dz));
            }
        }
        if (VALID(body)) oa_call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
        g_obj_seen[s].last[0] = hm[3], g_obj_seen[s].last[1] = hm[7], g_obj_seen[s].last[2] = hm[11];
        g_obj_seen[s].still = 1000;  // (moved by him: not ours to send)
    }
    if (tick++ % 3) return;
    // ours: those that moved more than 2 cm since, the nearest first
    struct { int s; float d; } mv[OBJ_SEEN];
    int nm = 0;
    for (int s = 0; s < OBJ_SEEN; s++) {
        if (!g_obj_seen[s].a) continue;
        if (!oa_alive(g_obj_seen[s].a, g_obj_seen[s].w)) { memset(&g_obj_seen[s], 0, sizeof g_obj_seen[s]); continue; }
        const float *m = (const float *)(uintptr_t)(g_obj_seen[s].a + 0x398);
        float dx = m[3] - g_obj_seen[s].last[0], dy = m[7] - g_obj_seen[s].last[1], dz = m[11] - g_obj_seen[s].last[2];
        bool moved = dx * dx + dy * dy + dz * dz > 0.02f * 0.02f;
        bool his = false;
        for (int t = 0; t < nt && !his; t++) his = theirs[t] == g_obj_seen[s].hash && theirs_trunk[t] == g_obj_seen[s].trunk;
        // (one another player holds with Magnesis or stopped with Stasis, moved here to his: his too - sent back as ours,
        // each game pulled the other's to where its own was)
        if (!his) his = oa_rune_holds(g_obj_seen[s].a);
        g_obj_seen[s].last[0] = m[3], g_obj_seen[s].last[1] = m[7], g_obj_seen[s].last[2] = m[11];
        bool was_sending = g_obj_seen[s].sending;
        g_obj_seen[s].sending = false;
        if (his || g_obj_seen[s].his_hold > 0 || plat_now_ns() < g_obj_seen[s].quiet) continue;
        g_obj_seen[s].still = moved ? 0 : g_obj_seen[s].still + 1;
        if (g_obj_seen[s].still > 20) continue;  // (still a second: sent until it rests)
        float lx = m[3] - link[0], ly = m[7] - link[1], lz = m[11] - link[2], d = lx * lx + ly * ly + lz * lz;
        if (d > OBJ_FAR * OBJ_FAR) { memset(&g_obj_seen[s], 0, sizeof g_obj_seen[s]); continue; }
        if (d > OBJ_NEAR * OBJ_NEAR && !was_sending) continue;  // (one far starts no move of ours)
        if (!was_sending) g_obj_seen[s].send_t = plat_now_ns();
        g_obj_seen[s].sending = true;
        mv[nm].s = s, mv[nm].d = d, nm++;
    }
    for (int a = 1; a < nm; a++)
        for (int b = a; b > 0 && mv[b].d < mv[b - 1].d; b--) { int ts = mv[b].s; float td = mv[b].d; mv[b] = mv[b - 1]; mv[b - 1].s = ts, mv[b - 1].d = td; }
    memset(oa_g_objs_out, 0, sizeof oa_g_objs_out);
    oa_g_objs_out_trunk = 0;
    for (int k = 0; k < nm && k < ONLINE_OBJS; k++) {
        oa_g_objs_out[k].hash = g_obj_seen[mv[k].s].hash;
        if (g_obj_seen[mv[k].s].trunk) oa_g_objs_out_trunk |= (uint8_t)(1u << k);
        memcpy(oa_g_objs_out[k].m, (const float *)(uintptr_t)(g_obj_seen[mv[k].s].a + 0x398), sizeof oa_g_objs_out[k].m);
    }
    static int logged;
    if (nm && logged++ < 20) {
        char n[48];
        oa_actor_name_of(g_obj_seen[mv[0].s].a, n);
        LOG("[online] %d of the world's objects moving here (%s %u first)\n", nm, n, g_obj_seen[mv[0].s].hash);
    }
}

// ---- The level's mechanisms (protocol 36): what a level moves itself - lifts and elevators, doors and shutters,
// gears, bridges, turning and sliding floors, propellers, gates, a Divine Beast's parts (Remains*: Ruta's trunk,
// Naboris' rings, Rudania's body, Medoh's wings) - each game moved by its own logic, so one a player set going (a
// switch, a terminal, an orb) moved in his world alone. One within MOVER_NEAR of the local Link that moves or changes
// its AS (slot 0) while no other player's game moves it goes with his state (`movers`, the nearest ONLINE_MOVERS):
// its HashId, its matrix, its AS; the others' games put theirs there - through its body when it has one (FN_BODY_TO,
// as the objects pushed), else its matrix (FN_SET_MTX) - and play its AS, so it moves in every world as in his. One
// both move goes as the first one's (the other's game, seeing it his, stops sending it for MOVER_THEIRS_NS).
// SAVAGEWIND_ONLINE_MOVER_LOG=1: each one taken or sent, logged once
#define MOVER_NEAR 80.0f
#define MOVER_TRACK 48
#define MOVER_QUIET 90             // (frames still before ours stops being sent: 1.5 s)
#define MOVER_THEIRS_NS 700000000ull
static struct {
    uint64_t a; Watch w; uint32_t hash;
    float last[12]; char as[28];
    int quiet;                     // (frames since it last moved here by itself)
    uint64_t theirs_t;             // (when another's game last moved it here)
} g_mv[MOVER_TRACK];

static bool mover_kind(const char *n) {
    if (!strncmp(n, "Enemy_", 6) || !strncmp(n, "Npc", 3) || !strncmp(n, "Online", 6) || !strncmp(n, "Item_", 5) ||
        !strncmp(n, "Weapon_", 7) || !strncmp(n, "Animal_", 7) || !strncmp(n, "GameRom", 7) || !strcmp(n, "PlayerObj") ||
        strstr(n, "Far") || strstr(n, "_Stump") || strstr(n, "_Trunk") || strstr(n, "Tag") ||
        strstr(n, "LiftRock") || strstr(n, "StoneLift"))  // (tags don't move; the rocks to lift are objs_update's)
        return false;
    static const char *const kinds[] = {"Lift", "Elevator", "Gear", "Door", "Shutter", "Bridge", "Rotate", "Slide",
                                        "Move", "Floor", "Propeller", "Windmill", "Gate", "Remains", "Wheel", "Crane",
                                        "Piston", "Seesaw", "Turn", "Lever", "Swing", "Pendulum", "Waterwheel", "Fan"};
    for (size_t k = 0; k < sizeof kinds / sizeof *kinds; k++)
        if (strstr(n, kinds[k])) return true;
    return false;
}

static bool mtx_moved(const float *a, const float *b) {
    for (int k = 0; k < 12; k++) {
        float d = a[k] - b[k], tol = (k == 3 || k == 7 || k == 11) ? 0.01f : 0.002f;
        if (d > tol || d < -tol) return true;
    }
    return false;
}

void oa_movers_update(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me) {
    static int tick;
    static int log_on = -1;
    if (log_on < 0) log_on = getenv("SAVAGEWIND_ONLINE_MOVER_LOG") != NULL;
    uint64_t now = plat_now_ns();
    static bool rescan;  // (one lost - the game made it again: woken, or moved asleep - or one of another's not ours yet)
    // the mechanisms near us: looked for among the actors once a second, and at once after one was lost
    if (tick % 60 == 0 || rescan) {
        rescan = false;
        int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        for (int k = 0; k < na; k++) {
            uint64_t a = oa_g_actors[k];
            uint32_t hash = *(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID);
            if (!hash) continue;
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float dx = m[3] - link[0], dy = m[7] - link[1], dz = m[11] - link[2];
            if (dx * dx + dy * dy + dz * dz > MOVER_NEAR * MOVER_NEAR) continue;
            int f = -1, free_s = -1;
            for (int t = 0; t < MOVER_TRACK && f < 0; t++) {
                if (g_mv[t].a == a) f = t;
                else if (free_s < 0 && (!g_mv[t].a || !oa_alive_rewatch(g_mv[t].a, &g_mv[t].w))) free_s = t;
            }
            if (f >= 0 || free_s < 0) continue;
            char n[48];
            oa_actor_name_of(a, n);
            if (!mover_kind(n)) continue;
            memset(&g_mv[free_s], 0, sizeof g_mv[free_s]);
            g_mv[free_s].a = a, g_mv[free_s].w = oa_watch(a), g_mv[free_s].hash = hash;
            memcpy(g_mv[free_s].last, m, sizeof g_mv[free_s].last);
            snprintf(g_mv[free_s].as, sizeof g_mv[free_s].as, "%s", oa_playing(c, a, 0, 0));
            g_mv[free_s].quiet = MOVER_QUIET;
            if (log_on) LOG("[online] mechanism near: %s %u (AS %s)\n", n, hash, g_mv[free_s].as);
        }
    }
    tick++;
    {   // (test: SAVAGEWIND_ONLINE_MOVER_TEST=<s>: that many seconds in, the nearest mechanism here goes down 2 m in 2 s,
        // set as the others' are; SAVAGEWIND_ONLINE_MOVER_TEST_AS=<name>: that AS played on it as it starts)
        static const char *mt;
        static uint64_t t0;
        static int held = -1;
        static float from[12];
        if (!mt) mt = getenv("SAVAGEWIND_ONLINE_MOVER_TEST") ? getenv("SAVAGEWIND_ONLINE_MOVER_TEST") : "";
        if (*mt && !t0) t0 = now;
        double el = *mt ? (now - t0) / 1e9 - atof(mt) : -1;
        if (el >= 0 && el < 2) {
            if (held < 0) {
                float bd = 1e30f;
                for (int t = 0; t < MOVER_TRACK; t++) {
                    if (!g_mv[t].a || !oa_alive_rewatch(g_mv[t].a, &g_mv[t].w)) continue;
                    const float *m = (const float *)(uintptr_t)(g_mv[t].a + 0x398);
                    float dx = m[3] - link[0], dz = m[11] - link[2], d = dx * dx + dz * dz;
                    if (d < bd) bd = d, held = t;
                }
                if (held >= 0) {
                    char n[48];
                    oa_actor_name_of(g_mv[held].a, n);
                    memcpy(from, (const float *)(uintptr_t)(g_mv[held].a + 0x398), sizeof from);
                    LOG("[online] mechanism test: moving %s %u\n", n, g_mv[held].hash);
                    const char *as = getenv("SAVAGEWIND_ONLINE_MOVER_TEST_AS");
                    if (as) LOG("[online] mechanism test: AS %s %s\n", as, oa_play_as(c, g_mv[held].a, as, 0, 0, -1, true) ? "plays" : "not played");
                }
            }
            static uint32_t held_h;
            if (held >= 0 && !held_h) held_h = g_mv[held].hash;
            for (int t = 0; t < MOVER_TRACK && held_h; t++)  // (by its HashId: the game may make it again)
                if (g_mv[t].hash == held_h && g_mv[t].a) held = t;
            if (held >= 0 && oa_alive_rewatch(g_mv[held].a, &g_mv[held].w)) {
                float to[12];
                memcpy(to, from, sizeof to);
                to[7] -= (float)el;
                uint64_t a = g_mv[held].a;
                if (*(uint8_t *)(uintptr_t)(a + 0x64) != 1) oa_call(c, FN_ACTOR_WAKE, a, 0, 0, 0, 0, 0), g_mv[held].w = oa_watch(a);
                uint64_t body = *(uint64_t *)(uintptr_t)(a + 0x190);
                if (oa_has_body(a)) oa_call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)to, 0, 0, 0, 0);
                oa_call(c, FN_SET_MTX, a, (uint64_t)(uintptr_t)to, 0, 0, 0, 0);
            }
        }
    }
    // the others': theirs put where his is, playing what his plays
    for (int i = 0; i < ONLINE_MAX; i++) {
        if (i == me || !pl[i].used || !pl[i].pose.valid || (pl[i].flags & ONLINE_F_LOADING) || !oa_same_map(&pl[i])) continue;
        for (int q = 0; q < ONLINE_MOVERS; q++) {
            const OnlineMover *o = &pl[i].pose.movers[q];
            if (!o->hash) continue;
            bool finite = true;
            for (int z = 0; z < 12; z++) finite &= isfinite(o->m[z]);
            if (!finite) continue;
            bool tracked = false;
            for (int t = 0; t < MOVER_TRACK && !tracked; t++) tracked = g_mv[t].hash == o->hash && g_mv[t].a;
            if (!tracked && tick % 15 == 0) rescan = true;
            for (int t = 0; t < MOVER_TRACK; t++) {
                if (g_mv[t].hash != o->hash || !oa_alive_rewatch(g_mv[t].a, &g_mv[t].w)) continue;
                // (ours moving it now, his too: the first one's - the lower player number keeps it)
                if (g_mv[t].quiet < MOVER_QUIET && me < i) break;
                uint64_t a = g_mv[t].a;
                if (*(uint8_t *)(uintptr_t)(a + 0x64) != 1) {  // (woken, it goes to another proc unit: watched again)
                    oa_call(c, FN_ACTOR_WAKE, a, 0, 0, 0, 0, 0);
                    g_mv[t].w = oa_watch(a);
                }
                uint64_t body = *(uint64_t *)(uintptr_t)(a + 0x190);
                if (oa_has_body(a)) oa_call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)o->m, 0, 0, 0, 0);
                oa_call(c, FN_SET_MTX, a, (uint64_t)(uintptr_t)o->m, 0, 0, 0, 0);
                if (o->as[0] && strcmp(oa_playing(c, a, 0, 0), o->as)) {
                    bool ok = oa_play_as(c, a, o->as, 0, 0, -1, false);
                    if (log_on) LOG("[online] %s's mechanism %u: AS %s%s\n", pl[i].name, o->hash, o->as, ok ? "" : " (not played)");
                }
                if (log_on && !g_mv[t].theirs_t) LOG("[online] %s moves the mechanism %u here\n", pl[i].name, o->hash);
                memcpy(g_mv[t].last, (const float *)(uintptr_t)(a + 0x398), sizeof g_mv[t].last);
                snprintf(g_mv[t].as, sizeof g_mv[t].as, "%s", oa_playing(c, a, 0, 0));
                g_mv[t].theirs_t = now, g_mv[t].quiet = MOVER_QUIET;
                break;
            }
        }
    }
    // ours: those that moved here by themselves lately, the nearest first
    struct { int t; float d; } mv[MOVER_TRACK];
    int nm = 0;
    for (int t = 0; t < MOVER_TRACK; t++) {
        if (!g_mv[t].a) continue;
        if (!oa_alive_rewatch(g_mv[t].a, &g_mv[t].w)) {
            memset(&g_mv[t], 0, sizeof g_mv[t]);
            rescan = true;
            continue;
        }
        const float *m = (const float *)(uintptr_t)(g_mv[t].a + 0x398);
        float lx = m[3] - link[0], ly = m[7] - link[1], lz = m[11] - link[2], d = lx * lx + ly * ly + lz * lz;
        if (g_mv[t].theirs_t && now - g_mv[t].theirs_t < MOVER_THEIRS_NS) continue;
        g_mv[t].theirs_t = 0;
        const char *as = oa_playing(c, g_mv[t].a, 0, 0);
        bool changed = mtx_moved(m, g_mv[t].last) || strcmp(as, g_mv[t].as);
        if (changed && log_on && g_mv[t].quiet >= MOVER_QUIET) {
            char n[48];
            oa_actor_name_of(g_mv[t].a, n);
            LOG("[online] our mechanism %s %u moves (AS %s)\n", n, g_mv[t].hash, as);
        }
        memcpy(g_mv[t].last, m, sizeof g_mv[t].last);
        snprintf(g_mv[t].as, sizeof g_mv[t].as, "%s", as);
        g_mv[t].quiet = changed ? 0 : g_mv[t].quiet + 1;
        if (g_mv[t].quiet >= MOVER_QUIET) continue;
        mv[nm].t = t, mv[nm].d = d, nm++;
    }
    for (int a = 1; a < nm; a++)
        for (int b = a; b > 0 && mv[b].d < mv[b - 1].d; b--) { int tt = mv[b].t; float td = mv[b].d; mv[b] = mv[b - 1]; mv[b - 1].t = tt, mv[b - 1].d = td; }
    memset(oa_g_movers_out, 0, sizeof oa_g_movers_out);
    for (int k = 0; k < nm && k < ONLINE_MOVERS; k++) {
        oa_g_movers_out[k].hash = g_mv[mv[k].t].hash;
        memcpy(oa_g_movers_out[k].m, g_mv[mv[k].t].last, sizeof oa_g_movers_out[k].m);
        snprintf(oa_g_movers_out[k].as, sizeof oa_g_movers_out[k].as, "%s", g_mv[mv[k].t].as);
    }
}

void oa_movers_forget(void) { memset(g_mv, 0, sizeof g_mv), memset(oa_g_movers_out, 0, sizeof oa_g_movers_out); }

// ---- The map's objects broken (protocol 35): a map object (its HashId) within 15 m of the local Link that goes while
// he stays (no load: he moved less than 10 m) was broken by him: a crate, a barrel, a rock a bomb broke, an ore, a
// pot... or a tree he cut (it leaves <tree>_Stump where it stood and <tree>_Trunk, the log, falling). Its HashId goes
// with his state (`broken`, the last 8), and the others' games break the same object of theirs: a tree is cut as the
// game cuts it (it goes, its stump and its trunk are made where it stood, the trunk leaning away from him so it falls
// that way), anything else is broken by the game itself (its life set to 0: its own pieces, effects and drops), or
// goes. His trunk chopped (gone) goes for the others too (`trunks_gone`). Not enemies, players, items, animals.
// SAVAGEWIND_ONLINE_BREAK_TEST=<s> (test): the nearest tree here cut that many seconds in, as the others' are
#define BREAKS 96
static struct { uint64_t a; Watch w; uint32_t hash; float m[12]; char name[48]; } g_brk[BREAKS];
static struct { uint32_t hash; uint64_t trunk; Watch tw; } g_our_cut[8];        // (our cuts: their trunk, followed)
static struct { Handle *h; char name[48]; float m[12]; uint32_t hash; bool trunk, ours; } g_spawn[8];  // (being made)
static bool g_spawn_ours;  // (the next spawn_at's trunk ours, as a cut of ours: the break test's)
static struct { uint32_t hash; uint64_t a; Watch w; uint64_t t; } g_their_trunk[8];  // (the others' cuts: the trunk made here)
static struct { uint64_t a; Watch w; uint64_t t; } g_breaking[16];  // (broken here for another: gone, or taken away)

static uint64_t trunk_made_t(uint64_t a) {
    for (int k = 0; k < 8; k++)
        if (g_their_trunk[k].a == a) return g_their_trunk[k].t;
    return 0;
}

static int trunks_near(const float p[3], float r, uint64_t *a, uint32_t *h, int max) {
    int n = 0;
    for (int k = 0; k < 16 && n < max; k++) {
        uint64_t t = k < 8 ? g_our_cut[k].trunk : g_their_trunk[k - 8].a;
        uint32_t th = k < 8 ? g_our_cut[k].hash : g_their_trunk[k - 8].hash;
        if (!t || !th || !oa_alive(t, k < 8 ? g_our_cut[k].tw : g_their_trunk[k - 8].w)) continue;
        const float *m = (const float *)(uintptr_t)(t + 0x398);
        float dx = m[3] - p[0], dy = m[7] - p[1], dz = m[11] - p[2];
        if (dx * dx + dy * dy + dz * dz > r * r) continue;
        a[n] = t, h[n] = th, n++;
    }
    return n;
}

// An item the map placed (a HashId: a mushroom, a fruit lying on the ground, a rupee) picked up by one goes from every
// world as a broken object does (the same list and HashId; taken out, not broken, in the others'). Not weapons: those
// lying in the world go as PICKs (protocol 18)
static bool map_item(const char *n) {
    return (!strncmp(n, "Item_", 5) || !strncmp(n, "PutRupee", 8)) && !oa_players_own_item(n);
}

static bool breakable(const char *n) {
    if (!strncmp(n, "Enemy_", 6) || !strncmp(n, "Npc", 3) || !strncmp(n, "Online", 6) || !strncmp(n, "Item_", 5) ||
        !strncmp(n, "Weapon_", 7) || !strncmp(n, "Animal_", 7) || !strncmp(n, "GameRom", 7) || !strcmp(n, "PlayerObj") ||
        strstr(n, "_Stump") || strstr(n, "_Trunk") || strstr(n, "Sweep") || strstr(n, "Sun_") || strstr(n, "Moon_") ||
        strstr(n, "Far") || (strncmp(n, "Obj", 3) && strncmp(n, "Kibako", 6) && strncmp(n, "Barrel", 6) &&
        strncmp(n, "FldObj", 6) && strncmp(n, "TwnObj", 6) && strncmp(n, "DgnObj", 6)))
        return false;
    return true;
}

// A map object far from anything that moves it - a tree, a crate nobody touched - is asleep (its state +0x64 not 1:
// the game updates it never): deleted so, it stayed where it was, drawn, its deletion waiting for an update that never
// came; the game's own cut wakes it with its blows. So it is woken first (1.9.0 0x7001515988: state 1, its process
// manager told), then taken out
static void take_out(Cpu *c, uint64_t a) {
    if (*(uint8_t *)(uintptr_t)(a + 0x64) != 1) oa_call(c, FN_ACTOR_WAKE, a, 0, 0, 0, 0, 0);
    const float *m = (const float *)(uintptr_t)(a + 0x398);
    float at[3] = {m[3], m[7], m[11]}, out[4];
    oa_call(c, FN_MAPOBJ_GONE, a, (uint64_t)(uintptr_t)out, (uint64_t)(uintptr_t)at, 0, 0, 0);
    oa_call(c, FN_TAKE_OUT, a, 0, 0, 0, 0, 0);
}

// A tree cut here for another: woken now, taken out CUT_WAKE_NS later. Asleep, the game draws a tree from its map
// placement's own model, its actor drawing nothing; woken, its first updates hand the drawing to the actor. Taken out
// in the frame it was woken, the placement's tree stayed, drawn standing beside its stump; a moment later, it goes as
// the game's own cut takes it (its blows having woken it long before)
#define CUT_WAKE_NS 400000000ull
static struct { uint64_t a; Watch w; uint64_t t; } g_cutting[8];
static void cut_later(Cpu *c, uint64_t a) {
    if (*(uint8_t *)(uintptr_t)(a + 0x64) != 1) oa_call(c, FN_ACTOR_WAKE, a, 0, 0, 0, 0, 0);
    for (int k = 0; k < 8; k++)
        if (!g_cutting[k].a) {
            g_cutting[k].a = a, g_cutting[k].w = oa_watch(a), g_cutting[k].t = plat_now_ns();
            return;
        }
    take_out(c, a);  // (none free: at once)
}
static void cutting_update(Cpu *c) {
    for (int k = 0; k < 8; k++) {
        if (!g_cutting[k].a || plat_now_ns() - g_cutting[k].t < CUT_WAKE_NS) continue;
        if (oa_alive(g_cutting[k].a, g_cutting[k].w)) take_out(c, g_cutting[k].a);
        memset(&g_cutting[k], 0, sizeof g_cutting[k]);
    }
}

static void spawn_at(Cpu *c, const char *name, const float m[12], uint32_t hash, bool trunk) {
    for (int k = 0; k < 8; k++) {
        if (g_spawn[k].h) continue;
        Handle *h = oa_new_handle();
        if (!oa_request(c, name, h)) return;
        g_spawn[k].h = h, g_spawn[k].hash = hash, g_spawn[k].trunk = trunk, g_spawn[k].ours = g_spawn_ours;
        snprintf(g_spawn[k].name, sizeof g_spawn[k].name, "%s", name);
        memcpy(g_spawn[k].m, m, sizeof g_spawn[k].m);
        return;
    }
}

// The session's world: every map object broken in it, by anyone (its HashId; where the breaker was), kept while the
// session lasts so it stays broken in every game: an object of it that comes - near again, made again by a load or
// a game over - is broken again (world_enforce), and the list goes round in our pose's broken slots when nothing new
// is broken, so a player who joins later gets it all (world_rotate)
#define WORLD_MAX 2048
static struct { uint32_t h; uint64_t t; float from[3]; uint64_t a; Watch w; } g_world[WORLD_MAX];
static int g_nworld;
static int16_t g_widx[4096];  // (index + 1, by HashId: open addressing)
static uint64_t g_broken_new_t;  // (when we last broke something: world_rotate waits for quiet)

static int world_find(uint32_t h) {
    for (int k = 0; k < 4096; k++) {
        int16_t v = g_widx[(h + k) & 4095];
        if (!v) return -1;
        if (g_world[v - 1].h == h) return v - 1;
    }
    return -1;
}
static bool world_has(uint32_t h) { return world_find(h) >= 0; }
static void world_add(uint32_t h, const float from[3]) {
    if (!h || world_has(h) || g_nworld >= WORLD_MAX) return;
    g_world[g_nworld].h = h, g_world[g_nworld].t = plat_now_ns(), g_world[g_nworld].a = 0;
    memcpy(g_world[g_nworld].from, from, sizeof g_world[g_nworld].from);
    for (int k = 0; k < 4096; k++)
        if (!g_widx[(h + k) & 4095]) {
            g_widx[(h + k) & 4095] = (int16_t)(g_nworld + 1);
            break;
        }
    g_nworld++;
}
static void world_seen(uint32_t h, uint64_t a) {  // (the actor of it broken now: not again while it lasts)
    int k = world_find(h);
    if (k >= 0) g_world[k].a = a, g_world[k].w = oa_watch(a);
}
static void world_clear(void) {
    if (g_nworld) LOG("[online] the session's world list cleared (%d broken)\n", g_nworld);
    g_nworld = 0;
    memset(g_widx, 0, sizeof g_widx);
}

// A map object broken here (by another player, or again after it came back - a load, a game over): as the game
// breaks it, a tree cut (its stump; its trunk leaning away from the cutter, trunk_too) or anything else its life 0
static void break_here(Cpu *c, uint64_t a, const char *n, uint32_t h, const float from[3], bool trunk_too, const char *who) {
    for (int t = 0; t < BREAKS; t++)  // (not ours to send)
        if (g_brk[t].a == a) memset(&g_brk[t], 0, sizeof g_brk[t]);
    float m[12];
    memcpy(m, (const float *)(uintptr_t)(a + 0x398), sizeof m);
    float at[3] = {m[3], m[7], m[11]};
    if (!map_item(n)) oa_drop_hush(at);  // (what ours drops of it isn't his: his comes as his loot)
    if (!strncmp(n, "Obj_Tree", 8)) {  // (cut as the game cuts it: its stump, its trunk leaning away from him)
        char stump[64], trunk[64];
        snprintf(stump, sizeof stump, "%s_Stump", n), snprintf(trunk, sizeof trunk, "%s_Trunk", n);
        cut_later(c, a);
        spawn_at(c, stump, m, h, false);
        float ax = m[3] - from[0], az = m[11] - from[2], al = sqrtf(ax * ax + az * az);
        float tm[12];
        memcpy(tm, m, sizeof tm);
        if (al > 0.01f) {  // (tilted 8 degrees from upright toward (ax, az): about the axis up x that, Rodrigues)
            ax /= al, az /= al;
            const float sn = 0.139f, cs = 0.990f;
            float rot[9] = {cs + az * az * (1 - cs), ax * sn, -ax * az * (1 - cs),
                            -ax * sn, cs, -az * sn,
                            -ax * az * (1 - cs), az * sn, cs + ax * ax * (1 - cs)};
            for (int r = 0; r < 3; r++)  // (the tilt times the tree's own turn)
                for (int q2 = 0; q2 < 3; q2++)
                    tm[4 * r + q2] = rot[3 * r] * m[q2] + rot[3 * r + 1] * m[4 + q2] + rot[3 * r + 2] * m[8 + q2];
            tm[7] += 0.3f;
        }
        if (trunk_too) spawn_at(c, trunk, tm, h, true);
        char field[32], flag[128];
        snprintf(field, sizeof field, "%s", oa_g_map[0] ? oa_g_map : "MainField");
        field[strcspn(field, "/")] = 0;
        snprintf(flag, sizeof flag, "%s_%s_%u", field, n, h);
        uint32_t fh = oa_crc32_str(flag);
        uint32_t *fv = oa_bool_flag(fh);
        if (fv && !(*fv & 1) && !oa_set_bool_flag(c, fh)) *fv |= 1;
        LOG("[online] %s cut %s (%u): cut here too%s\n", who, n, h, fv ? ", its flag set" : "");
    } else if (map_item(n)) {  // (an item he picked up: taken out of the world here, its map object marked gone)
        take_out(c, a);
        char field[32], flag[128];
        snprintf(field, sizeof field, "%s", oa_g_map[0] ? oa_g_map : "MainField");
        field[strcspn(field, "/")] = 0;
        snprintf(flag, sizeof flag, "%s_%s_%u", field, n, h);
        uint32_t fh = oa_crc32_str(flag);
        uint32_t *fv = oa_bool_flag(fh);
        if (fv && !(*fv & 1) && !oa_set_bool_flag(c, fh)) *fv |= 1;
        LOG("[online] %s picked up %s (%u): gone here too%s\n", who, n, h, fv ? ", its flag set" : "");
    } else {  // (the game breaks it: its life to 0; one that stays is taken away a moment later)
        int32_t *life = (int32_t *)(uintptr_t)(a + ENEMY_LIFE);
        if (*(uint8_t *)(uintptr_t)(a + 0x64) != 1) oa_call(c, FN_ACTOR_WAKE, a, 0, 0, 0, 0, 0);  // (asleep: life 0 broke nothing)
        *life = 0;
        for (int q2 = 0; q2 < 16; q2++)  // (one that doesn't break - asleep, no life of its own - goes in 1.5 s)
            if (!g_breaking[q2].a) {
                g_breaking[q2].a = a, g_breaking[q2].w = oa_watch(a), g_breaking[q2].t = plat_now_ns();
                break;
            }
        // (and its flag set, <field>_<actor>_<HashId>, as a blow of ours would: else the game makes it again)
        char field[32], flag[128];
        snprintf(field, sizeof field, "%s", oa_g_map[0] ? oa_g_map : "MainField");
        field[strcspn(field, "/")] = 0;
        snprintf(flag, sizeof flag, "%s_%s_%u", field, n, h);
        uint32_t fh = oa_crc32_str(flag);
        uint32_t *fv = oa_bool_flag(fh);
        if (fv && !(*fv & 1) && !oa_set_bool_flag(c, fh)) *fv |= 1;
        LOG("[online] %s broke %s (%u): broken here too%s\n", who, n, h, fv ? ", its flag set" : "");
    }
}

// The world's broken objects that are here again (a load, a game over made them again; one far away came near): broken
// again, each actor of them once (its trunk only for a cut of the last WORLD_TRUNK_NS: an older one's trunk went)
#define WORLD_TRUNK_NS 20000000000ull
static void world_enforce(Cpu *c, uint64_t player, const float link[3]) {
    if (!g_nworld) return;
    int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
    for (int k = 0; k < na; k++) {
        uint64_t a = oa_g_actors[k];
        uint32_t h = *(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID);
        int w = h ? world_find(h) : -1;
        if (w < 0 || (g_world[w].a == a && oa_alive(a, g_world[w].w))) continue;
        if (*(uint8_t *)(uintptr_t)(a + 0x64) == 3) continue;  // (going already)
        char n[48];
        oa_actor_name_of(a, n);
        if (!breakable(n) && !map_item(n)) continue;
        bool cutting = false;
        for (int q = 0; q < 8; q++) cutting |= g_cutting[q].a == a;
        if (cutting) continue;
        g_world[w].a = a, g_world[w].w = oa_watch(a);
        break_here(c, a, n, h, g_world[w].from, plat_now_ns() - g_world[w].t < WORLD_TRUNK_NS, "the world's list:");
    }
}

// While we break nothing new, the world's list goes round in our pose's last broken slot, one each WORLD_ROTATE_NS:
// a player who joined later gets what was broken before him (each game takes a HashId once: world_has)
#define WORLD_ROTATE_NS 1500000000ull
static void world_rotate(void) {
    static int next;
    static uint64_t at;
    uint64_t now = plat_now_ns();
    if (!g_nworld || now - g_broken_new_t < 3 * WORLD_ROTATE_NS || now - at < WORLD_ROTATE_NS) return;
    at = now;
    next %= g_nworld;
    oa_g_broken[7] = g_world[next++].h;
}

void oa_breaks_update(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me) {
    static int tick;
    static float was[3];
    if (online_state() != ONLINE_CONNECTED) world_clear();
    cutting_update(c);
    world_rotate();
    if (tick % 60 == 30) world_enforce(c, player, link);  // (about once a second)
    // what is being made (the others' stumps and trunks): placed once ready
    for (int k = 0; k < 8; k++) {
        if (!g_spawn[k].h) continue;
        int *unit = (int *)(uintptr_t)g_spawn[k].h->unit;
        if (!unit) continue;
        if (*unit == 3 || *unit == 5) {
            oa_call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)g_spawn[k].h, 0, 0, 0, 0, 0);
            memset(&g_spawn[k], 0, sizeof g_spawn[k]);
            continue;
        }
        if (*unit != 2) continue;
        uint64_t a = oa_call(c, FN_HANDLE_TAKE, (uint64_t)(uintptr_t)g_spawn[k].h, 0, 0, 0, 0, 0);
        if (a) {
            oa_call(c, FN_SET_MTX, a, (uint64_t)(uintptr_t)g_spawn[k].m, 0, 0, 0, 0);
            if (g_spawn[k].trunk && g_spawn[k].ours) {  // (the break test's own cut: its trunk ours, as the game's cut's)
                memmove(g_our_cut + 1, g_our_cut, sizeof g_our_cut - sizeof *g_our_cut);
                g_our_cut[0].hash = g_spawn[k].hash, g_our_cut[0].trunk = a, g_our_cut[0].tw = oa_watch(a);
            } else if (g_spawn[k].trunk)
                for (int t = 0; t < 8; t++)
                    if (!g_their_trunk[t].a || g_their_trunk[t].hash == g_spawn[k].hash) {
                        g_their_trunk[t].hash = g_spawn[k].hash, g_their_trunk[t].a = a, g_their_trunk[t].w = oa_watch(a);
                        g_their_trunk[t].t = plat_now_ns();
                        break;
                    }
            LOG("[online] made here: %s\n", g_spawn[k].name);
        }
        memset(&g_spawn[k], 0, sizeof g_spawn[k]);
    }
    for (int k = 0; k < 16; k++) {
        if (!g_breaking[k].a || plat_now_ns() - g_breaking[k].t < 1500000000ull) continue;
        if (oa_alive(g_breaking[k].a, g_breaking[k].w)) {
            char n[48];
            oa_actor_name_of(g_breaking[k].a, n);
            LOG("[online] %s didn't break: taken away\n", n);
            take_out(c, g_breaking[k].a);
        }
        memset(&g_breaking[k], 0, sizeof g_breaking[k]);
    }
    if (tick++ % 6) return;
    float mx = link[0] - was[0], mz = link[2] - was[2];
    bool jumped = mx * mx + mz * mz > 10.0f * 10.0f;  // (a load, a teleport: what went isn't broken)
    memcpy(was, link, sizeof was);
    int na = -1;
    // ours: gone while we stood by (a tree: its stump is there now)
    for (int k = 0; k < BREAKS; k++) {
        if (!g_brk[k].a) continue;
        float dx = g_brk[k].m[3] - link[0], dz = g_brk[k].m[11] - link[2], d2 = dx * dx + dz * dz;
        if (oa_alive(g_brk[k].a, g_brk[k].w) && d2 < 20.0f * 20.0f) continue;
        if (!oa_alive(g_brk[k].a, g_brk[k].w) && !jumped && d2 < 15.0f * 15.0f && g_brk[k].hash) {
            bool tree = !strncmp(g_brk[k].name, "Obj_Tree", 8);
            bool item = map_item(g_brk[k].name);
            bool cut = !tree && (!item || d2 < 3.5f * 3.5f);  // (an item: gone with our Link by it, picked up)
            if (tree) {  // (a tree that went whole, out of a load, isn't cut: its stump tells)
                char stump[64];
                snprintf(stump, sizeof stump, "%s_Stump", g_brk[k].name);
                if (na < 0) na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
                for (int q = 0; q < na && !cut; q++) {
                    char n[64];
                    oa_actor_name_of(oa_g_actors[q], n);
                    if (strcmp(n, stump)) continue;
                    const float *m = (const float *)(uintptr_t)(oa_g_actors[q] + 0x398);
                    float sx = m[3] - g_brk[k].m[3], sz = m[11] - g_brk[k].m[11];
                    cut = sx * sx + sz * sz < 1.0f;
                }
                char trunk[64];  // (its trunk, followed: chopped, it goes for the others too)
                snprintf(trunk, sizeof trunk, "%s_Trunk", g_brk[k].name);
                for (int q = 0; cut && q < na; q++) {
                    char n[64];
                    oa_actor_name_of(oa_g_actors[q], n);
                    if (strcmp(n, trunk)) continue;
                    const float *m = (const float *)(uintptr_t)(oa_g_actors[q] + 0x398);
                    float sx = m[3] - g_brk[k].m[3], sz = m[11] - g_brk[k].m[11];
                    if (sx * sx + sz * sz > 6.0f * 6.0f) continue;
                    bool known = false;  // (the same cut seen again: once)
                    for (int o = 0; o < 8 && !known; o++) known = g_our_cut[o].hash == g_brk[k].hash && g_our_cut[o].trunk;
                    if (known) break;
                    memmove(g_our_cut + 1, g_our_cut, sizeof g_our_cut - sizeof *g_our_cut);
                    g_our_cut[0].hash = g_brk[k].hash, g_our_cut[0].trunk = oa_g_actors[q], g_our_cut[0].tw = oa_watch(oa_g_actors[q]);
                    break;
                }
            }
            static uint32_t told[256];  // (each once: a bush cut grows again as itself, and went again and again)
            static int ntold;
            // (one of the world's list already - another's, broken here for him, or ours again after a load - isn't
            // told as ours: else each game sent the other's cut back to it, and it made the tree's stump and trunk
            // again, a pile of them)
            if (cut && world_has(g_brk[k].hash)) cut = false;
            for (int q = 0; q < ntold && cut; q++) cut = told[q] != g_brk[k].hash;
            if (cut) told[ntold++ % 256] = g_brk[k].hash, ntold = ntold > 256 ? 256 : ntold;
            if (cut) {
                memmove(oa_g_broken + 1, oa_g_broken, sizeof oa_g_broken - sizeof *oa_g_broken);
                oa_g_broken[0] = g_brk[k].hash;
                g_broken_new_t = plat_now_ns();
                world_add(g_brk[k].hash, link);
                LOG("[online] we %s %s (%u)\n", tree ? "cut" : item ? "picked up" : "broke", g_brk[k].name, g_brk[k].hash);
            }
        }
        memset(&g_brk[k], 0, sizeof g_brk[k]);
    }
    for (int k = 0; k < 8; k++)  // (our trunks chopped)
        if (g_our_cut[k].trunk && !oa_alive(g_our_cut[k].trunk, g_our_cut[k].tw)) {
            memmove(oa_g_trunks_gone + 1, oa_g_trunks_gone, sizeof oa_g_trunks_gone - sizeof *oa_g_trunks_gone);
            oa_g_trunks_gone[0] = g_our_cut[k].hash;
            LOG("[online] our trunk of %u chopped\n", g_our_cut[k].hash);
            g_our_cut[k].trunk = 0;
        }
    if (tick % 5 == 1) {  // (those near us now: twice a second - a crate struck at once went untracked at 3 s)
        if (na < 0) na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        for (int k = 0; k < na; k++) {
            uint64_t a = oa_g_actors[k];
            uint32_t hash = *(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID);
            if (!hash) continue;
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float dx = m[3] - link[0], dz = m[11] - link[2];
            if (dx * dx + dz * dz > 18.0f * 18.0f) continue;
            int f = -1, free_k = -1;
            for (int t = 0; t < BREAKS && f < 0; t++) {
                if (g_brk[t].a == a) f = t;
                else if (free_k < 0 && !g_brk[t].a) free_k = t;
            }
            if (f >= 0 || free_k < 0) continue;
            char n[48];
            oa_actor_name_of(a, n);
            if (!breakable(n) && !map_item(n)) continue;
            g_brk[free_k].a = a, g_brk[free_k].w = oa_watch(a), g_brk[free_k].hash = hash;
            memcpy(g_brk[free_k].m, m, sizeof g_brk[free_k].m);
            snprintf(g_brk[free_k].name, sizeof g_brk[free_k].name, "%s", n);
        }
    }
    // the others': each new one broken here too (the same map object, by its HashId)
    // The others' breaks (each HashId once): kept in the session's world list and broken here - at once when the
    // object is here, else when it comes (world_enforce: a tree far from our Link is no actor until he comes near)
    for (int i = 0; i < ONLINE_MAX; i++) {
        if (i == me || !pl[i].used || !pl[i].pose.valid || !oa_same_map(&pl[i])) continue;
        for (int q = 0; q < 8; q++) {
            uint32_t h = pl[i].pose.broken[q];
            if (!h || world_has(h)) continue;
            world_add(h, pl[i].pos);
            if (na < 0) na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
            uint64_t a = 0;
            char n[48] = "";
            for (int k = 0; k < na && !a; k++) {
                if (*(uint32_t *)(uintptr_t)(oa_g_actors[k] + ACTOR_HASH_ID) != h) continue;
                oa_actor_name_of(oa_g_actors[k], n);
                if (breakable(n) || map_item(n)) a = oa_g_actors[k];
            }
            if (!a) {
                LOG("[online] %s broke %u: not here yet (broken here when it comes)\n", pl[i].name, h);
                continue;
            }
            world_seen(h, a);
            break_here(c, a, n, h, pl[i].pos, true, pl[i].name);
        }
        for (int q = 0; q < 4; q++) {  // (his trunks chopped: ours of them go)
            uint32_t h = pl[i].pose.trunks_gone[q];
            for (int t = 0; h && t < 8; t++)
                if (g_their_trunk[t].hash == h && g_their_trunk[t].a) {
                    if (oa_alive(g_their_trunk[t].a, g_their_trunk[t].w)) oa_call(c, FN_DELETE, g_their_trunk[t].a, 0, 0, 0, 0, 0);
                    LOG("[online] %s chopped the trunk of %u: gone here too\n", pl[i].name, h);
                    memset(&g_their_trunk[t], 0, sizeof g_their_trunk[t]);
                }
        }
    }
    {   // (test: SAVAGEWIND_ONLINE_DROP_TEST=<s>[,<actor>]: that many seconds in, an item - Item_Fruit_A unless named -
        // made 1.5 m before our Link, as if it fell there: what drops_update gives everyone)
        const char *dt = getenv("SAVAGEWIND_ONLINE_DROP_TEST");
        static uint64_t d0;
        static bool dropped;
        if (dt && !d0) d0 = plat_now_ns();
        if (dt && !dropped && plat_now_ns() - d0 > (uint64_t)(atof(dt) * 1e9)) {
            dropped = true;
            const char *nm = strchr(dt, ',') ? strchr(dt, ',') + 1 : "Item_Fruit_A";
            const float *pm = (const float *)(uintptr_t)(player + 0x398);
            float m[12] = {1, 0, 0, pm[3] + 1.5f * pm[2], 0, 1, 0, pm[7] + 1.0f, 0, 0, 1, pm[11] + 1.5f * pm[10]};
            spawn_at(c, nm, m, 0, false);
            LOG("[online] drop test: %s at %.1f %.1f %.1f\n", nm, m[3], m[7], m[11]);
        }
    }
    // (test: the nearest tree cut here, as the others' are, that many seconds in)
    const char *bt = getenv("SAVAGEWIND_ONLINE_BREAK_TEST");
    static uint64_t t0;
    static bool tested;
    if (bt && !t0) t0 = plat_now_ns();
    if (bt && !tested && plat_now_ns() - t0 > (uint64_t)(atof(bt) * 1e9)) {  // (none near yet: again later)
        float bd = 1e30f;
        int best = -1;
        for (int k = 0; k < BREAKS; k++) {
            if (!g_brk[k].a || strncmp(g_brk[k].name, "Obj_Tree", 8)) continue;
            float dx = g_brk[k].m[3] - link[0], dz = g_brk[k].m[11] - link[2];
            if (dx * dx + dz * dz < bd) bd = dx * dx + dz * dz, best = k;
        }
        if (best >= 0) {
            tested = true;
            char stump[64], trunk[64];
            snprintf(stump, sizeof stump, "%s_Stump", g_brk[best].name);
            snprintf(trunk, sizeof trunk, "%s_Trunk", g_brk[best].name);
            LOG("[online] break test: %s cut here\n", g_brk[best].name);
            g_break_test_t = plat_now_ns();
            cut_later(c, g_brk[best].a);
            spawn_at(c, stump, g_brk[best].m, 0, false);
            g_spawn_ours = true;
            spawn_at(c, trunk, g_brk[best].m, g_brk[best].hash, true);  // (its trunk ours, known by the tree, as a cut)
            g_spawn_ours = false;
            // (and sent as our cut: its stump is made a few frames later, after the check that would have told it)
            memmove(oa_g_broken + 1, oa_g_broken, sizeof oa_g_broken - sizeof *oa_g_broken);
            oa_g_broken[0] = g_brk[best].hash;
            g_broken_new_t = plat_now_ns();
            world_add(g_brk[best].hash, link);
            world_seen(g_brk[best].hash, g_brk[best].a);  // (cut here now: not again by world_enforce, a second trunk)
            memset(&g_brk[best], 0, sizeof g_brk[best]);
        } else {
            static bool told;
            if (!told) told = true, LOG("[online] break test: no tree near yet\n");
        }
    }
}

// rt_online_frame, step 14: the world's objects, movers and breaks
void oa_world_frame(Cpu *c, uint64_t player, const float link[3], int me) {
    if (VALID(player)) {
        OnlinePlayer op[ONLINE_MAX];
        online_players(op);
        oa_objs_update(c, player, link, op, me);
        oa_movers_update(c, player, link, op, me);
        oa_breaks_update(c, player, link, op, me);
    }
}
