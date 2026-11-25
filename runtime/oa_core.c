// Online play's actors, oa_core: calls into the game, actors watched and made (pieces, handles, the pool, orphans), the
// .txt maps, bones and matrices, the list of the world's actors. What the modules share:
// runtime/online_actors_internal.h
#include "online_actors_internal.h"

bool framepace_title_ui(void);
bool framepace_paused(void);

// whether an actor is a physical object: its body (+0x190) is a real physics object, whose vtable is in the game's
// image. Other kinds of actor (a LinkTagAnd, any tag) have other fields there: a heap pointer to a heap pointer, and
// at +0x710 whatever their class keeps (Stasis's "stopped" bit 8 was set on one: it was taken for a stopped object,
// and the other game, applying Stasis to it, crashed)
// (it reads through +0x190: only a pointer-like value, aligned as an object is, is followed - what another class keeps
// there could otherwise point at an unmapped page; callers check their cheap fields first, so few actors get here)
bool has_body(uint64_t a) {
    if (!VALID(a)) return false;
    uint64_t body = *(uint64_t *)(uintptr_t)(a + 0x190);
    return VALID(body) && !(body & 7) && IN_IMAGE(*(uint64_t *)(uintptr_t)body);
}

Watch watch(uint64_t actor) {
    Watch w = {*(uint64_t *)(uintptr_t)(actor + 0x68), *(uint32_t *)(uintptr_t)(actor + 0x60)};
    return w;
}

bool alive(uint64_t actor, Watch w) {
    return actor && VALID(w.unit) && *(uint32_t *)(uintptr_t)(w.unit + 0x40) == w.id &&
           *(uint64_t *)(uintptr_t)(w.unit + 0x48) == actor && *(uint8_t *)(uintptr_t)(actor + 0x64) != 3;
}

// alive, or the same actor gone to another proc unit (woken: by us, or by itself as the player comes near - a
// shrine's elevator): its id the same, not deleted; then watched again
bool alive_rewatch(uint64_t actor, Watch *w) {
    if (alive(actor, *w)) return true;
    if (!actor || *(uint32_t *)(uintptr_t)(actor + 0x60) != w->id || *(uint8_t *)(uintptr_t)(actor + 0x64) == 3) return false;
    Watch n = watch(actor);
    if (!VALID(n.unit) || *(uint64_t *)(uintptr_t)(n.unit + 0x48) != actor) return false;
    *w = n;
    return true;
}

#define HANDLES 4096
static Handle g_handles[HANDLES];
static int g_next_handle;
Handle *new_handle(void) {
    Handle *h = &g_handles[g_next_handle++ % HANDLES];
    memset(h, 0, sizeof *h);
    return h;
}

// A remote player in the world: his Link, his armour and his gear
OaAct g_act[ONLINE_MAX];
bool g_local_dead;  // the local Link is dead (his body's AS: Dead...): game over is coming
uint32_t g_aggro_id;   // the host's enemy we hit last (not ours: shown here), and that hit's number
uint16_t g_aggro_seq;  // (OnlinePose.aggro_*: the host's game hands it to us, so it turns on us)
OnlineObj g_objs_out[ONLINE_OBJS];  // (the world's objects moving near us: objs_update)
uint8_t g_objs_out_trunk;           // (bit k: g_objs_out[k] a cut tree's trunk, by the tree's HashId)
OnlineMover g_movers_out[ONLINE_MOVERS];  // (the level's mechanisms moving near us: movers_update)
uint32_t g_broken[8], g_trunks_gone[4];  // (the map's objects we broke, our trunks chopped: breaks_update)
volatile bool g_online_actor[ONLINE_MAX];  // (online.h online_has_actor: the overlay draws no figure for these)

bool online_has_actor(int slot) { return slot >= 0 && slot < ONLINE_MAX && g_online_actor[slot]; }

// SAVAGEWIND_ONLINE_ACTOR, else OnlinePlayer when the overlay is online play's (tools/online_mod.py)
const char *actor_name(void) {
    const char *n = getenv("SAVAGEWIND_ONLINE_ACTOR");
    if (n && *n) return n;
    static int mod = -1;
    if (mod < 0) {
        const char *ov = getenv("SAVAGEWIND_ROMFS_OVERLAY");
        char p[1100];
        snprintf(p, sizeof p, "%s/online_mod.txt", ov ? ov : "");
        FILE *f = ov && *ov ? fopen(p, "rb") : NULL;
        mod = f != NULL;
        if (f) fclose(f);
    }
    return mod ? "OnlinePlayer" : NULL;
}

