// Online play's actors, oa_enemies: the enemies: the host's, sent and shown in the others' games, owned, their blows;
// the bosses. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"
#include "hle.h"  // (RT_ON_RESET)

// The pool (oa_core.c) the enemies' puppets, their weapons and the element sources keep their pieces in: a slot of
// no player's, so that one going out of reach or leaving (oa_drop: his pool emptied, and full, its first piece
// deleted) doesn't take them. That slot (OA_POOL_WORLD, ONLINE_MAX: oa_g_pool a row longer, its loops up to it) is
// the coordinator's change to oa_core.c, oa_frame.c and online_actors_internal.h; until it is there, slot 0 as before
#ifdef OA_POOL_WORLD
#define POOL_WORLD OA_POOL_WORLD
#else
#define POOL_WORLD 0
#endif
_Static_assert(POOL_WORLD >= 0 && POOL_WORLD < (int)(sizeof oa_g_pool / sizeof oa_g_pool[0]),
               "the world's pool slot is a row of oa_g_pool");

// ---- The host's enemies (protocol 11, ENEMIES): his game sends those near any player, each by its id, as it is (its
// name, matrix, the AS it plays); the others delete their own near his Link and show his with OnlineEnemy_* actors
// (tools/online_mod.py: an enemy's model, AS list and AS, no AI), each where his is, playing what his plays. When his
// feed stops (he left, the connection went), they stay as they were until it comes again.

// online_mod/enemies.txt: each enemy's online actor ("Enemy_...=OnlineEnemy_...")
static const char *enemy_actor(const char *name) {
    static MapLine *m;
    static int n = -1;
    if (n < 0) n = oa_map_load("enemies.txt", &m), LOG("[online] %d enemies for the other players\n", n);
    return oa_map_find(m, n, name);
}

// the bosses: each game keeps its own (their fights are the game's events and phases); their life is everyone's
// (boss_update)
static bool is_boss(const char *n) {
    return !strcmp(n, "Enemy_Ganon") || !strcmp(n, "Enemy_GanonBeast") || !strncmp(n, "Enemy_SiteBoss", 14);
}

static bool is_enemy(uint64_t a, char name[48]) {
    oa_actor_name_of(a, name);
    return !strncmp(name, "Enemy_", 6) && !is_boss(name) && enemy_actor(name);
}

Refused oa_g_refused_enemy, oa_g_refused_weapon, oa_g_refused_loot;
void oa_name_refused(Refused *r, const char *what, const char *name) {
    r->n++;
    uint64_t now = plat_now_ns();
    if (r->last && now - r->last < 1000000000ull) return;
    r->last = now;
    LOG("[online] %s name refused: %.40s (%u so far)\n", what, name, r->n);
}

// an enemy's name another game sent, as is_enemy has its actor's: an Enemy_*, not a boss, one enemies.txt knows
static bool enemy_name_ok(const char *n) {
    return !strncmp(n, "Enemy_", 6) && !is_boss(n) && enemy_actor(n);
}

// enemy_weapon's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static int miss;

// what an enemy holds: a weapon actor it links (the link's place in an enemy found once, by looking through one
// holding a Weapon_*; again every second while none is found)
static uint64_t enemy_weapon(uint64_t a) {
    static int off = -1;
    char n[48];
    if (off >= 0) {
        uint64_t w = oa_linked(a + off);
        oa_actor_name_of(w, n);
        return !strncmp(n, "Weapon_", 7) ? w : 0;
    }
    if (miss++ % 30) return 0;
    for (int o = 0x100; o < 0x4000; o += 8) {
        uint64_t u = *(uint64_t *)(uintptr_t)(a + o);
        if (!VALID(u) || (u & 7)) continue;
        uint64_t w = oa_linked(a + o);
        oa_actor_name_of(w, n);
        if (strncmp(n, "Weapon_", 7)) continue;
        off = o;
        LOG("[online] an enemy's weapon: enemy +0x%x\n", o);
        return w;
    }
    return 0;
}

#define ENEMY_NEAR 150.0f   // (the host sends his enemies this near a player)
#define ENEMY_AREA 170.0f   // (the others' own ones this near his Link go)
// (an enemy is made in a game only this near its Link: a game removes an enemy it makes too far from its camera,
// short of its traverseDist (100 m when it has none): a Bokoblin made 136 m off, of 150, went at once, a Golem 90 m
// off, of 100, stayed; Great Plateau test 2026-11-13)
#define OWN_SAFE 80.0f
// (no one's: alive, but no player near enough to have it; kept as it was, its life and where it stood, shown so, until
// one comes within OWN_SAFE, whose game makes it; dropped when no player is within ENEMY_NEAR of it. The map's one in
// the host's game was deleted when it went to another: dropped at once, it was gone for everyone)
#define OWNER_NONE 0xff

// what an enemy holds (its weapon at +0xb98, its shield at +0xbb0: linked as the player links his), for the others:
// in the space of the hand bone it is in (each game shows its enemy playing its own AS: the weapon goes in its own
// hand there), else where it is in the world (on its back, on the ground near it)
static const char *const k_hands[2] = {"Weapon_R", "Weapon_L"};
static void enemy_held(Cpu *c, uint64_t a, uint64_t g, char name[40], float m[12], uint8_t *bone) {
    char n[48];
    oa_actor_name_of(g, n);
    name[0] = 0, *bone = 0;
    if (strncmp(n, "Weapon_", 7)) return;
    const float *w = (const float *)(uintptr_t)(g + 0x398), *am = (const float *)(uintptr_t)(a + 0x398);
    float dx = w[3] - am[3], dy = w[7] - am[7], dz = w[11] - am[11];
    if (dx * dx + dy * dy + dz * dz > 4 * 4) return;  // (fallen far from it: not its own any more)
    snprintf(name, 40, "%s", n);
    memcpy(m, w, 48);
    for (int h = 0; h < 2; h++) {
        float bm[12], inv[12], rel[12];
        if (!oa_bone_mtx(c, a, k_hands[h], bm)) continue;
        oa_mtx_inv(bm, inv);
        oa_mtx_mul(inv, w, rel);
        if (rel[3] * rel[3] + rel[7] * rel[7] + rel[11] * rel[11] > 0.6f * 0.6f) continue;
        memcpy(m, rel, 48), *bone = (uint8_t)(h + 1);
        return;
    }
}

// an enemy as it is here, for the others
static void own_guarded_life(uint64_t a, int32_t *life);
void oa_enemy_state(Cpu *c, uint64_t a, OnlineEnemy *e) {
    oa_actor_name_of(a, e->name);
    memcpy(e->m, (const float *)(uintptr_t)(a + 0x398), sizeof e->m);
    snprintf(e->as, sizeof e->as, "%s", oa_playing(c, a, 0, 0));
    e->life = *(int32_t *)(uintptr_t)(a + ENEMY_LIFE);
    own_guarded_life(a, &e->life);  // (one guarded: its real life, not the guard's)
    uint32_t h = *(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID);  // (one made by us has none: the one it was keeps its)
    if (h) e->hash = h;
    enemy_held(c, a, oa_linked(a + 0xb98), e->weapon, e->wm, &e->wbone);
    enemy_held(c, a, oa_linked(a + 0xbb0), e->shield, e->sm, &e->sbone);
}

#define GUARD_LIFE 1000  // (own_guard)
Own oa_g_own[OWNS];
static struct { uint64_t actor; Watch w; } g_own_left[OWNS];

static Own *own_find(uint32_t gid) {
    for (int k = 0; k < OWNS; k++)
        if (oa_g_own[k].state && oa_g_own[k].gid == gid) return &oa_g_own[k];
    return NULL;
}

static void enemy_delete(Cpu *c, uint64_t a);

void oa_owns_left(Cpu *c) {
    for (int k = 0; k < OWNS; k++) {
        if (g_own_left[k].actor && oa_alive(g_own_left[k].actor, g_own_left[k].w)) enemy_delete(c, g_own_left[k].actor);
        g_own_left[k].actor = 0;
    }
}

static bool own_is(uint64_t a) {
    for (int k = 0; k < OWNS; k++)
        if (oa_g_own[k].state == OWN_HAVE && oa_g_own[k].actor == a) return true;
    return false;
}

static Own *own_add(uint32_t gid) {
    for (int k = 0; k < OWNS; k++)
        if (!oa_g_own[k].state) {
            memset(&oa_g_own[k], 0, sizeof oa_g_own[k]);
            oa_g_own[k].gid = gid, oa_g_own[k].state = OWN_MAKING;
            return &oa_g_own[k];
        }
    return NULL;
}

// an enemy deleted with what it holds (else its weapon stays on the ground: one more each time it changes hands)
static void enemy_delete(Cpu *c, uint64_t a) {
    uint64_t w = enemy_weapon(a);
    if (w) oa_call(c, FN_DELETE, w, 0, 0, 0, 0, 0);
    for (int k = 1; k < 3; k++) {  // (its shield and bow: linked as the player links his, after the weapon)
        char n[48];
        uint64_t g2 = oa_linked(a + 0xb98 + 0x18 * k);
        oa_actor_name_of(g2, n);
        if (!strncmp(n, "Weapon_", 7)) oa_call(c, FN_DELETE, g2, 0, 0, 0, 0, 0);
    }
    oa_call(c, FN_DELETE, a, 0, 0, 0, 0, 0);
}

static void own_drop(Cpu *c, Own *o) {
    if (o->state == OWN_HAVE && oa_alive(o->actor, o->w)) enemy_delete(c, o->actor);
    oa_delete_piece(c, &o->made);
    memset(o, 0, sizeof *o);
}

