// Online play's actors, oa_runes: the remote players' runes here: Magnesis, Stasis, Cryonis. What the modules share:
// runtime/online_actors_internal.h
#include "online_actors_internal.h"

#define MAGNET_TIP 0.8f  // (the magnet over the hands, from them to its tips, toward what is held: in metres)

// what the local Link holds with Magnesis (0: none). 1.9.0: the rune's object at P_MAGNE, its +0x318 the magnetic part
// of the actor held, 0x7000b993e8 checks it so; that part on while its +0x10 bit 0; its actor at +8
uint64_t magne_held(void) {
    uint64_t g = *(uint64_t *)(uintptr_t)P_MAGNE;
    uint64_t part = VALID(g) ? *(uint64_t *)(uintptr_t)(g + 0x318) : 0;
    uint64_t a = VALID(part) && (*(uint32_t *)(uintptr_t)(part + 0x10) & 1) ? *(uint64_t *)(uintptr_t)(part + 8) : 0;
    return VALID(a) ? a : 0;
}

static void stasis_look(Cpu *c, uint64_t a, bool on) {
    uint64_t model = *(uint64_t *)(uintptr_t)(a + 0x4e0);
    call(c, on ? FN_ACTOR_LOOK_ON : FN_ACTOR_LOOK_OFF, a, 8, 0, 0, 0, 0);
    if (!VALID(model)) return;
    // (uk_special_effect_A: the red pulse as it is about to go, 0 meanwhile; uk_special_effect_B: its glow, 1)
    const char *ea = getenv("SAVAGEWIND_ONLINE_STASIS_A"), *eb = getenv("SAVAGEWIND_ONLINE_STASIS_B");
    float va = on && ea ? (float)atof(ea) : 0.0f, vb = on ? (eb ? (float)atof(eb) : 1.0f) : 0.0f;
    call_f(c, FN_SPECIAL_FX_B, 0, model, 0, 0, 0, 0, 0, vb);
    int n = *(int *)(uintptr_t)(model + 0x38);
    uint64_t arr = *(uint64_t *)(uintptr_t)(model + 0x40);
    for (int k = 0; VALID(arr) && k < n && k < 64; k++) {
        uint64_t u = *(uint64_t *)(uintptr_t)(arr + 8 * k);
        u = VALID(u) ? *(uint64_t *)(uintptr_t)u : 0;
        if (VALID(u)) call_f(c, FN_SPECIAL_FX_A_UNIT, u, 0, 0, 0, 0, 0, 0, va);
    }
}

static bool fx_on(const void *h);
static void fx_end(Cpu *c, void *h);
static void rune_keep(Cpu *c, int i, Piece *p, const char *name, const char *who);

// The Stasis chains' place and size, as the rune sets them after it plays BitaLocked_Start (1.9.0 0x70011f5970): the
// effect's matrix (+0xc0, flags +0x180 | 8, +0xb4..+0xbc 1) scaled by the stopped object's model's largest extent (its
// box from the model unit's vtable +0x208: x1 6 floats out, min then max) and put at the object. Not set, the chains
// were made and never seen
static void chains_place(Cpu *c, uint64_t h, uint64_t a) {
    if (!VALID(h) || !VALID(a)) return;
    const float *am = (const float *)(uintptr_t)(a + 0x398);
    float s = 1;
    uint64_t mdl = *(uint64_t *)(uintptr_t)(a + 0x4e0);
    uint64_t units = VALID(mdl) ? *(uint64_t *)(uintptr_t)(mdl + 0x40) : 0;
    uint64_t u = VALID(units) ? *(uint64_t *)(uintptr_t)units : 0;
    uint64_t unit = VALID(u) ? *(uint64_t *)(uintptr_t)u : 0;
    if (VALID(unit)) {
        float box[6] = {FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX};
        uint64_t vt = *(uint64_t *)(uintptr_t)unit;
        uint64_t fn = VALID(vt) ? *(uint64_t *)(uintptr_t)(vt + 0x208) : 0;
        if (fn) call(c, fn, unit, (uint64_t)(uintptr_t)box, 0, 0, 0, 0);
        float ex = box[3] - box[0], ey = box[4] - box[1], ez = box[5] - box[2];
        float e = fmaxf(ex, fmaxf(ey, ez));
        if (isfinite(e) && e > 0.05f && e < 100) s = e;
    }
    float m[12] = {s, 0, 0, am[3], 0, s, 0, am[7], 0, 0, s, am[11]};
    memcpy((void *)(uintptr_t)(h + 0xc0), m, sizeof m);
    float one[3] = {1, 1, 1};
    memcpy((void *)(uintptr_t)(h + 0xb4), one, sizeof one);
    *(uint32_t *)(uintptr_t)(h + 0x180) |= 8;
    static int told;
    if (told++ < 2) LOG("[online] Stasis chains placed: size %.2f\n", s);
}

