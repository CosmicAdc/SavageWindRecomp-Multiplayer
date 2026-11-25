#include "online_version.h"

#include <string.h>

// Written once by online_build_init; read by online_build_info. The caller serialises init (host/join run one at a
// time); everything else is plain bytes copied out.
static OnlineBuildInfo g_info;
static int g_have;

static void put(char *dst, size_t cap, const char *s) {
    size_t n = s ? strnlen(s, cap) : 0;
    if (n >= cap) n = cap - 1;
    memset(dst, 0, cap);
    if (n) memcpy(dst, s, n);
}

static void defaults(OnlineBuildInfo *o) {
    memset(o, 0, sizeof *o);
    o->dlc = ONLINE_BUILD_DLC_UNKNOWN;
    put(o->coop, sizeof o->coop, ONLINE_COOP_VERSION);
}

void online_build_reset(void) {
    memset(&g_info, 0, sizeof g_info);
    g_have = 0;
}

void online_build_init(const OnlineBuildSources *src) {
    if (g_have || !src) return;
    defaults(&g_info);
    g_info.protocol = src->protocol;
    g_info.overlay = src->overlay;
    g_info.dlc = src->dlc;
    put(g_info.gamelib, sizeof g_info.gamelib, src->gamelib);
    put(g_info.game, sizeof g_info.game, src->game);
    g_have = 1;
}

void online_build_info(OnlineBuildInfo *out) {
    if (!out) return;
    if (g_have) *out = g_info;
    else defaults(out);
}

// Text comparison that never reads past the array: an unterminated field is never equal to anything.
static int terminated(const char *s, size_t cap) { return memchr(s, 0, cap) != NULL; }
static int same_text(const char *a, const char *b, size_t cap) {
    return terminated(a, cap) && terminated(b, cap) && strcmp(a, b) == 0;
}

int online_build_compare(const OnlineBuildInfo *mine, const OnlineBuildInfo *theirs) {
    if (!mine || !theirs) return ONLINE_BUILD_PROTOCOL;
    if (mine->protocol != theirs->protocol) return ONLINE_BUILD_PROTOCOL;
    if (!same_text(mine->coop, theirs->coop, sizeof mine->coop)) return ONLINE_BUILD_COOP;
    if (!same_text(mine->gamelib, theirs->gamelib, sizeof mine->gamelib)) return ONLINE_BUILD_GAMELIB;
    // Empty on one side (a relay without a game) means "not applicable"; an unterminated field is not empty.
    int mine_none = terminated(mine->game, sizeof mine->game) && !mine->game[0];
    int theirs_none = terminated(theirs->game, sizeof theirs->game) && !theirs->game[0];
    if (!mine_none && !theirs_none && !same_text(mine->game, theirs->game, sizeof mine->game))
        return ONLINE_BUILD_GAME;
    if (mine->overlay != theirs->overlay) return ONLINE_BUILD_OVERLAY;
    if (mine->dlc != theirs->dlc) return ONLINE_BUILD_DLC;
    return ONLINE_BUILD_OK;
}

int online_build_valid(const OnlineBuildInfo *b) {
    if (!b) return 0;
    if (!terminated(b->coop, sizeof b->coop) || !terminated(b->gamelib, sizeof b->gamelib) ||
        !terminated(b->game, sizeof b->game))
        return 0;
    if (b->dlc > 7 && b->dlc != ONLINE_BUILD_DLC_UNKNOWN) return 0;
    if (b->pad[0] || b->pad[1] || b->pad[2]) return 0;
    return 1;
}

// Two FNV-1a 64-bit lanes with different bases, fed field by field (never the padding or the bytes after a
// terminator). Not a security hash: it only has to tell builds apart in a 16-byte announcement.
static void lane(uint64_t h[2], const void *p, size_t n) {
    const unsigned char *s = p;
    for (size_t i = 0; i < n; i++) {
        h[0] = (h[0] ^ s[i]) * 0x100000001b3ull;
        h[1] = (h[1] ^ (unsigned char)(s[i] + 0x5b)) * 0x100000001b3ull;
    }
}

static void lane_text(uint64_t h[2], const char *s, size_t cap) {
    static const unsigned char zero = 0;
    lane(h, s, strnlen(s, cap));
    lane(h, &zero, 1);
}

void online_build_digest(const OnlineBuildInfo *b, uint8_t out[16]) {
    uint64_t h[2] = {0xcbf29ce484222325ull, 0x84222325cbf29ce4ull};
    unsigned char v[3];
    if (!out) return;
    if (!b) {
        memset(out, 0, 16);
        return;
    }
    v[0] = (unsigned char)b->protocol;
    v[1] = (unsigned char)(b->protocol >> 8);
    lane(h, v, 2);
    lane_text(h, b->coop, sizeof b->coop);
    lane_text(h, b->gamelib, sizeof b->gamelib);
    lane_text(h, b->game, sizeof b->game);
    v[0] = (unsigned char)b->overlay;
    v[1] = (unsigned char)(b->overlay >> 8);
    v[2] = b->dlc;
    lane(h, v, 3);
    for (int i = 0; i < 8; i++) {
        out[i] = (uint8_t)(h[0] >> (8 * i));
        out[8 + i] = (uint8_t)(h[1] >> (8 * i));
    }
}

const char *online_build_reason_name(int reason) {
    switch (reason) {
    case ONLINE_BUILD_OK: return "ok";
    case ONLINE_BUILD_PROTOCOL: return "protocol";
    case ONLINE_BUILD_COOP: return "coop";
    case ONLINE_BUILD_GAMELIB: return "gamelib";
    case ONLINE_BUILD_GAME: return "game";
    case ONLINE_BUILD_OVERLAY: return "overlay";
    case ONLINE_BUILD_DLC: return "dlc";
    }
    return "unknown";
}
