// Tests for the network validation layer (runtime/online_validate.h) without the game or a network. Each structure
// the layer checks (STATE, EQUIP, an enemy, a loot entry, PICK, HIT, TIME, SKEL) is described here a second time, on
// its own: a table of its fields (where each one is, by offsetof, and what it may hold), which covers every byte of
// the structure once. STATE's table also says which fields are its core and which make up each entry of its lists,
// and what an entry is cleared to (T222: a bad core drops the STATE, a bad entry is cleared alone). The layer's verdict
// - and, for STATE, what it leaves and how many entries it cleared - is compared with the table's:
//   - a valid one, built by hand, passes untouched;
//   - each field, set to each value its rule refuses (one at a time, the rest valid), fails, or in STATE outside its
//     core, passes with its entry cleared; its limits pass;
//   - 100 000 structures from a fixed seed (random bytes, or a valid one with a few bytes or fields changed) get the
//     same verdict and the same bytes from both: whatever the layer lets through holds no number that isn't finite or
//     is out of its bounds, no string without its end, no count or enumeration out of range.
//   make test-online-validate
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/online_validate.h"

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

// ------------------------------------------------------------------ a fixed seed (xorshift64*)
static uint64_t g_seed = 0x54323133u;  // ("T213")
static uint64_t rnd(void) {
    g_seed ^= g_seed >> 12, g_seed ^= g_seed << 25, g_seed ^= g_seed >> 27;
    return g_seed * 0x2545F4914F6CDD1Dull;
}
static void rnd_bytes(unsigned char *b, size_t n) {
    for (size_t k = 0; k < n; k++) b[k] = (unsigned char)(rnd() >> 56);
}

// ------------------------------------------------------------------ the structures, described again
// T_F: floats, finite, within +-100000; T_HOUR: 0..24
// T_STR: a string that ends in its field; T_NAME: an actor's name, [A-Za-z0-9_], ends in its field ("" too)
// T_U8: each byte <= a, or == b (b < 0: no other value); T_MASK: each byte has no bit outside a
// T_I8 / T_I16 / T_I32: each value in a..b; T_ANY: anything (ids, hashes, sequence numbers, properties)
enum { T_ANY, T_F, T_HOUR, T_STR, T_NAME, T_U8, T_MASK, T_I8, T_I16, T_I32 };
// STATE's entries (Field.entry; 0: its core, or a field that takes anything): what one is cleared to, field by field
// (Field.empty: Z_ZERO all zeros, Z_FF all ones - -1 or 255 -, Z_FM1 the float -1), and how (Entry.how: E_PLAIN
// alone; E_OBJ also its bit of objs_trunk; E_COUNT a counted list's count, E_ITEM one of its entries)
enum { Z_ZERO, Z_FF, Z_FM1 };
enum { E_PLAIN, E_OBJ, E_COUNT, E_ITEM };
typedef struct { size_t off, size; int type; int64_t a, b; const char *what; int entry, empty; } Field;
typedef struct { int how, list, idx; } Entry;
// STATE's counted lists: where the count is, where its entries start, how big each one is, how many fit
typedef struct { size_t count_off, off, size; int cap; } List;
#define MAXF 200
#define MAXE 80
typedef struct {
    const char *what;
    size_t size;
    Field f[MAXF];
    int n;
    Entry e[MAXE];                         // (e[0] unused: the core)
    int ne, cur;                           // (cur: the entry the next fields go in)
    bool (*valid)(const unsigned char *, unsigned char *, int *);  // the layer: its verdict, what it leaves, how many
                                                                   // entries it cleared
    void (*term)(unsigned char *);         // the layer's one repair (NULL: no strings)
    unsigned char good[sizeof(WireState)];
    int passed, refused, cleared;          // (the random structures: how many of each; entries cleared in those passed)
} Kind;

static void add(Kind *k, size_t off, size_t size, int type, int64_t a, int64_t b, const char *what) {
    if (k->n == MAXF) {
        failures++, printf("FAIL %s: more than %d fields\n", k->what, MAXF);
        return;
    }
    k->f[k->n++] = (Field){off, size, type, a, b, what, k->cur, Z_ZERO};
}
static void empty_as(Kind *k, int empty) { k->f[k->n - 1].empty = empty; }  // (the field just added)
static void core(Kind *k) { k->cur = 0; }
static void entry(Kind *k, int how, int list, int idx) {  // (the fields added from now on: a new entry's)
    if (k->ne + 1 >= MAXE) {
        failures++, printf("FAIL %s: more than %d entries\n", k->what, MAXE);
        return;
    }
    k->cur = ++k->ne;
    k->e[k->cur] = (Entry){how, list, idx};
}

static float getf(const unsigned char *b) {
    float f;
    memcpy(&f, b, 4);
    return f;
}
static void setf(unsigned char *b, float f) { memcpy(b, &f, 4); }
static bool finite_bits(const unsigned char *b) {  // (its exponent not all ones: not an infinity, not a NaN)
    uint32_t u;
    memcpy(&u, b, 4);
    return (u & 0x7f800000u) != 0x7f800000u;
}

static bool field_ok(const unsigned char *base, const Field *f) {
    static const char name_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
    const unsigned char *b = base + f->off;
    switch (f->type) {
    case T_ANY:
        return true;
    case T_F:
        for (size_t k = 0; k + 4 <= f->size; k += 4)
            if (!finite_bits(b + k) || !(getf(b + k) >= -100000.0f && getf(b + k) <= 100000.0f)) return false;
        return true;
    case T_HOUR:
        return finite_bits(b) && getf(b) >= 0 && getf(b) <= 24.0f;
    case T_STR:
        return memchr(b, 0, f->size) != NULL;
    case T_NAME:
        for (size_t k = 0; k < f->size; k++) {
            if (!b[k]) return true;
            if (!memchr(name_chars, b[k], sizeof name_chars - 1)) return false;
        }
        return false;
    case T_U8:
        for (size_t k = 0; k < f->size; k++)
            if (!(b[k] <= f->a || (f->b >= 0 && b[k] == f->b))) return false;
        return true;
    case T_MASK:
        for (size_t k = 0; k < f->size; k++)
            if (b[k] & ~(unsigned)f->a & 0xffu) return false;
        return true;
    case T_I8:
        for (size_t k = 0; k < f->size; k++)
            if ((int8_t)b[k] < f->a || (int8_t)b[k] > f->b) return false;
        return true;
    case T_I16:
        for (size_t k = 0; k + 2 <= f->size; k += 2) {
            int16_t v;
            memcpy(&v, b + k, 2);
            if (v < f->a || v > f->b) return false;
        }
        return true;
    case T_I32: {
        int32_t v;
        memcpy(&v, b, 4);
        return v >= f->a && v <= f->b;
    }
    }
    return false;
}

static bool table_ok(const Kind *k, const unsigned char *b) {
    for (int i = 0; i < k->n; i++)
        if (!field_ok(b, &k->f[i])) return false;
    return true;
}

// ------------------------------------------------------------------ what the table expects the layer to leave
static const List g_list[2] = {
    {offsetof(WireState, shots.n), offsetof(WireState, shots.shot), sizeof(((WireState *)0)->shots.shot[0]), ONLINE_SHOTS},
    {offsetof(WireState, ice_n), offsetof(WireState, ice), sizeof(((WireState *)0)->ice[0]), 3},
};

static bool entry_ok(const Kind *k, int e, const unsigned char *b) {
    for (int i = 0; i < k->n; i++)
        if (k->f[i].entry == e && !field_ok(b, &k->f[i])) return false;
    return true;
}

