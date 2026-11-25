// Online play's actors, oa_loot: the loot: what falls, the host's list of it, what each player carries; the items on
// the ground. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"

// ---- Loot (protocol 15): what our enemies drop when they die here (the items that come near where one died in the
// seconds after, its weapon and shield) goes on everyone's list, kept by the host's game (each by an id of its own,
// the game it came from and its actor's id there); the others make each one where it is, a real item they can pick
// up. One picked up anywhere (gone from where it was with a Link next to it) is taken off the list, and goes in every
// game, ours too
// (not the Sheikah Slate on his hip nor the rune's glove: the player's own, Item_* actors too)
bool players_own_item(const char *n) {
    return !strcmp(n, "Item_Conductor") || !strncmp(n, "Item_Magnetglove", 16);
}
static bool is_loot(const char *n) {
    return (!strncmp(n, "Item_", 5) || !strncmp(n, "Weapon_", 7) || !strncmp(n, "PutRupee", 8)) && !players_own_item(n);
}
// loot named by another game (a field of cap bytes as it came): on its kind's list, and not the player's own
static bool loot_name_ok(const char *n, size_t cap) { return loot_name_listed(n, cap) && !players_own_item(n); }

#define DEATHS 8
static struct { float pos[3]; int frames; uint32_t before[48]; int nbefore; } g_death[DEATHS];  // (frames: watched)
#define MINE 32
// (picked: in this Link's hand now, taken off the list once it is on it)
// (thrown: his weapon thrown, on the list as it left his hand; gone (broken on a blow) it is taken off wherever it is)
static struct { uint32_t lid; uint64_t actor; Watch w; char name[40]; bool sent, listed, picked, thrown; uint32_t gid; float pos[3]; } g_mine[MINE];
uint64_t g_carried_now[LINK_CARRIED];  // (this Link's, as loot_update saw them last)
static OnlineLoot g_loot_list[ONLINE_LOOT];  // (the host's game: the list)
static int g_nloot_list;
static uint32_t g_next_gid = 1;
// What falls to the ground here - an ore's, a crate's or a pot's insides, a tree's fruit, what he drops from his
// pouch, a rupee - is everyone's loot too (drops_update): a new item (Item_*, Weapon_*, PutRupee*) near our Link that
// the map didn't place (no HashId), not ours already nor one of the others' made here, on the list once it has been
// there DROP_SETTLE_NS (the enemies' loot and a thrown weapon go on it their own way first). Where our game breaks
// something for another (break_here: his ore, his crate), what it drops is its own and his comes on the list: ours
// of it are taken away (g_drop_hush, DROP_HUSH_NS round where it broke)
#define DROP_SEEN 128
#define DROP_SETTLE_NS 400000000ull
#define DROP_HUSH_NS 5000000000ull
static struct { uint32_t id; uint64_t t; float pos[3]; } g_drop_seen[DROP_SEEN];  // (t: since it lay still there)
static struct { float pos[3]; uint64_t until; } g_drop_hush[8];
void drop_hush(const float p[3]) {
    int k = 0;
    for (int q = 1; q < 8; q++)
        if (g_drop_hush[q].until < g_drop_hush[k].until) k = q;
    memcpy(g_drop_hush[k].pos, p, 12), g_drop_hush[k].until = plat_now_ns() + DROP_HUSH_NS;
}

void loot_forget(void) {
    for (int k = 0; k < THEIRS; k++) orphan(&g_theirs[k].p);
    memset(g_death, 0, sizeof g_death), memset(g_mine, 0, sizeof g_mine), memset(g_theirs, 0, sizeof g_theirs);
    g_nloot_list = 0;
}

