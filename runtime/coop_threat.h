// Enemy threat for co-op (runtime/coop_threat.c): which player hurts an enemy most, so that the host can hand that
// enemy to him rather than to the nearest one (specs/casos-de-uso/CU-15). Pure logic: no game memory, no network, no
// clock of its own (every call is given the time, in nanoseconds of a monotonic clock) and no globals, so that
// tests/test_coop_threat.c can test it alone. The host's enemy code keeps one CoopThreat per enemy it lists.
//
// Threat is effective damage (a blow is worth no more than the life the enemy had left), the same for any kind of blow,
// and it halves every half_life_ns without blows. The others' blows reach the host as HITs (coop_threat_blow); the
// owner's, whose game simulates the enemy, only as the life it loses that no HIT explains (coop_threat_life).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_THREAT_SLOTS 8    // player slots (ONLINE_MAX; the integration asserts it)
#define COOP_THREAT_PENDING 8  // HIT damage not yet seen as lost life, kept blow by blow
#define COOP_THREAT_MAX 1e9f   // no slot's threat goes above it (a hostile stream of HITs stays finite)
#define COOP_THREAT_FLOOR 0.5f // faded below half a point of damage, a slot's threat is 0 (nearness decides again)

typedef struct {
    uint64_t half_life_ns;  // threat halves every this long (0: it never fades)
    float margin;           // a clear leader has at least this many times the second's threat (< 1 or NaN: 1)
    uint64_t pending_ns;    // a HIT's damage still explains a life drop for this long after it
} CoopThreatCfg;

// The defaults: half life 8 s (Q-15-3), margin 1.5 (Q-15-2), pending 2 s
CoopThreatCfg coop_threat_cfg_default(void);

typedef struct {
    int32_t damage;     // left to be seen as lost life (> 0)
    uint64_t until_ns;  // after this it explains nothing
} CoopThreatPending;

// One enemy's threat. Zeroed (or coop_threat_reset) = no threat, no life seen yet.
typedef struct {
    float threat[COOP_THREAT_SLOTS];  // per slot, >= 0
    int32_t life;                     // the last life seen (if seen)
    bool seen;                        // a life has been seen
    bool timed;                       // decay_ns is set
    uint64_t decay_ns;                // when threat was last decayed
    CoopThreatPending pending[COOP_THREAT_PENDING];  // oldest first
    int npending;
} CoopThreat;

typedef enum {
    COOP_THREAT_NEAREST,  // no clear leader that may own it: today's nearest-player rule decides
    COOP_THREAT_KEEP,     // its owner is the clear leader and may keep it: skip the nearest-player rule
    COOP_THREAT_TO,       // hand it to another slot, the clear leader (still only once the hold is over)
} CoopThreatPick;

// A cfg of NULL is the defaults, in every call.
void coop_threat_reset(CoopThreat *t);

// Fade the threat to now. blow and life do it first themselves. A time before the last one changes nothing.
void coop_threat_decay(CoopThreat *t, const CoopThreatCfg *cfg, uint64_t now_ns);

// A HIT on the enemy by slot (not its owner: received with its slot, or sent by the host himself), of damage,
// the enemy's life being life_now (the last seen). Adds min(damage, life_now less the pending damage) to the slot's
// threat and to the pending; nothing if that is not > 0 or slot is out of range.
void coop_threat_blow(CoopThreat *t, const CoopThreatCfg *cfg, int slot, int32_t damage, int32_t life_now,
                      uint64_t now_ns);

// The enemy's life as seen now (in the owner's game: its own enemy, or its ESTATE), owner its owner's slot (-1 or
// out of range: none). A drop is taken first from the pending damage (oldest first, expired ones dropped); what is
// left is the owner's damage. A rise counts for no one. The first life seen only sets it; a negative one is ignored.
void coop_threat_life(CoopThreat *t, const CoopThreatCfg *cfg, int owner, int32_t life, uint64_t now_ns);

// Slot's threat to 0 on this enemy (dead, loading, on another map, disconnected)
void coop_threat_forget_slot(CoopThreat *t, int slot);

// Whom the enemy should belong to by threat. can[i]: slot i may own it (alive, on the same map, not loading, within
// 80 m of it), worked out by the caller. owner: its owner's slot, -1 (or out of range) none. The clear leader is the
// slot with threat > 0, more than any other's and at least margin times the second highest, every slot counted (its
// owner and those beyond 80 m too). to (if not NULL): the leader for KEEP and TO, -1 for NEAREST.
CoopThreatPick coop_threat_pick(const CoopThreat *t, const CoopThreatCfg *cfg, const bool can[COOP_THREAT_SLOTS],
                                int owner, int *to);

#ifdef __cplusplus
}
#endif