static void entry_clear(const Kind *k, int e, unsigned char *out) {
    for (int i = 0; i < k->n; i++) {
        const Field *f = &k->f[i];
        if (f->entry != e) continue;
        if (f->empty == Z_FF) memset(out + f->off, 0xff, f->size);
        else if (f->empty == Z_FM1) setf(out + f->off, -1.0f);
        else memset(out + f->off, 0, f->size);
    }
    if (k->e[e].how == E_OBJ) out[offsetof(WireState, objs_trunk)] &= (unsigned char)~(1u << k->e[e].idx);
}

// a counted list: a count past its room clears it all; of its first count entries, the ones that fail go and the
// others keep their order from the start; past the count, one that fails is cleared
static void list_expect(const Kind *k, int list, const unsigned char *in, unsigned char *out, int *cleared) {
    const List *l = &g_list[list];
    int count_e = 0, item_e[ONLINE_SHOTS] = {0};
    for (int e = 1; e <= k->ne; e++)
        if (k->e[e].list == list && k->e[e].how == E_COUNT) count_e = e;
        else if (k->e[e].list == list && k->e[e].how == E_ITEM) item_e[k->e[e].idx] = e;
    if (!entry_ok(k, count_e, in)) {
        out[l->count_off] = 0;
        memset(out + l->off, 0, l->size * l->cap);
        ++*cleared;
        return;
    }
    int count = in[l->count_off], kept = 0;
    for (int q = 0; q < l->cap; q++) {
        bool ok = entry_ok(k, item_e[q], in);
        if (!ok) ++*cleared;
        if (q < count && ok) memcpy(out + l->off + l->size * kept++, in + l->off + l->size * q, l->size);
        else if (q >= count && !ok) memset(out + l->off + l->size * q, 0, l->size);
    }
    for (int q = kept; q < count; q++) memset(out + l->off + l->size * q, 0, l->size);
    out[l->count_off] = (unsigned char)kept;
}

// the table's verdict: a field of the core that fails, false; else each entry that fails cleared (out), and counted
static bool expect_of(const Kind *k, const unsigned char *in, unsigned char *out, int *cleared) {
    memcpy(out, in, k->size);
    *cleared = 0;
    for (int i = 0; i < k->n; i++)
        if (!k->f[i].entry && !field_ok(in, &k->f[i])) return false;
    for (int e = 1; e <= k->ne; e++)
        if ((k->e[e].how == E_PLAIN || k->e[e].how == E_OBJ) && !entry_ok(k, e, in)) entry_clear(k, e, out), ++*cleared;
    for (int l = 0; k->ne && l < 2; l++) list_expect(k, l, in, out, cleared);
    return true;
}

// STATE: every field of WireState (runtime/online_wire.h), in its order
#define WO(m) offsetof(WireState, m)
#define WS(m) sizeof(((WireState *)0)->m)
#define WE(arr, mem, i) (WO(arr[0].mem) + (size_t)(i) * WS(arr[0]))  // (arr[i].mem)
static void describe_state(Kind *k) {
    // (the core: where he is, flags, anim, as, the session's rules; and, in no entry, the fields that take anything)
    add(k, WO(pos), WS(pos), T_F, 0, 0, "pos");
    add(k, WO(yaw), WS(yaw), T_F, 0, 0, "yaw");
    add(k, WO(flags), 1, T_MASK, ONLINE_F_PAUSED | ONLINE_F_LOADING, 0, "flags");
    add(k, WO(ping_ms), WS(ping_ms), T_ANY, 0, 0, "ping_ms");
    add(k, WO(anim), 1, T_U8, ONLINE_A_COUNT - 1, -1, "anim");
    for (int i = 0; i < ONLINE_AS_CHANNELS; i++) add(k, WO(as[0]) + i * WS(as[0]), WS(as[0]), T_STR, 0, 0, "as[]");
    add(k, WO(collide), 1, T_U8, 1, -1, "collide");
    add(k, WO(revive), 1, T_U8, 1, -1, "revive");
    add(k, WO(nopause), 1, T_U8, 1, -1, "nopause");
    add(k, WO(fx_props), WS(fx_props), T_ANY, 0, 0, "fx_props");
    add(k, WO(pvp_seq), WS(pvp_seq), T_ANY, 0, 0, "pvp_seq");
    add(k, WO(ground_props), WS(ground_props), T_ANY, 0, 0, "ground_props");
    add(k, WO(aggro_id), WS(aggro_id), T_ANY, 0, 0, "aggro_id");
    add(k, WO(aggro_seq), WS(aggro_seq), T_ANY, 0, 0, "aggro_seq");
    add(k, WO(broken), WS(broken), T_ANY, 0, 0, "broken");
    add(k, WO(trunks_gone), WS(trunks_gone), T_ANY, 0, 0, "trunks_gone");
    add(k, WO(objs_trunk), 1, T_ANY, 0, 0, "objs_trunk");
    // (the entries, each with what it is cleared to)
    for (int i = 0; i < ONLINE_GEAR; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WO(gear_bone) + i, 1, T_U8, 8, ONLINE_GEAR_WORLD, "gear_bone[]");
        add(k, WO(gear_rel[0]) + i * WS(gear_rel[0]), WS(gear_rel[0]), T_F, 0, 0, "gear_rel[]");
    }
    entry(k, E_COUNT, 0, 0);
    add(k, WO(shots.n), 1, T_U8, ONLINE_SHOTS, -1, "shots.n");
    for (int i = 0; i < ONLINE_SHOTS; i++) {
        entry(k, E_ITEM, 0, i);
        add(k, WE(shots.shot, id, i), WS(shots.shot[0].id), T_ANY, 0, 0, "shots.shot[].id");
        add(k, WE(shots.shot, kind, i), 1, T_U8, ONLINE_K_COUNT - 1, -1, "shots.shot[].kind");
        add(k, WE(shots.shot, m, i), WS(shots.shot[0].m), T_F, 0, 0, "shots.shot[].m");
    }
    for (int i = 0; i < ONLINE_BOOMS; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WE(shots.boom, seq, i), WS(shots.boom[0].seq), T_ANY, 0, 0, "shots.boom[].seq");
        add(k, WE(shots.boom, kind, i), 1, T_U8, ONLINE_K_COUNT - 1, -1, "shots.boom[].kind");
        add(k, WE(shots.boom, pos, i), WS(shots.boom[0].pos), T_F, 0, 0, "shots.boom[].pos");
    }
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(effects), 1, T_MASK, ONLINE_E_KAGO | ONLINE_E_KAGO_COUNT | ONLINE_E_GALE | ONLINE_E_DEMO, 0, "effects");
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(grab.name), WS(grab.name), T_NAME, 0, 0, "grab.name");
    add(k, WO(grab.from), WS(grab.from), T_F, 0, 0, "grab.from");
    add(k, WO(grab.m), WS(grab.m), T_F, 0, 0, "grab.m");
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(stop.name), WS(stop.name), T_NAME, 0, 0, "stop.name");
    add(k, WO(stop.from), WS(stop.from), T_F, 0, 0, "stop.from");
    add(k, WO(stop.m), WS(stop.m), T_F, 0, 0, "stop.m");
    entry(k, E_COUNT, 1, 0);
    add(k, WO(ice_n), 1, T_U8, 3, -1, "ice_n");
    for (int i = 0; i < 3; i++) {
        entry(k, E_ITEM, 1, i);
        add(k, WO(ice[0]) + i * WS(ice[0]), WS(ice[0]), T_F, 0, 0, "ice[]");
    }
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(map), WS(map), T_STR, 0, 0, "map");
    for (int i = 0; i < ONLINE_BOSSES; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WE(boss, hash, i), 4, T_ANY, 0, 0, "boss[].hash");
        add(k, WE(boss, name, i), WS(boss[0].name), T_NAME, 0, 0, "boss[].name");
        add(k, WE(boss, life, i), 4, T_I32, 0, INT32_MAX, "boss[].life");
    }
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(fx), WS(fx), T_STR, 0, 0, "fx");
    for (int i = 0; i < 3; i++) {  // (a slot of his trail: its animation, -1 none or up to 4095 - T222's bound -, its frame)
        entry(k, E_PLAIN, -1, i);
        add(k, WO(fx_anim) + 2 * i, 2, T_I16, -1, 4095, "fx_anim[]"), empty_as(k, Z_FF);
        add(k, WO(fx_frame) + 4 * i, 4, T_F, 0, 0, "fx_frame[]");
    }
    for (int i = 0; i < ONLINE_FX_EVENTS; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WE(fxev, seq, i), WS(fxev[0].seq), T_ANY, 0, 0, "fxev[].seq");
        add(k, WE(fxev, who, i), 1, T_U8, 5, -1, "fxev[].who");
        add(k, WE(fxev, name, i), WS(fxev[0].name), T_STR, 0, 0, "fxev[].name");
    }
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(weather), 1, T_U8, 8, 255, "weather"), empty_as(k, Z_FF);
    for (int i = 0; i < 3; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WO(dye) + i, 1, T_I8, -1, INT8_MAX, "dye[]"), empty_as(k, Z_FF);
    }
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(pvp_target), 1, T_U8, ONLINE_MAX - 1, -1, "pvp_target");
    add(k, WO(pvp_damage), WS(pvp_damage), T_I16, 0, INT16_MAX, "pvp_damage");
    entry(k, E_PLAIN, -1, 0);
    add(k, WO(horse), WS(horse), T_NAME, 0, 0, "horse");
    add(k, WO(horse_m), WS(horse_m), T_F, 0, 0, "horse_m");
    add(k, WO(horse_as), WS(horse_as), T_STR, 0, 0, "horse_as");
    for (int i = 0; i < 2; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WO(tack[0]) + i * WS(tack[0]), WS(tack[0]), T_NAME, 0, 0, "tack[]");
        add(k, WO(tack_m[0]) + i * WS(tack_m[0]), WS(tack_m[0]), T_F, 0, 0, "tack_m[]");
    }
    entry(k, E_PLAIN, -1, 0);  // (any number: the window 0..4000 is the consumer's, T222)
    add(k, WO(bloody_moon), 4, T_F, 0, 0, "bloody_moon"), empty_as(k, Z_FM1);
    for (int i = 0; i < ONLINE_OBJS; i++) {
        entry(k, E_OBJ, -1, i);
        add(k, WE(objs, hash, i), 4, T_ANY, 0, 0, "objs[].hash");
        add(k, WE(objs, m, i), WS(objs[0].m), T_F, 0, 0, "objs[].m");
    }
    for (int i = 0; i < ONLINE_MOVERS; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WE(movers, hash, i), 4, T_ANY, 0, 0, "movers[].hash");
        add(k, WE(movers, m, i), WS(movers[0].m), T_F, 0, 0, "movers[].m");
        add(k, WE(movers, as, i), WS(movers[0].as), T_STR, 0, 0, "movers[].as");
    }
    for (int i = 0; i < ONLINE_HELD; i++) {
        entry(k, E_PLAIN, -1, i);
        add(k, WE(held, name, i), WS(held[0].name), T_NAME, 0, 0, "held[].name");
        add(k, WE(held, mode, i), 1, T_U8, ONLINE_HELD_WORLD, -1, "held[].mode");
        add(k, WE(held, m, i), WS(held[0].m), T_F, 0, 0, "held[].m");
    }
    core(k);
}