// one of ours died here (at pos): the items near it now are no loot of its
static void loot_death(const float pos[3], int na) {
    int d = 0;
    while (d < DEATHS && g_death[d].frames) d++;
    if (d == DEATHS) return;
    memcpy(g_death[d].pos, pos, 12);
    g_death[d].frames = 300, g_death[d].nbefore = 0;  // (10 s)
    for (int k = 0; k < na && g_death[d].nbefore < 48; k++) {
        char n[48];
        uint64_t a = g_actors[k];
        actor_name_of(a, n);
        if (!is_loot(n)) continue;
        const float *m = (const float *)(uintptr_t)(a + 0x398);
        float dx = m[3] - pos[0], dy = m[7] - pos[1], dz = m[11] - pos[2];
        if (dx * dx + dy * dy + dz * dz < 8 * 8) g_death[d].before[g_death[d].nbefore++] = *(uint32_t *)(uintptr_t)(a + 0x60);
    }
}

void loot_mine_add(uint64_t a, const char *n, int me) {
    uint32_t id = *(uint32_t *)(uintptr_t)(a + 0x60);
    for (int k = 0; k < MINE; k++)
        if (g_mine[k].actor && g_mine[k].lid == id) return;
    for (int k = 0; k < MINE; k++)
        if (!g_mine[k].actor) {
            memset(&g_mine[k], 0, sizeof g_mine[k]);
            g_mine[k].lid = id, g_mine[k].actor = a, g_mine[k].w = watch(a);
            snprintf(g_mine[k].name, sizeof g_mine[k].name, "%s", n);
            LOG("[online] loot of ours: %s %u\n", n, id);
            (void)me;
            return;
        }
}

void loot_mine_thrown(uint64_t a, const char *n) {
    loot_mine_add(a, n, 0);
    uint32_t id = *(uint32_t *)(uintptr_t)(a + 0x60);
    for (int k = 0; k < MINE; k++)
        if (g_mine[k].actor == a && g_mine[k].lid == id) g_mine[k].thrown = true;
}

static bool near_link(const float link[3], const float p[3], float r) {
    float dx = p[0] - link[0], dy = p[1] - link[1], dz = p[2] - link[2];
    return dx * dx + dy * dy + dz * dz < r * r;
}

// What the local Link carries: his weapon, shield and bow (+0xb98, +0xbb0, +0xbc8) and the weapon's and bow's sheath
// and quiver (+0x958 of each). A weapon picked up into a free hand is the same actor that lay there, not deleted:
// a pickup is also one that comes to be in this list
void link_carried(uint64_t player, uint64_t out[LINK_CARRIED]) {
    memset(out, 0, LINK_CARRIED * sizeof *out);
    for (int k = 0; k < 3; k++) out[k] = linked(player + 0xb98 + 0x18 * k);
    out[3] = out[0] ? linked(out[0] + 0x958) : 0, out[4] = out[2] ? linked(out[2] + 0x958) : 0;
}

bool carries(const uint64_t carried[LINK_CARRIED], uint64_t a) {
    for (int k = 0; a && k < LINK_CARRIED; k++)
        if (carried[k] == a) return true;
    return false;
}

// the host's game: its list, from what came (the others' adds and takes) and its own
static void loot_host_take(uint32_t gid) {
    for (int k = 0; k < g_nloot_list; k++)
        if (g_loot_list[k].gid == gid) {
            LOG("[online] loot %u (%s) taken\n", gid, g_loot_list[k].name);
            g_loot_list[k] = g_loot_list[--g_nloot_list];
            return;
        }
}

static void loot_host_add(const OnlineLoot *l) {
    if (!loot_name_ok(l->name, sizeof l->name)) {  // (on no one's list: an item, a weapon or a rupee of the lists only)
        name_refused(&g_refused_loot, "loot", l->name);
        return;
    }
    for (int k = 0; k < g_nloot_list; k++)
        if (g_loot_list[k].origin == l->origin && g_loot_list[k].lid == l->lid) return;
    if (g_nloot_list == ONLINE_LOOT) return;  // (full: this one stays only where it fell; taking one off the list
                                              // would have it go in every game, as one picked up)
    g_loot_list[g_nloot_list] = *l;
    g_loot_list[g_nloot_list++].gid = g_next_gid++;
}