// What the others stopped with the Stasis rune: the same object here (as Magnesis: the one of that name nearest where it
// was), held where it is in his game (its body taken there each frame: it stays still as his does), drawn as stopped,
// with the game's own chains (BitaLocked_Start, ReactionHit's: played once on his OnlineRuneFx, chains_place); let go
// when it's over there
void stop_update(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me) {
    for (int i = 0; i < ONLINE_MAX; i++) {
        const OnlineGrab *g = i != me && pl[i].used && pl[i].pose.valid ? &pl[i].pose.stop : NULL;
        bool want = g && g->name[0];
        if (g_act[i].stop && (!want || strcmp(g->name, g_act[i].stop_name) || !alive(g_act[i].stop, g_act[i].stop_w))) {
            if (alive(g_act[i].stop, g_act[i].stop_w)) stasis_look(c, g_act[i].stop, false);
            fx_end(c, &g_act[i].chains);
            LOG("[online] %s's Stasis on %s: over\n", pl[i].name, g_act[i].stop_name);
            g_act[i].released = g_act[i].stop, g_act[i].released_t = plat_now_ns();
            g_act[i].stop = 0, g_act[i].chained = false;
        }
        if (!want) {
            g_act[i].stop_name[0] = 0, g_act[i].stop_wait = 0;
            continue;
        }
        if (!g_act[i].stop && (strcmp(g->name, g_act[i].stop_name) || g_act[i].stop_wait-- <= 0)) {
            snprintf(g_act[i].stop_name, sizeof g_act[i].stop_name, "%s", g->name);
            g_act[i].stop_wait = 30;
            int na = all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), g_actors, MAX_ACTORS);
            // (the one of that name nearest where he stopped it or where it is in his game, 15 m: the two worlds' may
            // have parted, a trunk fallen its own way in each; 4 m and his trunk was "not here")
            float best = 15 * 15;
            for (int k = 0; k < na; k++) {
                char n[48];
                actor_name_of(g_actors[k], n);
                if (strcmp(n, g->name) || !has_body(g_actors[k])) continue;  // (never a tag: what he sends isn't trusted)
                const float *m = (const float *)(uintptr_t)(g_actors[k] + 0x398);
                float d = (m[3] - g->from[0]) * (m[3] - g->from[0]) + (m[7] - g->from[1]) * (m[7] - g->from[1]) +
                          (m[11] - g->from[2]) * (m[11] - g->from[2]);
                float dn = (m[3] - g->m[3]) * (m[3] - g->m[3]) + (m[7] - g->m[7]) * (m[7] - g->m[7]) +
                           (m[11] - g->m[11]) * (m[11] - g->m[11]);
                if (dn < d) d = dn;
                if (d < best) best = d, g_act[i].stop = g_actors[k];
            }
            if (g_act[i].stop) g_act[i].stop_w = watch(g_act[i].stop);
            LOG("[online] %s stopped %s with Stasis: %s\n", pl[i].name, g->name, g_act[i].stop ? "here too" : "not here");
        }
        if (!g_act[i].stop) continue;
        uint64_t body = *(uint64_t *)(uintptr_t)(g_act[i].stop + 0x190);
        if (VALID(body)) call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)g->m, 0, 0, 0, 0);
        stasis_look(c, g_act[i].stop, true);
        // its chains, the game's own, where it is
        if (!link_live(i)) continue;
        rune_keep(c, i, &g_act[i].stop_fx, "OnlineRuneFx", pl[i].name);
        uint64_t fxa = g_act[i].stop_fx.actor;
        if (!fxa) continue;
        call(c, FN_SET_MTX, fxa, g_act[i].stop + 0x398, 0, 0, 0, 0);
        if (running(fxa) && !g_act[i].chained) {
            call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&g_act[i].chains, fxa, (uint64_t)(uintptr_t)"BitaLocked_Start");
            g_act[i].chained = fx_on(&g_act[i].chains);
            static int told;
            if (told++ < 4) LOG("[online] %s's Stasis chains: BitaLocked_Start %s\n", pl[i].name, fx_on(&g_act[i].chains) ? "on" : "not made");
        }
        if (fx_on(&g_act[i].chains)) chains_place(c, g_act[i].chains.p, g_act[i].stop);
    }
}