#define EO(m) offsetof(OnlineEnemy, m)
#define ES(m) sizeof(((OnlineEnemy *)0)->m)
static void describe_enemy(Kind *k) {
    add(k, EO(id), 4, T_ANY, 0, 0, "id");
    add(k, EO(name), ES(name), T_NAME, 0, 0, "name");
    add(k, EO(m), ES(m), T_F, 0, 0, "m");
    add(k, EO(as), ES(as), T_STR, 0, 0, "as");
    add(k, EO(weapon), ES(weapon), T_NAME, 0, 0, "weapon");
    add(k, EO(wm), ES(wm), T_F, 0, 0, "wm");
    add(k, EO(life), 4, T_I32, -1, INT32_MAX, "life");
    add(k, EO(shield), ES(shield), T_NAME, 0, 0, "shield");
    add(k, EO(sm), ES(sm), T_F, 0, 0, "sm");
    add(k, EO(wbone), 1, T_U8, 2, -1, "wbone");
    add(k, EO(sbone), 1, T_U8, 2, -1, "sbone");
    add(k, EO(owner), 1, T_U8, ONLINE_MAX - 1, 0xff, "owner");
    add(k, EO(hash), 4, T_ANY, 0, 0, "hash");
}

#define LO(m) offsetof(OnlineLoot, m)
#define LS(m) sizeof(((OnlineLoot *)0)->m)
static void describe_loot(Kind *k) {
    add(k, LO(gid), 4, T_ANY, 0, 0, "gid");
    add(k, LO(lid), 4, T_ANY, 0, 0, "lid");
    add(k, LO(origin), 1, T_U8, ONLINE_MAX - 1, -1, "origin");
    add(k, LO(name), LS(name), T_NAME, 0, 0, "name");
    add(k, LO(pos), LS(pos), T_F, 0, 0, "pos");
    add(k, LO(rot), LS(rot), T_F, 0, 0, "rot");
}

static void describe_equip(Kind *k) {
    for (int i = 0; i < 3; i++) add(k, offsetof(WireEquip, armor) + i * 48, 48, T_NAME, 0, 0, "armor[]");
    for (int i = 0; i < ONLINE_GEAR; i++) add(k, offsetof(WireEquip, gear) + i * 48, 48, T_NAME, 0, 0, "gear[]");
}

// PICK: a name (40) and where it was (3 floats); HIT as the layer is given it: the damage, the element (the id it
// doesn't look at); TIME: an hour
static void describe_pick(Kind *k) {
    add(k, 0, 40, T_NAME, 0, 0, "name");
    add(k, 40, 12, T_F, 0, 0, "pos");
}
static void describe_hit(Kind *k) {
    add(k, 0, 4, T_I32, 0, 1000000, "damage");
    add(k, 4, 1, T_U8, ONLINE_ELEM_COUNT - 1, -1, "element");
}
static void describe_time(Kind *k) { add(k, 0, 4, T_HOUR, 0, 0, "hour"); }

