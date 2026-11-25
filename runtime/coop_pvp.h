// Damage between players by agreement, each blow checked by the host (runtime/coop_pvp.c; specs/casos-de-uso/CU-12).
// Pure logic: no game memory, no network, no clock of its own (every call that needs the time is given it, in
// nanoseconds of a monotonic clock) and no globals, so that tests/test_coop_pvp.c can test it alone. The host's game
// keeps one CoopPvp for the session; the others adopt the phase it publishes.
//
// The agreement: OFF -> PROPOSED (by one slot) -> ON once every player present has accepted. Anyone may propose
// (P4-6 is still open: its default, "anyone proposes; it takes both"). Anyone turns it off, in any phase, at once.
// The host's rule "PvP allowed" (allowed) and a no-duel zone (no_duel_zone: either of them in a shrine or a cutscene)
// refuse proposing and accepting; the rule turned off also ends a duel. A death in a duel ends it (the normal revive
// follows, and nothing is taken from anyone's pouch: this module has no say over items).
//
// A blow (coop_pvp_validate): the attacker only says whom he hit and the blow's number; the host fills CoopPvpHit with
// what it knows of both (positions, the reach and attack power of the weapon the attacker has equipped by his last
// EQUIP) and gets the damage to send, or 0 and why. No number from the attacker is trusted. Melee and bombs only.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_PVP_SLOTS 8                   // player slots (ONLINE_MAX; the integration asserts it)
#define COOP_PVP_DAMAGE_MAX 120            // a blow's damage, quarter hearts, at most (30 hearts)
#define COOP_PVP_SLACK_M 2.0f              // reach + this: the latency margin on the distance
#define COOP_PVP_REACH_MAX 4.0f            // a reach above this is taken as this (spear 3.7 m, a bomb's blast 4 m)
#define COOP_PVP_RATE 4                    // blows taken per attacker in any second, at most
#define COOP_PVP_RATE_NS 1000000000ull     // that second
#define COOP_PVP_ASK_NS 20000000000ull     // a proposal no one answers is dropped after this (CU-12: 20 s)
#define COOP_PVP_PUSH_NEAR 0.6f            // closer than this, the push follows the attacker's facing (N5)

typedef enum {
    COOP_PVP_OFF,       // no damage between players
    COOP_PVP_PROPOSED,  // one slot (by) proposed it; waiting for every other player present
    COOP_PVP_ON,        // every player in agreed accepted it
} CoopPvpPhase;

typedef enum {
    COOP_PVP_PROPOSE,
    COOP_PVP_ACCEPT,
    COOP_PVP_TURN_OFF,  // also how a proposal is declined
} CoopPvpOp;

typedef enum {
    COOP_PVP_CTL_OK,
    COOP_PVP_CTL_BAD,          // who out of range, or an unknown op
    COOP_PVP_CTL_ABSENT,       // who is not in the session (coop_pvp_present)
    COOP_PVP_CTL_FORBIDDEN,    // the host does not allow PvP in this session
    COOP_PVP_CTL_ZONE,         // no-duel zone (shrine or cutscene)
    COOP_PVP_CTL_BUSY,         // a proposal or a duel is already on
    COOP_PVP_CTL_ALONE,        // no one else is present to duel with
    COOP_PVP_CTL_NO_PROPOSAL,  // accepting with nothing proposed
    COOP_PVP_CTL_OWN,          // accepting one's own proposal
    COOP_PVP_CTL_ALREADY,      // accepting again
} CoopPvpCtl;

// Why a blow does no damage (COOP_PVP_HIT_OK: it does)
typedef enum {
    COOP_PVP_HIT_OK,
    COOP_PVP_HIT_BAD,          // a slot out of range, the same slot twice, a number not finite
    COOP_PVP_HIT_REPEAT,       // its number is 0, or not newer than the attacker's last one
    COOP_PVP_HIT_OFF,          // no duel on
    COOP_PVP_HIT_FORBIDDEN,    // the host does not allow PvP
    COOP_PVP_HIT_ZONE,         // no-duel zone
    COOP_PVP_HIT_NOT_IN_DUEL,  // the attacker or the victim is not in the agreement (came later, or gone)
    COOP_PVP_HIT_DEAD,         // either is dead or down
    COOP_PVP_HIT_MAP,          // not on the same map
    COOP_PVP_HIT_LOADING,      // either is loading
    COOP_PVP_HIT_FAR,          // farther than reach + COOP_PVP_SLACK_M
    COOP_PVP_HIT_WEAPON,       // no attack power (no weapon)
    COOP_PVP_HIT_RATE,         // more than COOP_PVP_RATE blows in a second
} CoopPvpWhy;

// The host's memory of one attacker's blows
typedef struct {
    bool seen;                         // seq is set
    uint16_t seq;                      // his last blow's number
    uint64_t taken_ns[COOP_PVP_RATE];  // his last blows taken, a ring
    int ntaken, next;
} CoopPvpAttacker;