// The others' Cryonis pillars: a pillar of ours where each of his is (made with the game's own actor: it rises as his
// did, and one can stand on it); gone when his goes
void ice_update(Cpu *c, const OnlinePlayer *pl, int me) {
    for (int i = 0; i < ONLINE_MAX; i++) {
        int n = i != me && pl[i].used && pl[i].pose.valid ? pl[i].pose.ice_n : 0;
        bool keep[3] = {false, false, false};
        for (int k = 0; k < n; k++) {
            const float *m = pl[i].pose.ice[k];
            int found = -1;
            for (int q = 0; q < 3 && found < 0; q++) {
                if (!g_act[i].ice[q].used) continue;
                float dx = g_act[i].ice[q].m[3] - m[3], dy = g_act[i].ice[q].m[7] - m[7], dz = g_act[i].ice[q].m[11] - m[11];
                if (dx * dx + dy * dy + dz * dz < 0.5f * 0.5f) found = q;
            }
            for (int q = 0; q < 3 && found < 0; q++)
                if (!g_act[i].ice[q].used && !keep[q]) {
                    g_act[i].ice[q].used = true, g_act[i].ice[q].rose = false, found = q;
                    memcpy(g_act[i].ice[q].m, m, sizeof g_act[i].ice[q].m);
                    LOG("[online] %s's ice pillar at %.1f %.1f %.1f\n", pl[i].name, m[3], m[7], m[11]);
                }
            if (found >= 0) keep[found] = true;
        }
        for (int q = 0; q < 3; q++) {
            if (!keep[q]) g_act[i].ice[q].used = false;
            Piece *p = &g_act[i].ice[q].p;
            if (!g_act[i].ice[q].used && (p->actor || p->pending)) {
                delete_piece(c, p);
                LOG("[online] an ice pillar of another player: gone\n");
                continue;
            }
            if (!g_act[i].ice[q].used) continue;
            piece_update(c, i, p, "OnlineIce", pl[i].name);
            if (!p->actor) continue;
            call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)g_act[i].ice[q].m, 0, 0, 0, 0);
            if (!g_act[i].ice[q].rose && running(p->actor) && play_as(c, p->actor, "Appear_Linear", 0, 0, -1, true))
                g_act[i].ice[q].rose = true;
        }
    }
}

static bool fx_on(const void *h) {  // (an effect's handle {effect, id}: still that effect)
    const struct { uint64_t p; uint32_t id, pad; } *e = h;
    return VALID(e->p) && *(uint32_t *)(uintptr_t)(e->p + 0x20) == e->id;
}

static void fx_end(Cpu *c, void *h) {
    if (fx_on(h)) call(c, FN_EFFECT_KILL, (uint64_t)(uintptr_t)h, 0, 0, 0, 0, 0);
    memset(h, 0, 16);
}