// ------------------------------------------------------------------ the layer, on bytes
static bool v_state(const unsigned char *b, unsigned char *out, int *cleared) {
    WireState w;
    memcpy(&w, b, sizeof w);
    bool v = online_validate_state(&w, cleared);
    memcpy(out, &w, sizeof w);
    return v;
}
static void t_state(unsigned char *b) {
    WireState w;
    memcpy(&w, b, sizeof w);
    online_terminate_state(&w);
    memcpy(b, &w, sizeof w);
}
static bool v_equip(const unsigned char *b, unsigned char *out, int *cleared) {
    WireEquip e;
    memcpy(&e, b, sizeof e);
    memcpy(out, b, sizeof e), *cleared = 0;
    return online_validate_equip(&e);
}
static void t_equip(unsigned char *b) {
    WireEquip e;
    memcpy(&e, b, sizeof e);
    online_terminate_equip(&e);
    memcpy(b, &e, sizeof e);
}
static bool v_enemy(const unsigned char *b, unsigned char *out, int *cleared) {
    OnlineEnemy e;
    memcpy(&e, b, sizeof e);
    memcpy(out, b, sizeof e), *cleared = 0;
    return online_validate_enemy(&e);
}
static void t_enemy(unsigned char *b) {
    OnlineEnemy e;
    memcpy(&e, b, sizeof e);
    online_terminate_enemy(&e);
    memcpy(b, &e, sizeof e);
}
static bool v_loot(const unsigned char *b, unsigned char *out, int *cleared) {
    OnlineLoot l;
    memcpy(&l, b, sizeof l);
    memcpy(out, b, sizeof l), *cleared = 0;
    return online_validate_loot(&l);
}
static void t_loot(unsigned char *b) {
    OnlineLoot l;
    memcpy(&l, b, sizeof l);
    online_terminate_loot(&l);
    memcpy(b, &l, sizeof l);
}
static bool v_pick(const unsigned char *b, unsigned char *out, int *cleared) {
    char name[40];
    float pos[3];
    memcpy(name, b, 40), memcpy(pos, b + 40, 12);
    memcpy(out, b, 52), *cleared = 0;
    return online_validate_pick(name, pos);
}
static void t_pick(unsigned char *b) { online_terminate_str((char *)b, 40); }
static bool v_hit(const unsigned char *b, unsigned char *out, int *cleared) {
    int32_t d;
    memcpy(&d, b, 4);
    memcpy(out, b, 5), *cleared = 0;
    return online_validate_hit(d, b[4]);
}
static bool v_time(const unsigned char *b, unsigned char *out, int *cleared) {
    memcpy(out, b, 4), *cleared = 0;
    return online_validate_time(getf(b));
}

// ------------------------------------------------------------------ valid ones, as a game would send them
static void put_str(unsigned char *dst, size_t cap, const char *s) {
    memset(dst, 0, cap);
    memcpy(dst, s, strlen(s) < cap - 1 ? strlen(s) : cap - 1);
}
static void put_mtx(void *dst, float x, float y, float z) {
    const float m[12] = {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z};
    memcpy(dst, m, sizeof m);
}

static void good_state(unsigned char *out) {
    WireState w;
    memset(&w, 0, sizeof w);
    w.pos[0] = 1200.5f, w.pos[1] = 160.25f, w.pos[2] = -2400.75f;
    w.yaw = 1.5f;
    w.flags = ONLINE_F_PAUSED;
    w.ping_ms = 40;
    w.anim = ONLINE_A_WALK;
    put_str((unsigned char *)w.as[0], ONLINE_AS_MAX, "Move");
    put_str((unsigned char *)w.as[1], ONLINE_AS_MAX, "Attack_Lsword_Combo1");
    const uint8_t bones[ONLINE_GEAR] = {1, 2, 5, 3, ONLINE_GEAR_WORLD, 0};
    memcpy(w.gear_bone, bones, sizeof bones);
    for (int k = 0; k < ONLINE_GEAR; k++) put_mtx(w.gear_rel[k], 0.1f * k, 0.2f, -0.3f);
    w.shots.n = 1;
    w.shots.shot[0].id = 3, w.shots.shot[0].kind = ONLINE_K_FIRE_ARROW;
    put_mtx(w.shots.shot[0].m, 1210, 161, -2390);
    w.shots.boom[0].seq = 2, w.shots.boom[0].kind = ONLINE_K_REMOTE_BOMB;
    setf((unsigned char *)w.shots.boom[0].pos, 1205), setf((unsigned char *)w.shots.boom[0].pos + 4, 160);
    w.effects = ONLINE_E_KAGO | (2 << ONLINE_E_KAGO_COUNT_SHIFT) | ONLINE_E_DEMO;
    put_str((unsigned char *)w.grab.name, ONLINE_GRAB_NAME, "FldObj_Box_A_01");
    put_mtx(w.grab.m, 1201, 165, -2401);
    put_str((unsigned char *)w.stop.name, ONLINE_GRAB_NAME, "TBox_Field_Iron");
    put_mtx(w.stop.m, 1199, 160, -2399);
    w.ice_n = 1;
    put_mtx(w.ice[0], 1190, 158, -2410);
    put_str((unsigned char *)w.map, sizeof w.map, "MainField/");
    w.boss[0].hash = 77, w.boss[0].life = 120;
    put_str((unsigned char *)w.boss[0].name, sizeof w.boss[0].name, "Enemy_GanonBeast");
    put_str((unsigned char *)w.fx, ONLINE_FX_MAX, "Blur_Default");
    w.fx_anim[0] = 12, w.fx_anim[1] = -1, w.fx_anim[2] = -1;
    w.fx_frame[0] = 3.5f;
    w.fxev[0].seq = 9, w.fxev[0].who = 4;
    put_str((unsigned char *)w.fxev[0].name, sizeof w.fxev[0].name, "Pl_Step_Grass");
    w.fxev[1].seq = 8, w.fxev[1].who = 2;
    put_str((unsigned char *)w.fxev[1].name, sizeof w.fxev[1].name, "Kago_Heal");
    w.fx_props[0] = 0xdeadbeef;
    w.weather = 3;
    w.dye[0] = -1, w.dye[1] = 5, w.dye[2] = 127;
    w.pvp_seq = 4, w.pvp_target = 2, w.pvp_damage = 30;
    put_str((unsigned char *)w.horse, sizeof w.horse, "GameRomHorse00");
    put_mtx(w.horse_m, 1203, 160, -2402);
    put_str((unsigned char *)w.horse_as, sizeof w.horse_as, "Run");
    put_str((unsigned char *)w.tack[0], sizeof w.tack[0], "GameRomHorseSaddle_00");
    put_str((unsigned char *)w.tack[1], sizeof w.tack[1], "GameRomHorseReins_00");
    put_mtx(w.tack_m[0], 1203, 161, -2402), put_mtx(w.tack_m[1], 1203, 161.5f, -2401);
    w.ground_props[3] = 0xffffffffu;
    w.collide = 1, w.revive = 1, w.nopause = 0;
    w.bloody_moon = 1234.5f;
    w.aggro_id = 99, w.aggro_seq = 7;
    w.objs[0].hash = 0x1234;
    put_mtx(w.objs[0].m, 1180, 150, -2380);
    w.broken[0] = 0x55, w.trunks_gone[0] = 0x66;
    w.movers[0].hash = 0x777;
    put_mtx(w.movers[0].m, 1100, 140, -2300);
    put_str((unsigned char *)w.movers[0].as, sizeof w.movers[0].as, "Open");
    w.objs_trunk = 0x81;
    put_str((unsigned char *)w.held[0].name, sizeof w.held[0].name, "Item_Fruit_A");
    w.held[0].mode = ONLINE_HELD_HANDS;
    setf((unsigned char *)w.held[0].m, 0.55f);
    put_str((unsigned char *)w.held[1].name, sizeof w.held[1].name, "Animal_Insect_A");
    w.held[1].mode = ONLINE_HELD_WORLD;
    put_mtx(w.held[1].m, 1201, 162, -2400);
    memcpy(out, &w, sizeof w);
}