// The session's agreement, and the host's memory of each attacker's blows. Set up with coop_pvp_reset.
typedef struct {
    CoopPvpPhase phase;
    int by;                // the slot that proposed it (PROPOSED, ON); -1 none
    uint32_t present;      // bit per slot in the session (coop_pvp_present)
    uint32_t accepted;     // bit per slot in the agreement (the proposer's included)
    uint64_t asked_ns;     // when it was proposed (PROPOSED)
    bool allowed;          // the host's rule "PvP allowed in this session"
    bool no_duel_zone;     // either player in a shrine or a cutscene
    CoopPvpAttacker atk[COOP_PVP_SLOTS];
} CoopPvp;

// One blow as the host's game knows it: only data, filled by the integration, none of it the attacker's word but
// the two slots and seq. For a bomb: from is where it went off and reach its blast (4 m).
typedef struct {
    int attacker, victim;         // slots
    uint16_t seq;                 // the attacker's number for this blow (0: none)
    float from[3];                // the attacker's position (the host's latest), or the bomb's
    float attacker_yaw;           // which way the attacker faces, radians (0: +z), his pose's
    float to[3];                  // the victim's position (the host's latest)
    float reach;                  // the weapon's reach, metres, as the attacker's game picks its target
    int power;                    // the attack power of the attacker's equipped weapon (power.txt; 0: none), or the
                                  // bomb's fixed damage
    bool attacker_alive, victim_alive;
    bool attacker_loading, victim_loading;
    bool same_map;                // both on the same map
    bool bomb;                    // a bomb's blast (the push is away from it), not a swing
    uint64_t now_ns;
} CoopPvpHit;

typedef struct {
    int damage;          // to send in PVP_DMG (0: none); the victim takes its defence off and its shield may block
    CoopPvpWhy why;
    float push[2];       // with damage: the way the victim is pushed, a unit (x, z); else 0, 0
} CoopPvpVerdict;

// Off, allowed (the host's rule's default), no zone, no one present, no blow seen
void coop_pvp_reset(CoopPvp *st);

// Who is in the session now, a bit per slot. A slot gone is out of the agreement and its blows are forgotten (a new
// player in that slot numbers his from anew). Then: a proposal whose proposer left is dropped; a duel or a proposal
// with fewer than two players left in it is off; a proposal every player present accepted is on. A player who comes
// in during a duel is not in it until he accepts.
void coop_pvp_present(CoopPvp *st, uint32_t present);

// The agreement: who (a slot) proposes, accepts or turns it off, now_ns being the time (a proposal's 20 s start
// there). Proposing and accepting need who present, allowed and no no-duel zone; accepting needs a proposal (or a
// duel on, to join it) and another slot than the proposer's. Turning off works for any slot in range, in any phase.
CoopPvpCtl coop_pvp_ctl(CoopPvp *st, int who, CoopPvpOp op, uint64_t now_ns);

// Each frame: a proposal unanswered for COOP_PVP_ASK_NS is dropped. Returns true if it was.
bool coop_pvp_tick(CoopPvp *st, uint64_t now_ns);

// The host's rule "PvP allowed". Off: a duel or a proposal ends. Returns true if one did.
bool coop_pvp_set_allowed(CoopPvp *st, bool allowed);

// Either player in a shrine or a cutscene (the integration works it out). Proposing and accepting are refused while
// it is set, and no blow does damage; a duel on stays on (it was opened outside).
void coop_pvp_set_zone(CoopPvp *st, bool no_duel_zone);

// who died (or went down): a duel he is in ends. Nothing else changes: the normal revive follows and no item is lost
// (ADR-011, decision 39). Returns true if a duel ended.
bool coop_pvp_on_death(CoopPvp *st, int who);

// A blow's damage, or 0. Needs the duel on, allowed, no zone, both in it, both alive, on the same map, neither
// loading, within reach + COOP_PVP_SLACK_M (in 3D), a newer blow number than the attacker's last one (taken or not),
// a weapon, and no more than COOP_PVP_RATE taken from him in the last second. The damage is min(power,
// COOP_PVP_DAMAGE_MAX), never negative. out (if not NULL): the damage, why, and the push: from the attacker towards
// the victim as the host sees them; closer than COOP_PVP_PUSH_NEAR, or the victim behind the attacker's facing (the
// positions are stale, the swing is not), along the attacker's facing. A bomb's: away from where it went off (along
// the thrower's facing if right on it). The first failed check is the reason.
int coop_pvp_validate(CoopPvp *st, const CoopPvpHit *h, CoopPvpVerdict *out);

// For the log: "ok", "distance", "rate"...
const char *coop_pvp_why_name(CoopPvpWhy why);

#ifdef __cplusplus
}
#endif