// ours, made when it came to us: as it was (where, its life); false while it is being made
// An enemy just made here from another's game (own_ready) is unaware, whatever it was doing there: our first blow
// would be a sneak strike, eight times its damage, a kill. For 8 s its life is kept GUARD_LIFE above its own (the
// real one apart: what is sent, enemy_state), and each blow takes off it no more than our weapon's attack power
// when it is more than twice that; at 0 it dies as any
static void own_guarded_life(uint64_t a, int32_t *life) {
    for (int k = 0; k < OWNS; k++)
        if (oa_g_own[k].guard_until && oa_g_own[k].actor == a) *life = oa_g_own[k].real_life;
}
void oa_own_guard(uint64_t player) {
    uint64_t now = plat_now_ns();
    for (int k = 0; k < OWNS; k++) {
        Own *o = &oa_g_own[k];
        if (!o->guard_until || o->state != OWN_HAVE || !oa_alive(o->actor, o->w)) { o->guard_until = 0; continue; }
        int32_t *life = (int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE);
        int32_t dmg = o->real_life + GUARD_LIFE - *life;
        // (its first half second: the game sets its life up as it starts; ours is put back, nothing taken)
        if (o->guard_until - now > 7500000000ull) dmg = 0;
        if (dmg > 0) {
            uint64_t ar[ARMOR], gr[ONLINE_GEAR];
            oa_equipment(player, ar, gr);
            char wn[48] = "";
            if (gr[0]) oa_actor_name_of(gr[0], wn);
            int pw = wn[0] ? oa_attack_power(wn) : 0;
            if (pw > 0 && dmg > pw * 2) {
                LOG("[online] a blow of %d on our %u, just ours: %d, as without a sneak strike\n", dmg, o->gid, pw);
                dmg = pw;
            }
            o->real_life = o->real_life > dmg ? o->real_life - dmg : 0;
        }
        if (o->real_life <= 0 || now > o->guard_until) {  // (over: its own life again)
            *life = o->real_life;
            o->guard_until = 0;
        } else *life = o->real_life + GUARD_LIFE;
    }
}

static bool own_ready(Cpu *c, Own *o, const OnlineEnemy *e) {
    if (o->state == OWN_HAVE) return true;
    if (o->state == OWN_GONE) return false;
    if (!enemy_name_ok(e->name)) {  // (not asked for, here nor by piece_update: a name not an enemy's makes nothing)
        oa_name_refused(&oa_g_refused_enemy, "enemy", e->name);
        return false;
    }
    if (!o->made.pending && !o->made.actor && !o->made.name[0]) {  // (asked for with the weapon it holds)
        memset(&o->params, 0, sizeof o->params);
        // (what it holds only if weapons.txt has it: a shield is on it too)
        if (e->weapon[0] && !oa_weapon_name_listed(e->weapon, sizeof e->weapon))
            oa_name_refused(&oa_g_refused_weapon, "enemy weapon", e->weapon);
        else if (e->weapon[0]) oa_inst_params_string(&o->params, "EquipItem1", e->weapon);
        if (e->shield[0] && !oa_weapon_name_listed(e->shield, sizeof e->shield))
            oa_name_refused(&oa_g_refused_weapon, "enemy shield", e->shield);
        else if (e->shield[0]) oa_inst_params_string(&o->params, "EquipItem2", e->shield);
        o->made.handle = oa_new_handle();
        if (oa_request_p(c, e->name, o->made.handle, o->params.n ? (uint64_t)(uintptr_t)&o->params : 0)) {
            o->made.pending = true;
            snprintf(o->made.name, sizeof o->made.name, "%s", e->name);
        }
    }
    oa_piece_update(c, POOL_WORLD, &o->made, e->name, "own");
    if (!o->made.actor) return false;
    o->actor = o->made.actor, o->w = o->made.w, o->state = OWN_HAVE, o->remade = true;
    memset(&o->made, 0, sizeof o->made);
    oa_call(c, FN_SET_MTX, o->actor, (uint64_t)(uintptr_t)e->m, 0, 0, 0, 0);
    if (e->life > 0) *(int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE) = e->life;
    // (it was another's, fighting there: made here it is unaware, and our first blow would be a sneak strike, eight
    // times; guarded a while: own_guard)
    o->real_life = e->life > 0 ? e->life : *(int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE);
    o->guard_until = plat_now_ns() + 8000000000ull;
    *(int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE) = o->real_life + GUARD_LIFE;
    LOG("[online] we have %s %u for real now\n", e->name, o->gid);
    return true;
}

// the others' hits on ours: their life goes down by it (at 0 its own AI has it die)
// An element's state on one of ours, as another player's element arrow on its puppet there would have set it: a source
// of it made a metre over the enemy, deleted 4 s later (tested on the Great Plateau's Bokoblins, 2026-11-13,
// SAVAGEWIND_ONLINE_ELEMENT_TEST): fire FireRodLv1Fire (burning, no damage of its own), electric ThunderRodLv1Thunder
// (shocked, none of its own; ExpandElectricField killed a 13-life Bokoblin), ice ExpandIceField (frozen, but 10 damage
// of its own: taken back for its first second, the arrow's damage being the hit's; neither rod's ice froze). The
// arrows themselves made in place do nothing (they need a bow's shot)
#define ELEM_SOURCES 4
static struct { Piece p; uint32_t gid; int32_t keep; uint64_t made, until; uint8_t elem; } g_esrc[ELEM_SOURCES];
static const char *const k_elem_source[ONLINE_ELEM_COUNT] = {NULL, "FireRodLv1Fire", "ExpandIceField",
                                                             "ThunderRodLv1Thunder"};

static void elem_start(uint32_t gid, uint8_t elem, int32_t keep) {
    if (elem == ONLINE_ELEM_NONE || elem >= ONLINE_ELEM_COUNT) return;
    for (int k = 0; k < ELEM_SOURCES; k++)
        if (!g_esrc[k].gid) {
            memset(&g_esrc[k], 0, sizeof g_esrc[k]);
            g_esrc[k].gid = gid, g_esrc[k].elem = elem, g_esrc[k].keep = keep;
            g_esrc[k].until = plat_now_ns() + 4000000000ull;
            return;
        }
}

static void elem_update(Cpu *c) {
    uint64_t now = plat_now_ns();
    for (int k = 0; k < ELEM_SOURCES; k++) {
        if (!g_esrc[k].gid) continue;
        Own *o = own_find(g_esrc[k].gid);
        bool there = o && o->state == OWN_HAVE && oa_alive(o->actor, o->w);
        if (!there || now > g_esrc[k].until) {
            oa_delete_piece(c, &g_esrc[k].p);
            memset(&g_esrc[k], 0, sizeof g_esrc[k]);
            continue;
        }
        if (!g_esrc[k].made) {
            oa_piece_update(c, POOL_WORLD, &g_esrc[k].p, k_elem_source[g_esrc[k].elem], "element");
            if (!g_esrc[k].p.actor) continue;
            const float *m = (const float *)(uintptr_t)(o->actor + 0x398);
            float sm[12] = {1, 0, 0, m[3], 0, 1, 0, m[7] + 1.0f, 0, 0, 1, m[11]};
            oa_call(c, FN_SET_MTX, g_esrc[k].p.actor, (uint64_t)(uintptr_t)sm, 0, 0, 0, 0);
            g_esrc[k].made = now;
            LOG("[online] %s on our %u\n", k_elem_source[g_esrc[k].elem], g_esrc[k].gid);
            continue;
        }
        int32_t *life = (int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE);
        if (g_esrc[k].elem == ONLINE_ELEM_ICE && now - g_esrc[k].made < 1000000000ull && *life > 0 && *life < g_esrc[k].keep)
            *life = g_esrc[k].keep;  // (the field's own damage taken back)
    }
}

static void own_hits(Cpu *c) {
    uint32_t hid[64];
    int32_t hd[64];
    uint8_t he[64];
    int nh = online_take_hits(hid, hd, he, 64);
    for (int h = 0; h < nh; h++) {
        Own *o = own_find(hid[h]);
        if (!o || o->state != OWN_HAVE || !oa_alive(o->actor, o->w)) continue;
        int32_t *life = (int32_t *)(uintptr_t)(o->actor + ENEMY_LIFE);
        LOG("[online] a hit on our %u: %d of its %d%s\n", hid[h], hd[h], *life,
            he[h] == ONLINE_ELEM_FIRE ? " (fire)" : he[h] == ONLINE_ELEM_ICE ? " (ice)"
            : he[h] == ONLINE_ELEM_ELECTRIC ? " (electric)" : "");
        *life = *life > hd[h] ? *life - hd[h] : 0;
        if (*life > 0) elem_start(hid[h], he[h], *life);
    }
    elem_update(c);
}

// the host's game: every enemy of everyone's, its state and its owner (slot)
#define REG 64
// (nearer: ticks another has been much nearer; aggro: ticks before a blow may hand it to its striker again)
// (striker: the slot it turned on, struck by him: his while he is within 45 m of it, whoever is nearer; -1 none)
static struct { OnlineEnemy e; int hold, nearer, aggro, striker; bool used; } g_reg[REG];
OnlineEnemy oa_g_reg_out[ONLINE_ENEMIES];  // (the list sent last: the others' are shown here from it each frame)
int oa_g_reg_n;

