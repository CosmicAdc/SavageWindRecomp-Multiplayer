// Online play's actors, oa_frame: rt_online_frame, each frame's work in its order; forget_all. What the modules share:
// runtime/online_actors_internal.h
#include "online_actors_internal.h"
#include "hle.h"  // (RT_ON_RESET)

static float g_last_link[3];
static int g_settle;  // frames to wait after a load before creating anything

// (oa_puppet_pieces / oa_runes_pieces hand each piece to this: c and i unused)
static void orphan_piece(Cpu *c, int i, Piece *p) {
    (void)c, (void)i;
    oa_orphan(p);
}

static void forget_all(void) {
    for (int i = 0; i < ONLINE_MAX; i++) {
        oa_puppet_pieces(NULL, i, orphan_piece);
        oa_runes_pieces(NULL, i, orphan_piece);
        for (int k = 0; k < POOL; k++) oa_orphan(&oa_g_pool[i][k]);
    }
    for (int k = 0; k < POOL; k++) oa_orphan(&oa_g_pool[OA_POOL_WORLD][k]);  // (the world's row: the enemies' pieces)
    oa_puppet_forget();  // (and each Link's anim and covered to -1: none yet; nothing below reads them)
    oa_runes_forget();
    oa_puppets_forget();
    oa_owns_forget();
    oa_loot_forget();
    oa_booms_forget();
    oa_movers_forget();
    memset(oa_g_pool, 0, sizeof oa_g_pool);
}

static bool was_dead;  // (step 9: the local Link was dead, last frame)

// step 3: out of the session in the world. True: the frame ends
static bool frame_out_of_session(Cpu *c, uint64_t pinfo, const float link[3], bool in_world, int me) {
    // out of the session (left, or the connection lost) in the world: the players' actors are still there, deleted
    if (in_world && me < 0) {
        bool any = false;
        for (int i = 0; i < ONLINE_MAX; i++) {
            bool has = oa_pup(i)->link != 0;
            for (int k = 0; k < POOL; k++) has |= oa_g_pool[i][k].actor || oa_g_pool[i][k].pending;
            if (has) any = true, oa_drop(c, i, true);
        }
        for (int k = 0; k < POOL; k++)  // (the world's row: the enemies' pieces it kept, out of sight)
            if (oa_g_pool[OA_POOL_WORLD][k].actor || oa_g_pool[OA_POOL_WORLD][k].pending)
                any = true, oa_delete_piece(c, &oa_g_pool[OA_POOL_WORLD][k]);
        if (any) LOG("[online] out of the session: the players' actors deleted\n");
        if (pinfo && VALID(*(uint64_t *)(uintptr_t)(pinfo + 0x60)))  // (the host's enemies stay as they were)
            oa_enemies_client(c, *(uint64_t *)(uintptr_t)(pinfo + 0x60), -1);
        oa_booms_forget();
        memcpy(g_last_link, link, sizeof g_last_link);
        return true;
    }
    return false;
}

// step 4: a load seen; each player's online actor flag. True: out of the world, the frame ends
static bool frame_load(const float link[3], bool world) {
    // a load (no Link, or Link somewhere else at once): the game has removed the world's actors, ours with them
    float dx = link[0] - g_last_link[0], dy = link[1] - g_last_link[1], dz = link[2] - g_last_link[2];
    if (!world || dx * dx + dy * dy + dz * dz > 100 * 100) {
        // (a long jump that loaded nothing - a teleport within the loaded map - leaves them: their puppets still
        // alive, kept as they are; forgotten, they stayed in the world and were made again beside them, two of him)
        bool any = false, kept = false;
        for (int i = 0; i < ONLINE_MAX; i++) any |= oa_pup(i)->link != 0, kept |= world && oa_alive(oa_pup(i)->link, oa_pup(i)->link_w);
        if (kept) {
            LOG("[online] a long jump: the players' actors still here, kept\n");
        } else {
            if (any) LOG("[online] a load: the players' actors are gone\n");
            forget_all();
            g_settle = 60;
        }
    }
    memcpy(g_last_link, link, sizeof g_last_link);
    for (int i = 0; i < ONLINE_MAX; i++) oa_g_online_actor[i] = world && oa_running(oa_puppet_link(i));
    if (!world) return true;
    return false;
}

