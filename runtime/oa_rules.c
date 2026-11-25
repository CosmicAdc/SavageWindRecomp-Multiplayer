// Online play's actors, oa_rules: the session's rules: players colliding, PvP, the fairy, menus that don't pause, A
// held back. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"

volatile int g_nopause_now;  // (a menu is up and the world goes on: rt_scene_state says playing)

// Our own enemies while our menu is up and the world goes on for it (menus without pause): their blows don't reach the
// local Link then (he is the menu's, his own hits aren't taken), so each one that starts an attack lands as the host's
// enemies' do here (strike: within its reach in front of it, his shield not up), STRIKE_FRAMES later.
// SAVAGEWIND_ONLINE_MENU_LOG=1 logs their AS as they change
enum { MENU_LIVE_AFTER = 4 };  // (frames: rt_online_menu_live)
volatile int g_nopause_frames;  // (frames the menu has been up with the world going on)
void own_strikes_in_menu(Cpu *c, uint64_t player) {
    static char last[OWNS][32];
    static int wait[OWNS], arrow[OWNS];
    static float arrow_d[OWNS];
    static bool shot[OWNS];
    static int log = -1;
    if (log < 0) log = getenv("SAVAGEWIND_ONLINE_MENU_LOG") != NULL;
    for (int k = 0; k < OWNS; k++) {
        Own *o = &g_own[k];
        if (!g_nopause_now || o->state != OWN_HAVE || !alive(o->actor, o->w)) { last[k][0] = 0, wait[k] = arrow[k] = 0; continue; }
        OnlineEnemy e;
        memset(&e, 0, sizeof e);
        enemy_state(c, o->actor, &e);
        if (e.life <= 0) continue;
        if (strcmp(e.as, last[k])) {
            if (log) LOG("[online] menu: our %s %u: %s\n", e.name, o->gid, e.as);
            if (strstr(e.as, "Attack") || strstr(e.as, "Kick")) wait[k] = STRIKE_FRAMES;
            snprintf(last[k], sizeof last[k], "%s", e.as);
        }
        if (wait[k] > 0 && --wait[k] == 0) strike(c, player, &e);
        // (an archer's arrow: shot at him (ArrowShoot) facing him within 40 m, it lands as the arrow would, at 40 m/s,
        // its bow's attack power less his armour's defence; his shield up takes it)
        const float *pm = (const float *)(uintptr_t)(player + 0x398);
        float dx = pm[3] - e.m[3], dz = pm[11] - e.m[11], d = sqrtf(dx * dx + dz * dz);
        float fx = e.m[2], fz = e.m[10], fl = sqrtf(fx * fx + fz * fz);
        if (!strcmp(e.as, "ArrowShoot") && !arrow[k] && d < 40 && d > 0.5f && fl > 0 && (dx * fx + dz * fz) / (d * fl) > 0.9f)
            arrow[k] = 1 + (int)(d / 40.0f * 60.0f), arrow_d[k] = d;
        if (strcmp(e.as, "ArrowShoot")) shot[k] = false;
        if (arrow[k] > 0 && --arrow[k] == 0 && !shot[k]) {
            shot[k] = true;
            int32_t *life = (int32_t *)(uintptr_t)(player + ENEMY_LIFE);
            const char *up = playing(c, player, 1, 0), *body = playing(c, player, 0, 0);
            int power = e.weapon[0] ? attack_power(e.weapon) : 0;
            if (power <= 0) power = 4;
            if (strstr(up, "Guard") || strstr(body, "Guard")) LOG("[online] %s's arrow: blocked\n", e.name);
            else if (*life > 0) {
                int def = our_defense(player), dmg = power > def ? power - def : 1;
                *life = *life > dmg ? *life - dmg : 0;
                LOG("[online] %s's arrow hit us from %.0f m: %d (life %d)\n", e.name, arrow_d[k], dmg, *life);
            }
        }
    }
}