// (a load: the game removed them all; when it didn't (a long jump that was no load), those still there are deleted
// on the next frame, not left in the world for no one)
void oa_owns_forget(void) {
    for (int k = 0; k < OWNS; k++) {
        oa_orphan(&oa_g_own[k].made);
        if (oa_g_own[k].state == OWN_HAVE) g_own_left[k].actor = oa_g_own[k].actor, g_own_left[k].w = oa_g_own[k].w;
    }
    memset(oa_g_own, 0, sizeof oa_g_own);
    memset(g_reg, 0, sizeof g_reg);
    oa_g_reg_n = 0;
}

// An enemy that died in another game: its dead flag here (<map>_<actor>_<HashId>, the flag the game sets when one of
// its map's enemies dies: the game doesn't make it again until a Blood Moon, and what waits for its group to be
// beaten, a camp's chest, sees it beaten)
uint32_t oa_crc32_str(const char *s);
static void mark_dead(Cpu *c, const char *name, uint32_t hash) {
    if (!hash || !name[0]) return;
    static const char *const maps[] = {"MainField", "AocField", "MainFieldDungeon", "CDungeon"};
    for (int k = 0; k < 4; k++) {
        char f[128];
        snprintf(f, sizeof f, "%s_%s_%u", maps[k], name, hash);
        uint32_t h = oa_crc32_str(f);
        uint32_t *v = oa_bool_flag(h);
        if (!v) continue;
        if (!(*v & 1) && !oa_set_bool_flag(c, h)) *v |= 1;
        LOG("[online] %s: dead here too\n", f);
        return;
    }
    LOG("[online] %s %u: no dead flag\n", name, hash);
}

