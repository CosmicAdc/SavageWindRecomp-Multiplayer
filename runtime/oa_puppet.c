// Online play's actors, oa_puppet: the remote players' Links here: posed and animated, their armour, their blasts, the
// blur. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"

static void animate(Cpu *c, int i, const OnlinePlayer *p);

// A remote player in the world: his Link, his armour and his gear (his runes: oa_runes' g_rune, g_ice)
static OaPup g_pup[ONLINE_MAX];

// player i's puppet state (oa_puppet_frame, the loop over the players, and rt_online_frame's steps 3 and 4 work on it)
OaPup *oa_pup(int i) { return &g_pup[i]; }

// player i's Link here, if the game still has it (0: none, or deleted by the game since it was made). Other modules
// reach a remote Link through this (or oa_link_live, the same)
uint64_t oa_puppet_link(int i) { return oa_link_live(i); }

// player i's name, as his Link was made with
const char *oa_puppet_name(int i) { return g_pup[i].name; }

// player i's puppet pieces, each to each(c, i, piece), in this order (oa_drop, forget_all: before his runes')
void oa_puppet_pieces(Cpu *c, int i, void (*each)(Cpu *c, int i, Piece *p)) {
    for (int k = 0; k < ARMOR; k++) each(c, i, &g_pup[i].armor[k]);
    for (int k = 0; k < ONLINE_GEAR; k++) each(c, i, &g_pup[i].gear[k]);
    each(c, i, &g_pup[i].horse);
    each(c, i, &g_pup[i].tack[0]), each(c, i, &g_pup[i].tack[1]);
    for (int k = 0; k < ONLINE_SHOTS; k++) each(c, i, &g_pup[i].shot[k]);
    for (int k = 0; k < ONLINE_HELD; k++) each(c, i, &g_pup[i].held[k]);
    each(c, i, &g_pup[i].nock);
}

// player i gone (oa_drop): his Link deleted and his puppet forgotten (his pieces already handed on with
// oa_puppet_pieces)
void oa_puppet_drop(Cpu *c, int i) {
    if (oa_alive(g_pup[i].link, g_pup[i].link_w)) oa_call(c, FN_DELETE, g_pup[i].link, 0, 0, 0, 0, 0);
    LOG("[online] removed %s's Link\n", g_pup[i].name);
    memset(&g_pup[i], 0, sizeof g_pup[i]);
    g_pup[i].anim = -1, g_pup[i].covered = -1;
}

// a load (forget_all): every player's puppet forgotten (their pieces already handed on with oa_puppet_pieces)
void oa_puppet_forget(void) {
    memset(g_pup, 0, sizeof g_pup);
    for (int i = 0; i < ONLINE_MAX; i++) g_pup[i].anim = -1, g_pup[i].covered = -1;
}

// a player's Link here, if the game still has it (0: none, or deleted since the last frame - game over, a load - and
// not forgotten yet: rt_online_frame finds that later in the frame). The runes' work on it goes through this
uint64_t oa_link_live(int i) { return g_pup[i].link && oa_alive(g_pup[i].link, g_pup[i].link_w) ? g_pup[i].link : 0; }

// tools/online_mod.py ANIMS: the AS names of OnlinePlayer's one-clip animations, in ONLINE_A_* order
static const char *const k_anims[ONLINE_A_COUNT] = {
    "OnlineWait", "OnlineWalk", "OnlineRun", "OnlineDash", "OnlineJump", "OnlineFall", "OnlineGlide",
    "OnlineSwimWait", "OnlineSwim", "OnlineClimbWait", "OnlineClimb", "OnlineSquatWait", "OnlineSquat"};
OaBlast oa_g_blast[4];  // (the others' bombs gone off lately: pvp_update)

// Link's body and his armour as the player's own: each part shows the materials no other part covers (Link's skin
// under a tunic, the top of trousers under a long one, his hair under a hood); all once all parts run (their models)
void oa_cover(Cpu *c, int i) {
    uint64_t x0 = *(uint64_t *)(uintptr_t)P_HIDE_GROUPS_X0;
    x0 = x0 ? *(uint64_t *)(uintptr_t)x0 : 0;
    uint64_t part[1 + ARMOR] = {g_pup[i].link};
    for (int k = 0; k < ARMOR; k++) part[1 + k] = g_pup[i].armor[k].actor;
    for (int k = 0; k < 1 + ARMOR; k++) {
        uint64_t model = part[k] ? *(uint64_t *)(uintptr_t)(part[k] + 0x4e0) : 0;
        if (!model) continue;
        oa_call(c, FN_HIDE_GROUPS, x0, model, 0, 1, 0, 0);
        for (int j = 0; j < 1 + ARMOR; j++) {
            uint64_t other = j != k && part[j] ? *(uint64_t *)(uintptr_t)(part[j] + 0x4e0) : 0;
            if (other) oa_call(c, FN_HIDE_GROUPS, x0, model, other, 0, 0, 0);
        }
    }
}

OaBlur volatile oa_g_blur[ONLINE_MAX];  // (each puppet's)