// Damage between players (OnlineSettings.pvp, both players with it on): the puppets have no bodies a blow lands on, so
// as his swing starts (his weapon's trail, fx_update) the nearest other player within its reach in front of him is hit:
// the blow goes in his state (pvp_*: that player's slot, his weapon's attack power, by its number), and that player's
// game takes it as the host's enemies' blows (strike): his armour's defence off, a quarter heart at least, he flinches
// The players bump into each other: the puppets have no body the local Link stands against, so each game pushes its own
// Link out of the others' (a capsule each: 0.35 m wide, as tall as Link), on foot, where his puppet is shown. The push
// is written to Link's real position (control_cell_player_pos, what `teleport` writes), all of the overlap at once, so
// two players walking into each other stop face to face; each game pushes its own, so it is the same both ways.
// The host chooses (Settings > Online > Combat, `collide` in online.ini, on by default): his state carries it, and the
// others follow it. SAVAGEWIND_ONLINE_NO_COLLIDE=1: none
#define COLLIDE_R 0.7f    // (between their centres)
#define COLLIDE_H 1.7f    // (heights closer than this: the same level)
// A player who dies in online play: the game asks whether he has a fairy (tools/patches.py _has_fairy); while in a
// session whose host has it on (Settings > Online > Combat, `revive`), he has: a fairy revives him where he fell, his
// hearts back, and his world isn't loaded again (game over would load his last save, away from the others). One of his
// own is used first, as always
static volatile int g_revive_rule = -1;  // (the session's: the host's choice, set each frame; -1 not in a session)
volatile int g_nopause_rule = -1;  // (menus_no_pause: the same)
uint32_t rt_scene_state(uint32_t v) { return v == 1 && g_nopause_now ? 0 : v; }
// (the Vulkan backend, runtime/nvn/vulkan/exec.c stale_capture_check: a menu is up and the world goes on, so the
// menu's still picture of it is left out and the world drawn live shows. SAVAGEWIND_ONLINE_MENU_STILL: not)
// Not in its first frames: the world's camera is turned on again a frame after the menu opens (the frame hook), so
// until then the game's own picture, made as it opened, stays (made again from a frame without the world: a flash)
int rt_online_menu_live(void) {
    if (!online_version_ok(g_game_version)) return 0;
    static int still = -1;
    if (still < 0) still = getenv("SAVAGEWIND_ONLINE_MENU_STILL") != NULL;
    return g_nopause_now && g_nopause_frames > MENU_LIVE_AFTER && !still;
}
// (the menus' frame asking the world's drawing paused, 1.9.0 0x7000d45d84: 1 as the game asks it, a still picture of
// the world behind the menu; 0 while they don't pause the world, so it is drawn going on behind them. Its own
// reading of the menu's state, not g_nopause_now: that is set later in the frame, after the menu has opened.
// SAVAGEWIND_ONLINE_MENU_STILL: the still picture)
uint32_t rt_menu_draw_pause(void) {
    if (!online_version_ok(g_game_version)) return 1;
    static int still = -1;
    if (still < 0) still = getenv("SAVAGEWIND_ONLINE_MENU_STILL") != NULL;
    if (still || online_state() != ONLINE_CONNECTED || !g_site.pause_state) return 1;
    uint32_t ps = *(uint32_t *)(uintptr_t)g_site.pause_state;
    if (!ps || (ps & UI_PAUSE_LOAD)) return 1;
    int rule = g_nopause_rule < 0 ? g_online.nopause : g_nopause_rule;
    OnlinePlayer np[ONLINE_MAX];
    online_players(np);
    int others = 0, me = online_local_id();
    for (int i = 0; i < ONLINE_MAX; i++) others += np[i].used && i != me;
    return rule && others ? 0 : 1;
}
uint32_t rt_online_has_fairy(uint32_t had) {
    if (!online_version_ok(g_game_version)) return had;
    if (had || online_state() != ONLINE_CONNECTED) return had;
    int rule = g_revive_rule;
    if (rule < 0) rule = g_online.revive;
    if (rule) LOG("[online] Link died: revived by a fairy (online play)\n");
    return rule ? 1 : 0;
}