// an enemy's matrix (its position at 3, 7, 11) to a point
static float dist3(const float m[12], const float q[3]) {
    float dx = m[3] - q[0], dy = m[7] - q[1], dz = m[11] - q[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// whether another player's game may have enemies for real: in the world (not loading), his Link there and not dead
// (dead, his game stops at its game over, and what it has stops with it; trying again, his world goes)
static bool can_own(const OnlinePlayer *p, int i, int me) {
    return p->used && i != me && !(p->flags & ONLINE_F_LOADING) && p->pose.valid && strncmp(p->pose.as[0], "Dead", 4) &&
           oa_same_map(p);
}

// oa_enemies_host's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static uint64_t oa_enemies_host_t0;
static bool done;
static uint64_t oa_enemies_host_t0_2, until, next;
static Piece src;
static int who = -1, last_life, oa_enemies_host_turn, used[OWNS];
static char name[40], last_as[40];
static int8_t could[ONLINE_MAX];
static uint16_t aggro_seen[ONLINE_MAX];
static bool aggro_init[ONLINE_MAX];

void oa_enemies_host(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me) {
    own_hits(c);
    // the owners' states of theirs
    static OnlineEnemy st[64];
    uint8_t from[64];
    int ns = online_take_estates(st, from, 64);
    for (int k = 0; k < ns; k++)
        for (int r = 0; r < REG; r++) {
            if (!g_reg[r].used || g_reg[r].e.id != st[k].id || g_reg[r].e.owner != from[k]) continue;
            if (st[k].life < 0) {  // (gone in his game: died)
                LOG("[online] %s %u gone in slot %d's game\n", g_reg[r].e.name, st[k].id, from[k]);
                if (g_reg[r].e.life <= 0) {  // (it had died there)
                    mark_dead(c, g_reg[r].e.name, g_reg[r].e.hash);
                    g_reg[r].used = false;
                } else if (dist3(g_reg[r].e.m, link) > OWN_SAFE) {  // (alive, but too far for ours: made here it would
                    // go at once; no one's, until a player comes near it)
                    LOG("[online] %s %u: alive, no one's until a player is near it\n", g_reg[r].e.name, st[k].id);
                    g_reg[r].e.owner = OWNER_NONE, g_reg[r].hold = 0, g_reg[r].nearer = 0;
                } else {  // (alive, gone with his world: he died and tries again, or loads elsewhere): ours again
                    LOG("[online] %s %u: alive, ours again\n", g_reg[r].e.name, st[k].id);
                    g_reg[r].e.owner = (uint8_t)me, g_reg[r].hold = 100, g_reg[r].nearer = 0;
                    if (!own_find(st[k].id)) own_add(st[k].id);
                }
            } else {
                uint8_t owner = g_reg[r].e.owner;
                uint32_t hash = g_reg[r].e.hash;
                char name[sizeof g_reg[r].e.name];  // (its name is ours, as its owner: what one sends is not what it is)
                memcpy(name, g_reg[r].e.name, sizeof name);
                g_reg[r].e = st[k], g_reg[r].e.owner = owner;
                memcpy(g_reg[r].e.name, name, sizeof name);
                if (!g_reg[r].e.hash) g_reg[r].e.hash = hash;
            }
            break;
        }
    {   // (test: SAVAGEWIND_ONLINE_KILL_TEST=<s>: ours die that many seconds in, all at once)
        const char *kt = getenv("SAVAGEWIND_ONLINE_KILL_TEST");
        if (kt && !oa_enemies_host_t0) oa_enemies_host_t0 = plat_now_ns();  // (seconds from the first time here: it was counted in calls, whose rate varies)
        if (kt && !done && plat_now_ns() - oa_enemies_host_t0 >= (uint64_t)(atof(kt) * 1e9) && (done = true))
            for (int k = 0; k < OWNS; k++)
                if (oa_g_own[k].state == OWN_HAVE && oa_alive(oa_g_own[k].actor, oa_g_own[k].w))
                    *(int32_t *)(uintptr_t)(oa_g_own[k].actor + ENEMY_LIFE) = 0, LOG("[online] kill test: %u\n", oa_g_own[k].gid);
    }
    {   // (test: SAVAGEWIND_ONLINE_ELEMENT_TEST=<actor>[|<actor>...],<s>: that many seconds in, each actor (an element's
        // source: a rod's bullet, a field, an arrow) made at another enemy we have for real, 8 s apart, its life and AS
        // logged for 6 s: which of them sets an enemy burning, frozen or shocked, for a player's element arrow on a
        // puppet in another game)
        const char *et = getenv("SAVAGEWIND_ONLINE_ELEMENT_TEST");
        if (et && !oa_enemies_host_t0_2) oa_enemies_host_t0_2 = plat_now_ns();
        const char *comma = et ? strchr(et, ',') : NULL;
        if (et && who < 0 && name[0] != '-' && plat_now_ns() - oa_enemies_host_t0_2 >= (uint64_t)((comma ? atof(comma + 1) : 120) * 1e9) &&
            plat_now_ns() >= next) {
            const char *s = et;  // (the turn-th name of the list)
            for (int k = 0; k < oa_enemies_host_turn && s; k++) s = strchr(s, '|'), s = s ? s + 1 : NULL;
            if (!s || (comma && s > comma)) name[0] = '-';
            else {
                const char *e = s;
                while (*e && *e != '|' && *e != ',') e++;
                snprintf(name, sizeof name, "%.*s", (int)(e - s), s);
                for (int k = 0; k < OWNS && who < 0; k++)
                    if (oa_g_own[k].state == OWN_HAVE && !used[k] && oa_alive(oa_g_own[k].actor, oa_g_own[k].w)) who = k, used[k] = 1;
            }
        }
        if (until && plat_now_ns() >= until) {  // (this one done: the next in 8 s)
            oa_delete_piece(c, &src);
            who = -1, until = 0, oa_enemies_host_turn++, next = plat_now_ns() + 2000000000ull;
        }
        if (who >= 0 && !until) {
            oa_piece_update(c, POOL_WORLD, &src, name, "element test");
            if (src.actor && oa_alive(oa_g_own[who].actor, oa_g_own[who].w)) {
                const float *m = (const float *)(uintptr_t)(oa_g_own[who].actor + 0x398);
                float sm[12] = {1, 0, 0, m[3], 0, 1, 0, m[7] + 1.0f, 0, 0, 1, m[11]};
                oa_call(c, FN_SET_MTX, src.actor, (uint64_t)(uintptr_t)sm, 0, 0, 0, 0);
                until = plat_now_ns() + 6000000000ull;
                last_life = *(int32_t *)(uintptr_t)(oa_g_own[who].actor + ENEMY_LIFE);
                LOG("[online] element test: %s at our %u (life %d)\n", name, oa_g_own[who].gid, last_life);
            }
        }
        if (until && plat_now_ns() < until && oa_alive(oa_g_own[who].actor, oa_g_own[who].w)) {
            int life = *(int32_t *)(uintptr_t)(oa_g_own[who].actor + ENEMY_LIFE);
            const char *as = oa_playing(c, oa_g_own[who].actor, 0, 0);
            if (life != last_life || strcmp(as, last_as))
                LOG("[online] element test: %u life %d, AS %s\n", oa_g_own[who].gid, life, as);
            last_life = life, snprintf(last_as, sizeof last_as, "%s", as);
        }
    }
    // ours: their state; gone (died, or the game removed them): gone for everyone
    for (int k = 0; k < OWNS; k++) {
        Own *o = &oa_g_own[k];
        if (o->state != OWN_HAVE) continue;
        int r = 0;
        while (r < REG && !(g_reg[r].used && g_reg[r].e.id == o->gid)) r++;
        if (!oa_alive(o->actor, o->w)) {
            // (one made here, gone alive: the game removed it, far from the camera; the map's own was deleted when it
            // went to another, so it is kept, no one's, not dropped. The map's own, removed or dead: off the list)
            if (r < REG && o->remade && g_reg[r].e.life > 0) {
                LOG("[online] %s %u: alive, no one's until a player is near it\n", g_reg[r].e.name, o->gid);
                g_reg[r].e.owner = OWNER_NONE, g_reg[r].hold = 0, g_reg[r].nearer = 0;
            } else if (r < REG) g_reg[r].used = false;
            memset(o, 0, sizeof *o);
            continue;
        }
        if (r < REG) oa_enemy_state(c, o->actor, &g_reg[r].e), g_reg[r].e.owner = (uint8_t)me;
        // (died here: its dead flag, by the map's HashId the list keeps; one made again, after it was another's, has
        // none of its own, and without it the camp it belongs to wasn't cleared in this game, its chest shut)
        if (r < REG && g_reg[r].e.life <= 0 && !o->marked) mark_dead(c, g_reg[r].e.name, g_reg[r].e.hash), o->marked = true;
    }
    // ours in the world, newly near a player: everyone's from now on (ours to start with)
    int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
    for (int k = 0; k < na; k++) {
        char name[48];
        uint64_t a = oa_g_actors[k];
        if (!oa_running(a) || !is_enemy(a, name) || own_is(a)) continue;
        const float *m = (const float *)(uintptr_t)(a + 0x398);
        bool near = false;
        for (int i = -1; i < ONLINE_MAX && !near; i++) {
            const float *q = i < 0 ? link : pl[i].pos;
            if (i >= 0 && (!pl[i].used || i == me || (pl[i].flags & ONLINE_F_LOADING) || !oa_same_map(&pl[i]))) continue;
            float dx = m[3] - q[0], dz = m[11] - q[2];
            near = dx * dx + dz * dz < ENEMY_NEAR * ENEMY_NEAR;
        }
        uint32_t id = *(uint32_t *)(uintptr_t)(a + 0x60);
        if (!near) continue;
        int r = 0, free_r = -1;
        for (; r < REG && !(g_reg[r].used && g_reg[r].e.id == id); r++)
            if (!g_reg[r].used && free_r < 0) free_r = r;
        if (r < REG || free_r < 0) continue;
        Own *o = own_add(id);
        if (!o) continue;
        o->state = OWN_HAVE, o->actor = a, o->w = oa_watch(a);
        memset(&g_reg[free_r], 0, sizeof g_reg[free_r]);
        g_reg[free_r].used = true, g_reg[free_r].striker = 0, g_reg[free_r].aggro = 0;
        g_reg[free_r].e.id = id;
        oa_enemy_state(c, a, &g_reg[free_r].e);
        g_reg[free_r].e.owner = (uint8_t)me;
    }
    // (each player's can-own changes, with why: a flicker here hands his enemies back and forth)
    for (int i = 0; i < ONLINE_MAX; i++) {
        if (i == me) continue;
        int8_t now = (int8_t)(can_own(&pl[i], i, me) ? 1 : 0);
        if (now != could[i] && pl[i].used)
            LOG("[online] slot %d %s enemies (loading %d, pose %d, as %.12s, same map %d)\n", i, now ? "can own" : "can't own",
                !!(pl[i].flags & ONLINE_F_LOADING), pl[i].pose.valid, pl[i].pose.as[0], oa_same_map(&pl[i]));
        could[i] = now;
    }
    // (blows: a player who hits one another's game has gets it, so it turns on him, as an enemy turns on whoever
    // strikes it; within OWN_SAFE of it, and not again for 6 s)
    uint32_t aggro_of[ONLINE_MAX] = {0};
    for (int i = 0; i < ONLINE_MAX; i++) {
        uint16_t s = i == me ? oa_g_aggro_seq : pl[i].used && pl[i].pose.valid ? pl[i].pose.aggro_seq : 0;
        uint32_t id = i == me ? oa_g_aggro_id : pl[i].pose.aggro_id;
        if (!aggro_init[i]) aggro_seen[i] = s, aggro_init[i] = true;
        if (s != aggro_seen[i]) {
            aggro_of[i] = id, aggro_seen[i] = s;
            if (getenv("SAVAGEWIND_ONLINE_AGGRO_LOG")) LOG("[online] slot %d struck %u (hit %u)\n", i, id, s);
        }
        if (i != me && !pl[i].used) aggro_init[i] = false;
    }
    // owners: the player nearest each one (another only when he is clearly nearer, and not again for 3 s)
    for (int r = 0; r < REG; r++) {
        if (!g_reg[r].used) continue;
        OnlineEnemy *e = &g_reg[r].e;
        if (g_reg[r].aggro > 0) g_reg[r].aggro--;
        int striker = -1;
        for (int i = 0; i < ONLINE_MAX; i++) {
            if (!aggro_of[i] || aggro_of[i] != e->id) continue;
            if (getenv("SAVAGEWIND_ONLINE_AGGRO_LOG"))
                LOG("[online] %u struck by slot %d: owner %d, wait %d, can own %d, %.0f m\n", e->id, i, e->owner,
                    g_reg[r].aggro, i == me || can_own(&pl[i], i, me), dist3(e->m, i == me ? link : pl[i].pos));
            if (e->owner == OWNER_NONE || g_reg[r].aggro || i == e->owner) continue;
            if (i != me && !can_own(&pl[i], i, me)) continue;
            if (i == me && oa_g_local_dead) continue;
            if (dist3(e->m, i == me ? link : pl[i].pos) > OWN_SAFE) continue;  // (an arrow from afar too)
            striker = i;
        }
        if (striker >= 0) {
            LOG("[online] %s %u: slot %d's now (was %d's; struck by it)\n", e->name, e->id, striker, e->owner);
            if (e->owner == me) {
                Own *o = own_find(e->id);
                if (o) own_drop(c, o);
            }
            if (striker == me) own_add(e->id);
            e->owner = (uint8_t)striker;
            g_reg[r].hold = 100, g_reg[r].nearer = 0, g_reg[r].aggro = 120;  // (6 s: this runs every 3 frames)
            g_reg[r].striker = striker + 1;  // (+1: 0 none, as a new entry is)
            continue;
        }
        float best = 1e30f, cur = 1e30f, best3 = 1e30f;
        int nearest = me;
        for (int i = -1; i < ONLINE_MAX; i++) {
            int slot = i < 0 ? me : i;
            const float *q = i < 0 ? link : pl[i].pos;
            if (i >= 0 && !can_own(&pl[i], i, me)) continue;
            float dx = e->m[3] - q[0], dz = e->m[11] - q[2], d = sqrtf(dx * dx + dz * dz);
            if (i < 0 && oa_g_local_dead) d += 1e6f;  // (we're dead: the others' first)
            if (d < best) best = d, nearest = slot, best3 = dist3(e->m, q);
            if (slot == e->owner) cur = d;
        }
        if (e->owner == me && oa_g_local_dead && best < 1e6f) cur = 1e30f;  // (we died: another has it now)
        if (e->owner == OWNER_NONE) {  // (no one's: the nearest one's once he is near enough; no one near: off the list)
            if (best3 <= OWN_SAFE && !(nearest == me && oa_g_local_dead)) {
                LOG("[online] %s %u: slot %d's now (was no one's)\n", e->name, e->id, nearest);
                if (nearest == me) own_add(e->id);
                e->owner = (uint8_t)nearest;
                g_reg[r].hold = 100, g_reg[r].nearer = 0;
            } else if (best3 > ENEMY_NEAR) {
                LOG("[online] %s %u: no one's and no player near it: off the list\n", e->name, e->id);
                g_reg[r].used = false;
            }
            continue;
        }
        if (g_reg[r].hold > 0) g_reg[r].hold--;
        bool gone_owner = cur >= 1e30f;  // (he left, or is loading)
        // (each change makes it anew in one game and deletes it in another: only when its owner is far from it (40 m),
        // or another has been 15 m nearer for 5 s, and not again for 10 s; players fighting it together keep it where
        // it is)
        // (one that turned on the player who struck it stays his while he fights it, within 45 m of it)
        if (g_reg[r].striker && (g_reg[r].striker - 1 != e->owner || cur > 45)) g_reg[r].striker = 0;
        if (g_reg[r].striker && !gone_owner) continue;
        g_reg[r].nearer = nearest != e->owner && best + 15 < cur ? g_reg[r].nearer + 1 : 0;
        // (its owner far, it goes only to one clearly nearer: with both players about 40 m from it, it went back and
        // forth between them every few seconds - "owner at 42 m, new one at 42 m")
        bool far = (cur > 40 && best + 10 < cur) || g_reg[r].nearer > 50;
        if (nearest == e->owner || (!gone_owner && (g_reg[r].hold > 0 || !far))) continue;
        // (nor to one too far from it: his game would remove it as it is made, and it would be gone for everyone; its
        // owner gone, it is no one's meanwhile)
        if (best3 > OWN_SAFE) {
            if (gone_owner) {
                LOG("[online] %s %u: no one's (was %d's) until a player is near it\n", e->name, e->id, e->owner);
                if (e->owner == me) {
                    Own *o = own_find(e->id);
                    if (o) own_drop(c, o);
                }
                e->owner = OWNER_NONE, g_reg[r].hold = 0, g_reg[r].nearer = 0;
            }
            continue;
        }
        LOG("[online] %s %u: slot %d's now (was %d's; %s, owner at %.0f m, new one at %.0f m)\n", e->name, e->id, nearest,
            e->owner, gone_owner ? "owner gone" : g_reg[r].nearer > 50 ? "new one nearer" : "owner far", cur, best);
        if (e->owner == me) {  // (ours no more: it goes here, its state stays for the next one)
            Own *o = own_find(e->id);
            if (o) own_drop(c, o);
        }
        if (nearest == me) own_add(e->id);
        e->owner = (uint8_t)nearest;
        g_reg[r].hold = 100, g_reg[r].nearer = 0;  // (10 s: this runs every 3 frames)
    }
    // ours being made: as they were
    for (int r = 0; r < REG; r++) {
        if (!g_reg[r].used || g_reg[r].e.owner != me) continue;
        Own *o = own_find(g_reg[r].e.id);
        if (o && o->state == OWN_MAKING) own_ready(c, o, &g_reg[r].e);
    }
    // everyone's
    int n = 0;
    for (int r = 0; r < REG && n < ONLINE_ENEMIES; r++)
        if (g_reg[r].used) oa_g_reg_out[n++] = g_reg[r].e;
    oa_g_reg_n = n;
    online_set_local_enemies(link, oa_g_reg_out, n);
}

// the others: his enemies' actors here
#define PUPPETS 64
static struct {
    uint32_t id;
    Piece p, w, sh;  // (the enemy, its weapon, its shield)
    char as[32];   // (the AS it plays: the host's one, once it plays)
    int tries;
    int32_t life;  // (its life as set: what it lost since is a hit)
    char seen[32];  // (the host's last AS for it: a new attack starts)
    int strike;     // (frames until its attack lands, 0 none)
    int dying;      // (frames left of its death here: it died in its owner's game; 0 alive)
    struct { uint64_t p; uint32_t id, pad; } fx;  // (its death's effect)
} g_puppet[PUPPETS];

// the scale an enemy holds a weapon at (online_mod/wscale.txt: its gparam's WeaponScale* per kind; 1 when not there)
static float weapon_scale(const char *enemy, const char *weapon) {
    static MapLine *m;
    static int n = -1;
    if (n < 0) n = oa_map_load("wscale.txt", &m);
    const char *v = oa_map_find(m, n, enemy);
    float s[5] = {1, 1, 1, 1, 1};
    if (!v || sscanf(v, "%f,%f,%f,%f,%f", &s[0], &s[1], &s[2], &s[3], &s[4]) != 5) return 1;
    return strstr(weapon, "_Lsword_") ? s[1] : strstr(weapon, "_Spear_") ? s[2] : strstr(weapon, "_Bow_") ? s[3]
         : strstr(weapon, "_Shield_") ? s[4] : s[0];
}

// an enemy's or a weapon's attack power (online_mod/power.txt), 0 unknown
int oa_attack_power(const char *name) {
    static MapLine *m;
    static int n = -1;
    if (n < 0) n = oa_map_load("power.txt", &m);
    const char *v = oa_map_find(m, n, name);
    return v ? atoi(v) : 0;
}

// the local Link's defence: his armour's (online_mod/defense.txt: each piece's armorDefenceAddLevel, the defence the game
// shows for it), the pieces he wears
int oa_our_defense(uint64_t player) {
    static MapLine *m;
    static int n = -1;
    if (n < 0) n = oa_map_load("defense.txt", &m);
    uint64_t armor[ARMOR], gear[ONLINE_GEAR];
    oa_equipment(player, armor, gear);
    int d = 0;
    for (int k = 0; k < ARMOR; k++) {
        char an[48];
        if (!armor[k]) continue;
        oa_actor_name_of(armor[k], an);
        const char *v = an[0] ? oa_map_find(m, n, an) : NULL;
        if (v) d += atoi(v);
    }
    return d;
}

void oa_strike(Cpu *c, uint64_t player, const OnlineEnemy *e) {
    int32_t *life = (int32_t *)(uintptr_t)(player + ENEMY_LIFE);
    if (*life <= 0) return;
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    float dx = pm[3] - e->m[3], dy = pm[7] - e->m[7], dz = pm[11] - e->m[11];
    float d = sqrtf(dx * dx + dz * dz);
    float fx = e->m[2], fz = e->m[10], fl = sqrtf(fx * fx + fz * fz);
    bool big = strstr(e->name, "Moriblin") || strstr(e->name, "Lynel") || strstr(e->name, "Giant") || strstr(e->name, "Golem");
    float reach = (big ? 3.5f : 2.2f) + (strstr(e->weapon, "Spear") || strstr(e->weapon, "Lsword") ? 1.5f : e->weapon[0] ? 0.8f : 0);
    if (d > reach || dy > 2.5f || dy < -2.5f) return;
    if (d > 0.8f && fl > 0 && (dx * fx + dz * fz) / (d * fl) < 0.3f) return;  // (behind it, or to its side)
    const char *up = oa_playing(c, player, 1, 0), *body = oa_playing(c, player, 0, 0);
    if (strstr(up, "Guard") || strstr(body, "Guard")) {
        LOG("[online] the host's %s: blocked\n", e->name);
        return;
    }
    int power = e->weapon[0] ? oa_attack_power(e->weapon) : 0;
    if (!power) power = oa_attack_power(e->name);
    if (power <= 0) return;
    int def = oa_our_defense(player);
    power = power > def ? power - def : 1;  // (his armour's defence off it, a quarter heart at least: the game's own way)
    *life = *life > power ? *life - power : 0;
    // a heavy blow (a kick, a big or jumping attack, any of a big one's) shakes him: OnlineShock (an enemy's shock wave,
    // unseen) a metre from him on its side; he flinches (OnlineKnock, a bomb's blast, knocks him down, but its explosion
    // is seen: SAVAGEWIND_ONLINE_KNOCK=OnlineKnock)
    bool heavy = big || strstr(e->as, "Kick") || strstr(e->as, "Large") || strstr(e->as, "Jump");
    if (heavy && *life > 0 && d > 0.01f) {
        float at[3] = {pm[3] - dx / d, pm[7] + 0.3f, pm[11] - dz / d};
        oa_boom_start(c, getenv("SAVAGEWIND_ONLINE_KNOCK") ? getenv("SAVAGEWIND_ONLINE_KNOCK") : "OnlineShock", at);
    }
    LOG("[online] %s hit us: %d (life %d)%s\n", e->name, power, *life, heavy ? ", knocked down" : "");
}

void oa_puppets_forget(void) {
    for (int k = 0; k < PUPPETS; k++) oa_orphan(&g_puppet[k].p), oa_orphan(&g_puppet[k].w), oa_orphan(&g_puppet[k].sh);
    memset(g_puppet, 0, sizeof g_puppet);
}

// oa_enemies_client's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static int oa_enemies_client_tick;

// a game not the host's: the host's enemies, each ours for real or shown
void oa_enemies_client(Cpu *c, uint64_t player, int me) {
    static OnlineEnemy e[ONLINE_ENEMIES];
    float centre[3];
    uint64_t age;
    int n = online_enemies(centre, e, &age);
    if (n < 0) return;  // (no host's enemies ever: his game is older, or none came)
    bool fresh = age < 1000000000ull;
    own_hits(c);
    if (fresh) {
        // ours: made when they come to us; no longer ours (or no longer anyone's): gone here
        for (int k = 0; k < n; k++)
            if (e[k].owner == me && me >= 0) {
                Own *o = own_find(e[k].id);
                if (!o) o = own_add(e[k].id);
                if (o) own_ready(c, o, &e[k]);
            }
        for (int k2 = 0; k2 < OWNS; k2++) {
            if (!oa_g_own[k2].state) continue;
            bool mine = false;
            for (int k = 0; k < n && !mine; k++) mine = e[k].id == oa_g_own[k2].gid && e[k].owner == me;
            if (!mine) LOG("[online] %u is not ours now\n", oa_g_own[k2].gid), own_drop(c, &oa_g_own[k2]);
        }
        // and their state, for the host's game (gone: died here)
        if (oa_enemies_client_tick++ % 3 == 0) {
            static OnlineEnemy st[ONLINE_ENEMIES];
            int ns = 0;
            for (int k2 = 0; k2 < OWNS && ns < ONLINE_ENEMIES; k2++) {
                Own *o = &oa_g_own[k2];
                if (o->state != OWN_HAVE) continue;
                memset(&st[ns], 0, sizeof st[ns]);
                st[ns].id = o->gid;
                if (oa_alive(o->actor, o->w)) oa_enemy_state(c, o->actor, &st[ns]);
                else {  // (kept as gone: his list has it ours a while longer, and it isn't to be made again)
                    uint32_t gid = o->gid;
                    st[ns].life = -1, memset(o, 0, sizeof *o);
                    o->gid = gid, o->state = OWN_GONE;
                }
                ns++;
            }
            if (ns) online_send_estates(st, ns);
        }
    }
    oa_enemies_show(c, player, e, n, fresh, centre, me, true);
}

// oa_enemies_show's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static int gone_logged;
static uint32_t marked[64];
static int nm;
static int slog2;
static int plog;
static uint64_t oa_enemies_show_t0;
static int oa_enemies_show_turn;
static uint32_t done_ids[ONLINE_ELEM_COUNT];
static bool logged[ONLINE_ELEM_COUNT];
static uint64_t oa_enemies_show_t0_2;
static Piece fire;
static bool placed;
static int at = -1;

void oa_enemies_show(Cpu *c, uint64_t player, OnlineEnemy *e, int n, bool fresh, const float centre[3], int me,
                         bool suppress) {
    // his area: our own enemies go (each frame: those the game makes there go as they come), but those we have for
    // everyone
    int na = suppress ? oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS) : 0, gone = 0;
    for (int k = 0; k < na; k++) {
        char name[48];
        uint64_t a = oa_g_actors[k];
        if (!oa_running(a) || !is_enemy(a, name) || own_is(a)) continue;  // (one going already: not again)
        const float *m = (const float *)(uintptr_t)(a + 0x398);
        float dx = m[3] - centre[0], dz = m[11] - centre[2];
        if (dx * dx + dz * dz > ENEMY_AREA * ENEMY_AREA) continue;
        enemy_delete(c, a);
        gone++;
    }
    if (gone && gone_logged++ < 20) LOG("[online] %d of our enemies gone: the host's are there\n", gone);
    // his: each one's actor, where his is, playing what his plays (his feed stopped: they stay as they are)
    for (int k = 0; fresh && k < n; k++) {
        if (e[k].life <= 0 && suppress) {  // (died, wherever: dead here too, once)
            bool done = false;
            for (int q = 0; q < 64 && !done; q++) done = marked[q] == e[k].id;
            if (!done) marked[nm++ % 64] = e[k].id, mark_dead(c, e[k].name, e[k].hash);
        }
        Own *ours = own_find(e[k].id);
        if (e[k].owner == me && ours && (ours->state == OWN_HAVE || ours->state == OWN_GONE))
            continue;  // (ours for real, or gone here: not shown too)
        int s2 = -1, free_s = -1;
        for (int j = 0; j < PUPPETS; j++) {
            if (g_puppet[j].id == e[k].id) s2 = j;
            if (!g_puppet[j].id && free_s < 0) free_s = j;
        }
        if (s2 < 0 && (s2 = free_s) < 0) continue;
        if (!g_puppet[s2].id) memset(&g_puppet[s2], 0, sizeof g_puppet[s2]), g_puppet[s2].id = e[k].id;
        if (g_puppet[s2].dying) continue;  // (dying here: as it is, until it goes)
        const char *pa = enemy_actor(e[k].name);
        Piece *p = &g_puppet[s2].p;
        // died in its owner's game (its body there a ragdoll, its root going anywhere): here it stays where it was,
        // plays its own Die, drops its weapon, and goes 2.5 s later with the game's "Dead" effect
        if (e[k].life <= 0 && p->actor) {
            g_puppet[s2].dying = 75;
            oa_delete_piece(c, &g_puppet[s2].w), oa_delete_piece(c, &g_puppet[s2].sh);
            oa_play_as(c, p->actor, "Die", 0, 0, -1, true);
            LOG("[online] the host's %s %u died\n", e[k].name, e[k].id);
            continue;
        }
        if (oa_piece_update(c, POOL_WORLD, p, pa ? pa : "", "host")) {
            g_puppet[s2].as[0] = 0;
            g_puppet[s2].life = 0;  // (a new actor: what it has is its own life, no hit)
            LOG("[online] the host's %s %u at %.1f %.1f %.1f, AS %s\n", e[k].name, e[k].id, e[k].m[3], e[k].m[7], e[k].m[11], e[k].as);
        }
        // its weapon and shield (OnlineGear actors, as the players'): in this one's own hand, as the owner's holds them
        // there (else where his are), at the size it holds them (a Moblin's are bigger)
        for (int h = 0; h < 2; h++) {
            const char *gn = h ? e[k].shield : e[k].weapon;
            const float *gm = h ? e[k].sm : e[k].wm;
            int bone = h ? e[k].sbone : e[k].wbone;
            const char *ga = gn[0] ? oa_gear_actor(gn) : NULL;
            Piece *w = h ? &g_puppet[s2].sh : &g_puppet[s2].w;
            if (w->name[0] && strcmp(w->name, ga ? ga : "")) oa_delete_piece(c, w);
            oa_piece_update(c, POOL_WORLD, w, ga ? ga : "", "host");
            if (!w->actor) continue;
            if (getenv("SAVAGEWIND_ONLINE_SCALE_LOG") && slog2++ % 300 == 0) {
                const float *am = (const float *)(uintptr_t)(w->actor + 0x398);
                LOG("[online] copy %s's %s: want %.2f, its matrix %.2f\n", e[k].name, gn, weapon_scale(e[k].name, gn),
                    sqrtf(am[0] * am[0] + am[4] * am[4] + am[8] * am[8]));
            }
            float wm[12], bm[12], sc = weapon_scale(e[k].name, gn);
            if (bone >= 1 && bone <= 2 && p->actor && oa_bone_mtx(c, p->actor, k_hands[bone - 1], bm)) oa_mtx_mul(bm, gm, wm);
            else if (bone) continue;  // (its hand not there yet)
            else memcpy(wm, gm, sizeof wm);
            oa_call(c, FN_SET_MTX, w->actor, (uint64_t)(uintptr_t)wm, 0, 0, 0, 0);
            // (its size: its model's root bone scaled; a scale in its matrix isn't drawn)
            int nb = 0;
            uint64_t unit = sc != 1 ? oa_first_unit(w->actor, &nb) : 0;
            if (unit && nb > 0) {
                float lm[12], lsc[3], ssc[3] = {sc, sc, sc};
                oa_call(c, FN_BONE_LOCAL_GET, unit, (uint64_t)(uintptr_t)lm, (uint64_t)(uintptr_t)lsc, 0, 0, 0);
                oa_call(c, FN_BONE_LOCAL_SET, unit, (uint64_t)(uintptr_t)lm, (uint64_t)(uintptr_t)ssc, 0, 0, 0);
            }
        }
        if (!p->actor) continue;
        if (getenv("SAVAGEWIND_ONLINE_PUPPET_LOG") && plog++ % 300 == 0) {  // (where a frame of the game left it)
            const float *am = (const float *)(uintptr_t)(p->actor + 0x398);
            LOG("[online] puppet %s: his at %.1f %.1f %.1f, ours at %.1f %.1f %.1f\n", e[k].name, e[k].m[3], e[k].m[7],
                e[k].m[11], am[3], am[7], am[11]);
        }
        oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)e[k].m, 0, 0, 0, 0);
        // its attacks on us
        if (strcmp(e[k].as, g_puppet[s2].seen)) {
            snprintf(g_puppet[s2].seen, sizeof g_puppet[s2].seen, "%s", e[k].as);
            if (getenv("SAVAGEWIND_ONLINE_ENEMY_AS_LOG")) LOG("[online] the host's %s %u: %s\n", e[k].name, e[k].id, e[k].as);
            if (strstr(e[k].as, "Attack") || strstr(e[k].as, "Kick")) g_puppet[s2].strike = STRIKE_FRAMES;
        }
        if (g_puppet[s2].strike > 0 && --g_puppet[s2].strike == 0) oa_strike(c, player, &e[k]);
        // hit here: what it lost since it was set goes to his game; its life is his (never 0 here: his dies there)
        int32_t *life = (int32_t *)(uintptr_t)(p->actor + ENEMY_LIFE);
        if (e[k].life > 0 && g_puppet[s2].life > 0 && *life < g_puppet[s2].life) {  // (dead in his game: no more)
            const float *pm = (const float *)(uintptr_t)(p->actor + 0x398);
            float at[3] = {pm[3], pm[7], pm[11]};
            uint8_t elem = oa_elem_near(at, 2.5f);  // (one of our element arrows there: its element goes with it)
            int32_t dmg = g_puppet[s2].life - *life;
            // (a sneak strike: a copy here has no senses, the game takes it as unaware and its blow from behind does
            // eight times its damage; one that is fighting in its owner's game - not waiting, asleep, sat, talking,
            // walking about - takes no more than our weapon's attack power, twice for a critical)
            const char *eas = e[k].as;
            bool unaware = !strncmp(eas, "Wait", 4) || strstr(eas, "Sleep") || !strncmp(eas, "Squat", 5) ||
                           strstr(eas, "Talk") || !strncmp(eas, "Walk", 4) || strstr(eas, "Sit") || strstr(eas, "Relax") ||
                           strstr(eas, "Eat") || strstr(eas, "Dance") || !strncmp(eas, "Look", 4) || !eas[0];
            if (!unaware) {
                uint64_t ar[ARMOR], gr[ONLINE_GEAR];
                oa_equipment(player, ar, gr);
                char wn[48] = "";
                if (gr[0]) oa_actor_name_of(gr[0], wn);
                int pw = wn[0] ? oa_attack_power(wn) : 0;
                if (pw > 0 && dmg > pw * 2) {
                    LOG("[online] a blow of %d on the host's %s %u (%s, fighting): %d, as without a sneak strike\n", dmg,
                        e[k].name, e[k].id, eas, pw);
                    dmg = pw;
                }
            }
            online_send_hit(e[k].id, dmg, elem);
            oa_g_aggro_id = e[k].id;
            if (!++oa_g_aggro_seq) oa_g_aggro_seq = 1;
            LOG("[online] we hit the host's %s %u: %d%s\n", e[k].name, e[k].id, dmg,
                elem == ONLINE_ELEM_FIRE ? " (fire)" : elem == ONLINE_ELEM_ICE ? " (ice)"
                : elem == ONLINE_ELEM_ELECTRIC ? " (electric)" : "");
        }
        *life = g_puppet[s2].life = e[k].life > 1 ? e[k].life : 1;
        // (again each frame until it plays: a just made actor can't yet; given up after a second)
        if (e[k].as[0] && strcmp(e[k].as, g_puppet[s2].as) && oa_running(p->actor)) {
            if (oa_play_as(c, p->actor, e[k].as, 0, 0, -1, false) || ++g_puppet[s2].tries > 30) {
                if (g_puppet[s2].tries > 30) LOG("[online] the host's %s: no AS %s\n", e[k].name, e[k].as);
                snprintf(g_puppet[s2].as, sizeof g_puppet[s2].as, "%s", e[k].as);
                g_puppet[s2].tries = 0;
            }
        }
    }
    // the dying ones: after their Die, the game's own vanishing (its XLink "Disappear", as 0x70015302c0 has a dying
    // actor's: 0x700159f00c(actor, name, 2, ...)), hidden, gone a second later
    for (int j = 0; j < PUPPETS; j++) {
        if (!g_puppet[j].id || !g_puppet[j].dying) continue;
        int d = --g_puppet[j].dying;
        uint64_t a = g_puppet[j].p.actor;
        bool there = a && oa_alive(a, g_puppet[j].p.w);
        if (d == 30 && there) {
            uint64_t out[4] = {0, 0, 0, 0};
            oa_call(c, FN_XLINK_EMIT, a, (uint64_t)(uintptr_t)"Disappear", 2, (uint64_t)(uintptr_t)out, 0, 0);
        }
        if (d == 20 && there) oa_model_visible(c, a, false);
        if (d > 0) continue;
        oa_delete_piece(c, &g_puppet[j].p), oa_delete_piece(c, &g_puppet[j].w), oa_delete_piece(c, &g_puppet[j].sh);
        g_puppet[j].id = 0;
    }
    {   // (test: SAVAGEWIND_ONLINE_ELEM_HIT_TEST=<s>: that many seconds after the first copy here, a hit of 1 with fire,
        // then ice, then electric, each on another copy 8 s apart, as our element arrows on them would send: its owner's
        // game gives the enemy the element's state)
        static const char *ht;
        if (!ht) ht = getenv("SAVAGEWIND_ONLINE_ELEM_HIT_TEST") ? getenv("SAVAGEWIND_ONLINE_ELEM_HIT_TEST") : "";
        if (*ht && !oa_enemies_show_t0)
            for (int j = 0; j < PUPPETS && !oa_enemies_show_t0; j++)
                if (g_puppet[j].id && g_puppet[j].p.actor) oa_enemies_show_t0 = plat_now_ns();
        for (int q = 0; *ht && oa_enemies_show_t0 && q < oa_enemies_show_turn; q++)  // (4 s after each: its life in the host's list, once)
            if (!logged[q] && plat_now_ns() - oa_enemies_show_t0 >= (uint64_t)((atof(ht) + 8.0 * q + 4.0) * 1e9))
                for (int k = 0; k < n; k++)
                    if (e[k].id == done_ids[q]) {
                        LOG("[online] element hit test: %u now at %d life (element %d)\n", e[k].id, e[k].life, q + 1);
                        logged[q] = true;
                        break;
                    }
        if (*ht && oa_enemies_show_t0 && oa_enemies_show_turn < ONLINE_ELEM_COUNT - 1 &&
            plat_now_ns() - oa_enemies_show_t0 >= (uint64_t)((atof(ht) + 8.0 * oa_enemies_show_turn) * 1e9))
            for (int pick = 0; pick < 1; pick++) {  // (the nearest copy not hit yet)
                int k = -1;
                float bd = 1e30f;
                for (int j = 0; j < n; j++) {
                    bool used = false;
                    for (int q = 0; q < oa_enemies_show_turn; q++) used |= done_ids[q] == e[j].id;
                    if (used || e[j].owner == me || e[j].life <= 1) continue;
                    const float *lm = (const float *)(uintptr_t)(player + 0x398);
                    float lp[3] = {lm[3], lm[7], lm[11]}, d = dist3(e[j].m, lp);
                    if (d < bd) bd = d, k = j;
                }
                if (k < 0) break;
                uint8_t elem = (uint8_t)(ONLINE_ELEM_FIRE + oa_enemies_show_turn);
                online_send_hit(e[k].id, 1, elem);
                oa_g_aggro_id = e[k].id;
                if (!++oa_g_aggro_seq) oa_g_aggro_seq = 1;
                LOG("[online] element hit test: %u, element %d\n", e[k].id, elem);
                done_ids[oa_enemies_show_turn++] = e[k].id;
                break;
            }
    }
    {   // (test: SAVAGEWIND_ONLINE_FIRE_TEST=<s>: that many seconds after the first copy here, a lit campfire, FireWood,
        // under it, made in this game only: what the copy loses in it is no player's, and its owner's enemy isn't in it)
        static const char *ft;
        if (!ft) ft = getenv("SAVAGEWIND_ONLINE_FIRE_TEST") ? getenv("SAVAGEWIND_ONLINE_FIRE_TEST") : "";
        if (*ft && !placed) {
            if (at < 0)
                for (int j = 0; j < PUPPETS && at < 0; j++)
                    if (g_puppet[j].id && !g_puppet[j].dying && g_puppet[j].p.actor) at = j, oa_enemies_show_t0_2 = plat_now_ns();
            if (at >= 0 && plat_now_ns() - oa_enemies_show_t0_2 >= (uint64_t)(atof(ft) * 1e9)) {
                oa_piece_update(c, POOL_WORLD, &fire, "FireWood", "fire test");
                uint64_t a = g_puppet[at].p.actor;
                if (fire.actor && a && oa_alive(a, g_puppet[at].p.w)) {
                    const float *m = (const float *)(uintptr_t)(a + 0x398);
                    float fm[12] = {1, 0, 0, m[3], 0, 1, 0, m[7], 0, 0, 1, m[11]};
                    oa_call(c, FN_SET_MTX, fire.actor, (uint64_t)(uintptr_t)fm, 0, 0, 0, 0);
                    placed = true;
                    LOG("[online] fire test: FireWood under the copy of %u at %.1f %.1f %.1f\n", g_puppet[at].id, m[3],
                        m[7], m[11]);
                }
            }
        }
    }
    // those no one has (killed, gone from the host's world), or we have for real now, go
    for (int j = 0; fresh && j < PUPPETS; j++) {
        if (!g_puppet[j].id) continue;
        bool there = false;
        for (int k = 0; k < n && !there; k++) {
            Own *ours = own_find(e[k].id);
            there = e[k].id == g_puppet[j].id &&
                    !(e[k].owner == me && ours && (ours->state == OWN_HAVE || ours->state == OWN_GONE));
        }
        if (!there && !g_puppet[j].dying)
            oa_delete_piece(c, &g_puppet[j].p), oa_delete_piece(c, &g_puppet[j].w), oa_delete_piece(c, &g_puppet[j].sh), g_puppet[j].id = 0;
    }
}

