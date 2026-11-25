// Revive policy for co-op (runtime/coop_revive.c): whether a fallen player is revived, and the fallen state a manual
// rescue will need. Pure logic: no game memory, no network, no positions or maps, and no clock of its own (every call
// is given the time, in nanoseconds of a monotonic clock), so that tests/test_coop_revive.c can test it alone.
//
// First delivery, automatic revival (ADR-007, decision 7; ADR-008, decision 15; ADR-011, decisions 33 and 35): a fallen
// player is revived while at least one partner is standing, wherever that partner is (no limit of distance, of map or
// of number of times), at no cost or penalty to anyone, with the life the game's own fairy gives. If every player has
// fallen at once, it is the game over of the normal game and each reloads his last save.
//
// Later delivery (T432, ADR-007 decision 7; ADR-011 decision 34): the fallen state, UP -> DOWN -> REVIVING -> UP, with a
// countdown (30 s) and a rescue (a partner holds the button within 2 m for 3 s). When the countdown runs out the fallen
// player is revived on the spot with 3 hearts: there is no game over by time. Nothing of the first delivery uses it.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_REVIVE_MAX_PEERS 8                          // the most partners looked at (ONLINE_MAX; the integration asserts it)
#define COOP_REVIVE_BOTH_WINDOW_DEFAULT_NS 2000000000ull  // "at once": two falls this close are one (2 s)

// A partner, as the revived player's game knows him
typedef struct {
    bool present;      // in the session (false: a free slot or one that left; it counts for nothing)
    bool alive;        // not fallen now
    bool loading;      // in a loading screen or a transition: no one to be counted on
    bool fell;         // he has fallen at least once (fell_ns is meaningful)
    uint64_t fell_ns;  // when he fell last
} CoopRevivePeer;

typedef struct {
    bool rule_on;                 // the session's revive rule
    uint64_t my_fell_ns;          // when the player to be revived fell
    uint64_t both_window_ns;      // two falls within this long of each other (inclusive) are "at once"; 0: only the
                                  // very same instant. The integration passes COOP_REVIVE_BOTH_WINDOW_DEFAULT_NS.
    const CoopRevivePeer *peers;  // the other players (not the one to be revived)
    int npeers;                   // how many, 0..COOP_REVIVE_MAX_PEERS (more are not looked at; < 0 is 0)
} CoopReviveIn;

// Whether the player is revived: the rule is on and some partner is standing, that is, present, alive, not loading, and
// not fallen at the same time as he (he fell within both_window_ns of my_fell_ns, even if he is up again by now: the two
// went down together, so neither can raise the other). NULL in: false. No distance, map or count of earlier revivals
// is looked at, by decision.
bool coop_revive_allowed(const CoopReviveIn *in);

// ---- The fallen state (T432)

typedef enum {
    COOP_REVIVE_UP,        // on his feet
    COOP_REVIVE_DOWN,      // fallen: the countdown runs
    COOP_REVIVE_REVIVING,  // fallen, and a partner has held the button within range so far this long
} CoopReviveState;

typedef struct {
    uint64_t countdown_ns;  // a fallen player is revived this long after he fell, on the spot (default 30 s)
    uint64_t hold_ns;       // a rescue takes this long of the partner holding in range (default 3 s)
    float range_m;          // the partner holds within this distance (default 2 m)
    int timeout_hearts;     // the life he gets when the countdown ends (default 3 hearts)
} CoopReviveCfg;

// The defaults: 30 s, 3 s, 2 m, 3 hearts. A cfg of NULL is the defaults, in every call below.
CoopReviveCfg coop_revive_cfg_default(void);

typedef struct {
    CoopReviveState state;
    uint64_t down_ns;      // when he fell (DOWN, REVIVING)
    uint64_t progress_ns;  // how long of rescue has been held (REVIVING)
    uint64_t last_ns;      // the time of the last tick (REVIVING)
} CoopReviveMachine;

typedef enum {
    COOP_REVIVE_EV_NONE,       // nothing changed
    COOP_REVIVE_EV_RESCUED,    // a partner raised him: DOWN or REVIVING -> UP
    COOP_REVIVE_EV_TIMED_OUT,  // the countdown ran out: DOWN or REVIVING -> UP, on the spot, with timeout_hearts
} CoopReviveEvent;

// UP, nothing in progress. (also what to call when the player is revived some other way, or the session ends)
void coop_revive_reset(CoopReviveMachine *m);

// He has fallen (now_ns): UP -> DOWN. In any other state nothing changes (he cannot fall twice).
void coop_revive_fall(CoopReviveMachine *m, uint64_t now_ns);

// Whether a partner's button counts for the rescue: held, and within range_m of him (a negative, NaN or infinite distance
// does not count).
bool coop_revive_helper_ok(const CoopReviveCfg *cfg, float distance_m, bool holding);

// Time passes to now_ns; helper_ok is coop_revive_helper_ok for the best placed partner now. By time, not by frame
// (the same at 30, 60, 90 or 120 FPS); a time before the last one passes nothing.
//   DOWN: the countdown ending revives him (TIMED_OUT); else a helper starts the rescue (REVIVING, progress 0).
//   REVIVING: with the helper, progress grows by the time since the last tick, and at hold_ns he is raised (RESCUED);
//     if the helper lets go or moves away, the progress is lost and it is DOWN again, the countdown not restarted.
//     The countdown ending first revives him (TIMED_OUT) unless the rescue is done the same tick.
// UP: nothing. Once revived, the machine is UP.
CoopReviveEvent coop_revive_tick(CoopReviveMachine *m, const CoopReviveCfg *cfg, uint64_t now_ns, bool helper_ok);

// The life he gets for an event, in hearts: timeout_hearts for TIMED_OUT; 0 for RESCUED and NONE, that is, whatever the
// game's own fairy gives (decision 33).
int coop_revive_hearts(CoopReviveEvent ev, const CoopReviveCfg *cfg);

// What is left of the countdown at now_ns (0 if he is UP or it is over), and how far the rescue is, 0..1.
uint64_t coop_revive_remaining_ns(const CoopReviveMachine *m, const CoopReviveCfg *cfg, uint64_t now_ns);
float coop_revive_progress(const CoopReviveMachine *m, const CoopReviveCfg *cfg);

#ifdef __cplusplus
}
#endif
