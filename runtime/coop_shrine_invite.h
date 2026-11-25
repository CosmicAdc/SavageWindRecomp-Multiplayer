// Shrine invitation: when another player walks into a shrine, we are offered to join (ADR-025 n.º 11-17; T465a).
// Pure logic: no game memory, no network, no clock of its own (every call that needs the time is given it, in
// nanoseconds of a monotonic clock), no allocation, no globals, so tests/test_coop_shrine_invite.c can test it alone.
//
// Nothing is sent: the invitation is deduced from the map text that every STATE already carries (OnlinePose.map). The
// game thread feeds each STATE with coop_si_state(), polls once per frame with coop_si_poll() and follows the action
// it returns (show a notice, close it, travel to a shrine's door); the player's answer comes in by coop_si_accept() or
// coop_si_decline(). The coordinates of the door are NOT here and never come from the network: the caller takes them
// from its own table of places by the shrine number (0-135).
//
// Rules: only the exact text "CDungeon/Dungeon" + 3 digits, 000-135, is a shrine; the "" a STATE carries while a pose
// is stale or loading is ignored; malformed text is ignored; a map is believed after 2 STATE in a row; the same
// (player, shrine) does not invite again within 10 s; a notice lasts 30 s; it closes if the player leaves the shrine
// or the session; a player already inside the first time we see him does not invite (ADR-025 n.º 17).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_SI_SLOTS 8                    // player slots (ONLINE_MAX; the integration asserts it)
#define COOP_SI_SHRINES 136                // shrine numbers 0-135
#define COOP_SI_MAP_CAP 32                 // size of OnlinePose.map: the text and its NUL
#define COOP_SI_CONFIRM 2                  // STATE in a row that show the same map before it is believed
#define COOP_SI_REPEAT_NS 10000000000ull   // the same (player, shrine) invites again no sooner than this
#define COOP_SI_ASK_NS 30000000000ull      // a notice no one answers closes after this

typedef enum {
    COOP_SI_MAP_EMPTY,      // "": a pose gone stale, a pause or a load. Says nothing.
    COOP_SI_MAP_MALFORMED,  // no NUL in range, a byte that is not printable, or "CDungeon/" with a bad rest. Ignored.
    COOP_SI_MAP_OTHER,      // a well-formed map that is not a shrine (the field, a divine beast, ...)
    COOP_SI_MAP_SHRINE,     // CDungeon/DungeonNNN, NNN in 0-135
} CoopSiMapKind;

// Why an action happened or an answer did nothing
typedef enum {
    COOP_SI_WHY_NONE,
    COOP_SI_WHY_SETTING_OFF,   // shrine_invite is off
    COOP_SI_WHY_EXPIRED,       // 30 s without an answer
    COOP_SI_WHY_LEFT,          // the player left the shrine or the session
    COOP_SI_WHY_NO_NOTICE,     // an answer with nothing open
    // hard blocks: the notice is not opened, or is closed
    COOP_SI_WHY_DEAD,
    COOP_SI_WHY_SAME_SHRINE,   // we are already inside that shrine
    COOP_SI_WHY_OTHER_MAP,     // we are in a map that is not the field
    COOP_SI_WHY_CHALLENGE,     // inside a T451 challenge
    // soft blocks: the notice stays open (its 30 s run) but accepting does nothing until they clear
    COOP_SI_WHY_PAUSED,
    COOP_SI_WHY_SCENE,
    COOP_SI_WHY_LOADING,
    COOP_SI_WHY_MOUNTED,
    COOP_SI_WHY_GLIDING,
    COOP_SI_WHY_SWIMMING,
    COOP_SI_WHY_CLIMBING,
} CoopSiWhy;

// Our own states, given at every poll
#define COOP_SI_F_PAUSED    0x001u   // the game is paused or a menu pauses it
#define COOP_SI_F_SCENE     0x002u   // a cutscene or event
#define COOP_SI_F_LOADING   0x004u
#define COOP_SI_F_DEAD      0x008u   // dead, or on the game-over screen
#define COOP_SI_F_OTHER_MAP 0x010u   // in any map that is not the field (a shrine included)
#define COOP_SI_F_CHALLENGE 0x020u   // a T451 challenge
#define COOP_SI_F_MOUNTED   0x040u
#define COOP_SI_F_GLIDING   0x080u
#define COOP_SI_F_SWIMMING  0x100u
#define COOP_SI_F_CLIMBING  0x200u

typedef struct {
    bool enabled;       // the setting shrine_invite
    uint32_t flags;     // COOP_SI_F_*
    int own_shrine;     // the shrine we are in (0-135), or -1
} CoopSiCtx;

typedef enum {
    COOP_SI_NONE,
    COOP_SI_SHOW,       // show "<slot> entered shrine <shrine>. Join?" (a player who enters another shrine first gets a CLOSE for the old one)
    COOP_SI_CLOSE,      // close the notice (why says why)
    COOP_SI_TRAVEL,     // (from coop_si_accept only) go to the door of <shrine>; the notice is closed
} CoopSiAct;

typedef struct {
    CoopSiAct act;
    CoopSiWhy why;
    int slot;           // whose invitation
    int shrine;         // 0-135
} CoopSiOut;

typedef struct {
    bool known;                // the first map seen was taken as the baseline
    int16_t shrine;            // the believed shrine, -1 for none
    int16_t cand;              // the map being confirmed: shrine, or -1; meaningless while ncand == 0
    uint8_t ncand;
    int16_t pending;           // an ENTER not yet handled by a poll, -1 for none
    bool repeat_set;
    int16_t repeat_n;
    uint64_t repeat_ns;
} CoopSiPlayer;

typedef struct {
    CoopSiPlayer p[COOP_SI_SLOTS];
    bool open;                 // a notice is on
    int slot, shrine;
    uint64_t opened_ns;
    bool queued;               // one more waiting behind it, from another player
    int qslot, qshrine;
} CoopSi;

void coop_si_reset(CoopSi *s);                  // new session (or every slot gone)
void coop_si_leave(CoopSi *s, int slot);        // a player left the session or joined: forget him
CoopSiMapKind coop_si_classify(const char *map, size_t cap, int *shrine);  // shrine set for SHRINE, else -1
void coop_si_state(CoopSi *s, int slot, const char *map, size_t cap, uint64_t now_ns);  // one STATE of a player
// Once per frame; one action per call, so call it again until it returns COOP_SI_NONE.
CoopSiAct coop_si_poll(CoopSi *s, const CoopSiCtx *ctx, uint64_t now_ns, CoopSiOut *out);
// The player accepts: COOP_SI_TRAVEL, or COOP_SI_CLOSE (it had expired, or hard-blocked), or COOP_SI_NONE with the
// soft block in out->why (the notice stays).
CoopSiAct coop_si_accept(CoopSi *s, const CoopSiCtx *ctx, uint64_t now_ns, CoopSiOut *out);
// The player declines: the notice closes. Returns false if none was open.
bool coop_si_decline(CoopSi *s);
// A soft block that stops accepting now (COOP_SI_WHY_NONE if none): for greying the button
CoopSiWhy coop_si_soft_block(const CoopSiCtx *ctx);

#ifdef __cplusplus
}
#endif