// The bosses near the local Link and the others': each game sends the damage its own player has done to each boss so far
// (what its life lost here, but for what the other players' damage took: OnlinePose.boss[].life); what another's grows
// by is taken from that boss here too (the same map object: its HashId), so every player's blows add up in every game
// (its phases follow its life, each game its own way; a phase's new life, or a boss made again, takes nothing back)
static struct { uint64_t a; Watch w; uint32_t hash; char name[28]; } g_boss[ONLINE_BOSSES];
#define BOSS_TRACK 4
static struct { uint64_t a; uint32_t hash; int32_t last, applied, done; } g_btrack[BOSS_TRACK];  // (done: ours so far)
static struct { uint32_t hash; int32_t seen; } g_bseen[ONLINE_MAX][BOSS_TRACK];  // (each player's damage as last seen)
static int btrack(uint64_t a, uint32_t hash, int32_t life) {
    for (int k = 0; k < BOSS_TRACK; k++)
        if (g_btrack[k].a == a) return k;
    int k = 0;
    while (k < BOSS_TRACK - 1 && g_btrack[k].a) k++;
    g_btrack[k].a = a, g_btrack[k].hash = hash, g_btrack[k].last = life, g_btrack[k].applied = 0, g_btrack[k].done = 0;
    return k;
}