// his slate and the actors his runes' effects play on (OnlineSlate, OnlineRuneFx, OnlineMagnetFx, and OnlineRuneFx
// for his Stasis chains) made once and kept with his puppet, where he is, models hidden, the slate's drawn in his
// right hand while he holds. Not pooled under the world between his grabs: the game removed them there, and a piece
// made again for him is never drawn (his beam and magnet were then gone for good)
static void rune_keep(Cpu *c, int i, Piece *p, const char *name, const char *who) {
    if (piece_update(c, i, p, name, who)) model_visible(c, p->actor, false);
    uint64_t lk = link_live(i);
    if (!p->actor || !lk) return;
    const float *lm = (const float *)(uintptr_t)(lk + 0x398);
    float at[12] = {1, 0, 0, lm[3], 0, 1, 0, lm[7] + 1, 0, 0, 1, lm[11]};
    call(c, FN_SET_MTX, p->actor, (uint64_t)(uintptr_t)at, 0, 0, 0, 0);  // (moved again where it's used, after)
}

static void rune_pieces(Cpu *c, int i, bool holding, const char *who) {
    bool made = piece_update(c, i, &g_act[i].slate, "OnlineSlate", who);
    if (g_act[i].slate.actor && (made || g_act[i].slate_shown != holding)) {
        model_visible(c, g_act[i].slate.actor, holding);
        g_act[i].slate_shown = holding;
    }
    rune_keep(c, i, &g_act[i].rune_fx, "OnlineRuneFx", who);
    rune_keep(c, i, &g_act[i].magnet_fx, "OnlineMagnetFx", who);
    if (!g_act[i].stop) rune_keep(c, i, &g_act[i].stop_fx, "OnlineRuneFx", who);
    float hand[12];
    uint64_t lk = link_live(i);
    if (g_act[i].slate.actor && lk) {
        if (holding && bone_mtx(c, lk, "Weapon_R", hand))
            call(c, FN_SET_MTX, g_act[i].slate.actor, (uint64_t)(uintptr_t)hand, 0, 0, 0, 0);
        else if (!holding) {
            const float *lm = (const float *)(uintptr_t)(lk + 0x398);
            float at[12] = {1, 0, 0, lm[3], 0, 1, 0, lm[7] + 1, 0, 0, 1, lm[11]};
            call(c, FN_SET_MTX, g_act[i].slate.actor, (uint64_t)(uintptr_t)at, 0, 0, 0, 0);
        }
    }
}