bool is_online_player(void) { return actor_name() && !strcmp(actor_name(), "OnlinePlayer"); }

// (not on another version of the game: the addresses here are 1.9.0's, runtime/online_gate.h; said once in the log)
bool online_actors_on(void) {
    static int on = -1;
    if (on < 0) {
        bool version = online_version_ok(g_game_version);
        on = version && actor_name() != NULL;
        LOG("[online] game version %s: actors %s\n", g_game_version && *g_game_version ? g_game_version : "unknown",
            on ? "on" : version ? "off" : "off (online play's actors need 1.9.0)");
    }
    return on;
}

// a call into the game with the frame loop's registers kept as they were
uint64_t call_f(Cpu *c, uint64_t fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                       uint64_t a6, float s0) {
    static Cpu saved;  // (the main thread only)
    memcpy(&saved, c, sizeof saved);
    c->x[0] = a0, c->x[1] = a1, c->x[2] = a2, c->x[3] = a3, c->x[4] = a4, c->x[5] = a5, c->x[6] = a6;
    union { float f; uint32_t u; } v = {s0};
    c->v[0] = v.u;
    c->sp = (c->sp - 0x400) & ~0xFull;  // (room below the frame loop's own stack frame)
    guest_call(c, fn);
    uint64_t r = c->x[0];
    memcpy(c, &saved, sizeof saved);
    return r;
}

// a call whose result the game returns in memory (x8: where)
void call_sret(Cpu *c, uint64_t fn, uint64_t out, uint64_t a0, uint64_t a1) {
    static Cpu saved;  // (the main thread only)
    memcpy(&saved, c, sizeof saved);
    for (int k = 0; k < 8; k++) c->x[k] = 0;
    c->x[0] = a0, c->x[1] = a1, c->x[8] = out;
    c->sp = (c->sp - 0x400) & ~0xFull;
    guest_call(c, fn);
    memcpy(c, &saved, sizeof saved);
}

uint64_t call(Cpu *c, uint64_t fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return call_f(c, fn, a0, a1, a2, a3, a4, a5, 0, 0);
}

// The AS an actor plays in a slot and bank ("" for none)
const char *playing(Cpu *c, uint64_t actor, int slot, int bank) {
    uint64_t mgr = *(uint64_t *)(uintptr_t)(actor + 0x560);
    uint64_t as = mgr ? call(c, FN_AS_IN_SLOT, mgr, slot, bank, 0, 0, 0) : 0;
    const char *now = as ? *(const char **)(uintptr_t)(as + 8) : NULL;
    return now ? now : "";
}

// play the AS `name` on actor in a slot and bank (blend: frames, -1 the AS's own; again: even if it plays it already,
// from its start); true when it plays
bool play_as(Cpu *c, uint64_t actor, const char *name, int slot, int bank, float blend, bool again) {
    uint64_t fake_action[2] = {0, actor};
    call_f(c, FN_PLAY_AS, (uint64_t)(uintptr_t)fake_action, (uint64_t)(uintptr_t)name, again ? 0 : 1, slot, bank, 0, 0,
           blend);
    const char *now = playing(c, actor, slot, bank);
    if (getenv("SAVAGEWIND_ONLINE_ACTOR_LOG")) LOG("[online] AS %s on %llx %d.%d: now %s\n", name, (unsigned long long)actor, slot, bank, now);
    return !strcmp(now, name);
}

Piece g_pool[ONLINE_MAX][POOL];

// requests still under way when all was forgotten: what they make is deleted once made (else it would stay in the
// world, a piece nobody moves)
#define ORPHANS 128
static Piece g_orphan[ORPHANS];
void orphan(Piece *p) {
    if (!p->pending) return;
    for (int k = 0; k < ORPHANS; k++)
        if (!g_orphan[k].pending) { g_orphan[k] = *p; return; }
}

void orphans_update(Cpu *c) {
    for (int k = 0; k < ORPHANS; k++) {
        Piece *q = &g_orphan[k];
        if (!q->pending) continue;
        int *unit = (int *)(uintptr_t)q->handle->unit;
        if (unit && *unit == 2) {
            uint64_t a = call(c, FN_HANDLE_TAKE, (uint64_t)(uintptr_t)q->handle, 0, 0, 0, 0, 0);
            if (a) call(c, FN_DELETE, a, 0, 0, 0, 0, 0);
            memset(q, 0, sizeof *q);
        } else if (unit && (*unit == 3 || *unit == 5)) {
            call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)q->handle, 0, 0, 0, 0, 0);
            memset(q, 0, sizeof *q);
        }
    }
}