static void good_enemy(unsigned char *out) {
    OnlineEnemy e;
    memset(&e, 0, sizeof e);
    e.id = 5;
    put_str((unsigned char *)e.name, sizeof e.name, "Enemy_Bokoblin_Junior");
    put_mtx(e.m, 1250, 160, -2450);
    put_str((unsigned char *)e.as, sizeof e.as, "Attack");
    put_str((unsigned char *)e.weapon, sizeof e.weapon, "Weapon_Lsword_001");
    put_mtx(e.wm, 0.05f, 0, 0.1f);
    e.life = 13;
    put_mtx(e.sm, 1250, 160, -2450);
    e.wbone = 1, e.sbone = 0, e.owner = 2, e.hash = 0xabcdef;
    memcpy(out, &e, sizeof e);
}

static void good_loot(unsigned char *out) {
    OnlineLoot l;
    memset(&l, 0, sizeof l);
    l.gid = 1, l.lid = 2, l.origin = 1;
    put_str((unsigned char *)l.name, sizeof l.name, "Item_Enemy_00");
    setf((unsigned char *)l.pos, 1251), setf((unsigned char *)l.pos + 4, 159.5f), setf((unsigned char *)l.pos + 8, -2449);
    const float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    memcpy(l.rot, rot, sizeof rot);
    memcpy(out, &l, sizeof l);
}

static void good_equip(unsigned char *out) {
    WireEquip e;
    memset(&e, 0, sizeof e);
    put_str((unsigned char *)e.armor[0], 48, "Armor_001_Head");
    put_str((unsigned char *)e.armor[1], 48, "Armor_001_Upper");
    put_str((unsigned char *)e.armor[2], 48, "Armor_001_Lower");
    put_str((unsigned char *)e.gear[0], 48, "Weapon_Sword_001");
    put_str((unsigned char *)e.gear[1], 48, "Weapon_Shield_001");
    put_str((unsigned char *)e.gear[2], 48, "Weapon_Bow_001");
    memcpy(out, &e, sizeof e);
}

static void good_pick(unsigned char *out) {
    memset(out, 0, 52);
    put_str(out, 40, "Weapon_Sword_002");
    setf(out + 40, 1199), setf(out + 44, 160), setf(out + 48, -2399);
}
static void good_hit(unsigned char *out) {
    int32_t d = 50;
    memcpy(out, &d, 4);
    out[4] = ONLINE_ELEM_FIRE;
}
static void good_time(unsigned char *out) { setf(out, 13.25f); }

// ------------------------------------------------------------------ the kinds
enum { K_STATE, K_EQUIP, K_ENEMY, K_LOOT, K_PICK, K_HIT, K_TIME, K_COUNT };
static Kind g_kind[K_COUNT];

static void kinds_init(void) {
    static const struct {
        const char *what;
        size_t size;
        void (*describe)(Kind *);
        bool (*valid)(const unsigned char *, unsigned char *, int *);
        void (*term)(unsigned char *);
        void (*good)(unsigned char *);
    } d[K_COUNT] = {
        {"STATE", sizeof(WireState), describe_state, v_state, t_state, good_state},
        {"EQUIP", sizeof(WireEquip), describe_equip, v_equip, t_equip, good_equip},
        {"enemy", sizeof(OnlineEnemy), describe_enemy, v_enemy, t_enemy, good_enemy},
        {"loot", sizeof(OnlineLoot), describe_loot, v_loot, t_loot, good_loot},
        {"PICK", 52, describe_pick, v_pick, t_pick, good_pick},
        {"HIT", 5, describe_hit, v_hit, NULL, good_hit},
        {"TIME", 4, describe_time, v_time, NULL, good_time},
    };
    for (int i = 0; i < K_COUNT; i++) {
        Kind *k = &g_kind[i];
        memset(k, 0, sizeof *k);
        k->what = d[i].what, k->size = d[i].size, k->valid = d[i].valid, k->term = d[i].term;
        d[i].describe(k);
        d[i].good(k->good);
    }
}

// the table covers every byte of its structure, once (a field forgotten here, or one added to the structure, fails)
static void test_tables_cover(void) {
    static unsigned char seen[sizeof(WireState)];
    for (int i = 0; i < K_COUNT; i++) {
        const Kind *k = &g_kind[i];
        memset(seen, 0, sizeof seen);
        bool ok = true;
        for (int f = 0; f < k->n; f++)
            for (size_t b = k->f[f].off; b < k->f[f].off + k->f[f].size; b++)
                if (b >= k->size || seen[b]++) ok = false, printf("FAIL %s: byte %u (%s) out of it, or twice\n", k->what, (unsigned)b, k->f[f].what);
        for (size_t b = 0; b < k->size; b++)
            if (!seen[b]) {
                ok = false;
                printf("FAIL %s: byte %u is in no field\n", k->what, (unsigned)b);
                break;
            }
        checks++;
        if (!ok) failures++;
    }
}

// ------------------------------------------------------------------ one field at a time
static unsigned char g_buf[sizeof(WireState)], g_out[sizeof(WireState)], g_want[sizeof(WireState)];

// the layer and the table on b: the same verdict, and (passed) the same bytes left and entries cleared, nothing left
// that fails
static bool layer_vs_table(const Kind *k, const unsigned char *b, bool *verdict, int *cleared) {
    int lc, tc;
    bool v = k->valid(b, g_out, &lc), t = expect_of(k, b, g_want, &tc);
    *verdict = v, *cleared = lc;
    return v == t && (!v || (lc == tc && !memcmp(g_out, g_want, k->size) && table_ok(k, g_out)));
}

// g_buf, one field set: ok, a value its rule takes (passes, nothing cleared); else one it refuses (fails; in STATE
// outside the core, passes with that field's entry cleared)
static void expect(const Kind *k, const Field *f, bool ok, const char *how) {
    checks++;
    bool v;
    int cleared;
    bool same = layer_vs_table(k, g_buf, &v, &cleared);
    bool want = ok || (f && f->entry);
    bool count_ok = !v || (ok ? cleared == 0 : cleared > 0);
    if (!same || v != want || !count_ok) {
        failures++;
        printf("FAIL %s.%s (%s): the layer says %d (%d cleared), expected %d%s\n", k->what, f ? f->what : "-", how, v,
               cleared, want, same ? "" : "; the table disagrees");
    }
}

