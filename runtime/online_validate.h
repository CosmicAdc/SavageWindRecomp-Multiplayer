// What comes from the network, checked in one place (specs/contratos/validacion-red.md): runtime/online.c's client
// (cl_message) gives every structure it keeps to one of these before it reaches a queue or a snapshot, and drops what
// fails (the message, or in a list the entry), never repairing it - but for the one repair the contract allows: a
// string that fills its field is cut there (online_terminate_*), as online.c always did.
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
#define ONLINE_V_BLOODY_MOON 4000.0f  // OnlinePose.bloody_moon: -1 (unknown) or 0..this (online_actors.c takes no more)
#define ONLINE_V_GEAR_BONES 8         // OnlinePose.gear_bone: 0, a bone + 1 up to this (online_actors.c k_bones), or
                                      // ONLINE_GEAR_WORLD
#define ONLINE_V_HAND_BONES 2         // OnlineEnemy.wbone / sbone: 0 the world, 1 Weapon_R, 2 Weapon_L
#define ONLINE_V_OWNER_NONE 0xff      // OnlineEnemy.owner: a slot, or nobody's (online_actors.c OWNER_NONE)
#define ONLINE_V_FXEV_WHO_MAX 4       // OnlineFxEvent.who: 0 his Link, 1 his weapon, 2 / 3 their sounds, 4 his ground's
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

// STATE (a player's, WireState): every field, in the order online_wire.h has them
static inline bool online_validate_state(const WireState *w) {
    if (!online_validate_floats(w->pos, 3, ONLINE_V_LIMIT) || !online_validate_floats(&w->yaw, 1, ONLINE_V_LIMIT)) return false;
    if (w->flags & ~ONLINE_V_FLAGS) return false;
    // (ping_ms: any)
    if (w->anim >= ONLINE_A_COUNT) return false;
    for (int k = 0; k < ONLINE_AS_CHANNELS; k++)
        if (!online_validate_str(w->as[k], sizeof w->as[k])) return false;
    for (int k = 0; k < ONLINE_GEAR; k++)
        if (w->gear_bone[k] > ONLINE_V_GEAR_BONES && w->gear_bone[k] != ONLINE_GEAR_WORLD) return false;
    if (!online_validate_floats(w->gear_rel, ONLINE_GEAR * 12, ONLINE_V_LIMIT)) return false;
    // shots: the count, every entry (those past it are zero from a sender, and copied all the same)
    if (w->shots.n > ONLINE_SHOTS) return false;
    for (int k = 0; k < ONLINE_SHOTS; k++)  // (id: any)
        if (w->shots.shot[k].kind >= ONLINE_K_COUNT || !online_validate_floats(w->shots.shot[k].m, 12, ONLINE_V_LIMIT))
            return false;
    for (int k = 0; k < ONLINE_BOOMS; k++)  // (seq: any)
        if (w->shots.boom[k].kind >= ONLINE_K_COUNT || !online_validate_floats(w->shots.boom[k].pos, 3, ONLINE_V_LIMIT))
            return false;
    if (w->effects & ~ONLINE_V_EFFECTS) return false;
    if (!online_validate_grab(&w->grab) || !online_validate_grab(&w->stop)) return false;
    if (w->ice_n > 3 || !online_validate_floats(w->ice, 3 * 12, ONLINE_V_LIMIT)) return false;
    if (!online_validate_str(w->map, sizeof w->map)) return false;  // ("<type>/<name>": not an actor's name)
    for (int k = 0; k < ONLINE_BOSSES; k++)  // (hash: any; life: the damage done to it there, never below 0)
        if (!online_validate_name(w->boss[k].name, sizeof w->boss[k].name) || w->boss[k].life < 0) return false;
    if (!online_validate_str(w->fx, sizeof w->fx)) return false;
    for (int k = 0; k < 3; k++)  // (an animation's index, -1 none: its upper bound is the model's, not known here)
        if (w->fx_anim[k] < -1) return false;
    if (!online_validate_floats(w->fx_frame, 3, ONLINE_V_LIMIT)) return false;
    for (int k = 0; k < ONLINE_FX_EVENTS; k++)  // (seq: any)
        if (w->fxev[k].who > ONLINE_V_FXEV_WHO_MAX || !online_validate_str(w->fxev[k].name, sizeof w->fxev[k].name))
            return false;
    // (fx_props: any)
    if (w->weather > 8 && w->weather != 255) return false;
    for (int k = 0; k < 3; k++)
        if (w->dye[k] < -1) return false;
    // (pvp_seq: any)
    if (w->pvp_target >= ONLINE_MAX || w->pvp_damage < 0) return false;
    if (!online_validate_name(w->horse, sizeof w->horse) || !online_validate_floats(w->horse_m, 12, ONLINE_V_LIMIT) ||
        !online_validate_str(w->horse_as, sizeof w->horse_as))
        return false;
    for (int k = 0; k < 2; k++)
        if (!online_validate_name(w->tack[k], sizeof w->tack[k])) return false;
    if (!online_validate_floats(w->tack_m, 2 * 12, ONLINE_V_LIMIT)) return false;
    // (ground_props: any)
    if (w->collide > 1 || w->revive > 1 || w->nopause > 1) return false;
    if (!online_validate_floats(&w->bloody_moon, 1, ONLINE_V_LIMIT)) return false;
    if (w->bloody_moon != -1 && (w->bloody_moon < 0 || w->bloody_moon > ONLINE_V_BLOODY_MOON)) return false;
    // (aggro_id, aggro_seq: any)
    for (int k = 0; k < ONLINE_OBJS; k++)  // (hash: any)
        if (!online_validate_floats(w->objs[k].m, 12, ONLINE_V_LIMIT)) return false;
    // (broken, trunks_gone: any)
    for (int k = 0; k < ONLINE_MOVERS; k++)  // (hash: any)
        if (!online_validate_floats(w->movers[k].m, 12, ONLINE_V_LIMIT) ||
            !online_validate_str(w->movers[k].as, sizeof w->movers[k].as))
            return false;
    // (objs_trunk: a bit for each of the ONLINE_OBJS (8): any)
    for (int k = 0; k < ONLINE_HELD; k++)
        if (!online_validate_name(w->held[k].name, sizeof w->held[k].name) || w->held[k].mode > ONLINE_HELD_WORLD ||
            !online_validate_floats(w->held[k].m, 12, ONLINE_V_LIMIT))
            return false;
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