// step 9: waiting after a load, while the local Link is dead and once he is back. True: the frame ends
static bool frame_wait(void) {
    if (g_settle > 0) { g_settle--; return true; }
    // dead: game over; nothing made meanwhile, and three seconds more once he is up again. What is still there is kept
    // (an armour piece made again for a player is never drawn): what the game removed is seen gone and made again
    if (oa_g_local_dead) { was_dead = true; return true; }
    if (was_dead) {
        was_dead = false;
        LOG("[online] back after dying\n");
        g_settle = 180;
        return true;
    }
    return false;
}

// frame_owns_loot_quests's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static int st;

// step 11: our enemies left, the loot, the quests
static void frame_owns_loot_quests(Cpu *c, uint64_t player, const float link[3], int me) {
    oa_owns_left(c);
    if (VALID(player)) oa_loot_update(c, player, link, me, online_is_host());
    if (getenv("SAVAGEWIND_ONLINE_SLATE_TEST") && VALID(player)) {  // (test: the slate's event here, that many s in)
        if (++st == atoi(getenv("SAVAGEWIND_ONLINE_SLATE_TEST")) * 30) oa_slate_event(c, player);
    }
    oa_quests_update(c, player, online_is_host());
}

// step 12: what follows the others near: weather, Blood Moon, PvP, the players' collision, our blows in a
// menu, our guard
static void frame_near_players(Cpu *c, uint64_t player, const float link[3], int me) {
    if (VALID(player)) {
        OnlinePlayer wp[ONLINE_MAX];
        online_players(wp);
        oa_weather_follow(wp, me, link);  // (the first player near us: ours when that is us)
        oa_bloody_moon_follow(c, wp, me);
        oa_pvp_update(c, player, wp, me);
        oa_players_collide(c, player, wp, me);
        oa_own_strikes_in_menu(c, player);
        oa_own_guard(player);
    }
}

// step 15: the others' runes (Magnesis, Stasis, Cryonis) and the bosses
static void frame_runes_boss(Cpu *c, uint64_t player, int me) {
    if (VALID(player)) {
        OnlinePlayer gp[ONLINE_MAX];
        online_players(gp);
        oa_grab_update(c, player, gp, me);
        oa_stop_update(c, player, gp, me);
        oa_ice_update(c, gp, me);
        oa_boss_update(c, gp, me);
    }
}