// Each piece is made once for a player and kept for the session: a second actor of a piece made for him (after the
// first was deleted, or made and never taken) is never drawn, whatever was tried (made at once or asked for, with or
// without init params, worn or not when deleted). A piece he takes off goes to his pool: unworn, under the world; one
// still being made goes there as it is (0x70015168a8 doesn't stop a request under way). Put on again, it comes back
// from there. The pool goes when he leaves, or at a load (the game removes the world's actors)
void delete_piece(Cpu *c, Piece *p) {
    if (alive(p->actor, p->w)) {
        if (!strncmp(p->name, "Armor_", 6)) call(c, FN_ARMOR_BIND, p->actor, 0, 0, 0, 0, 0);
        call(c, FN_DELETE, p->actor, 0, 0, 0, 0, 0);
    }
    if (p->pending) call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)p->handle, 0, 0, 0, 0, 0);
    memset(p, 0, sizeof *p);
}

// out of sight: unworn, under the world
// every material of an actor's model shown or hidden (an armour piece taken off: unworn, it still goes with the
// skeleton it was on). The model's units: +0x38 how many, +0x40 the array (each **unit); unit->vtable[0xb8](unit)
// its material count, unit->vtable[0x1b0](unit, material, visible) (as 0x700126ba4c goes through them)
void model_visible(Cpu *c, uint64_t actor, bool visible) {
    uint64_t model = *(uint64_t *)(uintptr_t)(actor + 0x4e0);
    if (!VALID(model)) return;
    int n = *(int *)(uintptr_t)(model + 0x38);
    uint64_t units = *(uint64_t *)(uintptr_t)(model + 0x40);
    for (int u = 0; VALID(units) && u < n && u < 16; u++) {
        uint64_t unit = *(uint64_t *)(uintptr_t)(units + 8 * u);
        unit = VALID(unit) ? *(uint64_t *)(uintptr_t)unit : 0;
        if (!VALID(unit)) continue;
        uint64_t vt = *(uint64_t *)(uintptr_t)unit;
        int mats = (int)call(c, *(uint64_t *)(uintptr_t)(vt + 0xb8), unit, 0, 0, 0, 0, 0);
        for (int m = 0; m < mats && m < 256; m++) call(c, *(uint64_t *)(uintptr_t)(vt + 0x1b0), unit, m, visible, 0, 0, 0);
    }
}

static void hide_piece(Cpu *c, const Piece *p, const float *at) {
    if (!strncmp(p->name, "Armor_", 6)) call(c, FN_ARMOR_BIND, p->actor, 0, 0, 0, 0, 0), model_visible(c, p->actor, false);
    float m[12] = {1, 0, 0, at[0], 0, 1, 0, at[1] - 5000, 0, 0, 1, at[2]};
    call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
}

float g_pool_at[3];  // (where the pool's pieces are hidden: under the local Link)

static void pool_piece(Cpu *c, int i, Piece *p) {
    if (!p->pending && !alive(p->actor, p->w)) { memset(p, 0, sizeof *p); return; }
    if (p->actor) hide_piece(c, p, g_pool_at);
    Piece *slot = NULL;
    for (int k = 0; k < POOL && !slot; k++)
        if (!g_pool[i][k].actor && !g_pool[i][k].pending) slot = &g_pool[i][k];
    if (!slot) slot = &g_pool[i][0], delete_piece(c, slot);  // (full: one goes; it won't be drawn again)
    *slot = *p;
    memset(p, 0, sizeof *p);
}

// a piece of `want` from his pool into p: true when it is there (made; armour to be worn again)
static bool from_pool(Cpu *c, int i, Piece *p, const char *want) {
    for (int k = 0; k < POOL; k++) {
        Piece *q = &g_pool[i][k];
        if ((!q->actor && !q->pending) || strcmp(q->name, want)) continue;
        if (q->actor && !alive(q->actor, q->w)) { memset(q, 0, sizeof *q); continue; }
        *p = *q;
        memset(q, 0, sizeof *q);
        return p->actor != 0;
    }
    return false;
}

// the pool's pieces still being made, as they come: taken, out of sight
void pool_update(Cpu *c) {
    for (int i = 0; i < ONLINE_MAX; i++)
        for (int k = 0; k < POOL; k++) {
            Piece *q = &g_pool[i][k];
            int *unit = q->pending ? (int *)(uintptr_t)q->handle->unit : NULL;
            if (!unit) continue;
            if (*unit == 2) {
                q->pending = false;
                q->actor = call(c, FN_HANDLE_TAKE, (uint64_t)(uintptr_t)q->handle, 0, 0, 0, 0, 0);
                if (q->actor) q->w = watch(q->actor), hide_piece(c, q, g_pool_at);
                else memset(q, 0, sizeof *q);
            } else if (*unit == 3 || *unit == 5) {
                call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)q->handle, 0, 0, 0, 0, 0);
                memset(q, 0, sizeof *q);
            }
        }
}