void players_collide(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me) {
    static int off = -1;
    if (off < 0) off = getenv("SAVAGEWIND_ONLINE_NO_COLLIDE") != NULL;
    extern bool framepace_in_event(void);
    if (off || !VALID(player) || me < 0 || framepace_in_event()) return;
    g_revive_rule = online_is_host() || !pl[0].used || !pl[0].pose.valid ? g_online.revive : pl[0].pose.revive != 0;
    g_nopause_rule = online_is_host() || !pl[0].used || !pl[0].pose.valid ? g_online.nopause : pl[0].pose.nopause != 0;
    bool on = online_is_host() || !pl[0].used || !pl[0].pose.valid ? g_online.collide : pl[0].pose.collide != 0;
    if (!on) return;
    const char *body = playing(c, player, 0, 0);
    if (strstr(body, "Horse") || strstr(body, "Motorcycle") || !strncmp(body, "Dead", 4)) return;
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    float push[2] = {0, 0};
    for (int i = 0; i < ONLINE_MAX; i++) {
        const OnlinePlayer *p = &pl[i];
        if (!p->used || i == me || (p->flags & ONLINE_F_LOADING) || !p->pose.valid || !same_map(p) ||
            p->pose.horse[0] || !strncmp(p->pose.as[0], "Dead", 4))
            continue;
        float at[3] = {p->pos[0], p->pos[1], p->pos[2]};
        uint64_t pa = g_act[i].link;  // (where his puppet is shown)
        if (pa && running(pa)) {
            const float *am = (const float *)(uintptr_t)(pa + 0x398);
            at[0] = am[3], at[1] = am[7], at[2] = am[11];
        }
        float dx = pm[3] - at[0], dz = pm[11] - at[2], dy = pm[7] - at[1];
        if (dy > COLLIDE_H || dy < -COLLIDE_H) continue;
        float d = sqrtf(dx * dx + dz * dz);
        if (d >= COLLIDE_R) continue;
        if (d < 0.001f) dx = me < i ? 1.0f : -1.0f, dz = 0, d = 1.0f;  // (right on him: apart, each his own way)
        float in = COLLIDE_R - d;
        push[0] += dx / d * in, push[1] += dz / d * in;
    }
    float n = sqrtf(push[0] * push[0] + push[1] * push[1]);
    if (n < 0.001f) return;
    if (n > 0.5f) push[0] *= 0.5f / n, push[1] *= 0.5f / n;  // (all of it: less, and a runner got past the middle)
    uint64_t a = control_cell_addr(&control_cell_player_pos);
    if (!a) return;
    float *rp = (float *)(uintptr_t)a;
    rp[0] += push[0], rp[2] += push[1];
}

