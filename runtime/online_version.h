// What identifies this build of the game to the other player (T325): the co-op mode's version, the game-code build, the
// game version, the online_mod overlay stamp and the DLC state. Pure: it reads no file and calls nothing of the game;
// the program hands the values over once (online_build_init) and everything else works on copies of them.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Version of the co-op mode itself (the fork), not of the game. A change of any field of OnlineBuildInfo or of the
// meaning of one is a change of PROTOCOL (runtime/online_wire.h), not of this string.
#define ONLINE_COOP_VERSION "coop-1"

#define ONLINE_BUILD_COOP_CAP 16     // ONLINE_COOP_VERSION and its terminator fit here
#define ONLINE_BUILD_GAMELIB_CAP 24  // gamelib_id(): 16 hex digits
#define ONLINE_BUILD_GAME_CAP 16     // g_game_version ("1.9.0"; "" when no game is loaded)

// DLC state as the build sees it (one byte on the wire). 0..7 are RtDlcState (runtime/dlc_check.h) in its order.
#define ONLINE_BUILD_DLC_UNKNOWN 255

// Plain bytes, fixed size, no pointers, so that it can be copied into a message as it is. Every text field is
// zero-filled after its terminator and must be terminated inside its array. If T308 fixes a different layout for the
// wire, only the copy between that layout and this one changes; the rules below do not.
typedef struct {
    uint16_t protocol;                       // PROTOCOL
    uint16_t overlay;                        // online_mod stamp (the N of "online_mod/vN"); 0 = none
    uint8_t dlc;                             // RtDlcState, or ONLINE_BUILD_DLC_UNKNOWN
    uint8_t pad[3];                          // always zero
    char coop[ONLINE_BUILD_COOP_CAP];        // ONLINE_COOP_VERSION
    char gamelib[ONLINE_BUILD_GAMELIB_CAP];  // gamelib_id()
    char game[ONLINE_BUILD_GAME_CAP];        // g_game_version
} OnlineBuildInfo;

// Why two builds cannot play together. The values follow the order of the checks (T314): the first field that
// differs wins.
typedef enum {
    ONLINE_BUILD_OK = 0,
    ONLINE_BUILD_PROTOCOL,  // other PROTOCOL
    ONLINE_BUILD_COOP,      // other version of the co-op mode
    ONLINE_BUILD_GAMELIB,   // game code compiled differently
    ONLINE_BUILD_GAME,      // other game version
    ONLINE_BUILD_OVERLAY,   // other online_mod overlay
    ONLINE_BUILD_DLC,       // DLC differs
} OnlineBuildReason;

// The values that come from the rest of the program. Strings may be NULL (taken as ""); they are copied and cut to
// fit. Typical call, once the game library is loaded (or at --server, where game is ""):
//   { PROTOCOL, gamelib_id(), g_game_version, <N of online_mod/vN>, (uint8_t)rt_dlc_check(root).state }
typedef struct {
    uint16_t protocol;
    const char *gamelib;
    const char *game;
    uint16_t overlay;
    uint8_t dlc;
} OnlineBuildSources;

// Stores the sources. The first call wins until online_build_reset (a build does not change while running, and
// nothing reads a file or asks the game again). Call it at host/join, not from the game thread's frame hook.
void online_build_init(const OnlineBuildSources *src);
void online_build_reset(void);  // forget the stored sources (RT_ON_RESET, tests)

// Fills *out from the stored sources and ONLINE_COOP_VERSION. Never blocks, never touches a file. Before
// online_build_init it fills only the co-op version and leaves the rest empty/zero, with the DLC unknown.
void online_build_info(OnlineBuildInfo *out);

// 0 if the two can play together, else the reason of the first field that differs. The caller knows which side is
// which and shows both values. A text field that is not terminated inside its array counts as different (it comes
// from the network). A game version that is empty on either side (a relay server has no game) is not compared.
int online_build_compare(const OnlineBuildInfo *mine, const OnlineBuildInfo *theirs);

// 1 if every text field is terminated, the DLC byte is a known state and the padding is zero: what a receiver
// checks before it keeps or compares what arrived.
int online_build_valid(const OnlineBuildInfo *b);

// 16 bytes summarising every field, for the LAN announcement. Equal builds give equal digests. It covers the values
// as they are: a relay's empty game version gives another digest than a player's, so use online_build_compare when
// that matters.
void online_build_digest(const OnlineBuildInfo *b, uint8_t out[16]);

// Short English name of a reason, for logs ("ok", "protocol", "coop", "gamelib", "game", "overlay", "dlc").
const char *online_build_reason_name(int reason);

#ifdef __cplusplus
}
#endif