// his Link goes (out of reach, his Link removed by the game...): his pieces to his pool; left: they go too
void drop(Cpu *c, int i, bool left) {
    for (int k = 0; k < ARMOR; k++) pool_piece(c, i, &g_act[i].armor[k]);
    for (int k = 0; k < ONLINE_GEAR; k++) pool_piece(c, i, &g_act[i].gear[k]);
    pool_piece(c, i, &g_act[i].horse);
    pool_piece(c, i, &g_act[i].tack[0]), pool_piece(c, i, &g_act[i].tack[1]);
    for (int k = 0; k < ONLINE_SHOTS; k++) pool_piece(c, i, &g_act[i].shot[k]);
    for (int k = 0; k < ONLINE_HELD; k++) pool_piece(c, i, &g_act[i].held[k]);
    pool_piece(c, i, &g_act[i].nock);
    pool_piece(c, i, &g_act[i].slate), pool_piece(c, i, &g_act[i].rune_fx), pool_piece(c, i, &g_act[i].stop_fx);
    pool_piece(c, i, &g_act[i].magnet_fx);
    if (left)
        for (int k = 0; k < POOL; k++) delete_piece(c, &g_pool[i][k]);
    if (alive(g_act[i].link, g_act[i].link_w)) call(c, FN_DELETE, g_act[i].link, 0, 0, 0, 0, 0);
    LOG("[online] removed %s's Link\n", g_act[i].name);
    memset(&g_act[i], 0, sizeof g_act[i]);
    g_act[i].anim = -1, g_act[i].covered = -1;
}

// online_mod/gear.txt (tools/online_mod.py): each weapon, shield, bow, sheath and quiver's online actor, "actor=its
// online actor" a line
static char (*g_gear_map)[2][48];
static int g_gear_n = -1;
const char *gear_actor(const char *piece) {
    if (g_gear_n < 0) {
        g_gear_n = 0;
        const char *ov = getenv("SAVAGEWIND_ROMFS_OVERLAY");
        char p[1100], line[128];
        snprintf(p, sizeof p, "%s/gear.txt", ov ? ov : "");
        FILE *f = ov && *ov ? fopen(p, "r") : NULL;
        int cap = 0;
        while (f && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            if (g_gear_n == cap) g_gear_map = realloc(g_gear_map, sizeof *g_gear_map * (cap = cap ? cap * 2 : 256));
            snprintf(g_gear_map[g_gear_n][0], 48, "%s", line);
            snprintf(g_gear_map[g_gear_n][1], 48, "%s", eq + 1);
            g_gear_n++;
        }
        if (f) fclose(f);
        LOG("[online] %d pieces of gear for the other players\n", g_gear_n);
    }
    for (int i = 0; i < g_gear_n; i++)
        if (!strcmp(g_gear_map[i][0], piece)) return g_gear_map[i][1];
    return NULL;
}

int map_load(const char *file, MapLine **out) {
    const char *ov = getenv("SAVAGEWIND_ROMFS_OVERLAY");
    char p[1100];
    snprintf(p, sizeof p, "%s/%s", ov ? ov : "", file);
    FILE *f = ov && *ov ? fopen(p, "rb") : NULL;
    int n = 0, cap = 0;
    static char line[16384];
    while (f && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        if (n == cap) *out = realloc(*out, sizeof **out * (cap = cap ? cap * 2 : 256));
        snprintf((*out)[n].key, sizeof (*out)[n].key, "%s", line);
        (*out)[n].val = strdup(eq + 1);
        n++;
    }
    if (f) fclose(f);
    return n;
}

const char *map_find(MapLine *m, int n, const char *key) {
    for (int i = 0; i < n; i++)
        if (!strcmp(m[i].key, key)) return m[i].val;
    return NULL;
}

const char *armor_actor(const char *piece) {
    static MapLine *m;
    static int n = -1;
    if (n < 0) n = map_load("armor.txt", &m), LOG("[online] %d armour pieces for the other players\n", n);
    return map_find(m, n, piece);
}

