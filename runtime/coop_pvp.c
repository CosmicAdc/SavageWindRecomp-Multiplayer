// Damage between players by agreement: see runtime/coop_pvp.h. Pure logic, tested by tests/test_coop_pvp.c.
#include "coop_pvp.h"

#include <math.h>
#include <string.h>

#define ALL_SLOTS ((1u << COOP_PVP_SLOTS) - 1u)

static bool slot_ok(int slot) { return slot >= 0 && slot < COOP_PVP_SLOTS; }

static uint32_t bit(int slot) { return 1u << slot; }

static int count(uint32_t m) {
    int n = 0;
    for (; m; m &= m - 1u) n++;
    return n;
}

static void turn_off(CoopPvp *st) {
    st->phase = COOP_PVP_OFF, st->by = -1, st->accepted = 0, st->asked_ns = 0;
}

// (after a change in who is present or who accepted: those gone are out of it; a proposal without its proposer or
// with no one else is dropped, one every player present accepted is on; a duel down to one player is off)
static void settle(CoopPvp *st) {
    st->accepted &= st->present;
    if (st->phase == COOP_PVP_PROPOSED) {
        if (!slot_ok(st->by) || !(st->accepted & bit(st->by)) || count(st->present) < 2) {
            turn_off(st);
            return;
        }
        if (!(st->present & ~st->accepted)) st->phase = COOP_PVP_ON;
    } else if (st->phase == COOP_PVP_ON) {
        if (count(st->accepted) < 2) turn_off(st);
    }
}

void coop_pvp_reset(CoopPvp *st) {
    if (!st) return;
    memset(st, 0, sizeof *st);
    st->phase = COOP_PVP_OFF, st->by = -1, st->allowed = true;
}

void coop_pvp_present(CoopPvp *st, uint32_t present) {
    if (!st) return;
    present &= ALL_SLOTS;
    uint32_t gone = st->present & ~present;
    for (int i = 0; i < COOP_PVP_SLOTS; i++)
        if (gone & bit(i)) memset(&st->atk[i], 0, sizeof st->atk[i]);  // (a new player there numbers his blows anew)
    st->present = present;
    settle(st);
}

CoopPvpCtl coop_pvp_ctl(CoopPvp *st, int who, CoopPvpOp op, uint64_t now_ns) {
    if (!st || !slot_ok(who)) return COOP_PVP_CTL_BAD;
    if (op == COOP_PVP_TURN_OFF) {  // (anyone, any phase, without asking)
        turn_off(st);
        return COOP_PVP_CTL_OK;
    }
    if (op != COOP_PVP_PROPOSE && op != COOP_PVP_ACCEPT) return COOP_PVP_CTL_BAD;
    if (!(st->present & bit(who))) return COOP_PVP_CTL_ABSENT;
    if (!st->allowed) return COOP_PVP_CTL_FORBIDDEN;
    if (st->no_duel_zone) return COOP_PVP_CTL_ZONE;
    if (op == COOP_PVP_PROPOSE) {
        if (st->phase != COOP_PVP_OFF) return COOP_PVP_CTL_BUSY;
        if (count(st->present) < 2) return COOP_PVP_CTL_ALONE;
        st->phase = COOP_PVP_PROPOSED, st->by = who, st->accepted = bit(who), st->asked_ns = now_ns;
        return COOP_PVP_CTL_OK;
    }
    if (st->phase == COOP_PVP_OFF) return COOP_PVP_CTL_NO_PROPOSAL;
    if (st->phase == COOP_PVP_PROPOSED && who == st->by) return COOP_PVP_CTL_OWN;
    if (st->accepted & bit(who)) return COOP_PVP_CTL_ALREADY;
    st->accepted |= bit(who);  // (on a proposal, or joining a duel on)
    settle(st);
    return COOP_PVP_CTL_OK;
}

bool coop_pvp_tick(CoopPvp *st, uint64_t now_ns) {
    if (!st || st->phase != COOP_PVP_PROPOSED) return false;
    if (now_ns < st->asked_ns || now_ns - st->asked_ns < COOP_PVP_ASK_NS) return false;
    turn_off(st);
    return true;
}

bool coop_pvp_set_allowed(CoopPvp *st, bool allowed) {
    if (!st) return false;
    st->allowed = allowed;
    if (allowed || st->phase == COOP_PVP_OFF) return false;
    turn_off(st);
    return true;
}

void coop_pvp_set_zone(CoopPvp *st, bool no_duel_zone) {
    if (st) st->no_duel_zone = no_duel_zone;
}

bool coop_pvp_on_death(CoopPvp *st, int who) {
    if (!st || !slot_ok(who) || st->phase != COOP_PVP_ON || !(st->accepted & bit(who))) return false;
    turn_off(st);  // (only the agreement: the revive and his pouch are none of this module's)
    return true;
}

static bool finite3(const float p[3]) { return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]); }