// Each frame's work, in this order: the order is part of the behaviour (specs/fases/F2-cimientos/plan.md 3.6).
// "ends": the frame can end there.
//  1. control: the answers to `actors`, `flag`, `event`                        oa_control_frame
//  2. the gate: online play's actors on (1.9.0, its actor there)               online_actors_on            ends
//  3. out of the session in the world: the players' actors deleted             frame_out_of_session        ends
//  4. a load (no Link, or Link over 100 m away at once): all forgotten,        frame_load                  ends
//     g_settle = 60; each player's online actor flag; out of the world: ends
//  5. our pose and skeleton sampled                                            oa_sample_frame
//  6. the menus that don't pause the world (the host's rule)                   oa_rules_nopause_frame
//  6b. why our pose isn't sent, logged as it changes                           oa_sample_why_frame
//  7. the host's time of day                                                   oa_flags_hour_frame
//  8. the pool, the blasts, the effects, the orphans                           oa_pool_update ... oa_orphans_update
//  9. waiting: g_settle after a load; the local Link dead; 180 frames once     frame_wait                  ends
//     he is back
//  9b. a Link of theirs the game deleted: all forgotten, g_settle = 180,       oa_puppets_validate         ends
//     before any step below uses it (step 18 still checks: a second barrier)
// 10. the tests by environment variable (SAVAGEWIND_ONLINE_*_TEST, _LOG)       oa_control_tests_frame
// 11. our enemies left, the loot, the quests                                   frame_owns_loot_quests
// 12. weather, Blood Moon, PvP, the players' collision, our blows in a menu,   frame_near_players
//     our guard
// 13. the loot on the ground                                                   oa_ground_update
// 14. the world: objects, movers, breaks                                       oa_world_frame
// 15. the others' runes (Magnesis, Stasis, Cryonis), the bosses                frame_runes_boss
// 16. A held back near another player at a Sheikah pedestal                    oa_rules_block_a_frame
// 17. the enemies: the host's sent and the others' shown, or the host's here  oa_enemies_frame
// 18. the players: each one's Link made or dropped, placed, posed, his         oa_puppet_frame             ends
//     armour, gear, horse, effects, shots, bombs; a Link of theirs the game
//     deleted: all forgotten, g_settle = 180
// online_players is read once in each of steps 6, 12, 14, 15, 16, 17 and 18: each copy taken at its own moment, as
// before
void rt_online_frame(Cpu *c) {
    uint64_t pinfo = g_site.player_info ? *(uint64_t *)(uintptr_t)g_site.player_info : 0;
    oa_control_frame(c, pinfo);                                       // 1
    if (!online_actors_on()) return;                                  // 2
    int me = online_local_id();
    float link[3] = {0, 0, 0};
    if (pinfo) memcpy(link, (const void *)(uintptr_t)(pinfo + 0x88), sizeof link);
    bool in_world = pinfo && !framepace_title_ui() && (link[0] != 0 || link[1] != 0 || link[2] != 0);
    bool world = in_world && me >= 0;
    if (frame_out_of_session(c, pinfo, link, in_world, me)) return;   // 3
    if (frame_load(link, world)) return;                              // 4
    oa_sample_frame(c, pinfo);                                        // 5
    oa_rules_nopause_frame(c, me);                                    // 6
    oa_sample_why_frame();                                            // 6b
    oa_flags_hour_frame();                                            // 7
    memcpy(oa_g_pool_at, link, sizeof oa_g_pool_at);                  // 8
    oa_pool_update(c);
    oa_booms_update(c, *(uint64_t *)(uintptr_t)(pinfo + 0x60));
    oa_fx_update(*(uint64_t *)(uintptr_t)(pinfo + 0x60));
    oa_orphans_update(c);
    if (frame_wait()) return;                                         // 9
    if (oa_puppets_validate()) {                                      // 9b
        forget_all();  // (a Link of theirs deleted by the game, logged there: made again later)
        g_settle = 180;
        return;
    }
    oa_control_tests_frame(c, pinfo, link);                           // 10
    uint64_t player = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
    frame_owns_loot_quests(c, player, link, me);                      // 11
    frame_near_players(c, player, link, me);                          // 12
    if (VALID(player)) oa_ground_update(c, player, link);             // 13
    oa_world_frame(c, player, link, me);                              // 14
    frame_runes_boss(c, player, me);                                  // 15
    oa_rules_block_a_frame(link, me);                                 // 16
    oa_enemies_frame(c, player, link, me);                            // 17
    if (oa_puppet_frame(c, link, me)) {                               // 18
        forget_all();  // (a Link of theirs deleted by the game, logged there: made again later)
        g_settle = 180;
        return;
    }
}

// A reboot (back to the start screen: runtime/main.c teardown): the game's threads are stopped and its memory is about
// to be wiped. What this module keeps of the last boot goes, so the next one starts as in a fresh process; the
// actors themselves go with the game's memory (nothing here calls the game). The session is closed by online.c's
// own reset (ADR-025 n.º 2)
RT_ON_RESET(oa_frame_reset) {
    LOG("[online] reset\n");
    memset(g_last_link, 0, sizeof g_last_link);
    g_settle = 0;
    was_dead = false;
    st = 0;
}