uint32_t rt_blur_anim(uint64_t actor, uint32_t slot, uint32_t id) {
    if (!online_version_ok(g_game_version)) return id;
    for (int i = 0; actor && i < ONLINE_MAX; i++)
        if (oa_g_blur[i].actor == actor) return slot < 3 && oa_g_blur[i].anim[slot] >= 0 ? (uint32_t)oa_g_blur[i].anim[slot] << 16 : id;
    return id;
}

float rt_blur_frame(uint64_t actor, uint32_t slot, float frame) {
    if (!online_version_ok(g_game_version)) return frame;
    for (int i = 0; actor && i < ONLINE_MAX; i++)
        if (oa_g_blur[i].actor == actor) return slot < 3 && oa_g_blur[i].anim[slot] >= 0 ? oa_g_blur[i].frame[slot] : frame;
    return frame;
}

#define BOOMS_MAX 8
static struct { Handle *h; uint64_t actor; Watch w; float pos[3]; int frames; bool used, quiet; } g_boom[BOOMS_MAX];  // (quiet: OnlineKnock, unseen)

void oa_boom_start(Cpu *c, const char *name, const float pos[3]) {
    for (int k = 0; k < BOOMS_MAX; k++) {
        if (g_boom[k].used) continue;
        memset(&g_boom[k], 0, sizeof g_boom[k]);
        g_boom[k].h = oa_new_handle();
        if (!oa_request(c, name, g_boom[k].h)) return;
        g_boom[k].used = true;
        g_boom[k].quiet = !strcmp(name, "OnlineKnock") || !strncmp(name, "OnlineShock", 11);
        memcpy(g_boom[k].pos, pos, sizeof g_boom[k].pos);
        return;
    }
}

void oa_booms_forget(void) { memset(g_boom, 0, sizeof g_boom); }