static bool loot_actor(uint64_t a);
void loot_update(Cpu *c, uint64_t player, const float link[3], int me, bool host) {
    static int tick;
    if (tick++ % 3) return;
    uint64_t carried[LINK_CARRIED];
    link_carried(player, carried);
    memcpy(g_carried_now, carried, sizeof g_carried_now);
    // our owned enemies dying: watched for what they drop
    int na = 0;
    bool scanned = false;
    for (int k = 0; k < OWNS; k++) {
        Own *o = &g_own[k];
        if (o->state != OWN_HAVE) continue;
        bool there = alive(o->actor, o->w);
        int32_t life = there ? *(int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE) : 0;
        if (there) {
            const float *am = (const float *)(uintptr_t)(o->actor + 0x398);
            o->last_pos[0] = am[3], o->last_pos[1] = am[7], o->last_pos[2] = am[11];
        }
        for (int h = 0; there && life > 0 && h < 2; h++) {  // (what it holds now: dropped as it dies, unlinked then)
            char n[48];
            uint64_t g2 = linked(o->actor + 0xb98 + 0x18 * h);
            actor_name_of(g2, n);
            if (!strncmp(n, "Weapon_", 7) && g2 != o->held[h]) o->held[h] = g2, o->held_w[h] = watch(g2);
        }
        // (died: seen with no life left, its body there for seconds; gone without that, the game removed it alive)
        if (o->died || !there || life > 0) continue;
        o->died = true;
        if (!scanned) na = all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), g_actors, MAX_ACTORS), scanned = true;
        loot_death(o->last_pos, na);
        for (int h = 0; h < 2; h++) {  // (its weapon and shield: they fall)
            char n[48];
            if (!o->held[h] || !alive(o->held[h], o->held_w[h])) continue;
            actor_name_of(o->held[h], n);
            if (!strncmp(n, "Weapon_", 7)) loot_mine_add(o->held[h], n, me);
        }
        LOG("[online] %u died here: its loot watched\n", o->gid);
    }
    // the items that come near where one died
    bool watching = false;
    for (int d = 0; d < DEATHS; d++) watching |= g_death[d].frames > 0;
    if (watching) {
        if (!scanned) na = all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), g_actors, MAX_ACTORS);
        for (int k = 0; k < na; k++) {
            char n[48];
            uint64_t a = g_actors[k];
            actor_name_of(a, n);
            // (one being deleted, state 3, is still in the list: taken again it went and came every few frames)
            if (!is_loot(n) || *(uint8_t *)(uintptr_t)(a + 0x64) == 3) continue;
            uint32_t id = *(uint32_t *)(uintptr_t)(a + 0x60);
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float p[3] = {m[3], m[7], m[11]};
            for (int d = 0; d < DEATHS; d++) {
                if (!g_death[d].frames || !near_link(g_death[d].pos, p, 6)) continue;
                bool old = false;
                for (int b = 0; b < g_death[d].nbefore && !old; b++) old = g_death[d].before[b] == id;
                bool theirs = false;  // (one of the others' we made: not ours)
                for (int t = 0; t < THEIRS && !theirs; t++) theirs = g_theirs[t].p.actor == a;
                if (!old && !theirs) loot_mine_add(a, n, me);
                break;
            }
        }
        for (int d = 0; d < DEATHS; d++)
            if (g_death[d].frames) g_death[d].frames = g_death[d].frames > 3 ? g_death[d].frames - 3 : 0;
    }
    {   // what falls to the ground near us (every 9 frames: what he throws comes for the others soon after it lies)
        static int dt;
        static float was[3];
        static bool base;  // (the first look, and after a load or a teleport: what is there now isn't new)
        if (dt++ % 3 == 0) {
            float jx = link[0] - was[0], jz = link[2] - was[2];
            if (jx * jx + jz * jz > 30.0f * 30.0f) base = false, memset(g_drop_seen, 0, sizeof g_drop_seen);
            memcpy(was, link, sizeof was);
            if (!scanned) na = all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), g_actors, MAX_ACTORS), scanned = true;
            uint64_t now = plat_now_ns();
            for (int k = 0; k < na; k++) {
                uint64_t a = g_actors[k];
                if (*(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID) || *(uint8_t *)(uintptr_t)(a + 0x64) == 3) continue;
                const float *m = (const float *)(uintptr_t)(a + 0x398);
                float p[3] = {m[3], m[7], m[11]};
                if (!near_link(link, p, 15)) continue;
                char n[48];
                actor_name_of(a, n);
                if (!is_loot(n) || carries(carried, a) || loot_actor(a)) continue;
                // (one in his hands - materials he holds from his pouch, over him - isn't on the ground: before, the
                // others saw what he held lying at his feet. It is linked to him then: +0x4d4 0x72, as the slate on
                // his hip; 0 on the ground)
                const float *lm = (const float *)(uintptr_t)(player + 0x398);
                float hx = p[0] - lm[3], hz = p[2] - lm[11];
                bool held = *(uint32_t *)(uintptr_t)(a + 0x4d4) || (hx * hx + hz * hz < 1.5f * 1.5f && p[1] > lm[7] + 0.5f);
                uint32_t id = *(uint32_t *)(uintptr_t)(a + 0x60);
                int f = -1, old = 0;
                for (int q = 0; q < DROP_SEEN && f < 0; q++)
                    if (g_drop_seen[q].id == id) f = q;
                if (f < 0) {  // (first seen: the oldest one forgotten)
                    for (int q = 1; q < DROP_SEEN; q++)
                        if (g_drop_seen[q].t < g_drop_seen[old].t) old = q;
                    g_drop_seen[old].id = id, g_drop_seen[old].t = base ? now : 1;  // (1: there before, never ours)
                    memcpy(g_drop_seen[old].pos, p, sizeof p);
                    bool hushed = false;
                    for (int h = 0; h < 8 && !hushed; h++) hushed = now < g_drop_hush[h].until && near_link(g_drop_hush[h].pos, p, 5);
                    if (base && hushed) {
                        LOG("[online] %s dropped here by what we broke for another: taken away (his comes)\n", n);
                        call(c, FN_DELETE, a, 0, 0, 0, 0, 0);
                        g_drop_seen[old].t = 1;
                    }
                    continue;
                }
                if (g_drop_seen[f].t <= 1) continue;
                // (still a while where it lies, not in his hands: a weapon dropped from his pouch falls and bounces
                // first, and went on the list in the air, made there for the others)
                float mx = p[0] - g_drop_seen[f].pos[0], my = p[1] - g_drop_seen[f].pos[1], mz = p[2] - g_drop_seen[f].pos[2];
                if (held || mx * mx + my * my + mz * mz > 0.05f * 0.05f) {
                    memcpy(g_drop_seen[f].pos, p, sizeof p), g_drop_seen[f].t = now;
                    continue;
                }
                if (now - g_drop_seen[f].t < DROP_SETTLE_NS) continue;
                g_drop_seen[f].t = 1;  // (once)
                LOG("[online] %s fell to the ground here: everyone's\n", n);
                loot_mine_add(a, n, me);
            }
            base = true;
        }
    }
    // the list as it is now
    static OnlineLoot list[ONLINE_LOOT];
    int nl;
    if (host) {
        OnlineLoot add[32];
        uint32_t tk[32];
        int n = online_take_loot_adds(add, 32);
        for (int k = 0; k < n; k++) loot_host_add(&add[k]);
        n = online_take_loot_takes(tk, 32);
        for (int k = 0; k < n; k++) loot_host_take(tk[k]);
    }
    // ours: on the list once (the host's game adds it at once); picked up here (gone, a Link next to where it was):
    // taken off; off the list after being on it: picked up elsewhere, gone here
    for (int k = 0; k < MINE; k++) {
        if (!g_mine[k].actor) continue;
        bool there = alive(g_mine[k].actor, g_mine[k].w);
        if (there) {
            const float *am = (const float *)(uintptr_t)(g_mine[k].actor + 0x398);
            g_mine[k].pos[0] = am[3], g_mine[k].pos[1] = am[7], g_mine[k].pos[2] = am[11];
        }
        if (!g_mine[k].sent) {
            OnlineLoot l = {0};
            l.lid = g_mine[k].lid, l.origin = (uint8_t)me;
            snprintf(l.name, sizeof l.name, "%s", g_mine[k].name);
            const float *m = (const float *)(uintptr_t)(g_mine[k].actor + 0x398);
            if (there) {
                l.pos[0] = m[3], l.pos[1] = m[7], l.pos[2] = m[11];
                for (int r = 0; r < 3; r++)
                    for (int q = 0; q < 3; q++) l.rot[3 * r + q] = m[4 * r + q];
            }
            if (host) loot_host_add(&l);
            else online_send_loot_add(&l);
            g_mine[k].sent = true;
        }
        if (there && carries(carried, g_mine[k].actor)) g_mine[k].picked = true;  // (into his free hand: the same actor)
        if (g_mine[k].picked) {
            if (!g_mine[k].listed) continue;
            LOG("[online] loot of ours %s picked up here, into his hand\n", g_mine[k].name);
            if (host) loot_host_take(g_mine[k].gid);
            else online_send_loot_take(g_mine[k].gid);
            g_mine[k].actor = 0;
            continue;
        }
        if (!there) {
            // (picked up: taken from the others, whatever it is - a weapon, food, a material, a rupee - as in one world)
            if (g_mine[k].listed && (g_mine[k].thrown || near_link(link, g_mine[k].pos, 4))) {
                if (host) loot_host_take(g_mine[k].gid);
                else online_send_loot_take(g_mine[k].gid);
            }
            LOG("[online] loot of ours %s gone here\n", g_mine[k].name);
            g_mine[k].actor = 0;
        }
    }
    nl = host ? g_nloot_list : online_loots(list);
    if (host) memcpy(list, g_loot_list, nl * sizeof *list), online_set_loots(list, nl);
    if (nl < 0) return;
    for (int k = 0; k < MINE; k++) {
        if (!g_mine[k].actor) continue;
        bool on = false;
        for (int j = 0; j < nl && !on; j++)
            if (list[j].origin == me && list[j].lid == g_mine[k].lid) on = true, g_mine[k].gid = list[j].gid;
        if (on) g_mine[k].listed = true;
        else if (g_mine[k].listed) {  // (picked up by another)
            LOG("[online] loot of ours %s picked up elsewhere\n", g_mine[k].name);
            if (alive(g_mine[k].actor, g_mine[k].w) && !carries(carried, g_mine[k].actor))  // (never out of his hand)
                call(c, FN_DELETE, g_mine[k].actor, 0, 0, 0, 0, 0);
            g_mine[k].actor = 0;
        }
    }
    // the others': made here, where it is; picked up here: taken off; off the list: gone here
    for (int j = 0; j < nl; j++) {
        if (list[j].origin == me) continue;
        int t = -1, free_t = -1;
        for (int k = 0; k < THEIRS; k++) {
            if (g_theirs[k].gid == list[j].gid) t = k;
            if (!g_theirs[k].gid && free_t < 0) free_t = k;
        }
        if (t < 0) {
            if (free_t < 0 || !near_link(link, list[j].pos, 150)) continue;
            t = free_t;
            memset(&g_theirs[t], 0, sizeof g_theirs[t]);
            g_theirs[t].gid = list[j].gid;
            memcpy(g_theirs[t].pos, list[j].pos, 12);
            g_theirs[t].origin = list[j].origin;
        }
        if (g_theirs[t].gone) continue;
        Piece *pc = &g_theirs[t].p;
        bool had = pc->actor != 0;
        bool in_hand = had && alive(pc->actor, pc->w) && carries(carried, pc->actor);  // (into his free hand)
        // (gone the moment it was made - made where our Link stood, it was lost at once - isn't picked up: made again)
        if (had && !in_hand && !alive(pc->actor, pc->w) && plat_now_ns() - g_theirs[t].made_t < 1500000000ull &&
            ++g_theirs[t].tries < 5) {
            LOG("[online] loot %u (%s) lost as it was made here: made again\n", list[j].gid, list[j].name);
            memset(pc, 0, sizeof *pc);
            g_theirs[t].retry_t = plat_now_ns() + 300000000ull;
            continue;
        }
        if (had && (in_hand || !alive(pc->actor, pc->w))) {  // (gone here, or his now)
            if (in_hand || near_link(link, g_theirs[t].pos, 4)) {  // (picked up here: gone for everyone)
                LOG("[online] loot %u (%s) picked up here%s\n", list[j].gid, list[j].name, in_hand ? ", into his hand" : "");
                if (host) loot_host_take(list[j].gid);
                else online_send_loot_take(list[j].gid);
            } else
                LOG("[online] loot %u (%s) gone here, %.1f m from Link\n", list[j].gid, list[j].name,
                    sqrtf((g_theirs[t].pos[0] - link[0]) * (g_theirs[t].pos[0] - link[0]) +
                          (g_theirs[t].pos[1] - link[1]) * (g_theirs[t].pos[1] - link[1]) +
                          (g_theirs[t].pos[2] - link[2]) * (g_theirs[t].pos[2] - link[2])));
            g_theirs[t].gone = true;
            memset(pc, 0, sizeof *pc);  // (forgotten, not deleted: in his hand it is his)
            continue;
        }
        if (!had) {
            if (!pc->pending && !pc->name[0]) {  // (asked for; one the game didn't make is asked for again, 5 times)
                if (!loot_name_ok(list[j].name, sizeof list[j].name)) {  // (as it came: one on no list is never made)
                    name_refused(&g_refused_loot, "loot", list[j].name);
                    g_theirs[t].gone = true;
                    continue;
                }
                if (plat_now_ns() < g_theirs[t].retry_t) continue;
                pc->handle = new_handle();
                if (request(c, list[j].name, pc->handle)) pc->pending = true, snprintf(pc->name, sizeof pc->name, "%s", list[j].name);
                else if (++g_theirs[t].tries >= 5) { g_theirs[t].gone = true; continue; }
                else { g_theirs[t].retry_t = plat_now_ns() + 1000000000ull; continue; }
            }
            int *unit = (int *)(uintptr_t)pc->handle->unit;
            if (unit && *unit == 2) {
                pc->pending = false;
                pc->actor = call(c, FN_HANDLE_TAKE, (uint64_t)(uintptr_t)pc->handle, 0, 0, 0, 0, 0);
                if (pc->actor) {
                    pc->w = watch(pc->actor);
                    g_theirs[t].made_t = plat_now_ns();
                    // (as it lies in his game: its rotation with it; upright and a little above, without)
                    float m[12] = {1, 0, 0, list[j].pos[0], 0, 1, 0, list[j].pos[1] + 0.2f, 0, 0, 1, list[j].pos[2]};
                    bool rot = false;
                    for (int q = 0; q < 9; q++) rot |= list[j].rot[q] != 0 && isfinite(list[j].rot[q]);
                    for (int r = 0; rot && r < 3; r++) {
                        for (int q = 0; q < 3; q++) m[4 * r + q] = list[j].rot[3 * r + q];
                        m[4 * r + 3] = list[j].pos[r] + (r == 1 ? 0.02f : 0);
                    }
                    call(c, FN_SET_MTX, pc->actor, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
                    LOG("[online] loot %u (%s) made here at %.2f %.2f %.2f (actor %llx)\n", list[j].gid, list[j].name,
                        list[j].pos[0], list[j].pos[1], list[j].pos[2], (unsigned long long)pc->actor);
                }
            } else if (unit && (*unit == 3 || *unit == 5)) {
                call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)pc->handle, 0, 0, 0, 0, 0);
                pc->pending = false, pc->name[0] = 0;
                if (++g_theirs[t].tries >= 5) g_theirs[t].gone = true;
                else g_theirs[t].retry_t = plat_now_ns() + 1000000000ull;
            }
        } else {  // (where it is now: if it was picked up a moment from now, near it)
            const float *m = (const float *)(uintptr_t)(pc->actor + 0x398);
            g_theirs[t].pos[0] = m[3], g_theirs[t].pos[1] = m[7], g_theirs[t].pos[2] = m[11];
        }
    }
    for (int k = 0; k < THEIRS; k++) {
        if (!g_theirs[k].gid) continue;
        bool on = false;
        for (int j = 0; j < nl && !on; j++) on = list[j].gid == g_theirs[k].gid;
        if (on) continue;
        if (g_theirs[k].p.actor && alive(g_theirs[k].p.actor, g_theirs[k].p.w) && !carries(carried, g_theirs[k].p.actor)) {
            LOG("[online] loot %u picked up elsewhere\n", g_theirs[k].gid);
            call(c, FN_DELETE, g_theirs[k].p.actor, 0, 0, 0, 0, 0);
        } else if (g_theirs[k].p.pending) orphan(&g_theirs[k].p);
        memset(&g_theirs[k], 0, sizeof g_theirs[k]);
    }
}