static void test_fields(const Kind *k) {
    memcpy(g_buf, k->good, k->size);
    expect(k, NULL, true, "as built");
    static const float bad_f[] = {NAN, -NAN, INFINITY, -INFINITY, 1e30f, -1e30f, 100001.0f, -100001.0f};
    static const float bad_hour[] = {NAN, INFINITY, -INFINITY, -0.001f, 24.001f, 99.0f};
    static const float good_hour[] = {0.0f, 12.5f, 24.0f};
    static const char *const bad_names[] = {"a-b", "a b", "a/b", "a.b", "\xc3\xa9", "Item\x01", "Weapon\x7f"};
    char how[64];
    for (int i = 0; i < k->n; i++) {
        const Field *f = &k->f[i];
        unsigned char *b = g_buf + f->off;
        switch (f->type) {
        case T_ANY:  // (anything goes: all ones, all zeros)
            memcpy(g_buf, k->good, k->size), memset(b, 0xff, f->size), expect(k, f, true, "all ones");
            memcpy(g_buf, k->good, k->size), memset(b, 0, f->size), expect(k, f, true, "all zeros");
            break;
        case T_F:
            for (size_t j = 0; j < f->size / 4; j++) {
                for (size_t v = 0; v < sizeof bad_f / sizeof *bad_f; v++) {
                    memcpy(g_buf, k->good, k->size), setf(b + 4 * j, bad_f[v]);
                    snprintf(how, sizeof how, "[%u] = %g", (unsigned)j, bad_f[v]);
                    expect(k, f, false, how);
                }
                memcpy(g_buf, k->good, k->size), setf(b + 4 * j, 100000.0f), expect(k, f, true, "the limit");
                memcpy(g_buf, k->good, k->size), setf(b + 4 * j, -100000.0f), expect(k, f, true, "minus the limit");
            }
            break;
        case T_HOUR:
            for (size_t v = 0; v < sizeof bad_hour / sizeof *bad_hour; v++)
                snprintf(how, sizeof how, "%g", bad_hour[v]), memcpy(g_buf, k->good, k->size), setf(b, bad_hour[v]), expect(k, f, false, how);
            for (size_t v = 0; v < sizeof good_hour / sizeof *good_hour; v++)
                snprintf(how, sizeof how, "%g", good_hour[v]), memcpy(g_buf, k->good, k->size), setf(b, good_hour[v]), expect(k, f, true, how);
            break;
        case T_STR:  // (not an actor's name: any character, but it ends in its field)
            memcpy(g_buf, k->good, k->size), memset(b, 'A', f->size), expect(k, f, false, "no end");
            memcpy(g_buf, k->good, k->size), put_str(b, f->size, "a/b c-d"), expect(k, f, true, "a/b c-d");
            memcpy(g_buf, k->good, k->size), memset(b, 'Z', f->size - 1), b[f->size - 1] = 0, expect(k, f, true, "full");
            break;
        case T_NAME:
            memcpy(g_buf, k->good, k->size), memset(b, 'A', f->size), expect(k, f, false, "no end");
            for (size_t v = 0; v < sizeof bad_names / sizeof *bad_names; v++)
                memcpy(g_buf, k->good, k->size), put_str(b, f->size, bad_names[v]), expect(k, f, false, "a character not in a name");
            memcpy(g_buf, k->good, k->size), put_str(b, f->size, ""), expect(k, f, true, "empty");
            memcpy(g_buf, k->good, k->size), put_str(b, f->size, "Abc_09"), expect(k, f, true, "Abc_09");
            memcpy(g_buf, k->good, k->size), memset(b, 'Z', f->size - 1), b[f->size - 1] = 0, expect(k, f, true, "full");
            break;
        case T_U8:
        case T_MASK:
        case T_I8:  // (every value of every byte)
            for (size_t j = 0; j < f->size; j++)
                for (int v = 0; v < 256; v++) {
                    memcpy(g_buf, k->good, k->size), b[j] = (unsigned char)v;
                    bool want = f->type == T_U8 ? (v <= f->a || v == f->b)
                              : f->type == T_MASK ? !(v & ~(int)f->a) : ((int8_t)v >= f->a && (int8_t)v <= f->b);
                    snprintf(how, sizeof how, "[%u] = %d", (unsigned)j, v);
                    expect(k, f, want, how);
                }
            break;
        case T_I16:
            for (size_t j = 0; j + 2 <= f->size; j += 2) {
                const int32_t vals[] = {INT16_MIN, f->a - 1, f->a, f->a + 1, f->b - 1, f->b, f->b + 1, INT16_MAX, 0, -1};
                for (size_t v = 0; v < sizeof vals / sizeof *vals; v++) {
                    if (vals[v] < INT16_MIN || vals[v] > INT16_MAX) continue;
                    int16_t x = (int16_t)vals[v];
                    memcpy(g_buf, k->good, k->size), memcpy(b + j, &x, 2);
                    snprintf(how, sizeof how, "[%u] = %d", (unsigned)(j / 2), x);
                    expect(k, f, x >= f->a && x <= f->b, how);
                }
            }
            break;
        case T_I32: {
            const int64_t vals[] = {INT32_MIN, f->a - 1, f->a, f->a + 1, f->b - 1, f->b, f->b + 1, INT32_MAX, 0, -1, -50};
            for (size_t v = 0; v < sizeof vals / sizeof *vals; v++) {
                if (vals[v] < INT32_MIN || vals[v] > INT32_MAX) continue;
                int32_t x = (int32_t)vals[v];
                memcpy(g_buf, k->good, k->size), memcpy(b, &x, 4);
                snprintf(how, sizeof how, "%d", (int)x);
                expect(k, f, x >= f->a && x <= f->b, how);
            }
            break;
        }
        }
    }
}

// ------------------------------------------------------------------ named cases (the ones that happened, or nearly)
static void test_named(void) {
    // the shield of F1: an enemy whose strings fill their fields comes through the repair with each one ended
    OnlineEnemy e;
    memcpy(&e, g_kind[K_ENEMY].good, sizeof e);
    memset(e.shield, 'A', sizeof e.shield);
    CHECK(!online_validate_enemy(&e));  // (as it came: no end)
    online_terminate_enemy(&e);
    CHECK(online_validate_enemy(&e) && strlen(e.shield) == sizeof e.shield - 1);
    // ... the same with every string of a STATE filled
    WireState w;
    memcpy(&w, g_kind[K_STATE].good, sizeof w);
    memset(w.horse, 'H', sizeof w.horse), memset(w.held[4].name, 'I', sizeof w.held[4].name);
    memset(w.fxev[7].name, 'F', sizeof w.fxev[7].name), memset(w.movers[5].as, 'M', sizeof w.movers[5].as);
    WireState w0 = w;
    int cleared;
    CHECK(online_validate_state(&w0, &cleared) && cleared == 4);  // (as it came: those four entries, without their end)
    online_terminate_state(&w);
    CHECK(online_validate_state(&w, &cleared) && cleared == 0 && strlen(w.horse) == sizeof w.horse - 1 &&
          strlen(w.fxev[7].name) == 20);
    // a negative damage would heal; 0 changes nothing (as before T213); above any life, no
    CHECK(!online_validate_hit(-50, ONLINE_ELEM_NONE));
    CHECK(online_validate_hit(0, ONLINE_ELEM_NONE) && online_validate_hit(ONLINE_V_DAMAGE_MAX, ONLINE_ELEM_ELECTRIC));
    CHECK(!online_validate_hit(ONLINE_V_DAMAGE_MAX + 1, ONLINE_ELEM_NONE) && !online_validate_hit(1, ONLINE_ELEM_COUNT));
    // a loot entry from a slot that doesn't exist; an enemy owned by nobody (fine) or by slot 8 (not)
    OnlineLoot l;
    memcpy(&l, g_kind[K_LOOT].good, sizeof l);
    l.origin = ONLINE_MAX;
    CHECK(!online_validate_loot(&l));
    memcpy(&e, g_kind[K_ENEMY].good, sizeof e);
    e.owner = 0xff;
    CHECK(online_validate_enemy(&e));
    e.owner = ONLINE_MAX;
    CHECK(!online_validate_enemy(&e));
    CHECK(online_validate_slot(0) && online_validate_slot(ONLINE_MAX - 1) && !online_validate_slot(ONLINE_MAX) && !online_validate_slot(0xff));
    // a STATE while loading (all zeros but its flag), as online.c sends one
    memset(&w, 0, sizeof w);
    w.flags = ONLINE_F_LOADING;
    CHECK(online_validate_state(&w, &cleared) && cleared == 0);
    // names
    CHECK(online_validate_name("", 1) && online_validate_name("Weapon_Sword_001", 40));
    CHECK(!online_validate_name("abc", 3));  // (no room for its end)
    CHECK(online_validate_name("ab", 3) && !online_validate_name("a b", 4) && !online_validate_name("\xff", 2));
    CHECK(online_validate_str("x y/z", 6) && !online_validate_str("xyz", 3));
}