// oa_boss_sample's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static int oa_boss_sample_tick;
static int t;
static char last[28];
static int logt;

void oa_boss_sample(Cpu *c, uint64_t player, OnlinePose *pose) {
    if (oa_boss_sample_tick++ % 6 == 0) {
        memset(g_boss, 0, sizeof g_boss);
        float best[ONLINE_BOSSES] = {500.0f * 500.0f, 500.0f * 500.0f};
        int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        const float *pm = (const float *)(uintptr_t)(player + 0x398);
        for (int k = 0; k < na; k++) {
            char n[48];
            oa_actor_name_of(oa_g_actors[k], n);
            // (only the map's bosses, with a HashId, the one their damage is shared by: a Blight's drones,
            // Enemy_SiteBoss_*_ChildDev, have none, and nearer than it they took both places, so the Blight's own damage
            // wasn't shared while they flew)
            if (!is_boss(n) || !*(uint32_t *)(uintptr_t)(oa_g_actors[k] + ACTOR_HASH_ID)) continue;
            if (*(int32_t *)(uintptr_t)(oa_g_actors[k] + ENEMY_LIFE) <= 0) continue;  // (beaten: nothing more to share)
            const float *m = (const float *)(uintptr_t)(oa_g_actors[k] + 0x398);
            float d = (m[3] - pm[3]) * (m[3] - pm[3]) + (m[7] - pm[7]) * (m[7] - pm[7]) + (m[11] - pm[11]) * (m[11] - pm[11]);
            int slot = d < best[0] ? 0 : d < best[1] ? 1 : -1;
            if (slot < 0) continue;
            if (slot == 0) g_boss[1] = g_boss[0], best[1] = best[0];
            best[slot] = d;
            g_boss[slot].a = oa_g_actors[k], g_boss[slot].w = oa_watch(oa_g_actors[k]);
            g_boss[slot].hash = *(uint32_t *)(uintptr_t)(oa_g_actors[k] + ACTOR_HASH_ID);
            snprintf(g_boss[slot].name, sizeof g_boss[slot].name, "%s", n);
        }
        // (the tracks of bosses no longer here go: kept, after four bosses (the castle's Blights) the next one, Ganon,
        // shared the last track with another and its damage was counted from nothing again and again)
        for (int t = 0; t < BOSS_TRACK; t++) {
            bool here = false;
            for (int b = 0; b < ONLINE_BOSSES && !here; b++) here = g_btrack[t].a && g_btrack[t].a == g_boss[b].a;
            if (!here) memset(&g_btrack[t], 0, sizeof g_btrack[t]);
        }
    }
    {   // (test: SAVAGEWIND_ONLINE_BOSS_HIT=<s>: the nearest boss's life halved that many seconds after it is first here,
        // and again every 20 s, to 0; SAVAGEWIND_ONLINE_BOSS_KILL=<s>: to 0 then, each boss)
        if (g_boss[0].a && strcmp(last, g_boss[0].name)) t = 0, snprintf(last, sizeof last, "%s", g_boss[0].name);
        const char *bk = getenv("SAVAGEWIND_ONLINE_BOSS_KILL"), *bh = getenv("SAVAGEWIND_ONLINE_BOSS_HIT");
        bool here = g_boss[0].a && oa_alive(g_boss[0].a, g_boss[0].w);
        if (here && (bk || bh)) t++;
        int32_t *l = here ? (int32_t *)(uintptr_t)(g_boss[0].a + ENEMY_LIFE) : NULL;
        if (bk && here && t % (atoi(bk) * 30) == 0 && *l > 0) *l = 0, LOG("[online] boss kill test: %s\n", g_boss[0].name);
        if (bh && here && t >= atoi(bh) * 30 && (t - atoi(bh) * 30) % 600 == 0) {
            *l = *l > 10 ? *l / 2 : 0;
            LOG("[online] boss hit test: %s at %d\n", g_boss[0].name, *l);
        }
    }
    for (int k = 0; getenv("SAVAGEWIND_ONLINE_BOSS_LOG") && logt++ % 60 == 0 && k < ONLINE_BOSSES; k++)
        if (g_boss[k].a && oa_alive(g_boss[k].a, g_boss[k].w)) {
            const float *m = (const float *)(uintptr_t)(g_boss[k].a + 0x398);
            LOG("[online] boss %s %u: life %d at %.0f %.0f %.0f\n", g_boss[k].name, g_boss[k].hash,
                *(int32_t *)(uintptr_t)(g_boss[k].a + ENEMY_LIFE), m[3], m[7], m[11]);
        }
    for (int k = 0; k < ONLINE_BOSSES; k++) {
        if (!g_boss[k].a || !oa_alive(g_boss[k].a, g_boss[k].w)) continue;
        int32_t life = *(int32_t *)(uintptr_t)(g_boss[k].a + ENEMY_LIFE);
        int t = btrack(g_boss[k].a, g_boss[k].hash, life);
        if (life < g_btrack[t].last) {  // (lost: ours, but for what the others took)
            int32_t ours = g_btrack[t].last - life - g_btrack[t].applied;
            if (ours > 0) g_btrack[t].done += ours;
        }
        g_btrack[t].last = life, g_btrack[t].applied = 0;  // (more: a new phase, made again: nothing of ours)
        pose->boss[k].hash = g_boss[k].hash;
        memcpy(pose->boss[k].name, g_boss[k].name, sizeof pose->boss[k].name);
        pose->boss[k].life = g_btrack[t].done;
    }
}

