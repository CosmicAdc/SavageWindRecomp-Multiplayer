// What comes from the network, checked in one place (specs/contratos/validacion-red.md): runtime/online.c's client
// (cl_message) gives every structure it keeps to one of these before it reaches a queue or a snapshot, and drops what
// fails (the message, or in a list the entry; in STATE, all of it only for its core, else the entry, cleared to its
// "none": online_validate_state), never repairing it - but for the one repair the contract allows: a string that fills
// its field is cut there (online_terminate_*), as online.c always did.
//
// Pure functions: no state, nothing of the game's; each reads its structure field by field, by name, as online_wire.h
// and online.h define it (runtime/online_validate.c holds the sizes they were written for). They live here, inline,
// so that whatever includes online.c (tools/test_online_net.c, tools/test_online_wire.c) has them, and tools/
// test_online_validate.c tests them alone.
//
// What they don't do: an actor's name is only checked for its characters, not looked up (the overlay's tables, in
// runtime/online_actors.c: is_loot, enemy_actor, gear_actor, armor_actor); online_actors.c's own isfinite checks stay
// (a second barrier).
#pragma once

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "online.h"
#include "online_wire.h"

#define ONLINE_V_LIMIT 100000.0f      // every number: finite and |x| <= this (what online.c's sample_local holds its
                                      // own position to; rotation and scale too, until a tighter bound is set)
#define ONLINE_V_DAMAGE_MAX 1000000   // HIT: the life an enemy lost, 0..this (above any enemy's life)
#define ONLINE_V_FX_ANIM_MAX 4095     // OnlinePose.fx_anim: -1 none, or an index in his model's animation table up to
                                      // this (a hypothesis: no model's table size is known here; a conservative bound,
                                      // specs/bitacora/2026-11-25-F2-T222.md)
#define ONLINE_V_GEAR_BONES 8         // OnlinePose.gear_bone: 0, a bone + 1 up to this (online_actors.c k_bones), or
                                      // ONLINE_GEAR_WORLD
#define ONLINE_V_HAND_BONES 2         // OnlineEnemy.wbone / sbone: 0 the world, 1 Weapon_R, 2 Weapon_L
#define ONLINE_V_OWNER_NONE 0xff      // OnlineEnemy.owner: a slot, or nobody's (online_actors.c OWNER_NONE)
#define ONLINE_V_FXEV_WHO_MAX 5       // OnlineFxEvent.who: 0 his Link, 1 his weapon, 2 / 3 their sounds, 4 his ground's,
                                      // 5 the fairy's that revives him
#define ONLINE_V_FLAGS (ONLINE_F_PAUSED | ONLINE_F_LOADING)
#define ONLINE_V_EFFECTS (ONLINE_E_KAGO | ONLINE_E_KAGO_COUNT | ONLINE_E_GALE | ONLINE_E_DEMO)

// ------------------------------------------------------------------ the pieces

// n floats from p (packed or not: read byte by byte), each finite and within +-lim
static inline bool online_validate_floats(const void *p, int n, float lim) {
    const unsigned char *b = (const unsigned char *)p;
    for (int k = 0; k < n; k++) {
        float f;
        memcpy(&f, b + 4 * k, 4);
        if (!isfinite(f) || fabsf(f) > lim) return false;
    }
    return true;
}

// a string that ends within its field (cap bytes); any characters
static inline bool online_validate_str(const char *s, size_t cap) { return memchr(s, 0, cap) != NULL; }