// T222 (ADR-025, decision 1): a STATE goes whole only for its core; a bad entry of one of its lists is cleared alone,
// to its "none", and the rest stays as it came. Each expected STATE is written out here by hand
static void test_state_entries(void) {
    static WireState g, w, want;
    int cleared;
    memcpy(&g, g_kind[K_STATE].good, sizeof g);
    g.shots.n = 3;  // (more entries in the counted lists than the hand-built one has)
    g.shots.shot[1].id = 4, g.shots.shot[1].kind = ONLINE_K_NORMAL_ARROW;
    put_mtx(g.shots.shot[1].m, 1211, 161, -2391);
    g.shots.shot[2].id = 5, g.shots.shot[2].kind = ONLINE_K_ICE_ARROW;
    put_mtx(g.shots.shot[2].m, 1212, 161, -2392);
    g.ice_n = 2;
    put_mtx(g.ice[1], 1191, 158, -2411);
    w = g;
    CHECK(online_validate_state(&w, &cleared) && cleared == 0 && !memcmp(&w, &g, sizeof w));

    // (a) one bad entry in each list at once: the STATE passes, those cleared, the rest as it came
    w = g;
    w.gear_rel[2][5] = NAN;                   // a piece of gear: on no bone
    w.shots.shot[1].m[3] = NAN;               // the middle shot goes, the third moves up
    w.shots.boom[0].kind = ONLINE_K_COUNT;    // a bomb gone off
    w.ice[0][7] = INFINITY;                   // the first pillar goes, the second moves up
    w.boss[0].life = -1;
    w.fxev[1].who = 9;
    w.tack_m[1][0] = 1e30f;
    w.objs[0].m[0] = NAN;                     // (its bit of objs_trunk, set, goes with it)
    w.movers[0].m[11] = -1e9f;
    w.held[1].mode = 7;
    w.fx_anim[0] = 4096, w.dye[0] = -2;       // (d)
    want = g;
    want.gear_bone[2] = 0, memset(want.gear_rel[2], 0, sizeof want.gear_rel[2]);
    memcpy(&want.shots.shot[1], &g.shots.shot[2], sizeof want.shots.shot[1]);
    memset(&want.shots.shot[2], 0, sizeof want.shots.shot[2]), want.shots.n = 2;
    memset(&want.shots.boom[0], 0, sizeof want.shots.boom[0]);
    memcpy(want.ice[0], g.ice[1], sizeof want.ice[0]), memset(want.ice[1], 0, sizeof want.ice[1]), want.ice_n = 1;
    memset(&want.boss[0], 0, sizeof want.boss[0]);
    memset(&want.fxev[1], 0, sizeof want.fxev[1]);
    memset(want.tack[1], 0, sizeof want.tack[1]), memset(want.tack_m[1], 0, sizeof want.tack_m[1]);
    memset(&want.objs[0], 0, sizeof want.objs[0]), want.objs_trunk = (uint8_t)(g.objs_trunk & ~1);
    memset(&want.movers[0], 0, sizeof want.movers[0]);
    memset(&want.held[1], 0, sizeof want.held[1]);
    want.fx_anim[0] = -1, want.fx_frame[0] = 0, want.dye[0] = -1;
    CHECK(online_validate_state(&w, &cleared) && cleared == 12 && !memcmp(&w, &want, sizeof w));

    // (b) a bad core drops it, however good the rest is (or bad: a bad entry with it changes nothing)
    const float bad_f[] = {NAN, INFINITY, -INFINITY, 1e30f, 100001.0f};
    for (size_t v = 0; v < sizeof bad_f / sizeof *bad_f; v++) {
        for (int c = 0; c < 3; c++) { w = g; w.pos[c] = bad_f[v]; CHECK(!online_validate_state(&w, &cleared) && cleared == 0); }
        { w = g; w.yaw = bad_f[v]; CHECK(!online_validate_state(&w, &cleared)); }
        { w = g; w.yaw = bad_f[v]; w.held[0].mode = 9; CHECK(!online_validate_state(&w, &cleared)); }
    }
    { w = g; w.anim = ONLINE_A_COUNT; CHECK(!online_validate_state(&w, &cleared)); }
    { w = g; w.flags = 0x40; CHECK(!online_validate_state(&w, &cleared)); }
    { w = g; memset(w.as[3], 'A', sizeof w.as[3]); CHECK(!online_validate_state(&w, &cleared)); }
    { w = g; w.collide = 2; CHECK(!online_validate_state(&w, &cleared)); }
    { w = g; w.revive = 0xff; CHECK(!online_validate_state(&w, &cleared)); }
    { w = g; w.nopause = 3; CHECK(!online_validate_state(&w, &cleared)); }

    // (c) a Blood Moon timer past 4000 is a number like any other (I-1): it comes through as it is; only one that
    // isn't finite, or past any bound, is cleared to -1 (unknown)
    const float bm_good[] = {5000.0f, 4000.5f, 2520.0f, -1.0f, 0.0f, 100000.0f, -2.0f};
    for (size_t v = 0; v < sizeof bm_good / sizeof *bm_good; v++)
        { w = g; w.bloody_moon = bm_good[v]; CHECK(online_validate_state(&w, &cleared) && cleared == 0 && w.bloody_moon == bm_good[v]); }
    const float bm_bad[] = {NAN, INFINITY, -INFINITY, 1e30f, 100001.0f};
    for (size_t v = 0; v < sizeof bm_bad / sizeof *bm_bad; v++)
        { w = g; w.bloody_moon = bm_bad[v]; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.bloody_moon == -1.0f); }

    // (d) a trail's animation past its bound, or below -1: that slot to -1 (and its frame to 0); a dye below -1: -1
    const int16_t anim_bad[] = {4096, INT16_MAX, -2, INT16_MIN};
    for (size_t v = 0; v < sizeof anim_bad / sizeof *anim_bad; v++) {
        w = g, w.fx_anim[1] = anim_bad[v], w.fx_frame[1] = 7.5f;
        CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.fx_anim[1] == -1 && w.fx_frame[1] == 0 &&
              w.fx_anim[0] == g.fx_anim[0] && w.fx_frame[0] == g.fx_frame[0]);
    }
    { w = g; w.fx_anim[2] = 4095; CHECK(online_validate_state(&w, &cleared) && cleared == 0 && w.fx_anim[2] == 4095); }
    { w = g; w.fx_frame[2] = NAN; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.fx_anim[2] == -1 && w.fx_frame[2] == 0); }
    for (int d = -128; d < -1; d++)
        { w = g; w.dye[2] = (int8_t)d; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.dye[2] == -1 && w.dye[1] == g.dye[1]); }
    { w = g; w.dye[0] = 127; w.dye[1] = 0; CHECK(online_validate_state(&w, &cleared) && cleared == 0 && w.dye[0] == 127); }

    // the other single fields cleared to their own "none": the weather unknown, the blow on nobody with no damage (its
    // number kept: the blow it names is not taken later), an effect nobody shows, a count past its room (all of it)
    { w = g; w.weather = 9; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.weather == 255); }
    w = g, w.pvp_target = ONLINE_MAX;
    CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.pvp_target == 0 && w.pvp_damage == 0 && w.pvp_seq == g.pvp_seq);
    { w = g; w.pvp_damage = -5; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.pvp_damage == 0); }
    { w = g; w.effects = 0x40; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.effects == 0); }
    w = g, w.shots.n = ONLINE_SHOTS + 1;
    CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.shots.n == 0 && w.shots.shot[0].id == 0 &&
          !memcmp(&w.shots.boom, &g.shots.boom, sizeof w.shots.boom));
    { w = g; w.ice_n = 4; CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.ice_n == 0 && w.ice[0][3] == 0); }
    w = g, w.shots.shot[6].kind = 200;  // (past its count: cleared where it is, the count as it was)
    CHECK(online_validate_state(&w, &cleared) && cleared == 1 && w.shots.n == 3 && w.shots.shot[6].kind == 0);
    w = g, w.shots.shot[0].kind = 200, w.shots.shot[1].m[0] = NAN, w.shots.shot[2].m[11] = 1e9f;  // (all three: none left)
    CHECK(online_validate_state(&w, &cleared) && cleared == 3 && w.shots.n == 0 && w.shots.shot[0].id == 0);
}