void oa_boss_update(Cpu *c, const OnlinePlayer *pl, int me) {
    // (each player's damage as last seen, of the bosses he sends now only: the others' places free, else after four
    // bosses the next shared the last place with another, seen anew each time, its damage never taken)
    for (int i = 0; i < ONLINE_MAX; i++)
        for (int s = 0; s < BOSS_TRACK; s++) {
            bool sent = false;
            for (int q = 0; q < ONLINE_BOSSES && !sent && i != me && pl[i].used && pl[i].pose.valid; q++)
                sent = g_bseen[i][s].hash && pl[i].pose.boss[q].hash == g_bseen[i][s].hash && pl[i].pose.boss[q].name[0];
            if (!sent) g_bseen[i][s].hash = 0, g_bseen[i][s].seen = 0;
        }
    for (int k = 0; k < ONLINE_BOSSES; k++) {
        if (!g_boss[k].a || !oa_alive(g_boss[k].a, g_boss[k].w)) continue;
        int32_t *life = (int32_t *)(uintptr_t)(g_boss[k].a + ENEMY_LIFE);
        int t = btrack(g_boss[k].a, g_boss[k].hash, *life);
        for (int i = 0; i < ONLINE_MAX; i++) {
            if (i == me || !pl[i].used || !pl[i].pose.valid || !oa_same_map(&pl[i])) continue;
            for (int q = 0; q < ONLINE_BOSSES; q++) {
                const char *rn = pl[i].pose.boss[q].name;
                if (!rn[0] || strcmp(rn, g_boss[k].name) || pl[i].pose.boss[q].hash != g_boss[k].hash) continue;
                int32_t dmg = pl[i].pose.boss[q].life;
                int s2 = 0;  // (its place, else a free one: places of bosses he no longer sends were freed above)
                while (s2 < BOSS_TRACK && g_bseen[i][s2].hash != g_boss[k].hash) s2++;
                if (s2 == BOSS_TRACK)
                    for (s2 = 0; s2 < BOSS_TRACK - 1 && g_bseen[i][s2].hash; s2++) {}
                if (g_bseen[i][s2].hash != g_boss[k].hash) {  // (first seen: from now on)
                    g_bseen[i][s2].hash = g_boss[k].hash, g_bseen[i][s2].seen = dmg;
                    continue;
                }
                int32_t d = dmg - g_bseen[i][s2].seen;
                g_bseen[i][s2].seen = dmg;
                if (d <= 0 || *life <= 0) continue;
                if (d > *life) d = *life;
                *life -= d, g_btrack[t].applied += d;
                LOG("[online] %s: %d of %s's damage here (%d left)\n", g_boss[k].name, d, pl[i].name, *life);
            }
        }
    }
}