void pvp_update(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me) {
    static uint64_t swing_seen;
    static uint16_t seen[ONLINE_MAX];
    static bool init[ONLINE_MAX];
    if (!VALID(player) || me < 0) return;
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    // his blow
    if (g_online.pvp && g_fx_trail_t != swing_seen) {
        swing_seen = g_fx_trail_t;
        uint64_t armor[ARMOR], gear[ONLINE_GEAR];
        equipment(player, armor, gear);
        char wn[48];
        actor_name_of(gear[0], wn);
        int power = wn[0] ? attack_power(wn) : 0;
        float reach = 2.2f + (strstr(wn, "Spear") || strstr(wn, "Lsword") ? 1.5f : 0.8f);
        float fx = pm[2], fz = pm[10], fl = sqrtf(fx * fx + fz * fz);
        int best = -1;
        float bd = reach;
        for (int i = 0; power > 0 && i < ONLINE_MAX; i++) {
            if (i == me || !pl[i].used || !pl[i].pose.valid || (pl[i].flags & ONLINE_F_LOADING) || !same_map(&pl[i])) continue;
            float dx = pl[i].pos[0] - pm[3], dy = pl[i].pos[1] - pm[7], dz = pl[i].pos[2] - pm[11];
            float d = sqrtf(dx * dx + dz * dz);
            if (d > bd || dy > 2.0f || dy < -2.0f) continue;
            if (d > 0.6f && fl > 0 && (dx * fx + dz * fz) / (d * fl) < 0.3f) continue;  // (behind him or to his side)
            best = i, bd = d;
        }
        if (best >= 0) {
            if (!++g_pvp.seq) g_pvp.seq = 1;
            g_pvp.target = (uint8_t)best, g_pvp.damage = (int16_t)power;
            LOG("[online] our blow on %s: %d\n", pl[best].name, power);
        }
    }
    // the others' blows on him
    int32_t *life = (int32_t *)(uintptr_t)(player + ENEMY_LIFE);
    for (int i = 0; i < ONLINE_MAX; i++) {
        if (i == me || !pl[i].used || !pl[i].pose.valid) { init[i] = false; continue; }
        uint16_t s = pl[i].pose.pvp_seq;
        if (!init[i]) { seen[i] = s, init[i] = true; continue; }  // (from before he was here)
        if (s == seen[i]) continue;
        seen[i] = s;
        if (!g_online.pvp || pl[i].pose.pvp_target != me || pl[i].pose.pvp_damage <= 0 || *life <= 0) continue;
        const char *up = playing(c, player, 1, 0), *body = playing(c, player, 0, 0);
        if (strstr(up, "Guard") || strstr(body, "Guard")) {
            LOG("[online] %s's blow: blocked\n", pl[i].name);
            continue;
        }
        int def = our_defense(player), dmg = pl[i].pose.pvp_damage > def ? pl[i].pose.pvp_damage - def : 1;
        *life = *life > dmg ? *life - dmg : 0;
        float dx = pm[3] - pl[i].pos[0], dz = pm[11] - pl[i].pos[2], d = sqrtf(dx * dx + dz * dz);
        if (*life > 0 && d > 0.01f) {
            float at[3] = {pm[3] - dx / d, pm[7] + 0.3f, pm[11] - dz / d};
            boom_start(c, "OnlineShock", at);  // (he flinches, as from the host's enemies' heavy blows)
        }
        LOG("[online] %s hit us: %d after our defence %d (life %d)\n", pl[i].name, dmg, def, *life);
    }
    // another player's bomb gone off within 4 m of our Link, with damage between players on: two hearts less our
    // armour's defence, and its blast knocks him down (OnlineKnock: the game's own, which takes a quarter more)
    for (int q = 0; q < 4; q++) {
        if (!g_blast[q].fresh) continue;
        g_blast[q].fresh = false;
        float dx = pm[3] - g_blast[q].pos[0], dy = pm[7] - g_blast[q].pos[1], dz = pm[11] - g_blast[q].pos[2];
        if (!g_online.pvp || *life <= 0 || dx * dx + dy * dy + dz * dz > 4.0f * 4.0f) continue;
        int def = our_defense(player), dmg = 8 > def ? 8 - def : 1;
        *life = *life > dmg ? *life - dmg : 0;
        if (*life > 0) boom_start(c, "OnlineKnock", g_blast[q].pos);
        LOG("[online] %s's bomb hit us: %d after our defence %d (life %d)\n",
            g_blast[q].from >= 0 && g_blast[q].from < ONLINE_MAX ? pl[g_blast[q].from].name : "?", dmg, def, *life);
    }
}

volatile int g_online_block_a;  // (runtime/hle/input.c: A held back)
volatile uint64_t g_game_buttons;  // (runtime/hle/input.c: the buttons the game is given, as it reads them)