// SKEL as its owner sent it: counts and the bytes they ask for
static void test_skel(void) {
    static uint8_t p[SKEL_BYTES + 64];
    const int bone = SKEL_BONE_BYTES;
    memset(p, 0, sizeof p);
    CHECK(!online_validate_skel(p, 0) && !online_validate_skel(p, 1));
    p[0] = 0, p[1] = 0;
    CHECK(online_validate_skel(p, 2));  // (no bones: an empty skeleton)
    p[0] = 1, p[1] = 0;
    CHECK(online_validate_skel(p, 2 + bone) && !online_validate_skel(p, 2 + bone - 1));
    p[0] = 128, p[1] = 32;
    CHECK(online_validate_skel(p, 2 + 160 * bone));
    const int over[][2] = {{161, 0}, {160, 1}, {129, 32}, {100, 61}, {0, 33}, {0, 161}, {255, 255}};
    for (size_t k = 0; k < sizeof over / sizeof *over; k++) {
        p[0] = (uint8_t)over[k][0], p[1] = (uint8_t)over[k][1];
        int need = 2 + (over[k][0] + over[k][1]) * bone;
        CHECK(!online_validate_skel(p, need < (int)sizeof p ? need : (int)sizeof p));
    }
}

// ------------------------------------------------------------------ 100 000 from a fixed seed
#define RANDOM_N 100000

static void mutate(const Kind *k, unsigned char *b) {
    static const float specials[] = {NAN, INFINITY, -INFINITY, 1e30f, 100001.0f, -100000.0f, -1.0f, 0.5f, 4000.5f, 24.5f, 5000.0f};
    int ops = 1 + (int)(rnd() % 4);
    for (int o = 0; o < ops; o++) {
        const Field *f = &k->f[rnd() % (uint64_t)k->n];
        switch (rnd() % 4) {
        case 0:  // (one byte anywhere)
            b[rnd() % k->size] = (unsigned char)(rnd() >> 56);
            break;
        case 1:  // (one value of a field)
            if (f->type == T_F || f->type == T_HOUR) {
                size_t j = rnd() % (f->size / 4);
                if (rnd() & 1) setf(b + f->off + 4 * j, specials[rnd() % (sizeof specials / sizeof *specials)]);
                else rnd_bytes(b + f->off + 4 * j, 4);
            } else
                b[f->off + rnd() % f->size] = (unsigned char)(rnd() >> 56);
            break;
        case 2:  // (a whole field)
            rnd_bytes(b + f->off, f->size);
            break;
        default:  // (nothing: it stays as it is)
            break;
        }
    }
}

static void test_random(void) {
    static unsigned char b[sizeof(WireState)];
    static uint8_t s[SKEL_BYTES + 16];
    int skel_passed = 0, skel_refused = 0, disagree = 0;
    for (int i = 0; i < RANDOM_N; i++) {
        int kind = i % (K_COUNT + 1);
        bool mutated = (i / (K_COUNT + 1)) % 2;
        if (kind == K_COUNT) {  // SKEL
            int len = (int)(rnd() % (sizeof s + 1));
            rnd_bytes(s, sizeof s);
            if (mutated) {  // (counts near the limits, and as many bytes as they ask for, give or take one)
                s[0] = (uint8_t)(rnd() % 170), s[1] = (uint8_t)(rnd() % 40);
                len = 2 + (s[0] + s[1]) * 12 + (int)(rnd() % 3) - 1;
                if (len < 0) len = 0;
                if (len > (int)sizeof s) len = (int)sizeof s;
            }
            bool v = online_validate_skel(s, len);
            bool t = len >= 2 && s[1] <= 32 && s[0] + s[1] <= 160 && len >= 2 + (s[0] + s[1]) * 12;
            checks++;
            if (v != t) {
                failures++;
                if (disagree++ < 10) printf("FAIL SKEL %d: counts %d %d, %d bytes: the layer says %d, the table %d\n", i, s[0], s[1], len, v, t);
            }
            if (v) skel_passed++;
            else skel_refused++;
            continue;
        }
        Kind *k = &g_kind[kind];
        if (mutated) {
            memcpy(b, k->good, k->size);
            mutate(k, b);
        } else {
            rnd_bytes(b, k->size);
            if (k->term && (rnd() & 1)) k->term(b);
        }
        bool v;
        int cleared;
        checks++;
        if (!layer_vs_table(k, b, &v, &cleared)) {  // (the layer let through, or left, what it shouldn't; refused a valid
                                                     // one; or cleared other entries than the table)
            failures++;
            if (disagree++ < 10) {
                printf("FAIL %s %d: the layer says %d (%d cleared)", k->what, i, v, cleared);
                for (int f = 0; f < k->n; f++)
                    if (!field_ok(b, &k->f[f])) printf(" (%s)", k->f[f].what);
                printf("\n");
            }
        }
        if (v) k->passed++, k->cleared += cleared;
        else k->refused++;
        if (k->term) {  // (and the repair always ends every string)
            k->term(b);
            bool ended = true;
            for (int f = 0; f < k->n; f++)
                if ((k->f[f].type == T_STR || k->f[f].type == T_NAME) && !memchr(b + k->f[f].off, 0, k->f[f].size)) ended = false;
            CHECK(ended);
        }
    }
    printf("random (seed 0x54323133, %d):", RANDOM_N);
    for (int i = 0; i < K_COUNT; i++) {
        printf(" %s %d/%d", g_kind[i].what, g_kind[i].passed, g_kind[i].passed + g_kind[i].refused);
        CHECK(g_kind[i].passed > 0 && g_kind[i].refused > 0);  // (both verdicts came: the comparison means something)
    }
    printf(" (STATE: %d entries cleared in those passed)", g_kind[K_STATE].cleared);
    CHECK(g_kind[K_STATE].cleared > 0);
    printf(" SKEL %d/%d passed\n", skel_passed, skel_passed + skel_refused);
    CHECK(skel_passed > 0 && skel_refused > 0);
}

int main(void) {
    kinds_init();
    test_tables_cover();
    for (int i = 0; i < K_COUNT; i++) test_fields(&g_kind[i]);
    test_named();
    test_state_entries();
    test_skel();
    test_random();
    printf("test_online_validate: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