static int tick;  // (oa_enemies_frame's)

// rt_online_frame, step 17, the enemies: the host's for everyone
void oa_enemies_frame(Cpu *c, uint64_t player, const float link[3], int me) {
    OnlinePlayer epl[ONLINE_MAX];
    online_players(epl);
    if (VALID(player) && online_is_host()) {
        if (tick++ % 3 == 0) oa_enemies_host(c, player, link, epl, me);
        oa_enemies_show(c, player, oa_g_reg_out, oa_g_reg_n, true, link, me, false);  // (the others': shown here)
    } else if (VALID(player)) oa_enemies_client(c, player, me);
}

// A reboot (runtime/main.c teardown): the enemies, puppets, element sources and bosses here were actors of the last
// boot, gone with its memory - forgotten, never deleted (nothing here calls the game); the host's register, the lists,
// the counters and the tests start again as in a fresh process
RT_ON_RESET(oa_enemies_reset) {
    memset(&oa_g_refused_enemy, 0, sizeof oa_g_refused_enemy);
    memset(&oa_g_refused_weapon, 0, sizeof oa_g_refused_weapon);
    memset(&oa_g_refused_loot, 0, sizeof oa_g_refused_loot);
    miss = 0;
    memset(oa_g_own, 0, sizeof oa_g_own);
    memset(g_own_left, 0, sizeof g_own_left);
    memset(g_esrc, 0, sizeof g_esrc);
    memset(g_reg, 0, sizeof g_reg);
    memset(oa_g_reg_out, 0, sizeof oa_g_reg_out);
    oa_g_reg_n = 0;
    // oa_enemies_host's
    oa_enemies_host_t0 = 0;
    done = false;
    oa_enemies_host_t0_2 = until = next = 0;
    memset(&src, 0, sizeof src);
    who = -1;
    last_life = oa_enemies_host_turn = 0;
    memset(used, 0, sizeof used);
    memset(name, 0, sizeof name);
    memset(last_as, 0, sizeof last_as);
    memset(could, 0, sizeof could);
    memset(aggro_seen, 0, sizeof aggro_seen);
    memset(aggro_init, 0, sizeof aggro_init);
    memset(g_puppet, 0, sizeof g_puppet);
    oa_enemies_client_tick = 0;
    // oa_enemies_show's
    gone_logged = 0;
    memset(marked, 0, sizeof marked);
    nm = slog2 = plog = 0;
    oa_enemies_show_t0 = 0;
    oa_enemies_show_turn = 0;
    memset(done_ids, 0, sizeof done_ids);
    memset(logged, 0, sizeof logged);
    oa_enemies_show_t0_2 = 0;
    memset(&fire, 0, sizeof fire);
    placed = false;
    at = -1;
    memset(g_boss, 0, sizeof g_boss);
    memset(g_btrack, 0, sizeof g_btrack);
    memset(g_bseen, 0, sizeof g_bseen);
    oa_boss_sample_tick = t = logt = 0;
    memset(last, 0, sizeof last);
    tick = 0;
}
