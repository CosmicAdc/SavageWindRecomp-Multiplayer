// Online play's actors, oa_puppet: the remote players' Links here: posed and animated, their armour, their blasts, the
// blur. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"

static void animate(Cpu *c, int i, const OnlinePlayer *p);

// A remote player in the world: his Link, his armour and his gear (his runes: oa_runes' g_rune, g_ice)
static OaPup g_pup[ONLINE_MAX];

// player i's puppet state (rt_online_frame's loop over the players works on it)
OaPup *oa_pup(int i) { return &g_pup[i]; }

// player i's Link here (0: none), as made: maybe deleted by the game since (oa_link_live checks)
uint64_t oa_puppet_link(int i) { return g_pup[i].link; }

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