// an armour actor's bone map for unit u: Link bone index per bone (255 none); its length in *len
#define ARMOR_BONES 160
const uint8_t *armor_bones(const char *actor, int u, int *len) {
    static MapLine *m;
    static int n = -1;
    static struct { char key[48]; int units; uint8_t map[4][ARMOR_BONES]; int len[4]; } cache[16];
    static int next;
    if (n < 0) n = map_load("bones.txt", &m);
    for (int k = 0; k < 16; k++)
        if (!strcmp(cache[k].key, actor) && cache[k].units) {
            int uu = u < cache[k].units ? u : cache[k].units - 1;
            *len = cache[k].len[uu];
            return cache[k].map[uu];
        }
    const char *v = map_find(m, n, actor);
    if (!v) return NULL;
    int slot = next++ % 16;
    memset(&cache[slot], 0, sizeof cache[slot]);
    snprintf(cache[slot].key, sizeof cache[slot].key, "%s", actor);
    int unit = 0, b = 0;
    for (const char *c = v; *c && unit < 4; c++) {
        if (*c == '|') { cache[slot].len[unit++] = b, b = 0; continue; }
        if (*c == ',') continue;
        int x = atoi(c);
        while (c[1] >= '0' && c[1] <= '9') c++;
        if (b < ARMOR_BONES) cache[slot].map[unit][b++] = (uint8_t)(x > 255 ? 255 : x);
    }
    if (unit < 4) cache[slot].len[unit++] = b;
    cache[slot].units = unit;
    int uu = u < unit ? u : unit - 1;
    *len = cache[slot].len[uu];
    return cache[slot].map[uu];
}

uint64_t create(Cpu *c, const char *name) {
    uint64_t creator = *(uint64_t *)(uintptr_t)P_CREATOR, owner = *(uint64_t *)(uintptr_t)P_HEAP_OWNER;
    creator = creator ? *(uint64_t *)(uintptr_t)creator : 0;
    owner = owner ? *(uint64_t *)(uintptr_t)owner : 0;
    if (!creator || !owner) return 0;
    uint64_t heap = *(uint64_t *)(uintptr_t)(owner + 0x28);
    return call(c, FN_CREATE, creator, (uint64_t)(uintptr_t)name, heap, 0, 0, 0);
}

bool request(Cpu *c, const char *name, Handle *handle) { return request_p(c, name, handle, 0); }

// params: an InstParamPack (the values it is made with: an enemy's EquipItem1, ...), 0 none
bool request_p(Cpu *c, const char *name, Handle *handle, uint64_t params) {
    uint64_t creator = *(uint64_t *)(uintptr_t)P_CREATOR, owner = *(uint64_t *)(uintptr_t)P_HEAP_OWNER;
    creator = creator ? *(uint64_t *)(uintptr_t)creator : 0;
    owner = owner ? *(uint64_t *)(uintptr_t)owner : 0;
    if (!creator || !owner) return false;
    uint64_t heap = *(uint64_t *)(uintptr_t)(owner + 0x28);
    memset(handle, 0, 16);
    return call_f(c, FN_REQUEST, creator, (uint64_t)(uintptr_t)name, heap, (uint64_t)(uintptr_t)handle, params, 0, 1, 0) & 1;
}

void inst_params_s32(InstParams *p, const char *key, int32_t value) {  // (type 0, an s32)
    if (!p->vt) p->vt = *(uint64_t *)(uintptr_t)0x7001d664b0ull + 0x10;
    if (p->used + 13 > (int)sizeof p->data) return;
    uint64_t k = (uint64_t)(uintptr_t)key;
    memcpy(p->data + p->used, &k, 8);
    p->data[p->used + 8] = 0;
    memcpy(p->data + p->used + 9, &value, 4);
    p->used += 13, p->n++;
}

void inst_params_string(InstParams *p, const char *key, const char *value) {
    if (!p->vt) p->vt = *(uint64_t *)(uintptr_t)0x7001d664b0ull + 0x10;
    int len = (int)strlen(value) + 1;
    if (p->used + 9 + len > (int)sizeof p->data) return;
    uint64_t k = (uint64_t)(uintptr_t)key;
    memcpy(p->data + p->used, &k, 8);
    p->data[p->used + 8] = 5;
    memcpy(p->data + p->used + 9, value, len);
    p->used += 9 + len, p->n++;
}

float test_warp_hour(bool *held);   // runtime/services/test_warp.c
void test_warp_set_hour(float hour);
uint64_t test_warp_gdt_mgr(void);   // runtime/services/test_warp.c
float g_dist;  // (how far the player being done is)
OaPvp g_pvp;  // (his last blow on another player: pvp_update)
InstParams *g_piece_params;  // (the params the next piece asked for is made with: an armour piece's dye; 0 none)