void grab_update(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me) {
    for (int i = 0; i < ONLINE_MAX; i++) {
        const OnlineGrab *g = i != me && pl[i].used && pl[i].pose.valid ? &pl[i].pose.grab : NULL;
        uint64_t lk = link_live(i);
        bool want = g && g->name[0] && lk;
        if (g_act[i].grab) {
            float dx = want ? g->from[0] - g_act[i].grab_from[0] : 0, dz = want ? g->from[2] - g_act[i].grab_from[2] : 0;
            if (!want || strcmp(g->name, g_act[i].grab_name) || dx * dx + dz * dz > 0.25f ||
                !alive(g_act[i].grab, g_act[i].grab_w)) {
                LOG("[online] %s lets go of %s\n", pl[i].name[0] ? pl[i].name : g_act[i].name, g_act[i].grab_name);
                if (alive(g_act[i].grab, g_act[i].grab_w)) {
                    uint64_t model = *(uint64_t *)(uintptr_t)(g_act[i].grab + 0x4e0);
                    if (VALID(model)) call_f(c, FN_SPECIAL_FX_B, 0, model, 0, 0, 0, 0, 0, 0.0f);
                    call(c, FN_ACTOR_LOOK_OFF, g_act[i].grab, 2, 0, 0, 0, 0);
                }
                g_act[i].released = g_act[i].grab, g_act[i].released_t = plat_now_ns();
                g_act[i].grab = 0;
            }
        }
        if (!want) {  // (the rune's effects end as he lets go)
            fx_end(c, &g_act[i].magne);
            fx_end(c, &g_act[i].magnet);
            for (int q = 0; q < MAGNE_BEAM; q++) fx_end(c, &g_act[i].beam[q]);
        }
        if (!want) {
            g_act[i].grab_name[0] = 0, g_act[i].grab_wait = 0;
            if (lk && i != me && pl[i].used) rune_pieces(c, i, false, pl[i].name);
            continue;
        }
        if (!g_act[i].grab && (strcmp(g->name, g_act[i].grab_name) || g_act[i].grab_wait-- <= 0)) {
            snprintf(g_act[i].grab_name, sizeof g_act[i].grab_name, "%s", g->name);
            memcpy(g_act[i].grab_from, g->from, sizeof g_act[i].grab_from);
            g_act[i].grab_wait = 30;
            // his loot first (a weapon he dropped or threw: its copy here, wherever it went), then the world's: the
            // one of that name nearest where he caught it or where it is now in his game (the two worlds' may have
            // parted: let go, it fell its own way in each)
            float best = 30 * 30;
            for (int t = 0; t < THEIRS; t++) {
                Piece *lp = &g_theirs[t].p;
                if (!g_theirs[t].gid || g_theirs[t].gone || g_theirs[t].origin != i || !lp->actor ||
                    strcmp(lp->name, g->name) || !alive(lp->actor, lp->w))
                    continue;
                const float *m = (const float *)(uintptr_t)(lp->actor + 0x398);
                float d = (m[3] - g->from[0]) * (m[3] - g->from[0]) + (m[7] - g->from[1]) * (m[7] - g->from[1]) +
                          (m[11] - g->from[2]) * (m[11] - g->from[2]);
                float dn = (m[3] - g->m[3]) * (m[3] - g->m[3]) + (m[7] - g->m[7]) * (m[7] - g->m[7]) +
                           (m[11] - g->m[11]) * (m[11] - g->m[11]);
                if (dn < d) d = dn;
                if (d < best) best = d, g_act[i].grab = lp->actor;
            }
            int na = g_act[i].grab ? 0 : all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), g_actors, MAX_ACTORS);
            best = 15 * 15;
            for (int k = 0; k < na; k++) {
                char n[48];
                actor_name_of(g_actors[k], n);
                if (strcmp(n, g->name) || !has_body(g_actors[k])) continue;  // (never a tag: what he sends isn't trusted)
                const float *m = (const float *)(uintptr_t)(g_actors[k] + 0x398);
                float d = (m[3] - g->from[0]) * (m[3] - g->from[0]) + (m[7] - g->from[1]) * (m[7] - g->from[1]) +
                          (m[11] - g->from[2]) * (m[11] - g->from[2]);
                float dn = (m[3] - g->m[3]) * (m[3] - g->m[3]) + (m[7] - g->m[7]) * (m[7] - g->m[7]) +
                           (m[11] - g->m[11]) * (m[11] - g->m[11]);
                if (dn < d) d = dn;
                if (d < best) best = d, g_act[i].grab = g_actors[k];
            }
            if (g_act[i].grab) {
                g_act[i].grab_w = watch(g_act[i].grab);
                call(c, FN_ACTOR_LOOK_ON, g_act[i].grab, 2, 0, 0, 0, 0);  // (held by Magnesis: its golden glow)
            }
            LOG("[online] %s holds %s with Magnesis: %s\n", pl[i].name, g->name, g_act[i].grab ? "here too" : "not here");
        }
        rune_pieces(c, i, true, pl[i].name);  // (his slate up while he holds)
        if (g_act[i].grab) {
            // (its body moved there: the velocities that take it there in a step, 0x70012d4e18; its actor +0x190 the body)
            uint64_t body = *(uint64_t *)(uintptr_t)(g_act[i].grab + 0x190);
            if (VALID(body)) call(c, FN_BODY_TO, body, (uint64_t)(uintptr_t)g->m, 0, 0, 0, 0);
            else call(c, FN_SET_MTX, g_act[i].grab, (uint64_t)(uintptr_t)g->m, 0, 0, 0, 0);
            // its golden glow: the materials' uk_special_effect_B at 1 (the rune keeps it so while it holds; 0 on letting go)
            uint64_t model = *(uint64_t *)(uintptr_t)(g_act[i].grab + 0x4e0);
            if (VALID(model)) call_f(c, FN_SPECIAL_FX_B, 0, model, 0, 0, 0, 0, 0, 1.0f);
            {   // the rune's sparkles where it is, on the actor the game plays them on (P_MAGNE_FX's +0x30: ReactionHit)
                uint64_t fxo = *(uint64_t *)(uintptr_t)P_MAGNE_FX;
                fxo = VALID(fxo) ? *(uint64_t *)(uintptr_t)fxo : 0;
                uint64_t rh = VALID(fxo) ? *(uint64_t *)(uintptr_t)(fxo + 0x30) : 0;
                if (!fx_on(&g_act[i].magne) && VALID(rh)) {
                    static const char *const kn = "MagneCatchMoving";
                    call_sret(c, FN_XLINK_EFFECT, (uint64_t)(uintptr_t)&g_act[i].magne, rh, (uint64_t)(uintptr_t)kn);
                }
                uint64_t mp = g_act[i].magne.p;
                if (fx_on(&g_act[i].magne)) {
                    *(uint16_t *)(uintptr_t)(mp + 0x98) |= 0x100;
                    *(float *)(uintptr_t)(mp + 0xbc) = g->m[3];
                    *(float *)(uintptr_t)(mp + 0xc0) = g->m[7];
                    *(float *)(uintptr_t)(mp + 0xc4) = g->m[11];
                    *(uint16_t *)(uintptr_t)(mp + 0x9a) |= 0x100;
                }
            }
        }  // (the object here: none - not found - his magnet and beam toward where his is)
        // the beam, the game's own (MagneConnect, ReactionHit's: played on his OnlineRuneFx, and placed each frame as
        // the rune places its own, 1.9.0 0x7000b99cf0: 0x7000d40bb8(holder, matrix, s0 its length), the holder keeping
        // the effect's handle at +0x48 and its id at +0x50, its +0x13c 0 as the rune's while it holds: 0x7000d40bb8
        // gives it to the beam's particles, +0x190 +0x18). Its matrix is the world's: at the slate (between his hands:
        // the blue magnet is drawn there), its z toward what is held (the yellow end), its y as near up as it can be,
        // its x = y x z
        uint64_t fxa = g_act[i].rune_fx.actor;
        float hand[12], left[12];
        if (!fxa || !bone_mtx(c, lk, "Weapon_R", hand)) continue;
        float o[3] = {hand[3], hand[7], hand[11]};
        if (bone_mtx(c, lk, "Weapon_L", left))
            o[0] = (o[0] + left[3]) / 2, o[1] = (o[1] + left[7]) / 2, o[2] = (o[2] + left[11]) / 2;
        const float *gm = g_act[i].grab ? (const float *)(uintptr_t)(g_act[i].grab + 0x398) : g->m;  // (as it's drawn)
        float z[3] = {gm[3] - o[0], gm[7] - o[1], gm[11] - o[2]};
        float len = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        if (len < 0.05f) continue;
        z[0] /= len, z[1] /= len, z[2] /= len;
        float x[3] = {z[2], 0, -z[0]};  // (up x z)
        float xl = sqrtf(x[0] * x[0] + x[2] * x[2]);
        if (xl < 1e-4f) x[0] = 1, x[2] = 0, xl = 1;  // (straight up or down: any level x)
        x[0] /= xl, x[2] /= xl;
        float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
        // (his OnlineRuneFx goes where the slate is, so the game keeps it, unturned)
        float at[12] = {1, 0, 0, o[0], 0, 1, 0, o[1], 0, 0, 1, o[2]};
        // (the beam from the magnet's tips, MAGNET_TIP along it from his hands, as in his game: to what he holds)
        float tip = fminf(MAGNET_TIP, len * 0.5f), bl = fmaxf(len - tip, 0.1f);
        float mtx[12] = {x[0], y[0], z[0], o[0] + z[0] * tip, x[1], y[1], z[1], o[1] + z[1] * tip,
                         x[2], y[2], z[2], o[2] + z[2] * tip};
        call(c, FN_SET_MTX, fxa, (uint64_t)(uintptr_t)at, 0, 0, 0, 0);
        // the magnet over his hands, the game's own: Item_Magnetglove's key "Hand" (the particles MagneHand; the rune
        // plays it from its action, not by name), drawn about its actor with the U opening along its y: its y toward
        // what he holds, its x level
        uint64_t mf = g_act[i].magnet_fx.actor;
        if (mf) {
            float hx[3] = {z[2], 0, -z[0]};  // (level, across: y x up)
            float hl = sqrtf(hx[0] * hx[0] + hx[2] * hx[2]);
            if (hl < 1e-4f) hx[0] = 1, hx[2] = 0, hl = 1;
            hx[0] /= hl, hx[2] /= hl;
            float hz[3] = {hx[1] * z[2] - hx[2] * z[1], hx[2] * z[0] - hx[0] * z[2], hx[0] * z[1] - hx[1] * z[0]};  // (x x y)
            float hm[12] = {hx[0], z[0], hz[0], o[0], hx[1], z[1], hz[1], o[1], hx[2], z[2], hz[2], o[2]};
            call(c, FN_SET_MTX, mf, (uint64_t)(uintptr_t)hm, 0, 0, 0, 0);
            if (running(mf) && !fx_on(&g_act[i].magnet)) {
                call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&g_act[i].magnet, mf, (uint64_t)(uintptr_t)"Hand");
                static int told;
                if (told++ < 4) LOG("[online] %s's magnet: Hand %s\n", pl[i].name, fx_on(&g_act[i].magnet) ? "on" : "not made");
            }
        }
        if (running(fxa) && !fx_on(&g_act[i].beam[0])) {
            call_sret(c, FN_EFFECT_EMIT, (uint64_t)(uintptr_t)&g_act[i].beam[0], fxa, (uint64_t)(uintptr_t)"MagneConnect");
            LOG("[online] %s's Magnesis beam: MagneConnect %s\n", pl[i].name, fx_on(&g_act[i].beam[0]) ? "on" : "not made");
        }
        if (fx_on(&g_act[i].beam[0])) {
            // the beam's strands leave along the rune's aim and bend to what is held (two Vector3f the game keeps,
            // zero until the local Link first raises the rune: before that every MagneConnect, ours too, is made and
            // never drawn; after, they bent along his last aim). His beam's own way, each frame, unless the local Link
            // holds something with it (his own beam then)
            uint64_t ab = *(const uint64_t *)(uintptr_t)P_MAGNE_AIM;
            if (VALID(ab) && !magne_held()) {
                float *aim = (float *)(uintptr_t)(ab + MAGNE_AIM_OFF);
                for (int k = 0; k < 6; k++) aim[k] = z[k % 3];
            }
            uint8_t *hd = g_act[i].holder;
            memset(hd, 0, sizeof g_act[i].holder);
            *(uint64_t *)(hd + 0x48) = g_act[i].beam[0].p;
            *(uint32_t *)(hd + 0x50) = g_act[i].beam[0].id;
            *(float *)(hd + 0x13c) = 0.0f;
            call_f(c, FN_MAGNE_BEAM_SET, (uint64_t)(uintptr_t)hd, (uint64_t)(uintptr_t)mtx, 0, 0, 0, 0, 0, bl);
        }
    }
}