// an actor's name: ends within cap, only [A-Za-z0-9_] ("" is fine: none)
static inline bool online_validate_name(const char *s, size_t cap) {
    for (size_t k = 0; k < cap; k++) {
        char c = s[k];
        if (!c) return true;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return false;
}

// a slot the server put in front of a message, as the place it came from (ESTATE, LOOT_ADD)
static inline bool online_validate_slot(uint8_t slot) { return slot < ONLINE_MAX; }

static inline bool online_validate_grab(const OnlineGrab *g) {
    return online_validate_name(g->name, sizeof g->name) && online_validate_floats(g->from, 3, ONLINE_V_LIMIT) &&
           online_validate_floats(g->m, 12, ONLINE_V_LIMIT);
}

// ------------------------------------------------------------------ the structures

// a counted list's entries (OnlineShots.shot, WireState.ice), each one's verdict in ok[]: of its first *count, one that
// fails goes and those after it move up (the others' games know each by its id, or by where it is, not by its place);
// past them (zeros from a sender), one that fails is cleared. A count past the list's room clears it all. How many went
static inline int online_validate_list(void *list, size_t size, int cap, uint8_t *count, const bool *ok) {
    unsigned char *e = (unsigned char *)list;
    if (*count > cap) {
        *count = 0;
        memset(e, 0, size * cap);
        return 1;
    }
    int n = 0, kept = 0;
    for (int k = 0; k < cap; k++) {
        if (!ok[k]) {
            n++;
            if (k >= *count) memset(e + size * k, 0, size);
        } else if (k < *count) {
            if (kept != k) memcpy(e + size * kept, e + size * k, size);
            kept++;
        }
    }
    memset(e + size * kept, 0, size * (size_t)(*count - kept));
    *count = (uint8_t)kept;
    return n;
}

// STATE (a player's, WireState), in two parts (specs/contratos/validacion-red.md section 3.9; ADR-025, decision 1). Its core - where
// he is (pos, yaw), flags, anim, as and the session's rules (collide, revive, nopause) - is all or nothing: one of them
// fails, false, and the message goes. Any other entry that fails (a piece of gear, a shot, a bomb gone off, a pillar, a
// boss, a slot of his trail, an effect, a dye, the horse, an object, a mechanism, a material...) is cleared in w, the
// caller's copy, to the "none" its field already has (a count made smaller; zeros: a name "", a hash 0, a mode none;
// -1; 255), never set to another value, and counted in *cleared. What it lets through holds no number that isn't
// finite or is out of its bounds, no string without its end, no count or enumeration out of range.
static inline bool online_validate_state(WireState *w, int *cleared) {
    *cleared = 0;
    // the core, in the order online_wire.h has it
    if (!online_validate_floats(w->pos, 3, ONLINE_V_LIMIT) || !online_validate_floats(&w->yaw, 1, ONLINE_V_LIMIT)) return false;
    if (w->flags & ~ONLINE_V_FLAGS) return false;
    // (ping_ms: any)
    if (w->anim >= ONLINE_A_COUNT) return false;
    for (int k = 0; k < ONLINE_AS_CHANNELS; k++)
        if (!online_validate_str(w->as[k], sizeof w->as[k])) return false;
    if (w->collide > 1 || w->revive > 1 || w->nopause > 1) return false;

    // the entries, each alone, in the same order
    int n = 0;
    bool ok[ONLINE_SHOTS];
    for (int k = 0; k < ONLINE_GEAR; k++)  // (none: bone 0, not shown)
        if ((w->gear_bone[k] > ONLINE_V_GEAR_BONES && w->gear_bone[k] != ONLINE_GEAR_WORLD) ||
            !online_validate_floats(w->gear_rel[k], 12, ONLINE_V_LIMIT))
            w->gear_bone[k] = 0, memset(w->gear_rel[k], 0, sizeof w->gear_rel[k]), n++;
    for (int k = 0; k < ONLINE_SHOTS; k++)  // (id: any)
        ok[k] = w->shots.shot[k].kind < ONLINE_K_COUNT && online_validate_floats(w->shots.shot[k].m, 12, ONLINE_V_LIMIT);
    n += online_validate_list(w->shots.shot, sizeof w->shots.shot[0], ONLINE_SHOTS, &w->shots.n, ok);
    for (int k = 0; k < ONLINE_BOOMS; k++)  // (none: seq 0)
        if (w->shots.boom[k].kind >= ONLINE_K_COUNT || !online_validate_floats(w->shots.boom[k].pos, 3, ONLINE_V_LIMIT))
            memset(&w->shots.boom[k], 0, sizeof w->shots.boom[k]), n++;
    if (w->effects & ~ONLINE_V_EFFECTS) w->effects = 0, n++;
    if (!online_validate_grab(&w->grab)) memset(&w->grab, 0, sizeof w->grab), n++;  // (none: "")
    if (!online_validate_grab(&w->stop)) memset(&w->stop, 0, sizeof w->stop), n++;
    for (int k = 0; k < 3; k++) ok[k] = online_validate_floats(w->ice[k], 12, ONLINE_V_LIMIT);
    n += online_validate_list(w->ice, sizeof w->ice[0], 3, &w->ice_n, ok);
    if (!online_validate_str(w->map, sizeof w->map)) memset(w->map, 0, sizeof w->map), n++;  // ("<type>/<name>", not an
                                                                                             // actor's; none: "")
    for (int k = 0; k < ONLINE_BOSSES; k++)  // (hash: any; life: the damage done to it there, never below 0; none: 0, "")
        if (!online_validate_name(w->boss[k].name, sizeof w->boss[k].name) || w->boss[k].life < 0)
            memset(&w->boss[k], 0, sizeof w->boss[k]), n++;
    if (!online_validate_str(w->fx, sizeof w->fx)) memset(w->fx, 0, sizeof w->fx), n++;
    for (int k = 0; k < 3; k++)  // (an animation's index in his model's table, -1 none; its frame)
        if (w->fx_anim[k] < -1 || w->fx_anim[k] > ONLINE_V_FX_ANIM_MAX ||
            !online_validate_floats(&w->fx_frame[k], 1, ONLINE_V_LIMIT))
            w->fx_anim[k] = -1, w->fx_frame[k] = 0, n++;
    for (int k = 0; k < ONLINE_FX_EVENTS; k++)  // (seq: any; none: seq 0)
        if (w->fxev[k].who > ONLINE_V_FXEV_WHO_MAX || !online_validate_str(w->fxev[k].name, sizeof w->fxev[k].name))
            memset(&w->fxev[k], 0, sizeof w->fxev[k]), n++;
    // (fx_props: any. Each word is a number or a float as his Link's ELink defines it, which isn't known here; they are
    // written as they come into his puppet's ELink: specs/contratos/validacion-red.md section 3.9)
    if (w->weather > 8 && w->weather != 255) w->weather = 255, n++;  // (none: 255, unknown)
    for (int k = 0; k < 3; k++)  // (-1 none, or 0..127: oa_sample.c sends a slot's value only from 0 to 127, int8_t's top)
        if (w->dye[k] < -1) w->dye[k] = -1, n++;
    // (pvp_seq: any) his last blow on a player: on a slot, how hard (none: 0 and 0; its number stays, so the blow it
    // names goes for good: it is not taken when the next STATE brings it again)
    if (w->pvp_target >= ONLINE_MAX || w->pvp_damage < 0) w->pvp_target = 0, w->pvp_damage = 0, n++;
    if (!online_validate_name(w->horse, sizeof w->horse) || !online_validate_floats(w->horse_m, 12, ONLINE_V_LIMIT) ||
        !online_validate_str(w->horse_as, sizeof w->horse_as)) {  // (none: "")
        memset(w->horse, 0, sizeof w->horse), memset(w->horse_m, 0, sizeof w->horse_m);
        memset(w->horse_as, 0, sizeof w->horse_as), n++;
    }
    for (int k = 0; k < 2; k++)
        if (!online_validate_name(w->tack[k], sizeof w->tack[k]) || !online_validate_floats(w->tack_m[k], 12, ONLINE_V_LIMIT))
            memset(w->tack[k], 0, sizeof w->tack[k]), memset(w->tack_m[k], 0, sizeof w->tack_m[k]), n++;
    // (ground_props: any, as fx_props)
    // his Blood Moon timer: a number (none: -1, unknown); the window it is taken in, 0..4000, is oa_flags.c's
    if (!online_validate_floats(&w->bloody_moon, 1, ONLINE_V_LIMIT)) w->bloody_moon = -1, n++;
    // (aggro_id, aggro_seq: any)
    for (int k = 0; k < ONLINE_OBJS; k++)  // (hash: any; none: hash 0, and not a trunk)
        if (!online_validate_floats(w->objs[k].m, 12, ONLINE_V_LIMIT))
            memset(&w->objs[k], 0, sizeof w->objs[k]), w->objs_trunk &= (uint8_t)~(1u << k), n++;
    // (broken, trunks_gone: any)
    for (int k = 0; k < ONLINE_MOVERS; k++)  // (hash: any; none: hash 0)
        if (!online_validate_floats(w->movers[k].m, 12, ONLINE_V_LIMIT) ||
            !online_validate_str(w->movers[k].as, sizeof w->movers[k].as))
            memset(&w->movers[k], 0, sizeof w->movers[k]), n++;
    // (objs_trunk: a bit for each of the ONLINE_OBJS (8): any)
    for (int k = 0; k < ONLINE_HELD; k++)  // (none: ONLINE_HELD_NONE, "")
        if (!online_validate_name(w->held[k].name, sizeof w->held[k].name) || w->held[k].mode > ONLINE_HELD_WORLD ||
            !online_validate_floats(w->held[k].m, 12, ONLINE_V_LIMIT))
            memset(&w->held[k], 0, sizeof w->held[k]), n++;
    *cleared = n;
    return true;
}

// EQUIP (what a player wears and carries): nine actors' names
static inline bool online_validate_equip(const WireEquip *e) {
    for (int k = 0; k < 3; k++)
        if (!online_validate_name(e->armor[k], sizeof e->armor[k])) return false;
    for (int k = 0; k < ONLINE_GEAR; k++)
        if (!online_validate_name(e->gear[k], sizeof e->gear[k])) return false;
    return true;
}

// an enemy (ENEMIES, the host's; ESTATE, an owner's)
static inline bool online_validate_enemy(const OnlineEnemy *e) {
    // (id, hash: any)
    return online_validate_name(e->name, sizeof e->name) && online_validate_floats(e->m, 12, ONLINE_V_LIMIT) &&
           online_validate_str(e->as, sizeof e->as) && online_validate_name(e->weapon, sizeof e->weapon) &&
           online_validate_floats(e->wm, 12, ONLINE_V_LIMIT) && e->life >= -1 &&
           online_validate_name(e->shield, sizeof e->shield) && online_validate_floats(e->sm, 12, ONLINE_V_LIMIT) &&
           e->wbone <= ONLINE_V_HAND_BONES && e->sbone <= ONLINE_V_HAND_BONES &&
           (e->owner < ONLINE_MAX || e->owner == ONLINE_V_OWNER_NONE);
}

// a loot entry (LOOT_ADD with its origin set from the server's slot; LOOTS, the host's list)
static inline bool online_validate_loot(const OnlineLoot *l) {
    // (gid, lid: any)
    return l->origin < ONLINE_MAX && online_validate_name(l->name, sizeof l->name) &&
           online_validate_floats(l->pos, 3, ONLINE_V_LIMIT) && online_validate_floats(l->rot, 9, ONLINE_V_LIMIT);
}

// PICK: a weapon's name, where it was
static inline bool online_validate_pick(const char name[40], const float pos[3]) {
    return online_validate_name(name, 40) && online_validate_floats(pos, 3, ONLINE_V_LIMIT);
}

// HIT: the life an enemy lost here (its id: any), the element of the arrow that did it
static inline bool online_validate_hit(int32_t damage, uint8_t element) {
    return damage >= 0 && damage <= ONLINE_V_DAMAGE_MAX && element < ONLINE_ELEM_COUNT;
}

// TIME: the host's hour of the day
static inline bool online_validate_time(float hour) { return isfinite(hour) && hour >= 0 && hour <= 24; }

// SKEL as its owner sent it (p: after the slot the server put in front): Link's bone count, his bow's, and that many
// bones (each decodes to finite numbers whatever its bytes: online.c skel_decode)
static inline bool online_validate_skel(const uint8_t *p, int len) {
    if (len < 2) return false;
    int n = p[0], nbow = p[1];
    return nbow <= ONLINE_BOW_BONES && n + nbow <= ONLINE_BONES && len >= 2 + (n + nbow) * SKEL_BONE_BYTES;
}

// ------------------------------------------------------------------ the one repair: strings cut at their field's end

static inline void online_terminate_str(char *s, size_t cap) { s[cap - 1] = 0; }

static inline void online_terminate_state(WireState *w) {
    for (int k = 0; k < ONLINE_AS_CHANNELS; k++) online_terminate_str(w->as[k], sizeof w->as[k]);
    online_terminate_str(w->grab.name, sizeof w->grab.name);
    online_terminate_str(w->stop.name, sizeof w->stop.name);
    online_terminate_str(w->map, sizeof w->map);
    for (int k = 0; k < ONLINE_BOSSES; k++) online_terminate_str(w->boss[k].name, sizeof w->boss[k].name);
    online_terminate_str(w->fx, sizeof w->fx);
    for (int k = 0; k < ONLINE_FX_EVENTS; k++) online_terminate_str(w->fxev[k].name, sizeof w->fxev[k].name);
    online_terminate_str(w->horse, sizeof w->horse);
    online_terminate_str(w->horse_as, sizeof w->horse_as);
    for (int k = 0; k < 2; k++) online_terminate_str(w->tack[k], sizeof w->tack[k]);
    for (int k = 0; k < ONLINE_MOVERS; k++) online_terminate_str(w->movers[k].as, sizeof w->movers[k].as);
    for (int k = 0; k < ONLINE_HELD; k++) online_terminate_str(w->held[k].name, sizeof w->held[k].name);
}

static inline void online_terminate_equip(WireEquip *e) {
    for (int k = 0; k < 3; k++) online_terminate_str(e->armor[k], sizeof e->armor[k]);
    for (int k = 0; k < ONLINE_GEAR; k++) online_terminate_str(e->gear[k], sizeof e->gear[k]);
}

static inline void online_terminate_enemy(OnlineEnemy *e) {
    online_terminate_str(e->name, sizeof e->name);
    online_terminate_str(e->as, sizeof e->as);
    online_terminate_str(e->weapon, sizeof e->weapon);
    online_terminate_str(e->shield, sizeof e->shield);
}

static inline void online_terminate_loot(OnlineLoot *l) { online_terminate_str(l->name, sizeof l->name); }