// p should be `want` ("" none): a piece that changed goes, and the new one is asked for; true when it was made just now
bool piece_update(Cpu *c, int i, Piece *p, const char *want, const char *who) {
    if ((p->actor || p->pending || p->name[0]) && strcmp(p->name, want)) pool_piece(c, i, p);
    if (!p->actor && !p->pending && want[0] && from_pool(c, i, p, want)) {
        LOG("[online] %s: %s again (actor %llx)\n", who, want, (unsigned long long)p->actor);
        return true;
    }
    snprintf(p->name, sizeof p->name, "%s", want);
    if (p->actor && !alive(p->actor, p->w)) {
        LOG("[online] %s: the game removed his %s, made again (%.0f m away)\n", who, want, g_dist);
        p->actor = 0, p->fails = 0;
    }
    if (!want[0] || p->actor) return false;
    if (p->pending) {
        int *unit = (int *)(uintptr_t)p->handle->unit;
        if (unit && *unit == 2) {
            p->pending = false;
            p->actor = call(c, FN_HANDLE_TAKE, (uint64_t)(uintptr_t)p->handle, 0, 0, 0, 0, 0);
            if (p->actor) p->w = watch(p->actor);
            if (p->actor) LOG("[online] %s: %s (actor %llx)\n", who, want, (unsigned long long)p->actor);
            return p->actor != 0;
        }
        if (unit && (*unit == 3 || *unit == 5)) {
            call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)p->handle, 0, 0, 0, 0, 0);
            p->pending = false, p->fails++, p->wait = 120;  // (again in two seconds, a few times)
            LOG("[online] %s: %s couldn't be made\n", who, want);
        }
        return false;
    }
    if (p->wait > 0) { p->wait--; return false; }
    if (!strncmp(want, "Armor_Default", 13)) {  // (Link's default pieces, resident: asked for, they are never drawn;
        uint64_t a = create(c, want);           // made at once, they are)
        if (a) {
            p->actor = a, p->w = watch(a), p->fails = 0;
            LOG("[online] %s: %s (actor %llx)\n", who, want, (unsigned long long)a);
            return true;
        }
    }
    if (p->fails < 5) {
        p->handle = new_handle();
        if (request_p(c, want, p->handle, (uint64_t)(uintptr_t)g_piece_params)) p->pending = true;
        else if (p->fails++, p->wait = 60, p->fails == 1) LOG("[online] can't ask for a %s\n", want);
    }
    return false;
}

bool running(uint64_t actor) { return actor && *(uint8_t *)(uintptr_t)(actor + 0x64) == 1; }  // (calc)

uint64_t skel_unit(Cpu *c, uint64_t actor, int *bones) {
    uint64_t model = *(uint64_t *)(uintptr_t)(actor + 0x4e0);
    if (!VALID(model)) return 0;
    uint64_t name[6] = {*(uint64_t *)(uintptr_t)P_SAFE_STRING_VT + 0x10, (uint64_t)(uintptr_t)"Skl_Root"};
    uint32_t r = (uint32_t)call(c, FN_FIND_BONE, model, (uint64_t)(uintptr_t)name, 0, 0, 0, 0);
    int16_t unit_i = (int16_t)(r & 0xffff);
    if (unit_i < 0) return 0;
    uint64_t units = *(uint64_t *)(uintptr_t)(model + 0x40);
    uint64_t unit = VALID(units) ? *(uint64_t *)(uintptr_t)(units + 8 * unit_i) : 0;
    unit = VALID(unit) ? *(uint64_t *)(uintptr_t)unit : 0;
    uint64_t sk = VALID(unit) ? *(uint64_t *)(uintptr_t)(unit + 0x118) : 0;
    if (!VALID(sk)) return 0;
    *bones = *(uint16_t *)(uintptr_t)(sk + 0x50);
    return unit;
}

// The player's equipment, as the game keeps it: actor links (a BaseProcLink: the unit, the id; Watch) to the actors
// of what he wears and carries. His armour's manager (player +0x2500; the armour code's, 0x70011750c8: its +0x128 is
// the player) links its pieces at +0x10, 0x10 each: head, upper, lower; the player links his weapon at +0xb98, his
// shield at +0xbb0, his bow at +0xbc8, 0x18 each; a weapon links its sheath, a bow its quiver, at +0x958 (their
// PodName, QuiverName)
uint64_t linked(uint64_t at) {
    if (!VALID(at)) return 0;
    uint64_t u = *(uint64_t *)(uintptr_t)at;
    uint32_t id = *(uint32_t *)(uintptr_t)(at + 8);
    if (!VALID(u) || *(uint32_t *)(uintptr_t)(u + 0x40) != id) return 0;
    uint64_t a = *(uint64_t *)(uintptr_t)(u + 0x48);
    return VALID(a) && *(uint8_t *)(uintptr_t)(a + 0x64) != 3 ? a : 0;
}