static CoopPvpWhy judge(CoopPvp *st, const CoopPvpHit *h) {
    if (!st || !h || !slot_ok(h->attacker) || !slot_ok(h->victim) || h->attacker == h->victim)
        return COOP_PVP_HIT_BAD;
    if (!finite3(h->from) || !finite3(h->to) || !isfinite(h->attacker_yaw) || !isfinite(h->reach))
        return COOP_PVP_HIT_BAD;
    // (its number: newer than his last, by serial arithmetic over 16 bits; kept whatever comes of the blow, so that
    // the same blow sent again never counts twice)
    CoopPvpAttacker *a = &st->atk[h->attacker];
    uint16_t ahead = (uint16_t)(h->seq - a->seq);
    if (!h->seq || (a->seen && (ahead == 0 || ahead >= 0x8000u))) return COOP_PVP_HIT_REPEAT;
    a->seen = true, a->seq = h->seq;
    if (st->phase != COOP_PVP_ON) return COOP_PVP_HIT_OFF;
    if (!st->allowed) return COOP_PVP_HIT_FORBIDDEN;
    if (st->no_duel_zone) return COOP_PVP_HIT_ZONE;
    uint32_t in = st->accepted & st->present;
    if (!(in & bit(h->attacker)) || !(in & bit(h->victim))) return COOP_PVP_HIT_NOT_IN_DUEL;
    if (!h->attacker_alive || !h->victim_alive) return COOP_PVP_HIT_DEAD;
    if (!h->same_map) return COOP_PVP_HIT_MAP;
    if (h->attacker_loading || h->victim_loading) return COOP_PVP_HIT_LOADING;
    double reach = h->reach < 0.0f ? 0.0 : (double)h->reach;
    if (reach > (double)COOP_PVP_REACH_MAX) reach = (double)COOP_PVP_REACH_MAX;
    double lim = reach + (double)COOP_PVP_SLACK_M;
    double dx = (double)h->to[0] - h->from[0], dy = (double)h->to[1] - h->from[1], dz = (double)h->to[2] - h->from[2];
    if (dx * dx + dy * dy + dz * dz > lim * lim) return COOP_PVP_HIT_FAR;
    if (h->power <= 0) return COOP_PVP_HIT_WEAPON;
    // (no more than COOP_PVP_RATE taken from him in the last second: the oldest of his last ones a second old at least;
    // one stamped after now, a clock gone back, counts as recent)
    if (a->ntaken >= COOP_PVP_RATE) {
        uint64_t oldest = a->taken_ns[a->next];
        if (oldest > h->now_ns || h->now_ns - oldest < COOP_PVP_RATE_NS) return COOP_PVP_HIT_RATE;
    }
    a->taken_ns[a->next] = h->now_ns;
    a->next = (a->next + 1) % COOP_PVP_RATE;
    if (a->ntaken < COOP_PVP_RATE) a->ntaken++;
    return COOP_PVP_HIT_OK;
}

// (the way the victim is pushed: from the attacker to him as the host sees them, unless they are too close for that to
// mean anything or he is behind the swing (stale positions: the attacker's own game only hits in front of him), then
// the way the attacker faces, yaw 0 being +z; a bomb's away from the blast)
static void push_of(const CoopPvpHit *h, float push[2]) {
    double fx = sin((double)h->attacker_yaw), fz = cos((double)h->attacker_yaw);
    double dx = (double)h->to[0] - h->from[0], dz = (double)h->to[2] - h->from[2];
    double d = sqrt(dx * dx + dz * dz);
    bool apart = h->bomb ? d >= 0.001 : (d >= (double)COOP_PVP_PUSH_NEAR && dx * fx + dz * fz >= 0.0);
    if (apart)
        push[0] = (float)(dx / d), push[1] = (float)(dz / d);
    else
        push[0] = (float)fx, push[1] = (float)fz;
}

int coop_pvp_validate(CoopPvp *st, const CoopPvpHit *h, CoopPvpVerdict *out) {
    CoopPvpWhy why = judge(st, h);
    int damage = 0;
    float push[2] = {0.0f, 0.0f};
    if (why == COOP_PVP_HIT_OK) {
        damage = h->power < COOP_PVP_DAMAGE_MAX ? h->power : COOP_PVP_DAMAGE_MAX;  // (power > 0 here)
        push_of(h, push);
    }
    if (out) out->damage = damage, out->why = why, out->push[0] = push[0], out->push[1] = push[1];
    return damage;
}

const char *coop_pvp_why_name(CoopPvpWhy why) {
    static const char *const names[] = {
        [COOP_PVP_HIT_OK] = "ok",
        [COOP_PVP_HIT_BAD] = "bad input",
        [COOP_PVP_HIT_REPEAT] = "repeated",
        [COOP_PVP_HIT_OFF] = "no duel",
        [COOP_PVP_HIT_FORBIDDEN] = "not allowed",
        [COOP_PVP_HIT_ZONE] = "no-duel zone",
        [COOP_PVP_HIT_NOT_IN_DUEL] = "not in the duel",
        [COOP_PVP_HIT_DEAD] = "dead",
        [COOP_PVP_HIT_MAP] = "another map",
        [COOP_PVP_HIT_LOADING] = "loading",
        [COOP_PVP_HIT_FAR] = "distance",
        [COOP_PVP_HIT_WEAPON] = "no weapon",
        [COOP_PVP_HIT_RATE] = "rate",
    };
    int i = (int)why;
    return i >= 0 && i < (int)(sizeof names / sizeof names[0]) && names[i] ? names[i] : "?";
}
