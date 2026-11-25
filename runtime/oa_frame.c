// Online play's actors, oa_frame: rt_online_frame, each frame's work in its order; forget_all. What the modules share:
// runtime/online_actors_internal.h
#include "online_actors_internal.h"

static float g_last_link[3];
static int g_settle;  // frames to wait after a load before creating anything

static void forget_all(void) {
    for (int i = 0; i < ONLINE_MAX; i++) {
        for (int k = 0; k < ARMOR; k++) oa_orphan(&oa_g_act[i].armor[k]);
        for (int k = 0; k < ONLINE_GEAR; k++) oa_orphan(&oa_g_act[i].gear[k]);
        oa_orphan(&oa_g_act[i].horse);
        oa_orphan(&oa_g_act[i].tack[0]), oa_orphan(&oa_g_act[i].tack[1]);
        for (int k = 0; k < ONLINE_SHOTS; k++) oa_orphan(&oa_g_act[i].shot[k]);
        for (int k = 0; k < ONLINE_HELD; k++) oa_orphan(&oa_g_act[i].held[k]);
        oa_orphan(&oa_g_act[i].nock);
        oa_orphan(&oa_g_act[i].slate), oa_orphan(&oa_g_act[i].rune_fx), oa_orphan(&oa_g_act[i].stop_fx), oa_orphan(&oa_g_act[i].magnet_fx);
        for (int k = 0; k < POOL; k++) oa_orphan(&oa_g_pool[i][k]);
    }
    memset(oa_g_act, 0, sizeof oa_g_act);
    oa_puppets_forget();
    oa_owns_forget();
    oa_loot_forget();
    oa_booms_forget();
    oa_movers_forget();
    memset(oa_g_pool, 0, sizeof oa_g_pool);
    for (int i = 0; i < ONLINE_MAX; i++) oa_g_act[i].anim = -1, oa_g_act[i].covered = -1;
}