// his paraglider (PlayerStole2, or its _Vagrant look): an actor the player links (the link's
// place in the player found once, by looking through him for one; again every second while none is)
static uint64_t glider_of(uint64_t player) {
    static int off = -1, miss;
    char n[48];
    if (off >= 0) {
        uint64_t a = linked(player + off);
        actor_name_of(a, n);
        if (strncmp(n, "PlayerStole2", 12)) a = 0;
        if (a || miss++ % 30) return a;
    } else if (miss++ % 30) return 0;
    for (int o = 0x100; o < 0x4000; o += 8) {
        uint64_t u = *(uint64_t *)(uintptr_t)(player + o);
        if (!VALID(u) || (u & 7)) continue;
        uint64_t a = linked(player + o);
        actor_name_of(a, n);
        if (strncmp(n, "PlayerStole2", 12)) continue;
        if (off != o) LOG("[online] his paraglider: player +0x%x\n", o);
        off = o;
        return a;
    }
    return 0;
}

void equipment(uint64_t player, uint64_t armor[ARMOR], uint64_t gear[ONLINE_GEAR]) {
    uint64_t mgr = *(uint64_t *)(uintptr_t)(player + 0x2500);
    bool mine = VALID(mgr) && *(uint64_t *)(uintptr_t)(mgr + 0x128) == player;
    for (int k = 0; k < ARMOR; k++) armor[k] = mine ? linked(mgr + 0x10 + 0x10 * k) : 0;
    for (int k = 0; k < 3; k++) gear[k] = linked(player + 0xb98 + 0x18 * k);
    gear[3] = gear[0] ? linked(gear[0] + 0x958) : 0;
    gear[4] = gear[2] ? linked(gear[2] + 0x958) : 0;
    gear[5] = glider_of(player);
}

// an actor's name, when it is one ("" else)
void actor_name_of(uint64_t a, char out[48]) {
    memset(out, 0, 48);
    uint64_t n = a ? *(uint64_t *)(uintptr_t)(a + 0x10) : 0;
    if (!VALID(n)) return;
    const char *s = (const char *)(uintptr_t)n;
    int l = 0;
    while (l < 47 && s[l] > ' ' && s[l] < 127) l++;
    if (!s[l]) memcpy(out, s, l), out[l] = 0;
}

// a model's first unit and its bone count (a weapon's: one unit; a bow's bones bend it and pull its string)
uint64_t first_unit(uint64_t actor, int *bones) {
    uint64_t model = VALID(actor) ? *(uint64_t *)(uintptr_t)(actor + 0x4e0) : 0;
    uint64_t units = VALID(model) ? *(uint64_t *)(uintptr_t)(model + 0x40) : 0;
    uint64_t unit = VALID(units) ? *(uint64_t *)(uintptr_t)units : 0;
    unit = VALID(unit) ? *(uint64_t *)(uintptr_t)unit : 0;
    uint64_t sk = VALID(unit) ? *(uint64_t *)(uintptr_t)(unit + 0x118) : 0;
    if (!VALID(sk)) return 0;
    *bones = *(uint16_t *)(uintptr_t)(sk + 0x50);
    return unit;
}

// a bone's matrix in the world (Matrix34f, row-major), from an actor's model
bool bone_mtx(Cpu *c, uint64_t actor, const char *bone, float out[12]) {
    uint64_t model = *(uint64_t *)(uintptr_t)(actor + 0x4e0);
    if (!VALID(model)) return false;
    uint64_t name[6] = {*(uint64_t *)(uintptr_t)P_SAFE_STRING_VT + 0x10, (uint64_t)(uintptr_t)bone};
    uint32_t r = (uint32_t)call(c, FN_FIND_BONE, model, (uint64_t)(uintptr_t)name, 0, 0, 0, 0);
    int16_t unit_i = (int16_t)(r & 0xffff);
    if (unit_i < 0) return false;
    uint64_t units = *(uint64_t *)(uintptr_t)(model + 0x40);
    uint64_t unit = VALID(units) ? *(uint64_t *)(uintptr_t)(units + 8 * unit_i) : 0;
    unit = VALID(unit) ? *(uint64_t *)(uintptr_t)unit : 0;
    if (!VALID(unit)) return false;
    float m[12];
    call(c, *(uint64_t *)(uintptr_t)(*(uint64_t *)(uintptr_t)unit + 0x70), unit, (uint64_t)(uintptr_t)m, r >> 16, 0, 0, 0);
    for (int k = 0; k < 12; k++)
        if (!isfinite(m[k])) return false;
    memcpy(out, m, sizeof m);
    return true;
}