void oa_booms_update(Cpu *c, uint64_t player) {
    uint64_t mgr = VALID(player) ? *(uint64_t *)(uintptr_t)(player + 0x2500) : 0;
    for (int k = 0; k < BOOMS_MAX; k++) {
        if (!g_boom[k].used) continue;
        if (!g_boom[k].actor) {
            int *unit = (int *)(uintptr_t)g_boom[k].h->unit;
            if (!unit) continue;
            if (*unit == 3 || *unit == 5) {
                oa_call(c, FN_HANDLE_RELEASE, (uint64_t)(uintptr_t)g_boom[k].h, 0, 0, 0, 0, 0);
                g_boom[k].used = false;
                continue;
            }
            if (*unit != 2) continue;
            g_boom[k].actor = oa_call(c, FN_HANDLE_TAKE, (uint64_t)(uintptr_t)g_boom[k].h, 0, 0, 0, 0, 0);
            if (!g_boom[k].actor) { g_boom[k].used = false; continue; }
            g_boom[k].w = oa_watch(g_boom[k].actor);
            // as the rune does with its bombs (0x7000b3ba04): its xlink holder (+0x558) +0x148 = 5 (without, its
            // blast is only a small ring)
            uint64_t xl = *(uint64_t *)(uintptr_t)(g_boom[k].actor + 0x558);
            if (VALID(xl)) *(uint32_t *)(uintptr_t)(xl + 0x148) = 5;
            float m[12] = {1, 0, 0, g_boom[k].pos[0], 0, 1, 0, g_boom[k].pos[1], 0, 0, 1, g_boom[k].pos[2]};
            oa_call(c, FN_SET_MTX, g_boom[k].actor, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
            if (g_boom[k].quiet) oa_model_visible(c, g_boom[k].actor, false);
        }
        if (!oa_alive(g_boom[k].actor, g_boom[k].w)) {  // (gone off: deleted)
            if (getenv("SAVAGEWIND_BOOM_LOG")) LOG("[online] boom gone after %d frames\n", g_boom[k].frames);
            g_boom[k].used = false;
            continue;
        }
        int f = g_boom[k].frames++;
        if (getenv("SAVAGEWIND_BOOM_LOG")) {
            const float *m = (const float *)(uintptr_t)(g_boom[k].actor + 0x398);
            LOG("[online] boom f%d st %d AS %s at %.2f %.2f %.2f scale %.2f\n", f, *(uint8_t *)(uintptr_t)(g_boom[k].actor + 0x64),
                oa_playing(c, g_boom[k].actor, 0, 0), m[3], m[7], m[11], m[0]);
        }
        if (f > 600) {  // (never went off)
            oa_call(c, FN_DELETE, g_boom[k].actor, 0, 0, 0, 0, 0);
            g_boom[k].used = false;
            continue;
        }
        if (VALID(mgr) && oa_running(g_boom[k].actor) && f % 15 == 1) {
            uint64_t rx = *(uint64_t *)(uintptr_t)(g_boom[k].actor + 0x5f0);
            uint64_t msg = 0x800000a;
            oa_call(c, FN_SEND_MSG, mgr + 0xd8, rx, (uint64_t)(uintptr_t)&msg, 0, 1, 0);
            if (f == 1 || getenv("SAVAGEWIND_ONLINE_SHOT_LOG")) LOG("[online] boom %llx: go off (frame %d)\n", (unsigned long long)g_boom[k].actor, f);
        }
    }
}

// His Link posed: his player's skeleton, bone by bone, written into his Link's own (it plays no AS, so nothing
// writes over it before the game makes his bones' matrices in the world from it); while none has come, the AS
// his Link plays (online.h OnlinePose: an older game, the test bot)
static float g_pose_q[ONLINE_BONES][4], g_pose_t[ONLINE_BONES][3];  // (the skeleton pose_link posed his Link by)
static int g_pose_n;  // (its Link bones; 0: none this frame)

void oa_pose_link(Cpu *c, int i, const OnlinePlayer *p) {
    float (*q)[4] = g_pose_q, (*t)[3] = g_pose_t;
    int n = 0, nbow = 0, nb = 0, nbb = 0;
    g_pup[i].frames++;
    g_pose_n = 0;
    // (none lately: paused, in a menu - the last one he sent kept; his armour posed by it too)
    static struct { float q[ONLINE_BONES][4], t[ONLINE_BONES][3]; int n, nbow; } last[ONLINE_MAX];
    bool fresh = online_skeleton(i, &n, &nbow, q, t);
    if (fresh) memcpy(last[i].q, q, sizeof last[i].q), memcpy(last[i].t, t, sizeof last[i].t), last[i].n = n, last[i].nbow = nbow;
    else if (g_pup[i].skeleton && last[i].n)
        memcpy(q, last[i].q, sizeof last[i].q), memcpy(t, last[i].t, sizeof last[i].t), n = last[i].n, nbow = last[i].nbow;
    if (fresh || (g_pup[i].skeleton && last[i].n)) {
        g_pose_n = n;
        uint64_t unit = oa_skel_unit(c, g_pup[i].link, &nb);
        // his bow (its string drawn) when the one he carries is the one posed
        Piece *bp = &g_pup[i].gear[2];
        uint64_t bow_unit = nbow && oa_alive(bp->actor, bp->w) ? oa_first_unit(bp->actor, &nbb) : 0;
        g_pup[i].skeleton = true;
        for (int b = 0; b < n + nbow; b++) {
            uint64_t u = b < n ? unit : bow_unit;
            int k = b < n ? b : b - n;
            if (!u || k >= (b < n ? nb : nbb)) continue;
            float m[12], sc[3] = {1, 1, 1};
            oa_quat_to_mtx(q[b], m);
            m[3] = t[b][0], m[7] = t[b][1], m[11] = t[b][2];
            oa_call(c, FN_BONE_LOCAL_SET, u, (uint64_t)(uintptr_t)m, (uint64_t)(uintptr_t)sc, k, 0, 0);
        }
        return;
    }
    if (!g_pup[i].skeleton && g_pup[i].frames > 60) animate(c, i, p);  // (a second for his skeleton to come first)
    // (his armour posed as his Link stands then: his own bones, as his AS left them)
    uint64_t unit = oa_skel_unit(c, g_pup[i].link, &nb);
    if (!unit || nb <= 0) return;
    if (nb > ONLINE_BONES) nb = ONLINE_BONES;
    for (int b = 0; b < nb; b++) {
        float m[12], sc[3];
        oa_call(c, FN_BONE_LOCAL_GET, unit, (uint64_t)(uintptr_t)m, (uint64_t)(uintptr_t)sc, b, 0, 0);
        oa_mtx_to_quat(m, q[b]);
        t[b][0] = m[3], t[b][1] = m[7], t[b][2] = m[11];
    }
    g_pose_n = nb;
}

// an armour piece posed as his Link: each of its bones Link has, as Link's (its cloth and support bones follow them)
void oa_pose_armor(Cpu *c, const Piece *p) {
    if (!g_pose_n) return;
    uint64_t model = *(uint64_t *)(uintptr_t)(p->actor + 0x4e0);
    if (!VALID(model)) return;
    int nu = *(int *)(uintptr_t)(model + 0x38);
    uint64_t units = *(uint64_t *)(uintptr_t)(model + 0x40);
    for (int ui = 0; VALID(units) && ui < nu && ui < 8; ui++) {
        uint64_t unit = *(uint64_t *)(uintptr_t)(units + 8 * ui);
        unit = VALID(unit) ? *(uint64_t *)(uintptr_t)unit : 0;
        uint64_t sk = VALID(unit) ? *(uint64_t *)(uintptr_t)(unit + 0x118) : 0;
        if (!VALID(sk)) continue;
        int nb = *(uint16_t *)(uintptr_t)(sk + 0x50), len = 0;
        const uint8_t *map = oa_armor_bones(p->name, ui, &len);
        for (int b = 0; map && b < len && b < nb; b++) {
            int li = map[b];
            if (li == 255 || li >= g_pose_n) continue;
            float m[12], sc[3] = {1, 1, 1};
            oa_quat_to_mtx(g_pose_q[li], m);
            m[3] = g_pose_t[li][0], m[7] = g_pose_t[li][1], m[11] = g_pose_t[li][2];
            oa_call(c, FN_BONE_LOCAL_SET, unit, (uint64_t)(uintptr_t)m, (uint64_t)(uintptr_t)sc, b, 0, 0);
        }
    }
}

// the remote Link plays what his player's Link plays: the player's own AS on each channel (a channel that went
// quiet keeps its last one: its bank is played over by the others), or without a pose the one-clip AS of anim
static void animate(Cpu *c, int i, const OnlinePlayer *p) {
    uint64_t link = g_pup[i].link;
    if (!p->pose.valid) {
        if (p->anim < ONLINE_A_COUNT && p->anim != g_pup[i].anim && oa_play_as(c, link, k_anims[p->anim], 0, 0, 6.0f, true))
            g_pup[i].anim = p->anim;
        return;
    }
    for (int ch = 0; ch < ONLINE_AS_CHANNELS; ch++) {
        const char *want = p->pose.as[ch];
        if (!want[0] || !strcmp(g_pup[i].as[ch], want)) continue;
        oa_play_as(c, link, want, oa_k_channel[ch][0], oa_k_channel[ch][1], -1.0f, true);
        snprintf(g_pup[i].as[ch], ONLINE_AS_MAX, "%s", want);  // (once, played or not: an AS it lacks isn't asked again)
    }
    g_pup[i].anim = -1;
}

// rt_online_frame, step 9b: a Link of theirs deleted by the game (the local Link died: game over; a load not seen by
// where he is), seen before any later step uses it. Logged as step 18 logs it (his name as his Link was made with; the
// distance step 18 last measured). True: rt_online_frame forgets everything and waits, as after step 18
bool oa_puppets_validate(void) {
    for (int i = 0; i < ONLINE_MAX; i++)
        if (g_pup[i].link && !oa_alive(g_pup[i].link, g_pup[i].link_w)) {
            LOG("[online] the game removed %s's Link: the world's actors are going, made again later (%.0f m away)\n", g_pup[i].name, oa_g_dist);
            return true;
        }
    return false;
}

// rt_online_frame, step 18: each other player's Link here, made or dropped, placed and posed, with his armour, gear,
// horse, effects, shots and bombs. True: a Link of theirs deleted by the game (logged): rt_online_frame forgets
// everything and waits
bool oa_puppet_frame(Cpu *c, const float link[3], int me) {
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
            if (oa_pup(i)->link && !framepace_paused()) oa_drop(c, i, !pl[i].used);
            else if (!pl[i].used)  // (left: his pool goes)
                for (int k = 0; k < POOL; k++)
                    if (oa_g_pool[i][k].actor || oa_g_pool[i][k].pending) oa_delete_piece(c, &oa_g_pool[i][k]);
            continue;
        }
        // another player in his slot (one left and another came before it was seen): the first one's actors go
        if (oa_pup(i)->link && strcmp(oa_pup(i)->name, pl[i].name)) {
            LOG("[online] slot %d is %s now, not %s\n", i, pl[i].name, oa_pup(i)->name);
            oa_drop(c, i, true);
            continue;
        }
        // his Link gone, deleted by the game: the world's actors are going (the local Link died: game over; a load
        // not seen by where he is): all forgotten, made again two seconds later (oa_puppets_validate saw it first, at
        // step 9b: this is the second barrier, for a Link deleted during this frame's steps)
        if (oa_pup(i)->link && !oa_alive(oa_pup(i)->link, oa_pup(i)->link_w)) {
            LOG("[online] the game removed %s's Link: the world's actors are going, made again later (%.0f m away)\n", pl[i].name, oa_g_dist);
            return true;
        }
        if (!oa_pup(i)->link) {
            if (oa_pup(i)->link_fails > 10) continue;
            oa_pup(i)->link = oa_create(c, oa_actor_name());
            if (!oa_pup(i)->link) {
                if (oa_pup(i)->link_fails++ == 0) LOG("[online] can't create a %s for %s (not loaded?)\n", oa_actor_name(), pl[i].name);
                continue;
            }
            oa_pup(i)->link_w = oa_watch(oa_pup(i)->link);
            memcpy(oa_pup(i)->name, pl[i].name, sizeof oa_pup(i)->name);
            LOG("[online] %s: %s (actor %llx)\n", pl[i].name, oa_actor_name(), (unsigned long long)oa_pup(i)->link);
        }
        // the matrix: turned about y by the heading (forward is +z), at the player's feet
        float s = sinf(pl[i].yaw), co = cosf(pl[i].yaw);
        float m[12] = {co, 0, s, pl[i].pos[0], 0, 1, 0, pl[i].pos[1], -s, 0, co, pl[i].pos[2]};
        oa_call(c, FN_SET_MTX, oa_pup(i)->link, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
        if (!oa_is_online_player() || !oa_running(oa_pup(i)->link)) continue;
        oa_pose_link(c, i, &pl[i]);

        // his armour: an actor per piece of its model alone (OnlineArmor_*, tools/online_mod.py) on his Link's matrix,
        // posed bone by bone as his Link (the game's own Armor_* actors, worn on his skeleton, crashed and weren't drawn
        // made a second time)
        for (int k = 0; k < ARMOR; k++) {
            // (none: what the game puts on his Link then: his hair, his shorts; shirtless is his own body)
            const char *w = pl[i].armor[k][0] ? pl[i].armor[k] : k == 0 ? "Armor_Default_Head" : k == 2 ? "Armor_Default_Lower" : "";
            const char *oa = w[0] ? oa_armor_actor(w) : NULL;
            Piece *p = &oa_pup(i)->armor[k];
            bool had = p->actor != 0;
            static InstParams dp;  // (his dye on it, as the player's armour code makes his: ArmorDyeColor)
            memset(&dp, 0, sizeof dp);
            int8_t dye = pl[i].pose.valid ? pl[i].pose.dye[k] : -1;
            if (p->actor && dye != oa_pup(i)->dye[k]) oa_delete_piece(c, p);  // (another dye: made again with it)
            if (pl[i].pose.valid && pl[i].pose.dye[k] >= 0) oa_inst_params_s32(&dp, "ArmorDyeColor", pl[i].pose.dye[k]);
            oa_g_piece_params = dp.n ? &dp : NULL;
            bool made = oa_piece_update(c, i, p, oa ? oa : "", pl[i].name);
            oa_g_piece_params = NULL;
            if (made) oa_pup(i)->dye[k] = dye;
            if (made) {
                oa_model_visible(c, p->actor, true);
                oa_pup(i)->covered = -1;
            }
            if (had != (p->actor != 0)) oa_pup(i)->covered = -1;
            if (p->actor) {
                oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)m, 0, 0, 0, 0);
                oa_pose_armor(c, p);
            }
        }
        int pieces = 0;
        bool all_running = true;
        for (int k = 0; k < ARMOR; k++) {
            pieces |= (oa_pup(i)->armor[k].actor != 0) << k;
            if (oa_pup(i)->armor[k].actor) all_running &= oa_running(oa_pup(i)->armor[k].actor);
        }
        if (all_running && pieces != oa_pup(i)->covered) oa_cover(c, i), oa_pup(i)->covered = pieces;

        // Daruk's Protection: the player's own effects on his Link while they are on in his game, "Kago_Shield" (a hit
        // taken) and "Kago_ShieldCount_0<n>" (its ring while he guards) (0x7001100bd0: x8 the handle out, x0 the actor,
        // x1 the effect's name in its ELink; 0x700056d010(handle) ends one)
        for (int k = 0; k < 2; k++) {
            int e = pl[i].pose.valid ? pl[i].pose.effects : 0;
            int n = (e & ONLINE_E_KAGO_COUNT) >> ONLINE_E_KAGO_COUNT_SHIFT;
            bool want_k = k == 0 ? (e & ONLINE_E_KAGO) != 0 : n != 0;
            uint64_t kp = oa_pup(i)->kago[k].p;
            bool on = VALID(kp) && *(uint32_t *)(uintptr_t)(kp + 0x20) == oa_pup(i)->kago[k].id;
            if (on && (!want_k || (k == 1 && n != oa_pup(i)->kago_n))) {
                oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_pup(i)->kago[k], 0, 0, 0, 0, 0);
                memset(&oa_pup(i)->kago[k], 0, sizeof oa_pup(i)->kago[k]);
                on = false;
            }
            if (want_k && !on) {
                static char name[24];
                if (k == 0) snprintf(name, sizeof name, "Kago_Shield");
                else snprintf(name, sizeof name, "Kago_ShieldCount_0%d", n), oa_pup(i)->kago_n = n;
                oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_pup(i)->kago[k], oa_pup(i)->link, (uint64_t)(uintptr_t)name);
                LOG("[online] %s: %s (%llx)\n", pl[i].name, name, (unsigned long long)oa_pup(i)->kago[k].p);
            }
        }
        {   // his weapon's swing trail ("Blur_Default", the weapon's own ELink's: its OnlineGear has it) on the weapon in
            // his hand while his game plays it; a new one for each swing
            uint64_t tp = oa_pup(i)->trail.p;
            bool on = VALID(tp) && *(uint32_t *)(uintptr_t)(tp + 0x20) == oa_pup(i)->trail.id;
            const char *want = pl[i].pose.fx;
            uint64_t w = oa_pup(i)->gear[0].actor;
            bool held = VALID(w) && pl[i].pose.gear_bone[0] && pl[i].pose.gear_bone[0] != ONLINE_GEAR_WORLD;
            if (on && (!want[0] || !held || strcmp(want, oa_pup(i)->trail_name))) {
                oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_pup(i)->trail, 0, 0, 0, 0, 0);
                memset(&oa_pup(i)->trail, 0, sizeof oa_pup(i)->trail);
                oa_pup(i)->trail_name[0] = 0;
                on = false;
            }
            if (!on && want[0] && held && strcmp(want, oa_pup(i)->trail_name)) {
                oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_pup(i)->trail, w, (uint64_t)(uintptr_t)want);
                snprintf(oa_pup(i)->trail_name, sizeof oa_pup(i)->trail_name, "%s", want);
                // the sword trail (SwordBlur) draws from its actor's AnimInfo (+0x570 +0x148, its +0x2c0 the trail data:
                // 1.9.0 0x700152ff4c), the player's own in his game: the effect's user data (+0x190) has the weapon as
                // its actor (+8), whose AnimInfo is the empty Dummy; his Link here has the player's (online_mod)
                uint64_t ep = oa_pup(i)->trail.p, ud = VALID(ep) ? *(uint64_t *)(uintptr_t)(ep + 0x190) : 0;
                uint64_t lk = oa_pup(i)->link;
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
            if (!want[0]) oa_pup(i)->trail_name[0] = 0;
            // his Link's ELink property values on his puppet's (the same ELink: its effects play as his do)
            {
                uint64_t ui = oa_elink_ui(oa_pup(i)->link), pv = VALID(ui) ? *(uint64_t *)(uintptr_t)(ui + 0x70) : 0;
                bool any = false;
                for (int k = 0; k < ONLINE_FX_PROPS; k++) any |= pl[i].pose.fx_props[k] != 0;
                if (VALID(pv) && any) memcpy((void *)(uintptr_t)pv, pl[i].pose.fx_props, sizeof pl[i].pose.fx_props);
            }
            // his other effects, each once by its number, on his puppet or the weapon in its hand; ended after a while
            uint64_t now = plat_now_ns();
            for (int q = 0; q < ONLINE_FX_EVENTS; q++) {
                uint64_t ep = oa_pup(i)->fxev[q].p;
                if (ep && now - oa_pup(i)->fxev[q].t > FX_EVENT_NS) {
                    if (VALID(ep) && *(uint32_t *)(uintptr_t)(ep + 0x20) == oa_pup(i)->fxev[q].id)
                        oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_pup(i)->fxev[q], 0, 0, 0, 0, 0);
                    memset(&oa_pup(i)->fxev[q], 0, sizeof oa_pup(i)->fxev[q]);
                }
            }
            {   // (the newest: the first time, those before are from before he was here; after a gap, the last few)
                uint16_t newest = 0;
                for (int q = 0; q < ONLINE_FX_EVENTS; q++) {
                    uint16_t s = pl[i].pose.fxev[q].seq;
                    if (s && (!newest || (uint16_t)(s - newest) < 0x8000u)) newest = s;
                }
                if (newest && (!oa_pup(i)->fxev_seen || oa_pup(i)->frames < 60)) oa_pup(i)->fxev_seen = newest;
                else if (newest && (uint16_t)(newest - oa_pup(i)->fxev_seen) > ONLINE_FX_EVENTS &&
                         (uint16_t)(newest - oa_pup(i)->fxev_seen) < 0x8000u)
                    oa_pup(i)->fxev_seen = (uint16_t)(newest - ONLINE_FX_EVENTS);
            }
            for (int pass = 0; pass < ONLINE_FX_EVENTS; pass++) {  // (the oldest new one first)
                const OnlineFxEvent *best = NULL;
                for (int q = 0; q < ONLINE_FX_EVENTS; q++) {
                    const OnlineFxEvent *e = &pl[i].pose.fxev[q];
                    uint16_t d = (uint16_t)(e->seq - oa_pup(i)->fxev_seen);
                    if (!e->seq || !e->name[0] || !d || d > ONLINE_FX_EVENTS) continue;
                    if (!best || (uint16_t)(e->seq - best->seq) > 0x8000u) best = e;
                }
                if (!best) break;
                oa_pup(i)->fxev_seen = best->seq;
                if (best->who == 4) {  // (the ground's, on this game's ReactionField where his puppet is, left to end)
                    uint64_t rf = oa_g_rf;
                    if (!VALID(rf) || !oa_alive(rf, oa_g_rf_watch) || !VALID(oa_pup(i)->link) || !oa_running(oa_pup(i)->link)) continue;
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
                        const float *lm = (const float *)(uintptr_t)(oa_pup(i)->link + 0x398);
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
                uint64_t on = best->who & 1 ? (held ? w : 0) : oa_pup(i)->link;
                if (!VALID(on)) continue;
                // (a sound: on its SLink, the player's / the weapon's: 0x700159f00c, the game's own keyword emit, w2 1 its
                // SLink alone, x3 the two handles: the SLink's at +0x10)
                if (best->who >= 2 && !VALID(oa_slink_ui(on))) continue;
                // (the same one still playing - sent again, a Champion's ability used again and again - ended first:
                // they piled up, one over the other)
                for (int o = 0; o < ONLINE_FX_EVENTS; o++) {
                    uint64_t op = oa_pup(i)->fxev[o].p;
                    if (!op || strcmp(oa_pup(i)->fxev[o].name, best->name)) continue;
                    if (VALID(op) && *(uint32_t *)(uintptr_t)(op + 0x20) == oa_pup(i)->fxev[o].id)
                        oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_pup(i)->fxev[o], 0, 0, 0, 0, 0);
                    memset(&oa_pup(i)->fxev[o], 0, sizeof oa_pup(i)->fxev[o]);
                }
                int q = 0;
                while (q < ONLINE_FX_EVENTS - 1 && oa_pup(i)->fxev[q].p) q++;
                if (oa_pup(i)->fxev[q].p) {
                    uint64_t ep = oa_pup(i)->fxev[q].p;
                    if (VALID(ep) && *(uint32_t *)(uintptr_t)(ep + 0x20) == oa_pup(i)->fxev[q].id)
                        oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_pup(i)->fxev[q], 0, 0, 0, 0, 0);
                }
                memset(&oa_pup(i)->fxev[q], 0, sizeof oa_pup(i)->fxev[q]);
                if (best->who >= 2) {
                    struct { uint64_t p; uint32_t id, pad; } hh[2];
                    memset(hh, 0, sizeof hh);
                    oa_call(c, FN_XLINK_EMIT, on, (uint64_t)(uintptr_t)best->name, 1, (uint64_t)(uintptr_t)hh, 0, 0);
                    oa_pup(i)->fxev[q].p = hh[1].p, oa_pup(i)->fxev[q].id = hh[1].id;
                } else
                    oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_pup(i)->fxev[q], on, (uint64_t)(uintptr_t)best->name);
                oa_pup(i)->fxev[q].t = now;
                snprintf(oa_pup(i)->fxev[q].name, sizeof oa_pup(i)->fxev[q].name, "%s", best->name);
                if (getenv("SAVAGEWIND_ONLINE_FX_LOG")) {
                    static const char *const kinds[4] = {"Link", "weapon", "Link (sound)", "weapon (sound)"};
                    LOG("[online] %s: %s on his %s (%llx)\n", pl[i].name, best->name, kinds[best->who & 3],
                        (unsigned long long)oa_pup(i)->fxev[q].p);
                }
            }
            // the attack it is the trail of, for SwordBlur to find on his puppet (rt_blur_anim / rt_blur_frame)
            for (int k = 0; k < 3; k++) {
                oa_g_blur[i].anim[k] = want[0] && held ? pl[i].pose.fx_anim[k] : -1;
                oa_g_blur[i].frame[k] = pl[i].pose.fx_frame[k];
            }
            oa_g_blur[i].actor = oa_pup(i)->link;
        }
        {   // Revali's Gale: its charge on his Link while he charges it ("Kago_Wind", the player's own effect, as Daruk's)
            int e = pl[i].pose.valid ? pl[i].pose.effects : 0;
            uint64_t gp = oa_pup(i)->gale.p;
            bool on = VALID(gp) && *(uint32_t *)(uintptr_t)(gp + 0x20) == oa_pup(i)->gale.id;
            if (on && !(e & ONLINE_E_GALE)) {
                oa_call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)&oa_pup(i)->gale, 0, 0, 0, 0, 0);
                memset(&oa_pup(i)->gale, 0, sizeof oa_pup(i)->gale);
            } else if (!on && (e & ONLINE_E_GALE)) {
                oa_call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&oa_pup(i)->gale, oa_pup(i)->link, (uint64_t)(uintptr_t)"Kago_Wind");
                LOG("[online] %s: Kago_Wind (%llx)\n", pl[i].name, (unsigned long long)oa_pup(i)->gale.p);
            }
        }
        // his gear, on his Link's bones as in his game (not there: under the world, out of sight)
        float bm[BONES][12];
        int have = 0;  // (bones read: a bit each)
        for (int k = 0; k < ONLINE_GEAR; k++) {
            const char *ga = pl[i].gear[k][0] ? oa_gear_actor(pl[i].gear[k]) : NULL;
            Piece *p = &oa_pup(i)->gear[k];
            oa_piece_update(c, i, p, ga ? ga : "", pl[i].name);
            if (!p->actor) continue;
            int b = pl[i].pose.valid ? pl[i].pose.gear_bone[k] - 1 : -1;
            float gm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
            if (b >= 0 && b < BONES) {
                if (!(have >> b & 1) && oa_bone_mtx(c, oa_pup(i)->link, oa_k_bones[b], bm[b])) have |= 1 << b;
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
                        lp->actor == oa_rune_grab(i))
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
            Piece *hp = &oa_pup(i)->horse;
            if (oa_piece_update(c, i, hp, ha ? ha : "", pl[i].name)) oa_pup(i)->horse_as[0] = 0;
            if (hp->actor) {
                bool finite = true;
                for (int q = 0; q < 12; q++) finite &= isfinite(pl[i].pose.horse_m[q]);
                if (finite) oa_call(c, FN_SET_MTX, hp->actor, (uint64_t)(uintptr_t)pl[i].pose.horse_m, 0, 0, 0, 0);
                const char *want = pl[i].pose.horse_as;
                if (want[0] && strcmp(want, oa_pup(i)->horse_as) && oa_running(hp->actor)) {
                    oa_play_as(c, hp->actor, want, 0, 0, -1.0f, true);
                    snprintf(oa_pup(i)->horse_as, sizeof oa_pup(i)->horse_as, "%s", want);
                }
            }
            for (int k = 0; k < 2; k++) {  // (its saddle and reins, where his are)
                const char *ta = pl[i].pose.valid && pl[i].pose.horse[0] && pl[i].pose.tack[k][0] ? oa_gear_actor(pl[i].pose.tack[k]) : NULL;
                Piece *tp = &oa_pup(i)->tack[k];
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
            Piece *p = &oa_pup(i)->held[k];
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
                    if (!(have >> b & 1) && oa_bone_mtx(c, oa_pup(i)->link, oa_k_bones[b], bm[b])) have |= 1 << b;
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
                if (oa_pup(i)->shot_id[j2] == sh->shot[k].id) slot = j2;
            for (int j2 = 0; j2 < ONLINE_SHOTS && slot < 0; j2++) {
                bool in_use = false;
                for (int q = 0; q < sh->n; q++) in_use |= oa_pup(i)->shot_id[j2] == sh->shot[q].id;
                if (!in_use) slot = j2;
            }
            if (slot < 0 || sh->shot[k].kind >= ONLINE_K_COUNT) continue;
            used[slot] = true;
            if (oa_pup(i)->shot_id[slot] != sh->shot[k].id)
                LOG("[online] %s's shot %d (%s) in slot %d\n", pl[i].name, sh->shot[k].id, oa_k_shot_names[sh->shot[k].kind], slot);
            oa_pup(i)->shot_id[slot] = sh->shot[k].id;
            const char *sa = oa_gear_actor(oa_k_shot_names[sh->shot[k].kind]);
            Piece *p = &oa_pup(i)->shot[slot];
            if (oa_piece_update(c, i, p, sa ? sa : "", pl[i].name)) oa_model_visible(c, p->actor, true);
            if (p->actor) oa_call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)sh->shot[k].m, 0, 0, 0, 0);
            if (getenv("SAVAGEWIND_ONLINE_SHOT_LOG"))
                LOG("[online] shot %d %s at %.2f %.2f %.2f%s\n", sh->shot[k].id, oa_k_shot_names[sh->shot[k].kind], sh->shot[k].m[3],
                    sh->shot[k].m[7], sh->shot[k].m[11], p->actor ? "" : " (being made)");
        }
        for (int k = 0; k < ONLINE_SHOTS; k++) {
            if (used[k]) continue;
            if (oa_pup(i)->shot_id[k])
                LOG("[online] %s's shot %d out of sight (slot %d, actor %llx)\n", pl[i].name, oa_pup(i)->shot_id[k], k,
                    (unsigned long long)oa_pup(i)->shot[k].actor);
            oa_pup(i)->shot_id[k] = 0;
            if (oa_pup(i)->shot[k].actor) {
                float hm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
                oa_call(c, FN_SET_MTX, oa_pup(i)->shot[k].actor, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
            }
        }
        // an arrow on his string, in his right hand, while he nocks and draws his bow (the upper body's AS: BowReload,
        // BowCharge...; his bow in a hand), its point (+z) past the bow in his left; out of sight the rest of the time
        {
            Piece *ar = &oa_pup(i)->nock;
            const char *an = pl[i].gear[2][0] ? oa_gear_actor("NormalArrow") : NULL;
            if (oa_piece_update(c, i, ar, an ? an : "", pl[i].name)) oa_model_visible(c, ar->actor, true);
            if (ar->actor) {
                const char *up = pl[i].pose.as[2];
                bool drawn = pl[i].pose.valid && (pl[i].pose.gear_bone[2] == 1 || pl[i].pose.gear_bone[2] == 2) &&
                             (!strncmp(up, "BowCharge", 9) || !strncmp(up, "BowReload", 9));
                float am[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
                if (drawn && !(have & 1) && oa_bone_mtx(c, oa_pup(i)->link, oa_k_bones[0], bm[0])) have |= 1;
                if (drawn && !(have & 2) && oa_bone_mtx(c, oa_pup(i)->link, oa_k_bones[1], bm[1])) have |= 2;
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
            if (!seq || (uint16_t)(seq - oa_pup(i)->boom_seen) > 0x8000u || seq == oa_pup(i)->boom_seen) continue;
            oa_pup(i)->boom_seen = seq;
            if (oa_pup(i)->frames < 60 || sh->boom[k].kind >= ONLINE_K_COUNT) continue;  // (from before he was here)
            const char *ba = oa_gear_actor(oa_k_shot_names[sh->boom[k].kind]);
            for (int q = 0; ba && q < ONLINE_SHOTS; q++)
                if (!used[q] && oa_pup(i)->shot[q].actor && !strcmp(oa_pup(i)->shot[q].name, ba)) {
                    float hm[12] = {1, 0, 0, pl[i].pos[0], 0, 1, 0, pl[i].pos[1] - 5000, 0, 0, 1, pl[i].pos[2]};
                    oa_call(c, FN_SET_MTX, oa_pup(i)->shot[q].actor, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
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
    return false;
}
