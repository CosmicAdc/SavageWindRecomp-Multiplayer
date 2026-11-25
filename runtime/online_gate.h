// Online play's version gate: runtime/online_actors.c calls into the game at fixed 1.9.0 addresses (FN_*, P_*), so
// none of it runs on another version of the game (g_game_version, runtime/game_sites.h). Its own file, so that
// tools/test_online_gate.c can test it without the game.
#pragma once
#include <stdbool.h>
#include <string.h>

static inline bool online_version_ok(const char *v) { return v && !strcmp(v, "1.9.0"); }