// a * b (3x4: the 3x3 rotation and the translation)
void mtx_mul(const float a[12], const float b[12], float out[12]) {
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 3; k++)
            out[r * 4 + k] = a[r * 4] * b[k] + a[r * 4 + 1] * b[4 + k] + a[r * 4 + 2] * b[8 + k];
        out[r * 4 + 3] = a[r * 4] * b[3] + a[r * 4 + 1] * b[7] + a[r * 4 + 2] * b[11] + a[r * 4 + 3];
    }
}

// inverse of a rigid 3x4 (rotation, translation)
void mtx_inv(const float a[12], float out[12]) {
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) out[r * 4 + k] = a[k * 4 + r];
    for (int r = 0; r < 3; r++)
        out[r * 4 + 3] = -(out[r * 4] * a[3] + out[r * 4 + 1] * a[7] + out[r * 4 + 2] * a[11]);
}

uint64_t g_actors[MAX_ACTORS];
void loot_mine_add(uint64_t a, const char *n, int me);  // (loot: what we drop or throw, for everyone)
void loot_mine_thrown(uint64_t a, const char *n);  // (and one thrown, on the list as it leaves his hand)

// a rotation matrix (row-major 3x4's 3x3) as a quaternion (x y z w), and back
void mtx_to_quat(const float m[12], float q[4]) {
    float tr = m[0] + m[5] + m[10];
    if (tr > 0) {
        float s = sqrtf(tr + 1) * 2;
        q[3] = 0.25f * s, q[0] = (m[9] - m[6]) / s, q[1] = (m[2] - m[8]) / s, q[2] = (m[4] - m[1]) / s;
    } else if (m[0] > m[5] && m[0] > m[10]) {
        float s = sqrtf(1 + m[0] - m[5] - m[10]) * 2;
        q[3] = (m[9] - m[6]) / s, q[0] = 0.25f * s, q[1] = (m[1] + m[4]) / s, q[2] = (m[2] + m[8]) / s;
    } else if (m[5] > m[10]) {
        float s = sqrtf(1 + m[5] - m[0] - m[10]) * 2;
        q[3] = (m[2] - m[8]) / s, q[0] = (m[1] + m[4]) / s, q[1] = 0.25f * s, q[2] = (m[6] + m[9]) / s;
    } else {
        float s = sqrtf(1 + m[10] - m[0] - m[5]) * 2;
        q[3] = (m[4] - m[1]) / s, q[0] = (m[2] + m[8]) / s, q[1] = (m[6] + m[9]) / s, q[2] = 0.25f * s;
    }
}

void quat_to_mtx(const float q[4], float m[12]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    m[0] = 1 - 2 * (y * y + z * z), m[1] = 2 * (x * y - z * w), m[2] = 2 * (x * z + y * w);
    m[4] = 2 * (x * y + z * w), m[5] = 1 - 2 * (x * x + z * z), m[6] = 2 * (y * z - x * w);
    m[8] = 2 * (x * z - y * w), m[9] = 2 * (y * z + x * w), m[10] = 1 - 2 * (x * x + y * y);
}

// Every actor the game has, from one actor's proc unit (actor +0x68): the units are in one list (0x58 each: +0 their
// vtable, +0x10 / +0x18 the previous / next one's +0x10; +0x40 the actor's id, +0x48 the actor), its head no unit
int all_actors(uint64_t from, uint64_t *out, int max) {
    if (!VALID(from)) return 0;
    uint64_t vt = *(uint64_t *)(uintptr_t)from;
    int n = 0;
    for (int dir = 0; dir < 2; dir++) {
        uint64_t u = from;
        for (int k = 0; k < 16384 && n < max; k++) {
            if (dir == 1 || k > 0) {
                uint64_t a = *(uint64_t *)(uintptr_t)(u + 0x48);
                if (VALID(a) && *(uint32_t *)(uintptr_t)(a + 0x60) == *(uint32_t *)(uintptr_t)(u + 0x40)) out[n++] = a;
            }
            uint64_t nx = *(uint64_t *)(uintptr_t)(u + (dir ? 0x10 : 0x18));
            if (!VALID(nx) || nx - 0x10 == from) break;
            u = nx - 0x10;
            if (*(uint64_t *)(uintptr_t)u != vt) break;
        }
        if (dir == 0 && n) n--;  // (the first one: again going back)
    }
    return n;
}
OaTheirs g_theirs[THEIRS];