// ---- Weapons and shields on the ground (protocol 18): one picked up here (gone from where it lay with this Link
// next to it, not one he carries) goes in the others' games too (PICK: its name, where it was; there, the one of
// that name within 2 m of it is deleted). Food and materials each player picks up his own
#define GROUND 24
static struct { uint64_t actor; Watch w; char name[40]; float pos[3]; } g_ground[GROUND];

// one of the loot's (ours or the others' made here): the loot list has it, not the ground's PICK (else a pickup of it
// went twice, and the PICK deleted another of its name within 2 m, a second shield next to it)
static bool loot_actor(uint64_t a) {
    for (int k = 0; k < MINE; k++)
        if (g_mine[k].actor == a) return true;
    for (int k = 0; k < THEIRS; k++)
        if (g_theirs[k].p.actor == a) return true;
    return false;
}

void ground_update(Cpu *c, uint64_t player, const float link[3]) {
    static int tick;
    if (tick++ % 6) return;
    // the others' picks: gone here too
    char pn[16][40];
    float pp[16][3];
    int np = online_take_picks(pn, pp, 16);
    int na = all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), g_actors, MAX_ACTORS);
    uint64_t carried[LINK_CARRIED];
    link_carried(player, carried);
    for (int q = 0; q < np; q++)
        for (int k = 0; k < na; k++) {
            char n[48];
            uint64_t a = g_actors[k];
            actor_name_of(a, n);
            if (strcmp(n, pn[q]) || carries(carried, a) || loot_actor(a)) continue;  // (not his, nor the loot's)
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float p[3] = {m[3], m[7], m[11]};
            if (!near_link(pp[q], p, 2)) continue;
            LOG("[online] %s picked up by another: gone here\n", n);
            call(c, FN_DELETE, a, 0, 0, 0, 0, 0);
            break;
        }
    // ours, watched: picked up (gone, this Link next to where it lay; or in his hand now, the same actor)
    static int wlog;  // (SAVAGEWIND_ONLINE_WEAPONS_LOG=1: every 5 s, the Weapon_* actors within 10 m of Link)
    if (getenv("SAVAGEWIND_ONLINE_WEAPONS_LOG") && wlog++ % 50 == 0)
        for (int k = 0; k < na; k++) {
            char n[48];
            uint64_t a = g_actors[k];
            actor_name_of(a, n);
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float p[3] = {m[3], m[7], m[11]};
            if (strncmp(n, "Weapon_", 7) || !near_link(link, p, 10)) continue;
            LOG("[online] weapon near Link: %s (actor %llx) at %.2f %.2f %.2f, state %u\n", n, (unsigned long long)a, p[0],
                p[1], p[2], *(uint8_t *)(uintptr_t)(a + 0x64));
        }
    // and the enemies we have for real: what they hold, where they are (one of them picks up a weapon lying there, a
    // camp's: the others' games still have it lying where it was, and it would be there twice once it is his loot)
    uint64_t theirs_held[OWNS * 3];
    static float at[OWNS][3];
    const float *ours_at[OWNS];
    int nh = 0, no = 0;
    for (int k = 0; k < OWNS; k++) {
        if (g_own[k].state != OWN_HAVE || !alive(g_own[k].actor, g_own[k].w)) continue;
        for (int h = 0; h < 3; h++) theirs_held[nh++] = linked(g_own[k].actor + 0xb98 + 0x18 * h);
        const float *em = (const float *)(uintptr_t)(g_own[k].actor + 0x398);
        at[no][0] = em[3], at[no][1] = em[7], at[no][2] = em[11];
        ours_at[no] = at[no], no++;
    }
    for (int g = 0; g < GROUND; g++) {
        if (!g_ground[g].actor) continue;
        if (alive(g_ground[g].actor, g_ground[g].w)) {
            bool enemy_has = false;
            for (int k = 0; k < nh && !enemy_has; k++) enemy_has = theirs_held[k] == g_ground[g].actor;
            if (enemy_has) {  // (where it lay: the others delete theirs there)
                LOG("[online] %s picked up here by one of our enemies\n", g_ground[g].name);
                online_send_pick(g_ground[g].name, g_ground[g].pos);
                g_ground[g].actor = 0;
                continue;
            }
            if (carries(carried, g_ground[g].actor)) {  // (picked up into a free hand: the game keeps the same actor)
                LOG("[online] %s picked up here, into his hand\n", g_ground[g].name);
                online_send_pick(g_ground[g].name, g_ground[g].pos);
                g_ground[g].actor = 0;
                continue;
            }
            const float *m = (const float *)(uintptr_t)(g_ground[g].actor + 0x398);
            g_ground[g].pos[0] = m[3], g_ground[g].pos[1] = m[7], g_ground[g].pos[2] = m[11];
            bool near = near_link(link, g_ground[g].pos, 6);
            for (int k = 0; k < no && !near; k++) near = near_link(ours_at[k], g_ground[g].pos, 8);
            if (!near) g_ground[g].actor = 0;  // (far: not watched)
            continue;
        }
        if (near_link(link, g_ground[g].pos, 2.5f)) {
            LOG("[online] %s picked up here\n", g_ground[g].name);
            online_send_pick(g_ground[g].name, g_ground[g].pos);
        }
        g_ground[g].actor = 0;
    }
    for (int k = 0; k < na; k++) {
        char n[48];
        uint64_t a = g_actors[k];
        actor_name_of(a, n);
        if (strncmp(n, "Weapon_", 7)) continue;
        const float *m = (const float *)(uintptr_t)(a + 0x398);
        float p[3] = {m[3], m[7], m[11]};
        bool near = near_link(link, p, 4);
        for (int j = 0; j < no && !near; j++) near = near_link(ours_at[j], p, 6);
        if (!near) continue;
        if (loot_actor(a)) continue;
        bool known = false, held = carries(carried, a);
        for (int g = 0; g < GROUND && !known; g++) known = g_ground[g].actor == a;
        for (int j = 0; j < nh; j++) held |= theirs_held[j] == a;
        if (known || held) continue;
        for (int g = 0; g < GROUND; g++)
            if (!g_ground[g].actor) {
                g_ground[g].actor = a, g_ground[g].w = watch(a);
                snprintf(g_ground[g].name, sizeof g_ground[g].name, "%s", n);
                memcpy(g_ground[g].pos, p, 12);
                break;
            }
    }
}