void rt_online_frame(Cpu *c) {
    uint64_t pinfo = g_site.player_info ? *(uint64_t *)(uintptr_t)g_site.player_info : 0;
    oa_find_actors(pinfo);
    oa_flag_query(c);
    oa_event_query(c, pinfo);
    if (!online_actors_on()) return;
    int me = online_local_id();
    float link[3] = {0, 0, 0};
    if (pinfo) memcpy(link, (const void *)(uintptr_t)(pinfo + 0x88), sizeof link);
    bool in_world = pinfo && !framepace_title_ui() && (link[0] != 0 || link[1] != 0 || link[2] != 0);
    bool world = in_world && me >= 0;
    // out of the session (left, or the connection lost) in the world: the players' actors are still there, deleted
    if (in_world && me < 0) {
        bool any = false;
        for (int i = 0; i < ONLINE_MAX; i++) {
            bool has = oa_g_act[i].link != 0;
            for (int k = 0; k < POOL; k++) has |= oa_g_pool[i][k].actor || oa_g_pool[i][k].pending;
            if (has) any = true, oa_drop(c, i, true);
        }
        if (any) LOG("[online] out of the session: the players' actors deleted\n");
        if (pinfo && VALID(*(uint64_t *)(uintptr_t)(pinfo + 0x60)))  // (the host's enemies stay as they were)
            oa_enemies_client(c, *(uint64_t *)(uintptr_t)(pinfo + 0x60), -1);
        oa_booms_forget();
        memcpy(g_last_link, link, sizeof link);
        return;
    }
    // a load (no Link, or Link somewhere else at once): the game has removed the world's actors, ours with them
    float dx = link[0] - g_last_link[0], dy = link[1] - g_last_link[1], dz = link[2] - g_last_link[2];
    if (!world || dx * dx + dy * dy + dz * dz > 100 * 100) {
        // (a long jump that loaded nothing - a teleport within the loaded map - leaves them: their puppets still
        // alive, kept as they are; forgotten, they stayed in the world and were made again beside them, two of him)
        bool any = false, kept = false;
        for (int i = 0; i < ONLINE_MAX; i++) any |= oa_g_act[i].link != 0, kept |= world && oa_alive(oa_g_act[i].link, oa_g_act[i].link_w);
        if (kept) {
            LOG("[online] a long jump: the players' actors still here, kept\n");
        } else {
            if (any) LOG("[online] a load: the players' actors are gone\n");
            forget_all();
            g_settle = 60;
        }
    }
    memcpy(g_last_link, link, sizeof link);
    for (int i = 0; i < ONLINE_MAX; i++) oa_g_online_actor[i] = world && oa_running(oa_g_act[i].link);
    if (!world) return;
    // (sampled with a menu up too while the world goes on for it: the others see him as he is)
    if (!framepace_paused() || oa_g_nopause_now) oa_sample_pose(c, pinfo), oa_sample_skeleton(c, pinfo);
    {   // The pause menu and the map don't stop the world in a session with others (the host's rule, `nopause`): the
        // UI's pause state (0x7001e1611c) is 2 while they are up, and the game stops the world through the actor
        // system's pause bits (ksys::act::BaseProcMgr at 0x7001e937f8, its u16 at +0x168: bit n "paused for n";
        // 0x70015195c0 asks one, 0x70015195d0 / 0x70015195e0 set and clear them; the world manager's update is
        // skipped while bit 0 is set, 1.9.0 0x700127f810): opening either sets 0x7f. Those are cleared each frame
        // while the state is 2, so the others' blows, the enemies and the time of day go on while he looks at his
        // things, as in a game made for several. The quick menus (state 4) too: they stop it the same way. Not a
        // pause of another kind (cutscenes, loading: the state is 0 then)
        int others = 0;
        OnlinePlayer np[ONLINE_MAX];
        online_players(np);
        for (int i = 0; i < ONLINE_MAX; i++) others += np[i].used && i != me;
        int rule = oa_g_nopause_rule < 0 ? g_online.nopause : oa_g_nopause_rule;
        uint64_t bpm = *(uint64_t *)(uintptr_t)0x7001e937f8ull;
        static int logged;
        // (any of the UI's pauses: 2 the pause menu and the map, 4 the quick menus, others the windows that come up
        // in play, as a new item's (uiPickUpWin_00Screen); each new one logged once. Not with UI_PAUSE_LOAD, a load's:
        // the world waits for the map then)
        uint32_t ps_now = g_site.pause_state ? *(uint32_t *)(uintptr_t)g_site.pause_state : 0;
        oa_g_nopause_now = rule && others && online_state() == ONLINE_CONNECTED && ps_now && !(ps_now & UI_PAUSE_LOAD);
        // (but not once he presses A or X in the pouch - X "take", or A on a material and "take" in its list, to hold
        // it over his head; A for anything else there too: the world stops sooner than it must - until the menu goes:
        // what he takes out is made then, and made with the actors' pause bits cleared (the world going on) the game
        // never puts it in his hands: it hung in the air where he took it out as he walked off, his hands empty. The
        // world is stopped again as the menu stopped it as it opened (the physics world, 0x700156f61c(its system, 1,
        // 0) as 1.9.0 0x7000c40458; the bits, 0x70015195d0(manager, 0x7f), the manager's procs told; the scene's
        // paused byte): the menu's own resume undoes it as it goes)
        {
            static bool held_stop, x_was;
            bool x = (g_game_buttons & 5) != 0;  // (A or X: runtime/hle/input.c BTN_A, BTN_X)
            if (!ps_now) held_stop = false;
            else if (ps_now == 2 && x && !x_was && oa_g_nopause_now && !held_stop) {
                held_stop = true;
                LOG("[online] A or X in the pouch (take to hold): the menu stops the world\n");
                uint64_t phys = *(uint64_t *)(uintptr_t)0x7001d66ab0ull, scene = *(uint64_t *)(uintptr_t)0x7001d66b80ull;
                phys = VALID(phys) ? *(uint64_t *)(uintptr_t)phys : 0, scene = VALID(scene) ? *(uint64_t *)(uintptr_t)scene : 0;
                if (VALID(phys)) oa_call(c, 0x700156f61cull, phys, 1, 0, 0, 0, 0);
                if (VALID(bpm)) oa_call(c, 0x70015195d0ull, bpm, 0x7f, 0, 0, 0, 0), oa_call(c, 0x7001519720ull, bpm, 0, 0, 0, 0, 0);
                if (VALID(scene)) *(volatile uint8_t *)(uintptr_t)(scene + 0x8b8) = 1;
            }
            x_was = x;
            if (held_stop) oa_g_nopause_now = 0;
        }
        if (oa_g_nopause_now) {
            static uint32_t seen_ps[16];
            int q;
            for (q = 0; q < 16 && seen_ps[q] && seen_ps[q] != ps_now; q++) {}
            if (q < 16 && !seen_ps[q]) seen_ps[q] = ps_now, LOG("[online] the UI's pause %#x: the world goes on\n", ps_now);
        }
        oa_g_nopause_frames = oa_g_nopause_now ? oa_g_nopause_frames + 1 : 0;
        if (oa_g_nopause_now && VALID(bpm) && (*(uint16_t *)(uintptr_t)(bpm + 0x168) & 1)) {
            if (!logged++) LOG("[online] the menu doesn't stop the world (%#x)\n", *(uint16_t *)(uintptr_t)(bpm + 0x168));
            // (undone as the menu's own resume does it, 1.9.0 0x7000d462d0, for the world alone: the physics world
            // going on (0x700156f61c(its system, 0, 0)), the actors' pause bits cleared (0x70015195e0(manager, 0x7f))
            // and the manager's procs told (0x7001519720), the scene's paused byte (+0x8b8) cleared; the menu's own
            // screens, sounds and inputs are left as they are)
            uint64_t phys = *(uint64_t *)(uintptr_t)0x7001d66ab0ull, scene = *(uint64_t *)(uintptr_t)0x7001d66b80ull;
            phys = VALID(phys) ? *(uint64_t *)(uintptr_t)phys : 0, scene = VALID(scene) ? *(uint64_t *)(uintptr_t)scene : 0;
            if (VALID(phys)) oa_call(c, 0x700156f61cull, phys, 0, 0, 0, 0, 0);
            oa_call(c, 0x70015195e0ull, bpm, 0x7f, 0, 0, 0, 0);
            oa_call(c, 0x7001519720ull, bpm, 0, 0, 0, 0, 0);
            if (VALID(scene)) *(volatile uint8_t *)(uintptr_t)(scene + 0x8b8) = 0;
        } else if (g_site.pause_state && *(uint32_t *)(uintptr_t)g_site.pause_state == 0) logged = 0;
        if (oa_g_nopause_now && !getenv("SAVAGEWIND_ONLINE_MENU_STILL")) {
            // (the world drawn behind it: the pause menu also turns the world's camera off as it opens, 1.9.0
            // 0x7000e6840c: 0x7000e89490(1, false), camera 1 of the game's list ([[0x7001d6b0b8] +0x48][1]) made
            // inactive (its +0x90 bit 0), so its view isn't drawn; turned on again the same way, 0x7000e89490(1, true).
            // The menu's own drawing pause is the patch menu_draw, runtime rt_menu_draw_pause)
            uint64_t cl = *(uint64_t *)(uintptr_t)0x7001d6b0b8ull;
            cl = VALID(cl) ? *(uint64_t *)(uintptr_t)cl : 0;
            uint64_t arr = VALID(cl) && *(uint32_t *)(uintptr_t)(cl + 0x40) > 1 ? *(uint64_t *)(uintptr_t)(cl + 0x48) : 0;
            uint64_t cam = VALID(arr) ? *(uint64_t *)(uintptr_t)(arr + 8) : 0;
            if (VALID(cam) && !(*(uint16_t *)(uintptr_t)(cam + 0x90) & 1)) {
                static int told;
                if (!told++) LOG("[online] the world's camera on behind the menu\n");
                oa_call(c, 0x7000e89490ull, 1, 1, 0, 0, 0, 0);
            }
        }
    }
    {   // (why our pose isn't sent, logged as it changes: the others treat a player without one as on their map)
        static int why = -1;
        int now = framepace_paused() ? 1 : 0;
        if (now != why) LOG("[online] our pose: %s\n", now ? "not sampled (paused)" : "sampled"), why = now;
    }
    {   // the time of day: the host's for everyone (his game sends it; the others follow when theirs is off by more than
        // 3 game minutes, a few seconds: the two clocks run at the same speed)
        float mine = test_warp_hour(NULL), host;
        online_set_local_hour(mine);
        if (mine >= 0 && online_host_hour(&host)) {
            float d = host - mine;
            if (d > 12) d -= 24;
            if (d < -12) d += 24;
            if (d > 0.05f || d < -0.05f) {
                test_warp_set_hour(host);
                LOG("[online] the host's time: %.2f (was %.2f)\n", host, mine);
            }
        }
    }
    memcpy(oa_g_pool_at, link, sizeof oa_g_pool_at);
    oa_pool_update(c);
    oa_booms_update(c, *(uint64_t *)(uintptr_t)(pinfo + 0x60));
    oa_fx_update(*(uint64_t *)(uintptr_t)(pinfo + 0x60));
    oa_orphans_update(c);
    if (g_settle > 0) { g_settle--; return; }
    // dead: game over; nothing made meanwhile, and three seconds more once he is up again. What is still there is kept
    // (an armour piece made again for a player is never drawn): what the game removed is seen gone and made again
    static bool was_dead;
    if (oa_g_local_dead) { was_dead = true; return; }
    if (was_dead) {
        was_dead = false;
        LOG("[online] back after dying\n");
        g_settle = 180;
        return;
    }
    if (getenv("SAVAGEWIND_ONLINE_ENEMY_TEST")) {  // (test: each enemy's online actor made in turn, next to Link)
        static MapLine *m;
        static int n = -1, next, tick;
        static Piece p;
        if (n < 0) n = oa_map_load("enemies.txt", &m);
        if (tick++ % 20 == 0 && next < n) {
            if (p.actor || p.pending) oa_delete_piece(c, &p);
            LOG("[online] enemy test %d: %s\n", next, m[next].val);
            oa_piece_update(c, 0, &p, m[next].val, "test");
            next++;
        } else oa_piece_update(c, 0, &p, p.name, "test");
        if (p.actor) {
            float mm[12] = {1, 0, 0, link[0] + 4, 0, 1, 0, link[1], 0, 0, 1, link[2]};
            oa_call(c, FN_SET_MTX, p.actor, (uint64_t)(uintptr_t)mm, 0, 0, 0, 0);
        }
    }
    if (getenv("SAVAGEWIND_ONLINE_LOOK_LOG")) {  // (test: the actors near whose +0x710 / +0x518 change)
        static uint64_t seen_a[64];
        static uint32_t seen_v[64];
        static int tick;
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        if (VALID(pl0) && tick++ % 3 == 0) {
            int na = oa_all_actors(*(uint64_t *)(uintptr_t)(pl0 + 0x68), oa_g_actors, MAX_ACTORS);
            const float *pm = (const float *)(uintptr_t)(pl0 + 0x398);
            for (int k = 0; k < na; k++) {
                uint64_t a = oa_g_actors[k];
                const float *m = (const float *)(uintptr_t)(a + 0x398);
                if ((m[3] - pm[3]) * (m[3] - pm[3]) + (m[11] - pm[11]) * (m[11] - pm[11]) > 30 * 30) continue;
                uint32_t v = *(uint32_t *)(uintptr_t)(a + 0x710);
                int q = 0;
                while (q < 64 && seen_a[q] != a) q++;
                if (q == 64) { for (q = 0; q < 64 && seen_a[q]; q++) ; if (q == 64) continue; seen_a[q] = a, seen_v[q] = v; continue; }
                if (v != seen_v[q]) {
                    char n[48];
                    oa_actor_name_of(a, n);
                    LOG("[online] look %s: +0x710 %x -> %x\n", n, seen_v[q], v);
                    seen_v[q] = v;
                }
            }
        }
    }
    {   // (test: SAVAGEWIND_ONLINE_DIE_TEST=<s>: the local Link dies that many seconds in)
        static int t;
        const char *dt = getenv("SAVAGEWIND_ONLINE_DIE_TEST");
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        if (dt && VALID(pl0) && ++t == atoi(dt) * 30) *(int32_t *)(uintptr_t)(pl0 + ENEMY_LIFE) = 0, LOG("[online] die test\n");
    }
    if (getenv("SAVAGEWIND_ONLINE_LIFE_LOG")) {  // (test: the local Link's life, as it changes)
        static int32_t last = -12345;
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        int32_t v = VALID(pl0) ? *(int32_t *)(uintptr_t)(pl0 + ENEMY_LIFE) : -1;
        if (v != last) LOG("[online] our life %d\n", v), last = v;
    }
    {   // (test: SAVAGEWIND_ONLINE_KNOCK_TEST=<s>: a heavy blow's knock on the local Link every that many seconds)
        static int t;
        const char *kt = getenv("SAVAGEWIND_ONLINE_KNOCK_TEST");
        uint64_t pl0 = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        if (kt && VALID(pl0) && ++t % (atoi(kt) * 30) == 0) {
            const float *pm = (const float *)(uintptr_t)(pl0 + 0x398);
            float at[3] = {pm[3] + 1, pm[7] + 0.3f, pm[11]};
            const char *kn = getenv("SAVAGEWIND_ONLINE_KNOCK");
            oa_boom_start(c, kn ? kn : "OnlineShock", at);
            LOG("[online] knock test\n");
        }
    }
    {   // the enemies: the host's for everyone
        uint64_t player = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
        oa_owns_left(c);
        if (VALID(player)) oa_loot_update(c, player, link, me, online_is_host());
        if (getenv("SAVAGEWIND_ONLINE_SLATE_TEST") && VALID(player)) {  // (test: the slate's event here, that many s in)
            static int st;
            if (++st == atoi(getenv("SAVAGEWIND_ONLINE_SLATE_TEST")) * 30) oa_slate_event(c, player);
        }
        oa_quests_update(c, player, online_is_host());
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
        if (VALID(player)) oa_ground_update(c, player, link);
        if (VALID(player)) {
            OnlinePlayer op[ONLINE_MAX];
            online_players(op);
            oa_objs_update(c, player, link, op, me);
            oa_movers_update(c, player, link, op, me);
            oa_breaks_update(c, player, link, op, me);
        }
        if (VALID(player)) {
            OnlinePlayer gp[ONLINE_MAX];
            online_players(gp);
            oa_grab_update(c, player, gp, me);
            oa_stop_update(c, player, gp, me);
            oa_ice_update(c, gp, me);
            oa_boss_update(c, gp, me);
        }
        {   // another player at a Sheikah pedestal within 30 m (not us in an event): our A does nothing meanwhile
            OnlinePlayer bp[ONLINE_MAX];
            online_players(bp);
            int block = 0;
            for (int i = 0; i < ONLINE_MAX; i++) {
                if (i == me || !bp[i].used || !bp[i].pose.valid || !(bp[i].pose.effects & ONLINE_E_DEMO)) continue;
                float dx = bp[i].pos[0] - link[0], dz = bp[i].pos[2] - link[2];
                if (dx * dx + dz * dz < 30 * 30) block = 1;
            }
            extern bool framepace_in_event(void);
            static int dlog;
            if (getenv("SAVAGEWIND_ONLINE_DEMO_LOG") && dlog++ % 150 == 0)
                for (int i = 0; i < ONLINE_MAX; i++)
                    if (bp[i].used && i != me)
                        LOG("[online] demo check: %s valid %d effects %02x at %.0f m, we in event %d\n", bp[i].name, bp[i].pose.valid,
                            bp[i].pose.effects, sqrtf((bp[i].pos[0] - link[0]) * (bp[i].pos[0] - link[0]) +
                                                      (bp[i].pos[2] - link[2]) * (bp[i].pos[2] - link[2])), framepace_in_event());
            if (framepace_in_event()) block = 0;
            if (block != g_online_block_a) LOG("[online] %s\n", block ? "another player is at a Sheikah pedestal near: A held back" : "A back");
            g_online_block_a = block;
        }
        OnlinePlayer epl[ONLINE_MAX];
        online_players(epl);
        static int tick;
        if (VALID(player) && online_is_host()) {
            if (tick++ % 3 == 0) oa_enemies_host(c, player, link, epl, me);
            oa_enemies_show(c, player, oa_g_reg_out, oa_g_reg_n, true, link, me, false);  // (the others': shown here)
        } else if (VALID(player)) oa_enemies_client(c, player, me);
    }

    OnlinePlayer pl[ONLINE_MAX];
    online_players(pl);
    for (int i = 0; i < ONLINE_MAX; i++) {
        bool want = pl[i].used && i != me && !(pl[i].flags & ONLINE_F_LOADING) && oa_same_map(&pl[i]);  // (in a shrine or a
                                                                                                  // Divine Beast: not here)
        if (want) {
            float ex = pl[i].pos[0] - link[0], ez = pl[i].pos[2] - link[2];
            want = ex * ex + ez * ez < 400 * 400;  // (far away: the world there isn't loaded)
            oa_g_dist = sqrtf(ex * ex + ez * ez);
        }
        if (!want) {
            if (oa_g_act[i].link && !framepace_paused()) oa_drop(c, i, !pl[i].used);
            else if (!pl[i].used)  // (left: his pool goes)
                for (int k = 0; k < POOL; k++)
                    if (oa_g_pool[i][k].actor || oa_g_pool[i][k].pending) oa_delete_piece(c, &oa_g_pool[i][k]);
            continue;
        }
        // another player in his slot (one left and another came before it was seen): the first one's actors go
        if (oa_g_act[i].link && strcmp(oa_g_act[i].name, pl[i].name)) {
            LOG("[online] slot %d is %s now, not %s\n", i, pl[i].name, oa_g_act[i].name);
            oa_drop(c, i, true);
            continue;
        }
        // his Link gone, deleted by the game: the world's actors are going (the local Link died: game over; a load
        // not seen by where he is): all forgotten, made again two seconds later
        if (oa_g_act[i].link && !oa_alive(oa_g_act[i].link, oa_g_act[i].link_w)) {
            LOG("[online] the game removed %s's Link: the world's actors are going, made again later (%.0f m away)\n", pl[i].name, oa_g_dist);
            forget_all();
            g_settle = 180;
            return;
        }
        if (!oa_g_act[i].link) {
            if (oa_g_act[i].link_fails > 10) continue;
            oa_g_act[i].link = oa_create(c, oa_actor_name());
            if (!oa_g_act[i].link) {
                if (oa_g_act[i].link_fails++ == 0) LOG("[online] can't create a %s for %s (not loaded?)\n", oa_actor_name(), pl[i].name);
                continue;
            }
            oa_g_act[i].link_w = oa_watch(oa_g_act[i].link);
            memcpy(oa_g_act[i].name, pl[i].name, sizeof oa_g_act[i].name);
            LOG("[online] %s: %s (actor %llx)\n", pl[i].name, oa_actor_name(), (unsigned long long)oa_g_act[i].link);
        }
        // the matrix: turned about y by the heading (forward is +z), at the player's feet
        float s = sinf(pl[i].yaw), co = cosf(pl[i].yaw);
        float m[12] = {co, 0, s, pl[i].pos[0], 0, 1, 0, pl[i].pos[1], -s, 0, co, pl[i].pos[2]};
        oa_call(c, FN_SET_MTX, oa_g_act[i].link, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
        if (!oa_is_online_player() || !oa_running(oa_g_act[i].link)) continue;
        oa_pose_link(c, i, &pl[i]);

        // his armour: an actor per piece of its model alone (OnlineArmor_*, tools/online_mod.py) on his Link's matrix,
        // posed bone by bone as his Link (the game's own Armor_* actors, worn on his skeleton, crashed and weren't drawn
        // made a second time)
        for (int k = 0; k < ARMOR; k++) {
            // (none: what the game puts on his Link then: his hair, his shorts; shirtless is his own body)
            const char *w = pl[i].armor[k][0] ? pl[i].armor[k] : k == 0 ? "Armor_Default_Head" : k == 2 ? "Armor_Default_Lower" : "";
            const char *oa = w[0] ? oa_armor_actor(w) : NULL;
            Piece *p = &oa_g_act[i].armor[k];
            bool had = p->actor != 0;
            static InstParams dp;  // (his dye on it, as the player's armour code makes his: ArmorDyeColor)
            memset(&dp, 0, sizeof dp);
            int8_t dye = pl[i].pose.valid ? pl[i].pose.dye[k] : -1;
            if (p->actor && dye != oa_g_act[i].dye[k]) oa_delete_piece(c, p);  // (another dye: made again with it)
            if (pl[i].pose.valid && pl[i].pose.dye[k] >= 0) oa_inst_params_s32(&dp, "ArmorDyeColor", pl[i].pose.dye[k]);
            oa_g_piece_params = dp.n ? &dp : NULL;
            bool made = oa_piece_update(c, i, p, oa ? oa : "", pl[i].name);
            oa_g_piece_params = NULL;
            if (made) oa_g_act[i].dye[k] = dye;
            if (made) {
                oa_model_visible(c, p->actor, true);
                oa_g_act[i].covered = -1;
            }
            if (had != (p->actor != 0)) oa_g_act[i].covered = -1;
            if (p->actor) {
                oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
                oa_pose_armor(c, p);
            }
        }
        int pieces = 0;
        bool all_running = true;
        for (int k = 0; k < ARMOR; k++) {
            pieces |= (oa_g_act[i].armor[k].actor != 0) << k;
            if (oa_g_act[i].armor[k].actor) all_running &= oa_running(oa_g_act[i].armor[k].actor);
        }
        if (all_running && pieces != oa_g_act[i].covered) oa_cover(c, i), oa_g_act[i].covered = pieces;

        // Daruk's Protection: the player's own effects on his Link while they are on in his game, "Kago_Shield" (a hit
        // taken) and "Kago_ShieldCount_0<n>" (its ring while he guards) (0x7001100bd0: x8 the handle out, x0 the actor,
        // x1 the effect's name in its ELink; 0x700056d010(handle) ends one)
        for (int k = 0; k < 2; k++) {
            int e = pl[i].pose.valid ? pl[i].pose.effects : 0;
            int n = (e & ONLINE_E_KAGO_COUNT) >> ONLINE_E_KAGO_COUNT_SHIFT;
            bool want_k = k == 0 ? (e & ONLINE_E_KAGO) != 0 : n != 0;
            uint64_t kp = oa_g_act[i].kago[k].p;
            bool on = VALID(kp) && *(uint32_t *)(uintptr_t)(kp + 0x20) == oa_g_act[i].kago[k].id;
            if (on && (!want_k || (k == 1 && n != oa_g_act[i].kago_n))) {
                oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_g_act[i].kago[k], 0, 0, 0, 0, 0);
                memset(&oa_g_act[i].kago[k], 0, sizeof oa_g_act[i].kago[k]);
                on = false;
            }
            if (want_k && !on) {
                static char name[24];
                if (k == 0) snprintf(name, sizeof name, "Kago_Shield");
                else snprintf(name, sizeof name, "Kago_ShieldCount_0%d", n), oa_g_act[i].kago_n = n;
                oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_g_act[i].kago[k], oa_g_act[i].link, (uint64_t)(uintptr_t)name);
                LOG("[online] %s: %s (%llx)\n", pl[i].name, name, (unsigned long long)oa_g_act[i].kago[k].p);
            }
        }
        {   // his weapon's swing trail ("Blur_Default", the weapon's own ELink's: its OnlineGear has it) on the weapon in
            // his hand while his game plays it; a new one for each swing
            uint64_t tp = oa_g_act[i].trail.p;
            bool on = VALID(tp) && *(uint32_t *)(uintptr_t)(tp + 0x20) == oa_g_act[i].trail.id;
            const char *want = pl[i].pose.fx;
            uint64_t w = oa_g_act[i].gear[0].actor;
            bool held = VALID(w) && pl[i].pose.gear_bone[0] && pl[i].pose.gear_bone[0] != ONLINE_GEAR_WORLD;
            if (on && (!want[0] || !held || strcmp(want, oa_g_act[i].trail_name))) {
                oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_g_act[i].trail, 0, 0, 0, 0, 0);
                memset(&oa_g_act[i].trail, 0, sizeof oa_g_act[i].trail);
                oa_g_act[i].trail_name[0] = 0;
                on = false;
            }
            if (!on && want[0] && held && strcmp(want, oa_g_act[i].trail_name)) {
                oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_g_act[i].trail, w, (uint64_t)(uintptr_t)want);
                snprintf(oa_g_act[i].trail_name, sizeof oa_g_act[i].trail_name, "%s", want);
                // the sword trail (SwordBlur) draws from its actor's AnimInfo (+0x570 +0x148, its +0x2c0 the trail data:
                // 1.9.0 0x700152ff4c), the player's own in his game: the effect's user data (+0x190) has the weapon as
                // its actor (+8), whose AnimInfo is the empty Dummy; his Link here has the player's (online_mod)
                uint64_t ep = oa_g_act[i].trail.p, ud = VALID(ep) ? *(uint64_t *)(uintptr_t)(ep + 0x190) : 0;
                uint64_t lk = oa_g_act[i].link;
                if (VALID(ud) && VALID(lk) && *(uint64_t *)(uintptr_t)(ud + 8) == w) *(uint64_t *)(uintptr_t)(ud + 8) = lk;
                if (getenv("SAVAGEWIND_ONLINE_FX_LOG")) {
                    uint64_t r = VALID(lk) ? *(uint64_t *)(uintptr_t)(lk + 0x570) : 0;
                    uint64_t ai = VALID(r) ? *(uint64_t *)(uintptr_t)(r + 0x148) : 0;
                    LOG("[online] %s: %s on his weapon %llx (%llx: user data %llx, its actor %llx; his Link's trail data "
                        "%llx)\n", pl[i].name, want, (unsigned long long)w, (unsigned long long)ep, (unsigned long long)ud,
                        (unsigned long long)(VALID(ud) ? *(uint64_t *)(uintptr_t)(ud + 8) : 0),
                        (unsigned long long)(VALID(ai) ? *(uint64_t *)(uintptr_t)(ai + 0x2c0) : 0));
                }
            }
            if (!want[0]) oa_g_act[i].trail_name[0] = 0;
            // his Link's ELink property values on his puppet's (the same ELink: its effects play as his do)
            {
                uint64_t ui = oa_elink_ui(oa_g_act[i].link), pv = VALID(ui) ? *(uint64_t *)(uintptr_t)(ui + 0x70) : 0;
                bool any = false;
                for (int k = 0; k < ONLINE_FX_PROPS; k++) any |= pl[i].pose.fx_props[k] != 0;
                if (VALID(pv) && any) memcpy((void *)(uintptr_t)pv, pl[i].pose.fx_props, sizeof pl[i].pose.fx_props);
            }
            // his other effects, each once by its number, on his puppet or the weapon in its hand; ended after a while
            uint64_t now = plat_now_ns();
            for (int q = 0; q < ONLINE_FX_EVENTS; q++) {
                uint64_t ep = oa_g_act[i].fxev[q].p;
                if (ep && now - oa_g_act[i].fxev[q].t > FX_EVENT_NS) {
                    if (VALID(ep) && *(uint32_t *)(uintptr_t)(ep + 0x20) == oa_g_act[i].fxev[q].id)
                        oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_g_act[i].fxev[q], 0, 0, 0, 0, 0);
                    memset(&oa_g_act[i].fxev[q], 0, sizeof oa_g_act[i].fxev[q]);
                }
            }
            {   // (the newest: the first time, those before are from before he was here; after a gap, the last few)
                uint16_t newest = 0;
                for (int q = 0; q < ONLINE_FX_EVENTS; q++) {
                    uint16_t s = pl[i].pose.fxev[q].seq;
                    if (s && (!newest || (uint16_t)(s - newest) < 0x8000u)) newest = s;
                }
                if (newest && (!oa_g_act[i].fxev_seen || oa_g_act[i].frames < 60)) oa_g_act[i].fxev_seen = newest;
                else if (newest && (uint16_t)(newest - oa_g_act[i].fxev_seen) > ONLINE_FX_EVENTS &&
                         (uint16_t)(newest - oa_g_act[i].fxev_seen) < 0x8000u)
                    oa_g_act[i].fxev_seen = (uint16_t)(newest - ONLINE_FX_EVENTS);
            }
            for (int pass = 0; pass < ONLINE_FX_EVENTS; pass++) {  // (the oldest new one first)
                const OnlineFxEvent *best = NULL;
                for (int q = 0; q < ONLINE_FX_EVENTS; q++) {
                    const OnlineFxEvent *e = &pl[i].pose.fxev[q];
                    uint16_t d = (uint16_t)(e->seq - oa_g_act[i].fxev_seen);
                    if (!e->seq || !e->name[0] || !d || d > ONLINE_FX_EVENTS) continue;
                    if (!best || (uint16_t)(e->seq - best->seq) > 0x8000u) best = e;
                }
                if (!best) break;
                oa_g_act[i].fxev_seen = best->seq;
                if (best->who == 4) {  // (the ground's, on this game's ReactionField where his puppet is, left to end)
                    uint64_t rf = oa_g_rf;
                    if (!VALID(rf) || !oa_alive(rf, oa_g_rf_watch) || !VALID(oa_g_act[i].link) || !oa_running(oa_g_act[i].link)) continue;
                    uint64_t rui = oa_elink_ui(rf), rpv = VALID(rui) ? *(uint64_t *)(uintptr_t)(rui + 0x70) : 0;
                    if (VALID(rpv)) memcpy((void *)(uintptr_t)rpv, pl[i].pose.ground_props, sizeof pl[i].pose.ground_props);
                    struct { uint64_t p; uint32_t id, pad; } hh[2], h;
                    memset(hh, 0, sizeof hh);  // (the game's keyword emit, its ELink alone: w2 0, the handle at +0)
                    oa_call(c, FN_XLINK_EMIT, rf, (uint64_t)(uintptr_t)best->name, 0, (uint64_t)(uintptr_t)hh, 0, 0);
                    h = hh[0];
                    if (!VALID(h.p)) oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&h, rf, (uint64_t)(uintptr_t)best->name);
                    if (VALID(h.p) && *(uint32_t *)(uintptr_t)(h.p + 0x20) == h.id) {
                        // (as the game places them: a footprint by its matrix at +0xc0 (+0x180 bit 3), its scale at
                        // +0xb4; the rest at a point, +0xa8, with the ground's normal at +0xb4 (+0x180 bit 2))
                        const float *lm = (const float *)(uintptr_t)(oa_g_act[i].link + 0x398);
                        if (!strcmp(best->name, "Footprint")) {
                            float one[3] = {1, 1, 1};
                            memcpy((void *)(uintptr_t)(h.p + 0xc0), lm, 48);
                            memcpy((void *)(uintptr_t)(h.p + 0xb4), one, sizeof one);
                            *(uint32_t *)(uintptr_t)(h.p + 0x180) |= 8;
                        } else {
                            float at[6] = {lm[3], lm[7], lm[11], 0, 1, 0};
                            memcpy((void *)(uintptr_t)(h.p + 0xa8), at, sizeof at);
                            *(uint32_t *)(uintptr_t)(h.p + 0x180) |= 4;
                        }
                    }
                    if (getenv("SAVAGEWIND_ONLINE_FX_LOG"))
                        LOG("[online] %s: %s on the ground (%llx; %u %u, %.0f m away)\n", pl[i].name, best->name,
                            (unsigned long long)h.p, pl[i].pose.ground_props[0], pl[i].pose.ground_props[1],
                            sqrtf((pl[i].pos[0] - link[0]) * (pl[i].pos[0] - link[0]) + (pl[i].pos[2] - link[2]) * (pl[i].pos[2] - link[2])));
                    continue;
                }
                uint64_t on = best->who & 1 ? (held ? w : 0) : oa_g_act[i].link;
                if (!VALID(on)) continue;
                // (a sound: on its SLink, the player's / the weapon's: 0x700159f00c, the game's own keyword emit, w2 1 its
                // SLink alone, x3 the two handles: the SLink's at +0x10)
                if (best->who >= 2 && !VALID(oa_slink_ui(on))) continue;
                // (the same one still playing - sent again, a Champion's ability used again and again - ended first:
                // they piled up, one over the other)
                for (int o = 0; o < ONLINE_FX_EVENTS; o++) {
                    uint64_t op = oa_g_act[i].fxev[o].p;
                    if (!op || strcmp(oa_g_act[i].fxev[o].name, best->name)) continue;
                    if (VALID(op) && *(uint32_t *)(uintptr_t)(op + 0x20) == oa_g_act[i].fxev[o].id)
                        oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_g_act[i].fxev[o], 0, 0, 0, 0, 0);
                    memset(&oa_g_act[i].fxev[o], 0, sizeof oa_g_act[i].fxev[o]);
                }
                int q = 0;
                while (q < ONLINE_FX_EVENTS - 1 && oa_g_act[i].fxev[q].p) q++;
                if (oa_g_act[i].fxev[q].p) {
                    uint64_t ep = oa_g_act[i].fxev[q].p;
                    if (VALID(ep) && *(uint32_t *)(uintptr_t)(ep + 0x20) == oa_g_act[i].fxev[q].id)
                        oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_g_act[i].fxev[q], 0, 0, 0, 0, 0);
                }
                memset(&oa_g_act[i].fxev[q], 0, sizeof oa_g_act[i].fxev[q]);
                if (best->who >= 2) {
                    struct { uint64_t p; uint32_t id, pad; } hh[2];
                    memset(hh, 0, sizeof hh);
                    oa_call(c, FN_XLINK_EMIT, on, (uint64_t)(uintptr_t)best->name, 1, (uint64_t)(uintptr_t)hh, 0, 0);
                    oa_g_act[i].fxev[q].p = hh[1].p, oa_g_act[i].fxev[q].id = hh[1].id;
                } else
                    oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_g_act[i].fxev[q], on, (uint64_t)(uintptr_t)best->name);
                oa_g_act[i].fxev[q].t = now;
                snprintf(oa_g_act[i].fxev[q].name, sizeof oa_g_act[i].fxev[q].name, "%s", best->name);
                if (getenv("SAVAGEWIND_ONLINE_FX_LOG")) {
                    static const char *const kinds[4] = {"Link", "weapon", "Link (sound)", "weapon (sound)"};
                    LOG("[online] %s: %s on his %s (%llx)\n", pl[i].name, best->name, kinds[best->who & 3],
                        (unsigned long long)oa_g_act[i].fxev[q].p);
                }
            }
            // the attack it is the trail of, for SwordBlur to find on his puppet (rt_blur_anim / rt_blur_frame)
            for (int k = 0; k < 3; k++) {
                oa_g_blur[i].anim[k] = want[0] && held ? pl[i].pose.fx_anim[k] : -1;
                oa_g_blur[i].frame[k] = pl[i].pose.fx_frame[k];
            }
            oa_g_blur[i].actor = oa_g_act[i].link;
        }
        {   // Revali's Gale: its charge on his Link while he charges it ("Kago_Wind", the player's own effect, as Daruk's)
            int e = pl[i].pose.valid ? pl[i].pose.effects : 0;
            uint64_t gp = oa_g_act[i].gale.p;
            bool on = VALID(gp) && *(uint32_t *)(uintptr_t)(gp + 0x20) == oa_g_act[i].gale.id;
            if (on && !(e & ONLINE_E_GALE)) {
                oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_g_act[i].gale, 0, 0, 0, 0, 0);
                memset(&oa_g_act[i].gale, 0, sizeof oa_g_act[i].gale);
            } else if (!on && (e & ONLINE_E_GALE)) {
                oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_g_act[i].gale, oa_g_act[i].link, (uint64_t)(uintptr_t)"Kago_Wind");
                LOG("[online] %s: Kago_Wind (%llx)\n", pl[i].name, (unsigned long long)oa_g_act[i].gale.p);
            }
        }
        // his gear, on his Link's bones as in his game (not there: under the world, out of sight)
        float bm[BONES][12];
        int have = 0;  // (bones read: a bit each)
        for (int k = 0; k < ONLINE_GEAR; k++) {
            const char *ga = pl[i].gear[k][0] ? oa_gear_actor(pl[i].gear[k]) : NULL;
            Piece *p = &oa_g_act[i].gear[k];
            oa_piece_update(c, i, p, ga ? ga : "", pl[i].name);
            if (!p->actor) continue;
            int b = pl[i].pose.valid ? pl[i].pose.gear_bone[k] - 1 : -1;
            float gm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
            if (b >= 0 && b < BONES) {
                if (!(have >> b & 1) && oa_bone_mtx(c, oa_g_act[i].link, oa_k_bones[b], bm[b])) have |= 1 << b;
                if (have >> b & 1) oa_mtx_mul(bm[b], pl[i].pose.gear_rel[k], gm);
            } else if (pl[i].pose.valid && pl[i].pose.gear_bone[k] == ONLINE_GEAR_WORLD) {
                bool finite = true;  // (where it is in his world: thrown, in flight or fallen)
                for (int q = 0; q < 12; q++) finite &= isfinite(pl[i].pose.gear_rel[k][q]);
                if (finite) memcpy(gm, pl[i].pose.gear_rel[k], sizeof gm);
                // (his weapon thrown: the real one made here from the loot list, his newest of that name, is put there
                // instead, so it can be picked up in the air; his piece goes out of sight)
                int best = -1;
                for (int t = 0; finite && k == 0 && t < THEIRS; t++) {
                    Piece *lp = &oa_g_theirs[t].p;
                    if (!oa_g_theirs[t].gid || oa_g_theirs[t].gone || oa_g_theirs[t].origin != i || !lp->actor ||
                        strcmp(lp->name, pl[i].gear[k]) || !oa_alive(lp->actor, lp->w) || oa_carries(oa_g_carried_now, lp->actor) ||
                        lp->actor == oa_g_act[i].grab)
                        continue;
                    if (best < 0 || oa_g_theirs[t].gid > oa_g_theirs[best].gid) best = t;
                }
                if (best >= 0) {  // (a real weapon has a body: moved as Magnesis moves one, its velocities to get there)
                    uint64_t la = oa_g_theirs[best].p.actor;
                    uint64_t body = oa_has_body(la) ? *(uint64_t *)(uintptr_t)(la + 0x190) : 0;
                    if (VALID(body)) oa_call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)gm, 0, 0, 0, 0);
                    else oa_call(c, FN_SET_MTX, la, (uint64_t)(uintptr_t)gm, 0, 0, 0, 0);
                    gm[7] -= 5000;
                }
            }
            oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)gm, 0, 0, 0, 0);
            static int n;
            if (getenv("SAVAGEWIND_ONLINE_GEAR_LOG") && n++ % 120 < ONLINE_GEAR)
                LOG("[online] gear %d %s bone %d (%s) at %.2f %.2f %.2f, Link %.2f %.2f %.2f, rel t %.2f %.2f %.2f\n", k,
                    p->name, b, b >= 0 && (have >> b & 1) ? "ok" : "-", gm[3], gm[7], gm[11], pl[i].pos[0], pl[i].pos[1],
                    pl[i].pos[2], pl[i].pose.gear_rel[k][3], pl[i].pose.gear_rel[k][7], pl[i].pose.gear_rel[k][11]);
        }
        {   // the horse he rides, where his is, playing the AS his plays
            const char *ha = pl[i].pose.valid && pl[i].pose.horse[0] ? oa_gear_actor(pl[i].pose.horse) : NULL;
            Piece *hp = &oa_g_act[i].horse;
            if (oa_piece_update(c, i, hp, ha ? ha : "", pl[i].name)) oa_g_act[i].horse_as[0] = 0;
            if (hp->actor) {
                bool finite = true;
                for (int q = 0; q < 12; q++) finite &= isfinite(pl[i].pose.horse_m[q]);
                if (finite) oa_call(c, FN_SET_MTX, hp->actor, (uint64_t)(uintptr_t)pl[i].pose.horse_m, 0, 0, 0, 0);
                const char *want = pl[i].pose.horse_as;
                if (want[0] && strcmp(want, oa_g_act[i].horse_as) && oa_running(hp->actor)) {
                    oa_play_as(c, hp->actor, want, 0, 0, -1.0f, true);
                    snprintf(oa_g_act[i].horse_as, sizeof oa_g_act[i].horse_as, "%s", want);
                }
            }
            for (int k = 0; k < 2; k++) {  // (its saddle and reins, where his are)
                const char *ta = pl[i].pose.valid && pl[i].pose.horse[0] && pl[i].pose.tack[k][0] ? oa_gear_actor(pl[i].pose.tack[k]) : NULL;
                Piece *tp = &oa_g_act[i].tack[k];
                oa_piece_update(c, i, tp, ta ? ta : "", pl[i].name);
                bool finite = true;
                for (int q = 0; q < 12; q++) finite &= isfinite(pl[i].pose.tack_m[k][q]);
                if (tp->actor && finite) oa_call(c, FN_SET_MTX, tp->actor, (uint64_t)(uintptr_t)pl[i].pose.tack_m[k], 0, 0, 0, 0);
            }
        }
        // the materials he holds from his pouch: between his puppet's hands (Weapon_R, Weapon_L), heaped as his game
        // heaps them over his; and each he threw or let fall, where it flies in his game, until it lies (then it is on
        // the loot list, made here as a real one)
        for (int k = 0; k < ONLINE_HELD; k++) {
            const OnlineHeld *h = &pl[i].pose.held[k];
            bool on = pl[i].pose.valid && h->mode && h->name[0];
            const char *ga = on ? oa_gear_actor(h->name) : NULL;
            Piece *p = &oa_g_act[i].held[k];
            oa_piece_update(c, i, p, ga ? ga : "", pl[i].name);
            if (!p->actor) continue;
            float gm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
            // (its size: as his game draws it in his hands, its +0x418 - a scale on each axis; 1 flying)
            float scale = h->mode == ONLINE_HELD_HANDS && isfinite(h->m[0]) && h->m[0] > 0.05f && h->m[0] < 2 ? h->m[0] : 1;
            for (int q = 0; q < 3; q++) *(float *)(uintptr_t)(p->actor + 0x418 + 4 * q) = scale;
            if (h->mode == ONLINE_HELD_WORLD) {
                bool finite = true;
                for (int q = 0; q < 12; q++) finite &= isfinite(h->m[q]);
                if (finite) memcpy(gm, h->m, sizeof gm);
            } else if (h->mode == ONLINE_HELD_HANDS) {
                for (int b = 0; b < 2; b++)
                    if (!(have >> b & 1) && oa_bone_mtx(c, oa_g_act[i].link, oa_k_bones[b], bm[b])) have |= 1 << b;
                if ((have & 3) == 3) {
                    // (a heap on his hands, as his game heaps them: three side by side, two over them, apart as far as
                    // they are big - 12 cm for apples: closer, they went into each other, a red lump)
                    static const float heap[ONLINE_HELD][3] = {{0, 0, 0.2f}, {1, 0, 0.1f}, {-1, 0, 0.1f}, {0.5f, 0.8f, 0},
                                                               {-0.5f, 0.8f, 0}};
                    float sp = 0.22f * scale;
                    float s = sinf(pl[i].pose.yaw), co = cosf(pl[i].pose.yaw);
                    gm[3] = (bm[0][3] + bm[1][3]) * 0.5f + sp * (heap[k][0] * co + heap[k][2] * s);
                    gm[7] = (bm[0][7] + bm[1][7]) * 0.5f + sp * heap[k][1];
                    gm[11] = (bm[0][11] + bm[1][11]) * 0.5f + sp * (-heap[k][0] * s + heap[k][2] * co);
                    gm[0] = co, gm[2] = s, gm[8] = -s, gm[10] = co;  // (turned as he faces)
                }
            }
            oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)gm, 0, 0, 0, 0);
        }
        // what he holds, threw or shot, where it is (out of sight: those he no longer has)
        const OnlineShots *sh = &pl[i].pose.shots;
        bool used[ONLINE_SHOTS] = {false};
        for (int k = 0; pl[i].pose.valid && k < sh->n; k++) {
            int slot = -1;
            for (int j2 = 0; j2 < ONLINE_SHOTS && slot < 0; j2++)
                if (oa_g_act[i].shot_id[j2] == sh->shot[k].id) slot = j2;
            for (int j2 = 0; j2 < ONLINE_SHOTS && slot < 0; j2++) {
                bool in_use = false;
                for (int q = 0; q < sh->n; q++) in_use |= oa_g_act[i].shot_id[j2] == sh->shot[q].id;
                if (!in_use) slot = j2;
            }
            if (slot < 0 || sh->shot[k].kind >= ONLINE_K_COUNT) continue;
            used[slot] = true;
            if (oa_g_act[i].shot_id[slot] != sh->shot[k].id)
                LOG("[online] %s's shot %d (%s) in slot %d\n", pl[i].name, sh->shot[k].id, oa_k_shot_names[sh->shot[k].kind], slot);
            oa_g_act[i].shot_id[slot] = sh->shot[k].id;
            const char *sa = oa_gear_actor(oa_k_shot_names[sh->shot[k].kind]);
            Piece *p = &oa_g_act[i].shot[slot];
            if (oa_piece_update(c, i, p, sa ? sa : "", pl[i].name)) oa_model_visible(c, p->actor, true);
            if (p->actor) oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)sh->shot[k].m, 0, 0, 0, 0);
            if (getenv("SAVAGEWIND_ONLINE_SHOT_LOG"))
                LOG("[online] shot %d %s at %.2f %.2f %.2f%s\n", sh->shot[k].id, oa_k_shot_names[sh->shot[k].kind], sh->shot[k].m[3],
                    sh->shot[k].m[7], sh->shot[k].m[11], p->actor ? "" : " (being made)");
        }
        for (int k = 0; k < ONLINE_SHOTS; k++) {
            if (used[k]) continue;
            if (oa_g_act[i].shot_id[k])
                LOG("[online] %s's shot %d out of sight (slot %d, actor %llx)\n", pl[i].name, oa_g_act[i].shot_id[k], k,
                    (unsigned long long)oa_g_act[i].shot[k].actor);
            oa_g_act[i].shot_id[k] = 0;
            if (oa_g_act[i].shot[k].actor) {
                float hm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
                oa_call(c, FN_SET_MTX, oa_g_act[i].shot[k].actor, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
            }
        }
        // an arrow on his string, in his right hand, while he nocks and draws his bow (the upper body's AS: BowReload,
        // BowCharge...; his bow in a hand), its point (+z) past the bow in his left; out of sight the rest of the time
        {
            Piece *ar = &oa_g_act[i].nock;
            const char *an = pl[i].gear[2][0] ? oa_gear_actor("NormalArrow") : NULL;
            if (oa_piece_update(c, i, ar, an ? an : "", pl[i].name)) oa_model_visible(c, ar->actor, true);
            if (ar->actor) {
                const char *up = pl[i].pose.as[2];
                bool drawn = pl[i].pose.valid && (pl[i].pose.gear_bone[2] == 1 || pl[i].pose.gear_bone[2] == 2) &&
                             (!strncmp(up, "BowCharge", 9) || !strncmp(up, "BowReload", 9));
                float am[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
                if (drawn && !(have & 1) && oa_bone_mtx(c, oa_g_act[i].link, oa_k_bones[0], bm[0])) have |= 1;
                if (drawn && !(have & 2) && oa_bone_mtx(c, oa_g_act[i].link, oa_k_bones[1], bm[1])) have |= 2;
                if (drawn && (have & 3) == 3) {
                    float z[3] = {bm[1][3] - bm[0][3], bm[1][7] - bm[0][7], bm[1][11] - bm[0][11]}, l = 0;
                    for (int k = 0; k < 3; k++) l += z[k] * z[k];
                    l = l > 1e-8f ? 1 / sqrtf(l) : 0;
                    for (int k = 0; k < 3; k++) z[k] *= l;
                    float x[3] = {z[2], 0, -z[0]}, lx = sqrtf(x[0] * x[0] + x[2] * x[2]);  // (up x z)
                    if (lx < 1e-4f) x[0] = 1, x[2] = 0, lx = 1;
                    x[0] /= lx, x[2] /= lx;
                    float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
                    float m2[12] = {x[0], y[0], z[0], bm[0][3], x[1], y[1], z[1], bm[0][7], x[2], y[2], z[2], bm[0][11]};
                    if (l > 0) memcpy(am, m2, sizeof am);
                }
                oa_call(c, FN_SET_MTX, ar->actor, (uint64_t)(uintptr_t)am, 0, 0, 0, 0);
            }
        }
        // a bomb of his gone off, once by its number: his bomb as shown goes out of sight, and the bomb's own actor
        // without what hurts (OnlineBoom_*, tools/online_mod.py) is made there and set off as the rune sets his off
        for (int k = ONLINE_BOOMS - 1; pl[i].pose.valid && k >= 0; k--) {
            uint16_t seq = sh->boom[k].seq;
            if (!seq || (uint16_t)(seq - oa_g_act[i].boom_seen) > 0x8000u || seq == oa_g_act[i].boom_seen) continue;
            oa_g_act[i].boom_seen = seq;
            if (oa_g_act[i].frames < 60 || sh->boom[k].kind >= ONLINE_K_COUNT) continue;  // (from before he was here)
            const char *ba = oa_gear_actor(oa_k_shot_names[sh->boom[k].kind]);
            for (int q = 0; ba && q < ONLINE_SHOTS; q++)
                if (!used[q] && oa_g_act[i].shot[q].actor && !strcmp(oa_g_act[i].shot[q].name, ba)) {
                    float hm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
                    oa_call(c, FN_SET_MTX, oa_g_act[i].shot[q].actor, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
                }
            char key[48];
            snprintf(key, sizeof key, "Boom_%s", oa_k_shot_names[sh->boom[k].kind]);
            const char *boom = getenv("SAVAGEWIND_BOOM_ACTOR") ? getenv("SAVAGEWIND_BOOM_ACTOR") : oa_gear_actor(key);
            if (boom) oa_boom_start(c, boom, sh->boom[k].pos);
            for (int q = 0; q < 4; q++)  // (its blast on our Link, with damage between players on: pvp_update)
                if (!oa_g_blast[q].fresh) {
                    memcpy(oa_g_blast[q].pos, sh->boom[k].pos, sizeof oa_g_blast[q].pos);
                    oa_g_blast[q].from = i, oa_g_blast[q].fresh = true;
                    break;
                }
            LOG("[online] %s's bomb went off at %.1f %.1f %.1f\n", pl[i].name, sh->boom[k].pos[0], sh->boom[k].pos[1],
                sh->boom[k].pos[2]);
        }
    }
}
