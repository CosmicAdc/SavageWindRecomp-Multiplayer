// Online play's actors, oa_sample: the local Link, read for the others: his pose and skeleton, his shots, what he
// holds, his effects; the game's hooks that see them (arrows, bombs, effects). What the modules share:
// runtime/online_actors_internal.h
#include "online_actors_internal.h"

// OnlinePose's channels: (slot, bank)
const int oa_k_channel[ONLINE_AS_CHANNELS][2] = {{0, 0}, {1, 0}, {1, 1}, {2, 0}};
// the bones gear is on (OnlinePose.gear_bone - 1): hands, the back's mounts (Pod_*), ...
const char *const oa_k_bones[] = {"Weapon_R", "Weapon_L", "Pod_A", "Pod_B", "Pod_C", "Spine_2", "Waist", "Skl_Root"};

// OnlineShots' kinds: the game's actors
const char *const oa_k_shot_names[ONLINE_K_COUNT] = {
    "RemoteBomb", "RemoteBombCube", "RemoteBomb2", "RemoteBombCube2", "NormalArrow", "FireArrow", "IceArrow",
    "ElectricArrow", "BombArrow_A", "AncientArrow", "BrightArrow", "PlayerBeam"};
#define IS_BOMB(kind) ((kind) <= ONLINE_K_REMOTE_BOMB_CUBE2)

// Arrows in flight (tools/patches.py _arrow_move: each update of an arrow's flight action, on whatever thread the
// actors update on): their actors, noted in a ring for the frame loop to look at
#define ARROW_RING 64
static uint64_t volatile g_arrow_ring[ARROW_RING];
static volatile int g_arrow_next;

// the Remote Bomb rune's bombs, as it makes them (tools/patches.py _remote_bomb_made; the main thread): his
static uint64_t volatile g_bomb_made[4];
static volatile int g_bomb_next;

void rt_online_bomb(uint64_t bomb) {
    if (!online_version_ok(g_game_version) || !VALID(bomb)) return;
    int k = __atomic_fetch_add(&g_bomb_next, 1, __ATOMIC_RELAXED);
    g_bomb_made[k & 3] = bomb;
}

void rt_online_arrow(uint64_t action) {
    if (!online_version_ok(g_game_version)) return;
    uint64_t a = *(uint64_t *)(uintptr_t)(action + 8);
    if (!VALID(a)) return;
    int k = __atomic_fetch_add(&g_arrow_next, 1, __ATOMIC_RELAXED);
    g_arrow_ring[k & (ARROW_RING - 1)] = a;
}

// The effects the local Link and what he holds play (tools/patches.py _xlink_emit: xlink2::UserInstance::emit, 1.9.0
// 0x7000f51054, every effect and sound any actor plays, from its AS, its AI or by name): x0 the user instance, x1 the
// locator (+8 its asset call table, whose +0 is the asset's key name). An actor's ELink user instance is its +0x568
// +0x48 (0x7001100bd0 emits through it); the frame loop puts Link's and his weapon's, shield's and bow's in g_fx_ui,
// and the emits of those are noted in a ring (any thread) for it to take. His weapon's swing trail ("Blur_Default",
// its own ELink's, as each swing starts) goes to the others in his state (OnlinePose.fx), and they play it on the
// weapon in his puppet's hand. (Footprints, grass and water as he walks are ReactionField's, an actor of the game's
// that plays the ground's reactions for everyone: not his.)
#define FX_WATCH 6  // (Link, his weapon, shield, bow: their ELink; Link's and his weapon's SLink, their sounds)
#define FX_RING 64
#define FX_TRAIL_NS 300000000ull  // (a swing's trail sent this long after it starts: the game's own lasts the swing)
static uint64_t volatile g_fx_ui[FX_WATCH];
static struct { uint64_t name; uint8_t who; } volatile g_fx_ring[FX_RING];
static volatile int g_fx_next;
static int g_fx_taken;
static OnlineFxEvent g_fxev[ONLINE_FX_EVENTS];
static bool g_kago_wind;  // (the last Champion's effect on the local Link: Revali's Gale's charge, Kago_Wind)
static uint16_t g_fxev_seq;
static char g_fx_trail[ONLINE_FX_MAX];  // his weapon's trail now ("" none), and when it started
uint64_t oa_g_fx_trail_t;
uint64_t volatile oa_g_rf, oa_g_rf_ui;  // (ReactionField, the actor that plays the ground's reactions, and its ELink)
Watch oa_g_rf_watch;
static struct { uint64_t name; float pos[3]; uint32_t props[16]; } volatile g_rf_ring[64];
static uint32_t volatile g_rf_props[16];  // (its ELink property values at its last: what it chose to play by)
static volatile int g_rf_next;
static int g_rf_taken;

void rt_online_xlink_emit(uint64_t ui, uint64_t loc) {
    if (!online_version_ok(g_game_version) || !VALID(ui)) return;
    if (ui == oa_g_rf_ui && oa_g_rf_ui && VALID(loc)) {  // (ReactionField's: what and where, its matrix as it plays it)
        uint64_t t = *(uint64_t *)(uintptr_t)(loc + 8), nm = VALID(t) ? *(uint64_t *)(uintptr_t)t : 0;
        if (VALID(nm)) {
            int k = __atomic_fetch_add(&g_rf_next, 1, __ATOMIC_RELAXED) & 63;
            const float *m = (const float *)(uintptr_t)(oa_g_rf + 0x398);
            g_rf_ring[k].name = nm, g_rf_ring[k].pos[0] = m[3], g_rf_ring[k].pos[1] = m[7], g_rf_ring[k].pos[2] = m[11];
            uint64_t pv = *(uint64_t *)(uintptr_t)(ui + 0x70);  // (its 18 words; 16 of them sent)
            if (VALID(pv)) memcpy((void *)g_rf_props, (const void *)(uintptr_t)pv, sizeof g_rf_props);
            memcpy((void *)g_rf_ring[k].props, (const void *)g_rf_props, sizeof g_rf_props);
        }
        return;
    }
    int who = 0;
    while (who < FX_WATCH && g_fx_ui[who] != ui) who++;
    static int all = -1;  // (SAVAGEWIND_ONLINE_FX_ALL=1: every actor's to the log, to find which one plays an effect)
    if (all < 0) all = getenv("SAVAGEWIND_ONLINE_FX_ALL") != NULL;
    if ((who == FX_WATCH && !all) || !VALID(loc)) return;
    uint64_t table = *(uint64_t *)(uintptr_t)(loc + 8);
    if (!VALID(table)) return;
    uint64_t name = *(uint64_t *)(uintptr_t)table;
    if (!VALID(name)) return;
    if (who == FX_WATCH) {
        LOG("[online] fx other %llx: %.60s\n", (unsigned long long)ui, (const char *)(uintptr_t)name);
        return;
    }
    int k = __atomic_fetch_add(&g_fx_next, 1, __ATOMIC_RELAXED);
    g_fx_ring[k & (FX_RING - 1)].name = name;
    g_fx_ring[k & (FX_RING - 1)].who = (uint8_t)who;
}

// the effects' error lines (tools/patches.py _fmt128): x1 the format, x2 / x3 its first arguments (SAVAGEWIND_EFF_ERR_LOG=1)
void rt_game_fmt(uint64_t fmt, uint64_t a, uint64_t b) {
    static int on = -1;
    if (on < 0) on = getenv("SAVAGEWIND_EFF_ERR_LOG") != NULL;
    if (!on || !VALID(fmt)) return;
    const char *f = (const char *)(uintptr_t)fmt;
    if (strncmp(f, "EFF_ERR", 7) && strncmp(f, "SwordBlur", 9)) return;
    static int n;
    if (++n > 400) return;
    LOG("[eff] %.100s | %.60s | %.60s\n", f, VALID(a) ? (const char *)(uintptr_t)a : "", VALID(b) ? (const char *)(uintptr_t)b : "");
}

// A model's skeletal animation slots (actor +0x4e0 its model, +0xd0 its animation control: +0x40 the slots' count, +0x48
// the slots, 0xa8 each: +0 the animation playing, its index in the model's table (+0x60 / +0x68) << 16, -1 none; +0x20
// its frame's float). SwordBlur's emitter callback (1.9.0 0x7001510a90) finds the trail it draws by the animation in
// slots 0-2 and its frame, against its actor's AnimInfo trail entries (+0x570 +0x148 +0x2c0: the player's, which the
// puppets have too, tools/online_mod.py). A puppet is posed from his skeleton and plays no AS (one played would pose
// its bones over his skeleton): its slots are empty. His slots go in his state while his trail plays, and SwordBlur's
// reads of a puppet's slots get his (tools/patches.py _blur_anim, _blur_frame_a / _b, _blur_anim_draw)
static void anim_slots(uint64_t actor, int16_t idx[3], float frame[3]) {
    uint64_t m = VALID(actor) ? *(uint64_t *)(uintptr_t)(actor + 0x4e0) : 0;
    uint64_t ctl = VALID(m) ? *(uint64_t *)(uintptr_t)(m + 0xd0) : 0;
    if (!VALID(ctl)) return;
    int n = *(int *)(uintptr_t)(ctl + 0x40);
    uint64_t arr = *(uint64_t *)(uintptr_t)(ctl + 0x48);
    for (int k = 0; k < 3 && k < n && VALID(arr); k++) {
        int32_t id = *(int32_t *)(uintptr_t)(arr + 0xa8 * k);
        uint64_t fp = *(uint64_t *)(uintptr_t)(arr + 0xa8 * k + 0x20);
        idx[k] = id == -1 ? -1 : (int16_t)(id >> 16);
        frame[k] = VALID(fp) ? *(float *)(uintptr_t)fp : 0;
    }
}

uint64_t oa_slink_ui(uint64_t actor) {  // (an actor's SLink user instance: its +0x568 +0x50)
    if (!VALID(actor)) return 0;
    uint64_t h = *(uint64_t *)(uintptr_t)(actor + 0x568);
    return VALID(h) ? *(uint64_t *)(uintptr_t)(h + 0x50) : 0;
}

uint64_t oa_elink_ui(uint64_t actor) {
    if (!VALID(actor)) return 0;
    uint64_t h = *(uint64_t *)(uintptr_t)(actor + 0x568);
    return VALID(h) ? *(uint64_t *)(uintptr_t)(h + 0x48) : 0;
}

// each frame: what to watch; the effects noted since (SAVAGEWIND_ONLINE_FX_LOG=1: to the log); his weapon's trail
// A sound the local game emits continuously (its volume his speed: EquipNoise_* his gear's rattle, WindDirect and
// FallFlap* the wind of his fall - FallFlap once as it starts, a full-volume roar on the puppet -, ReactionSlide his
// slide; and any other emitted again within CONT_SOUND_NS, from then on)
// isn't sent: the others' games could only play it whole, at full volume (fx_update)
#define CONT_SOUND_NS 300000000ull
static bool continuous_sound(const char *n) {
    // (the paraglider's wind, ParashawlGlideMove / ParashawlGlideAscendant: its first one sent - before it was seen
    // again - played whole on his puppet, a loud hiss for seconds as he rose on Revali's Gale)
    static const char *const known[] = {"EquipNoise", "WindDirect", "ReactionSlide", "FallFlap", "ParashawlGlide"};
    for (unsigned k = 0; k < sizeof known / sizeof *known; k++)
        if (!strncmp(n, known[k], strlen(known[k]))) return true;
    static struct { uint32_t h; uint64_t t; bool cont; } seen[64];
    static int next;
    uint32_t h = oa_crc32_str(n);
    uint64_t now = plat_now_ns();
    for (int k = 0; k < 64; k++) {
        if (seen[k].h != h) continue;
        if (!seen[k].cont && now - seen[k].t < CONT_SOUND_NS) {
            seen[k].cont = true;
            LOG("[online] %s: a continuous sound, not sent\n", n);
        }
        seen[k].t = now;
        return seen[k].cont;
    }
    seen[next].h = h, seen[next].t = now, seen[next].cont = false;
    next = (next + 1) % 64;
    return false;
}

void oa_fx_update(uint64_t player) {
    // the ground's reactions to his steps (ReactionField plays them: his footprints, the dust, the water, the grass):
    // sent as his effects (who 4), the others play them on theirs where his puppet is
    static int rf_tick;
    if (VALID(player)) {
        if (VALID(oa_g_rf) && !oa_alive(oa_g_rf, oa_g_rf_watch)) oa_g_rf = 0, oa_g_rf_ui = 0;  // (a load: made again)
        if (rf_tick++ % 300 == 0 || !VALID(oa_g_rf)) {  // (ReactionField: found among the actors)
            int na = rf_tick % 30 == 1 || rf_tick % 300 == 1 ? oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS) : 0;
            for (int k = 0; k < na; k++) {
                char nm[48];
                oa_actor_name_of(oa_g_actors[k], nm);
                if (!strcmp(nm, "ReactionField")) {
                    oa_g_rf = oa_g_actors[k], oa_g_rf_ui = oa_elink_ui(oa_g_actors[k]), oa_g_rf_watch = oa_watch(oa_g_actors[k]);
                    break;
                }
            }
        }
        int end = __atomic_load_n(&g_rf_next, __ATOMIC_ACQUIRE);
        if (end - g_rf_taken > 64) g_rf_taken = end - 64;
        const float *pm = (const float *)(uintptr_t)(player + 0x398);
        static float rf_was[2];  // (where he was the frame before: moving, more than 1 cm a frame)
        float rdx = pm[3] - rf_was[0], rdz = pm[11] - rf_was[1];
        bool rf_moving = rdx * rdx + rdz * rdz > 0.01f * 0.01f && rdx * rdx + rdz * rdz < 4.0f;
        rf_was[0] = pm[3], rf_was[1] = pm[11];
        static int rf_log = -1;
        if (rf_log < 0) rf_log = getenv("SAVAGEWIND_ONLINE_RF_LOG") != NULL;
        for (; g_rf_taken < end; g_rf_taken++) {
            int k = g_rf_taken & 63;
            const char *n = (const char *)(uintptr_t)g_rf_ring[k].name;
            if (rf_log)
                LOG("[online] rf %.40s at %.2f %.2f %.2f (Link %.2f %.2f %.2f) %x %x %x %x %x %x %x %x %x %x %x %x\n", n,
                    g_rf_ring[k].pos[0], g_rf_ring[k].pos[1], g_rf_ring[k].pos[2], pm[3], pm[7], pm[11],
                    g_rf_ring[k].props[0], g_rf_ring[k].props[1], g_rf_ring[k].props[2], g_rf_ring[k].props[3],
                    g_rf_ring[k].props[4], g_rf_ring[k].props[5], g_rf_ring[k].props[6], g_rf_ring[k].props[7],
                    g_rf_ring[k].props[8], g_rf_ring[k].props[9], g_rf_ring[k].props[10], g_rf_ring[k].props[11]);
            // (ReactionField plays the others' too, every actor's steps about: his only while he moves, one of a name a
            // quarter of a second; not Footprint, which isn't made by its name in the others' games)
            if (!VALID(g_rf_ring[k].name) || strstr(n, "Dummy") || strstr(n, "Fish") || !strcmp(n, "Footprint") ||
                strlen(n) >= sizeof g_fxev[0].name || !rf_moving)
                continue;
            static char rf_last[24];
            static uint64_t rf_last_t;
            uint64_t t = plat_now_ns();
            if (!strcmp(rf_last, n) && t - rf_last_t < 250000000ull) continue;
            snprintf(rf_last, sizeof rf_last, "%s", n), rf_last_t = t;
            OnlineFxEvent *e = &g_fxev[g_fxev_seq % ONLINE_FX_EVENTS];
            if (!++g_fxev_seq) g_fxev_seq = 1;
            e->seq = g_fxev_seq, e->who = 4;
            snprintf(e->name, sizeof e->name, "%s", n);
        }
    }
    g_fx_ui[0] = oa_elink_ui(player);
    for (int k = 0; k < 3; k++) g_fx_ui[1 + k] = VALID(player) ? oa_elink_ui(oa_linked(player + 0xb98 + 0x18 * k)) : 0;
    g_fx_ui[4] = oa_slink_ui(player);
    g_fx_ui[5] = VALID(player) ? oa_slink_ui(oa_linked(player + 0xb98)) : 0;
    int end = __atomic_load_n(&g_fx_next, __ATOMIC_ACQUIRE);
    if (end - g_fx_taken > FX_RING) g_fx_taken = end - FX_RING;
    static const char *const who[FX_WATCH] = {"Link", "weapon", "shield", "bow", "Link sound", "weapon sound"};
    for (; g_fx_taken < end; g_fx_taken++) {
        uint64_t name = g_fx_ring[g_fx_taken & (FX_RING - 1)].name;
        int w = g_fx_ring[g_fx_taken & (FX_RING - 1)].who;
        if (!VALID(name) || w >= FX_WATCH) continue;
        const char *n = (const char *)(uintptr_t)name;
        if (getenv("SAVAGEWIND_ONLINE_FX_LOG")) LOG("[online] fx %s: %.60s\n", who[w], n);
        if (w == 0 && !strncmp(n, "Kago_", 5) && strcmp(n, "Kago_Ready")) g_kago_wind = !strcmp(n, "Kago_Wind");
        // (not his trail, nor the Champions' abilities: both their own way; nor the environment's, MaterialEnv,
        // FieldEnvEffect_*: his place's, not his)
        // (Daruk's and Revali's, Kago_Shield* and Kago_Wind, are sent by their handles in the player; Mipha's Grace
        // and Urbosa's Fury, Kago_Heal, Kago_Lightning / Kago_UrubozaRange..., go as these)
        // (his sounds, his Link's SLink and his weapon's, as his effects: who 2 and 3)
        // (nor his continuous sounds: his gear's rattle, the wind of his fall, his slide, any sound the game emits again
        // within CONT_SOUND_NS: it plays them every frame with his speed as their volume; emitted by their keyword on
        // the puppet each played whole at full volume, piling up into walls of noise as he walked, jumped and slid)
        bool sound = w >= 4;
        if (sound && continuous_sound(n)) continue;
        if ((w <= 1 || sound) && strncmp(n, "Blur", 4) && strncmp(n, "Kago_Shield", 11) && strcmp(n, "Kago_Wind") &&
            strncmp(n, "MaterialEnv", 11) &&
            strncmp(n, "FieldEnv", 8) && strlen(n) < sizeof g_fxev[0].name) {
            OnlineFxEvent *e = &g_fxev[g_fxev_seq % ONLINE_FX_EVENTS];
            if (!++g_fxev_seq) g_fxev_seq = 1;
            e->seq = g_fxev_seq, e->who = (uint8_t)(sound ? w - 2 : w);
            snprintf(e->name, sizeof e->name, "%s", n);
        }
        if (w == 1 && !strncmp(n, "Blur", 4) && strlen(n) < ONLINE_FX_MAX)
            snprintf(g_fx_trail, sizeof g_fx_trail, "%s", n), oa_g_fx_trail_t = plat_now_ns();
    }
    if (g_fx_trail[0] && plat_now_ns() - oa_g_fx_trail_t > FX_TRAIL_NS) g_fx_trail[0] = 0;
}

// The local Link's element arrows lately: where each was last seen (in flight, or where it went: it hit there), so a
// puppet hit near one is known to be hit by that element (ONLINE_ELEM_*: the puppet, without a chemical, isn't set
// burning, frozen or shocked itself; its owner's game is told so)
#define ELEM_SEEN 8
static struct { uint8_t elem; float pos[3]; uint64_t t; } g_elem_seen[ELEM_SEEN];  // (one per shot slot)

static uint8_t elem_of_kind(int kind) {
    return kind == ONLINE_K_FIRE_ARROW ? ONLINE_ELEM_FIRE : kind == ONLINE_K_ICE_ARROW ? ONLINE_ELEM_ICE
         : kind == ONLINE_K_ELECTRIC_ARROW ? ONLINE_ELEM_ELECTRIC : ONLINE_ELEM_NONE;
}

// the element of one of the local Link's arrows seen within r of p in the last half second, else ONLINE_ELEM_NONE
uint8_t oa_elem_near(const float p[3], float r) {
    uint64_t now = plat_now_ns();
    for (int k = 0; k < ELEM_SEEN; k++) {
        if (!g_elem_seen[k].elem || now - g_elem_seen[k].t > 500000000ull) continue;
        float dx = g_elem_seen[k].pos[0] - p[0], dy = g_elem_seen[k].pos[1] - p[1], dz = g_elem_seen[k].pos[2] - p[2];
        if (dx * dx + dy * dy + dz * dz < r * r) return g_elem_seen[k].elem;
    }
    return ONLINE_ELEM_NONE;
}

// What the local Link threw or shot: his rune's bombs (rt_online_bomb), his arrows in flight (rt_online_arrow), each
// followed from then until it goes (held, thrown, flying, stuck: as it is in the world), a bomb that goes having gone
// off (the Remote Bomb's bombs go when they do)
static void sample_shots(Cpu *c, uint64_t player, const char (*as)[ONLINE_AS_MAX], OnlineShots *out) {
    static struct { uint64_t a; Watch w; uint16_t id; uint8_t kind; bool boomed; float m[12]; } tr[ONLINE_SHOTS];
    static uint16_t next_id = 1, boom_seq;
    static struct { uint16_t seq; uint8_t kind; float pos[3]; int frames; } booms[ONLINE_BOOMS];
    // the bombs his rune made
    static uint64_t bomb_seen[4];
    for (int r = 0; r < 4; r++) {
        uint64_t held = g_bomb_made[r];
        if (!held || held == bomb_seen[r]) continue;
        bomb_seen[r] = held;
        char name[48];
        oa_actor_name_of(held, name);
        int kind = -1;
        for (int k = 0; IS_BOMB(k); k++)
            if (!strcmp(name, oa_k_shot_names[k])) kind = k;
        bool known = false;
        for (int k = 0; k < ONLINE_SHOTS; k++) known |= tr[k].a == held;
        for (int k = 0; kind >= 0 && !known && k < ONLINE_SHOTS; k++)
            if (!tr[k].a) {
                tr[k].a = held, tr[k].w = oa_watch(held), tr[k].kind = (uint8_t)kind, tr[k].id = next_id++;
                LOG("[online] shot %d: %s %llx\n", tr[k].id, name, (unsigned long long)held);
                if (!next_id) next_id = 1;
                break;
            }
    }
    // arrows in flight his own: those first seen near him while he is using his bow (any of his AS channels in a
    // Player_Bow* or *Bow* state - BowCharge, BowShoot, BowReload, SquatBow*, HorseBow*, BowBoard* - now or in the
    // last second); his beams (only his: anywhere). An arrow that fails either when first seen is never his: kept as
    // such (by its actor and its id), else the ring, which notes each arrow again on each update, took an enemy's
    // arrow as his once it came near him (it was sent as his shot, and a fire arrow of an enemy's landing by him gave
    // his next hit on a puppet its element). Distance alone wasn't enough: an archer within 4 m (on a tower right
    // above him) had all its arrows sent as his.
    static uint64_t seen[ARROW_RING];
    static struct { uint64_t a; uint32_t id; } not_his[64];
    static int not_his_next;
    static uint64_t bow_t, shoot_t;
    const float *me = (const float *)(uintptr_t)(player + 0x398);
    uint64_t now_ns = plat_now_ns();
    for (int ch = 0; ch < ONLINE_AS_CHANNELS; ch++) {
        if (strstr(as[ch], "Bow")) bow_t = now_ns;
        if (strstr(as[ch], "BowShoot")) shoot_t = now_ns;
    }
    bool bowing = bow_t && now_ns - bow_t < 1000000000ull;
    for (int r = 0; r < ARROW_RING; r++) {
        uint64_t a = g_arrow_ring[r];
        if (!a || a == seen[r]) continue;
        seen[r] = a;
        bool known2 = false;
        for (int k = 0; k < ONLINE_SHOTS; k++) known2 |= tr[k].a == a;
        if (known2) continue;
        uint32_t aid = *(uint32_t *)(uintptr_t)(a + 0x60);
        bool rejected = false;
        for (int k = 0; k < 64 && !rejected; k++) rejected = not_his[k].a == a && not_his[k].id == aid;
        if (rejected) continue;
        char an[48];
        oa_actor_name_of(a, an);
        int ak = -1;
        for (int k = ONLINE_K_NORMAL_ARROW; k < ONLINE_K_COUNT; k++)
            if (!strcmp(an, oa_k_shot_names[k])) ak = k;
        const float *am = (const float *)(uintptr_t)(a + 0x398);
        float dx = am[3] - me[3], dy = am[7] - me[7], dz = am[11] - me[11];
        if (ak >= 0 && getenv("SAVAGEWIND_ONLINE_SHOT_LOG"))
            LOG("[online] arrow %s first seen %.1f m from us, %s, BowShoot %lld ms ago\n", an,
                sqrtf(dx * dx + dy * dy + dz * dz), bowing ? "bowing" : "not bowing",
                shoot_t ? (long long)((now_ns - shoot_t) / 1000000) : -1LL);
        if (ak >= 0 && ak != ONLINE_K_BEAM && (!bowing || dx * dx + dy * dy + dz * dz > 4 * 4)) {
            if (!bowing && dx * dx + dy * dy + dz * dz <= 4 * 4)
                LOG("[online] %s near us, not ours (we aren't using a bow)\n", an);
            not_his[not_his_next].a = a, not_his[not_his_next].id = aid, not_his_next = (not_his_next + 1) % 64;
            continue;
        }
        if (ak < 0) continue;
        if (ak == ONLINE_K_BEAM) LOG("[online] his beam %llx\n", (unsigned long long)a);
        for (int k = 0; k < ONLINE_SHOTS; k++)
            if (!tr[k].a) {
                tr[k].a = a, tr[k].w = oa_watch(a), tr[k].kind = (uint8_t)ak, tr[k].id = next_id++;
                if (!next_id) next_id = 1;
                break;
            }
    }
    memset(out, 0, sizeof *out);
    for (int k = 0; k < ONLINE_SHOTS; k++) {
        if (!tr[k].a) continue;
        bool gone = !oa_alive(tr[k].a, tr[k].w);
        // a bomb goes off as it starts its AS "Explode" (it goes some frames later)
        if (!gone) memcpy(tr[k].m, (const float *)(uintptr_t)(tr[k].a + 0x398), sizeof tr[k].m);
        if (k < ELEM_SEEN && elem_of_kind(tr[k].kind)) {  // (an element arrow: where it is, or went, as of now)
            g_elem_seen[k].elem = elem_of_kind(tr[k].kind), g_elem_seen[k].t = plat_now_ns();
            g_elem_seen[k].pos[0] = tr[k].m[3], g_elem_seen[k].pos[1] = tr[k].m[7], g_elem_seen[k].pos[2] = tr[k].m[11];
        }
        // (gone without it: put away, cancelled - no explosion)
        if (IS_BOMB(tr[k].kind) && !tr[k].boomed && !gone && !strcmp(oa_playing(c, tr[k].a, 0, 0), "Explode")) {
            memmove(&booms[1], &booms[0], sizeof booms[0] * (ONLINE_BOOMS - 1));
            booms[0].seq = ++boom_seq ? boom_seq : ++boom_seq, booms[0].kind = tr[k].kind, booms[0].frames = 0;
            booms[0].pos[0] = tr[k].m[3], booms[0].pos[1] = tr[k].m[7], booms[0].pos[2] = tr[k].m[11];
            tr[k].boomed = true;
            LOG("[online] shot %d (%s) went off%s\n", tr[k].id, oa_k_shot_names[tr[k].kind], gone ? " (gone)" : "");
        }
        if (gone) {
            LOG("[online] shot %d (%s) gone\n", tr[k].id, oa_k_shot_names[tr[k].kind]);
            memset(&tr[k], 0, sizeof tr[k]);
            continue;
        }
        if (tr[k].boomed) continue;  // (going off: no longer shown as a bomb)
        out->shot[out->n].id = tr[k].id, out->shot[out->n].kind = tr[k].kind;
        memcpy(out->shot[out->n].m, tr[k].m, sizeof tr[k].m);
        out->n++;
    }
    for (int k = 0; k < ONLINE_BOOMS; k++)  // (each sent for two seconds: the others see it once by its number)
        if (booms[k].seq && booms[k].frames++ < 120) {
            out->boom[k].seq = booms[k].seq, out->boom[k].kind = booms[k].kind;
            memcpy(out->boom[k].pos, booms[k].pos, sizeof booms[k].pos);
        }
}

// What the local Link holds from his pouch over his head (Item_*, Animal_Insect_*: materials, food), for the others to
// see in his puppet's hands, and each he then throws or lets fall, where it is while it flies (it goes on the loot list
// once it lies: drops_update). One he holds is linked to him (+0x4d4 non-zero, as the slate on his hip: 0 on the
// ground); it is sent as in his hands (heaped there by the others' games), and as it flies only once it has left where
// it was while he held it. Looked for every 6 frames (all the actors), followed each frame; 1.5 s still after it left
// his hands, no longer sent
static bool held_kind(const char *n) {
    return (!strncmp(n, "Item_", 5) || !strncmp(n, "Animal_Insect_", 14)) && strcmp(n, "Item_Conductor") &&
           strncmp(n, "Item_Magnetglove", 16);
}

static void sample_held(uint64_t player, OnlineHeld out[ONLINE_HELD]) {
    static struct { uint64_t a; Watch w; char name[32]; float stale[3], last[3]; bool left, flying; uint64_t still_t; } tr[ONLINE_HELD];
    static int tick;
    memset(out, 0, sizeof(OnlineHeld) * ONLINE_HELD);
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    if (tick++ % 6 == 0) {
        int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
        for (int k = 0; k < na; k++) {
            uint64_t a = oa_g_actors[k];
            if (a == player || !*(uint32_t *)(uintptr_t)(a + 0x4d4) || *(uint32_t *)(uintptr_t)(a + ACTOR_HASH_ID)) continue;
            bool known = false;
            for (int t = 0; t < ONLINE_HELD && !known; t++) known = tr[t].a == a;
            if (known) continue;
            char n[48];
            oa_actor_name_of(a, n);
            if (!held_kind(n)) continue;
            const float *m = (const float *)(uintptr_t)(a + 0x398);
            float dx = m[3] - pm[3], dz = m[11] - pm[11];
            if (dx * dx + dz * dz > 60 * 60) continue;
            for (int t = 0; t < ONLINE_HELD; t++)
                if (!tr[t].a) {
                    memset(&tr[t], 0, sizeof tr[t]);
                    tr[t].a = a, tr[t].w = oa_watch(a);
                    snprintf(tr[t].name, sizeof tr[t].name, "%s", n);
                    tr[t].stale[0] = m[3], tr[t].stale[1] = m[7], tr[t].stale[2] = m[11];
                    LOG("[online] we hold %s\n", n);
                    break;
                }
        }
    }
    uint64_t now = plat_now_ns();
    for (int t = 0; t < ONLINE_HELD; t++) {
        if (!tr[t].a) continue;
        if (!oa_alive(tr[t].a, tr[t].w)) {  // (eaten, put back, picked up)
            tr[t].a = 0;
            continue;
        }
        const float *m = (const float *)(uintptr_t)(tr[t].a + 0x398);
        snprintf(out[t].name, sizeof out[t].name, "%s", tr[t].name);
        if (*(uint32_t *)(uintptr_t)(tr[t].a + 0x4d4)) {  // (in his hands)
            out[t].mode = ONLINE_HELD_HANDS;
            out[t].m[0] = *(float *)(uintptr_t)(tr[t].a + 0x418);  // (the size it is held at: its own - an apple 0.55, a
                                                                   // mushroom 0.372 - smaller than on the ground)
            tr[t].stale[0] = m[3], tr[t].stale[1] = m[7], tr[t].stale[2] = m[11], tr[t].left = false;
            continue;
        }
        float sx = m[3] - tr[t].stale[0], sy = m[7] - tr[t].stale[1], sz = m[11] - tr[t].stale[2];
        if (!tr[t].left && sx * sx + sy * sy + sz * sz < 0.05f * 0.05f) continue;  // (not yet at his hands)
        if (!tr[t].left) {
            tr[t].left = true, tr[t].still_t = 0;
            tr[t].last[0] = m[3], tr[t].last[1] = m[7], tr[t].last[2] = m[11];
            LOG("[online] %s left our hands\n", tr[t].name);
        }
        float lx = m[3] - tr[t].last[0], ly = m[7] - tr[t].last[1], lz = m[11] - tr[t].last[2];
        tr[t].last[0] = m[3], tr[t].last[1] = m[7], tr[t].last[2] = m[11];
        if (lx * lx + ly * ly + lz * lz > 0.01f * 0.01f) tr[t].still_t = 0;
        else if (!tr[t].still_t) tr[t].still_t = now;
        if (tr[t].still_t && now - tr[t].still_t > 1500000000ull) {
            tr[t].a = 0;  // (lying: on the loot list by now, the others' own)
            continue;
        }
        out[t].mode = ONLINE_HELD_WORLD;
        memcpy(out[t].m, m, sizeof out[t].m);
    }
}
char oa_g_map[32];
static void local_map(Cpu *c, char out[32]) {
    uint64_t t = oa_call(c, FN_MAP_TYPE, 0, 0, 0, 0, 0, 0), n = oa_call(c, FN_MAP_NAME, 0, 0, 0, 0, 0, 0);
    const char *ts = VALID(t) ? *(const char **)(uintptr_t)(t + 8) : NULL, *ns = VALID(n) ? *(const char **)(uintptr_t)(n + 8) : NULL;
    char m[32];
    snprintf(m, sizeof m, "%s/%s", ts && VALID((uint64_t)(uintptr_t)ts) ? ts : "", ns && VALID((uint64_t)(uintptr_t)ns) && strcmp(ts ? ts : "", "MainField") ? ns : "");
    if (strcmp(m, oa_g_map)) LOG("[online] map: %s\n", m), snprintf(oa_g_map, sizeof oa_g_map, "%s", m);
    memcpy(out, oa_g_map, 32);
}
// whether another player is on our map (unknown: as on ours)
bool oa_same_map(const OnlinePlayer *p) {
    return !p->pose.map[0] || !oa_g_map[0] || !strcmp(p->pose.map, oa_g_map);
}
// The local Link's pose for the others: his AS on each channel, his heading, and each piece of his gear on the bone
// nearest to it (the hands while drawn, the back's mounts while put away), as it sits on that bone
void oa_sample_pose(Cpu *c, uint64_t pinfo) {
    OnlinePose pose;
    memset(&pose, 0, sizeof pose);
    uint64_t player = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
    if (!VALID(player)) return;
    uint64_t mgr = *(uint64_t *)(uintptr_t)(player + 0x560);
    if (!VALID(mgr)) return;
    int slots = *(int *)(uintptr_t)(mgr + 0xb8);
    for (int ch = 0; ch < ONLINE_AS_CHANNELS; ch++) {
        int sl = oa_k_channel[ch][0], bank = oa_k_channel[ch][1];
        if (sl >= slots || bank >= *(int *)(uintptr_t)(*(uint64_t *)(uintptr_t)(mgr + 0xc0) + sl * 0x50 + 0x20)) continue;
        snprintf(pose.as[ch], ONLINE_AS_MAX, "%s", oa_playing(c, player, sl, bank));
    }
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    pose.yaw = atan2f(pm[2], pm[10]);
    uint64_t armor[ARMOR], gear[ONLINE_GEAR];
    oa_equipment(player, armor, gear);
    if (strncmp(pose.as[0], "Para", 4) || !strcmp(pose.as[0], "ParaEquipOff")) gear[5] = 0;  // (his paraglider: open)
    {   // his weapon thrown: it leaves his equipment as it leaves his hand, so it is kept as his while it flies and 0.3 s
        // after it comes to rest; then it lies in his world, and goes on the loot list (loot_update): the others' games
        // make it where it lies, a real weapon they can pick up (and one picked up goes in every game)
        // (on the list as it leaves his hand: the others make the real one at once and move it where his flies, so it
        // can be picked up in the air; shown until it has lain 3 s. Followed even when he draws another before it lands:
        // then it isn't shown flying, and theirs falls from where it left his hand)
        static uint64_t held, thrown, listed_at;
        static Watch hw, tw;
        static float last[3];
        static uint64_t still_since;
        uint64_t now = plat_now_ns();
        uint64_t carried[LINK_CARRIED];
        oa_link_carried(player, carried);
        const float *hm = held && oa_alive(held, hw) ? (const float *)(uintptr_t)(held + 0x398) : NULL;
        bool at_hand = hm && (hm[3] - pm[3]) * (hm[3] - pm[3]) + (hm[7] - pm[7]) * (hm[7] - pm[7]) +
                                     (hm[11] - pm[11]) * (hm[11] - pm[11]) < 4.0f * 4.0f;  // (not one put away elsewhere)
        if (held && gear[0] != held && at_hand && !oa_carries(carried, held)) {  // (let go of: followed from here)
            const float *m = (const float *)(uintptr_t)(held + 0x398);
            thrown = held, tw = hw, still_since = 0, listed_at = now;
            last[0] = m[3], last[1] = m[7], last[2] = m[11];
            char tn[48];
            oa_actor_name_of(held, tn);
            if (!strncmp(tn, "Weapon_", 7)) oa_loot_mine_thrown(held, tn);
            if (getenv("SAVAGEWIND_ONLINE_GEAR_LOG")) LOG("[online] our weapon left our hand: followed while it flies\n");
        }
        held = gear[0], hw = gear[0] ? oa_watch(gear[0]) : hw;
        if (thrown && (thrown == gear[0] || oa_carries(carried, thrown))) thrown = 0;  // (his again)
        if (thrown && oa_alive(thrown, tw)) {
            const float *m = (const float *)(uintptr_t)(thrown + 0x398);
            float dx = m[3] - last[0], dy = m[7] - last[1], dz = m[11] - last[2];
            last[0] = m[3], last[1] = m[7], last[2] = m[11];
            if (dx * dx + dy * dy + dz * dz > 0.01f * 0.01f) still_since = 0;
            else if (!still_since) still_since = now;
            if (still_since && now - still_since > 3000000000ull) thrown = 0;
            else if (!gear[0]) gear[0] = thrown;
        } else thrown = 0;
    }
    {   // his shield and bow let go of (dropped for another, from the menu): half a second after (where they fell
        // one swapped from the menu is deleted by then), on the loot list as his thrown weapon
        static uint64_t had[2], let_go[2], since[2];
        static Watch hw2[2], lw[2];
        uint64_t now = plat_now_ns();
        for (int k = 0; k < 2; k++) {
            uint64_t g2 = gear[1 + k];
            if (g2 != had[k] && had[k] && oa_alive(had[k], hw2[k])) let_go[k] = had[k], lw[k] = hw2[k], since[k] = now;
            if (g2) had[k] = g2, hw2[k] = oa_watch(g2);
            else had[k] = 0;
            if (let_go[k] && now - since[k] > 500000000ull) {
                uint64_t carried[LINK_CARRIED];
                oa_link_carried(player, carried);
                char tn[48];
                if (oa_alive(let_go[k], lw[k]) && !oa_carries(carried, let_go[k])) {
                    oa_actor_name_of(let_go[k], tn);
                    if (!strncmp(tn, "Weapon_", 7)) oa_loot_mine_add(let_go[k], tn, 0);
                }
                let_go[k] = 0;
            }
        }
    }
    {   // what he wears and carries, for the others (the default pieces are Link's own body: "")
        char an[ARMOR][48] = {{0}}, gn[ONLINE_GEAR][48] = {{0}};
        for (int k = 0; k < ARMOR; k++) {
            oa_actor_name_of(armor[k], an[k]);
            if (!strncmp(an[k], "Armor_Default", 13)) an[k][0] = 0;
        }
        for (int k = 0; k < ONLINE_GEAR; k++) oa_actor_name_of(gear[k], gn[k]);
        online_set_local_equip((const char (*)[48])an, (const char (*)[48])gn);
    }
    float bm[BONES][12];
    bool have[BONES];
    for (int b = 0; b < BONES; b++) have[b] = oa_bone_mtx(c, player, oa_k_bones[b], bm[b]);
    for (int k = 0; k < ONLINE_GEAR; k++) {
        uint64_t a = gear[k];
        if (!a) continue;
        const float *w = (const float *)(uintptr_t)(a + 0x398);
        static uint8_t last_bone[ONLINE_GEAR];
        // (farther from every bone: thrown, dropped; not shown. The paraglider is over his head, a shield he surfs
        // on under his feet)
        float reach = k == 5 ? 3.0f : k == 1 ? 1.5f : 0.6f;
        float best = reach * reach, kept = -1;
        float rel_of[BONES][12];
        for (int b = 0; b < BONES; b++) {
            if (!have[b]) continue;
            float inv[12], *rel = rel_of[b];
            oa_mtx_inv(bm[b], inv);
            oa_mtx_mul(inv, w, rel);
            float d = rel[3] * rel[3] + rel[7] * rel[7] + rel[11] * rel[11];
            if (b + 1 == last_bone[k]) kept = d;
            if (d < best) best = d, pose.gear_bone[k] = (uint8_t)(b + 1);
        }
        // the bone it was on, while it stays near it and no other is much nearer (two bones as near: no flicker)
        if (kept >= 0 && kept < reach * reach && kept < best * 4) pose.gear_bone[k] = last_bone[k];
        if (pose.gear_bone[k]) memcpy(pose.gear_rel[k], rel_of[pose.gear_bone[k] - 1], sizeof pose.gear_rel[k]);
        last_bone[k] = pose.gear_bone[k];
        // (on no bone but still his and near him - a weapon he threw, in flight or where it fell: its matrix in the
        // world, ONLINE_GEAR_WORLD. Not the paraglider: closed, it isn't sent at all)
        if (!pose.gear_bone[k] && k != 5 && a != oa_magne_held()) {
            float dx = w[3] - pm[3], dy = w[7] - pm[7], dz = w[11] - pm[11];
            if (dx * dx + dy * dy + dz * dz < 60 * 60) {
                pose.gear_bone[k] = ONLINE_GEAR_WORLD;
                memcpy(pose.gear_rel[k], w, sizeof pose.gear_rel[k]);
                static uint8_t logged[ONLINE_GEAR];
                if (!logged[k] && getenv("SAVAGEWIND_ONLINE_GEAR_LOG"))
                    LOG("[online] our gear %d off every bone, %.1f m from us: sent where it is\n", k,
                        sqrtf(dx * dx + dy * dy + dz * dz)), logged[k] = 1;
            }
        }
    }
    pose.valid = pose.as[0][0] != 0;
    {
        static int was = -1;
        if (was != (int)pose.valid) LOG("[online] our pose %s (AS slots %d)\n", pose.valid ? "valid" : "invalid: no AS", slots);
        was = (int)pose.valid;
    }
    {   // at a Sheikah pedestal: the Sheikah Slate's (Demo700_0, its first door Demo701_0) or a tower's (Demo004_0: the map
        // downloaded; Demo164_0: the first tower rising): the others near can't start it meanwhile (not a chest, a talk...)
        extern bool framepace_in_event(void);
        extern const char *framepace_event_name(void);
        static const char *const pedestals[] = {"Obj_DRStone_A_01", "Demo700_0", "Demo701_0", "Demo004_0", "Demo164_0"};
        const char *ev = framepace_in_event() ? framepace_event_name() : "";
        for (int k = 0; k < 5; k++)
            if (!strcmp(ev, pedestals[k])) pose.effects |= ONLINE_E_DEMO;
    }
    {   // Daruk's Protection: the player's own effects of it (1.9.0 0x7100cd39fc): "Kago_Shield" (a hit taken on it) at
        // player +0x2350, "Kago_ShieldCount_0<n>" (its ring while he guards, n charges left: player +0x1cd4) at +0x2360;
        // each a handle (the effect, its id: alive while the effect's +0x20 is that id)
        for (int k = 0; k < 2; k++) {
            uint64_t h = *(uint64_t *)(uintptr_t)(player + 0x2350 + 0x10 * k);
            if (!VALID(h) || *(uint32_t *)(uintptr_t)(h + 0x20) != *(uint32_t *)(uintptr_t)(player + 0x2358 + 0x10 * k)) continue;
            int n = *(int *)(uintptr_t)(player + 0x1cd4);
            if (k == 0) pose.effects |= ONLINE_E_KAGO;
            else if (n >= 1 && n <= 3) pose.effects |= (uint8_t)(n << ONLINE_E_KAGO_COUNT_SHIFT);
        }
    }
    {   // Revali's Gale: its charge's effect handle at player +0x23c0, alive while he holds to charge (found by diffing
        // the player before and during a Gale, 1.9.0; Daruk's handle layout: alive while the effect's +0x20 is the id
        // after it). Its updraft's handle at +0x2380 is a sound's ("Kago_WindRiseUp" is only in SLink2), not sent
        // That handle is the Champions' ready effect's (Kago_Ready), Daruk's guard's too: the Gale only when the last
        // Champion's effect his Link played was its charge, Kago_Wind (else Daruk's shield drew Revali's wind here)
        uint64_t h = *(uint64_t *)(uintptr_t)(player + 0x23c0);
        if (VALID(h) && *(uint32_t *)(uintptr_t)(h + 0x20) == *(uint32_t *)(uintptr_t)(player + 0x23c8) && g_kago_wind)
            pose.effects |= ONLINE_E_GALE;
    }
    {   // the Magnesis rune: what he holds (1.9.0: the rune's object at 0x7001e15f18, its +0x318 the magnetic part of
        // the actor held, 0x7000b993e8 checks it so; that part on while its +0x10 bit 0; its actor at +8)
        static uint64_t held;
        static float from[3];
        uint64_t a = oa_magne_held();
        const float *am = a ? (const float *)(uintptr_t)(a + 0x398) : NULL;
        if (a != held) {
            held = a;
            if (a) from[0] = am[3], from[1] = am[7], from[2] = am[11];
            char n[48] = "";
            if (a) oa_actor_name_of(a, n);
            LOG("[online] magnesis: %s\n", a ? n : "let go");
        }
        if (a) {
            char n[48];
            oa_actor_name_of(a, n);
            snprintf(pose.grab.name, sizeof pose.grab.name, "%s", n);
            memcpy(pose.grab.from, from, sizeof from);
            memcpy(pose.grab.m, am, sizeof pose.grab.m);
        }
    }
    {   // the Stasis rune: what he stopped (an actor near him drawn as stopped: its +0x710 bit 3, as the rune sets it;
        // bit 4 while he aims at it)
        static uint64_t stopped;
        static Watch sw;
        static float from[3];
        static int tick;
        if (stopped && (!oa_alive(stopped, sw) || !(*(uint32_t *)(uintptr_t)(stopped + 0x710) & 8))) {
            LOG("[online] stasis: over\n");
            stopped = 0;
        }
        if (!stopped && tick++ % 6 == 0) {
            int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
            const float *pm = (const float *)(uintptr_t)(player + 0x398);
            for (int k = 0; k < na && !stopped; k++) {
                uint64_t a = oa_g_actors[k];
                if (a == player || !(*(uint32_t *)(uintptr_t)(a + 0x710) & 8) ||
                    !VALID(*(uint64_t *)(uintptr_t)(a + 0x4e0)) || !oa_has_body(a))
                    continue;  // (a physical object with a model: no tag; the cheap checks first)
                bool theirs = false;  // (one another player stopped, shown here: not ours)
                theirs = oa_rune_stopped(a);
                if (theirs) continue;
                const float *m = (const float *)(uintptr_t)(a + 0x398);
                if ((m[3] - pm[3]) * (m[3] - pm[3]) + (m[11] - pm[11]) * (m[11] - pm[11]) > 60 * 60) continue;
                stopped = a, sw = oa_watch(a), from[0] = m[3], from[1] = m[7], from[2] = m[11];
                char n[48];
                oa_actor_name_of(a, n);
                LOG("[online] stasis: %s\n", n);
            }
        }
        if (stopped) {
            char n[48];
            oa_actor_name_of(stopped, n);
            snprintf(pose.stop.name, sizeof pose.stop.name, "%s", n);
            memcpy(pose.stop.from, from, sizeof from);
            memcpy(pose.stop.m, (const float *)(uintptr_t)(stopped + 0x398), sizeof pose.stop.m);
        }
    }
    {   // the Cryonis rune: his pillars (Obj_IceMakerBlock near him; not those we made for the others)
        static float last[3][12];
        static int n_last, tick;
        if (tick++ % 6 == 0) {
            n_last = 0;
            int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
            const float *pm = (const float *)(uintptr_t)(player + 0x398);
            for (int k = 0; k < na && n_last < 3; k++) {
                uint64_t a = oa_g_actors[k];
                char n[48];
                oa_actor_name_of(a, n);
                if (strcmp(n, "Obj_IceMakerBlock")) continue;
                bool theirs = false;
                theirs = oa_rune_ice(a);
                if (theirs) continue;
                const float *m = (const float *)(uintptr_t)(a + 0x398);
                if ((m[3] - pm[3]) * (m[3] - pm[3]) + (m[11] - pm[11]) * (m[11] - pm[11]) > 100 * 100) continue;
                memcpy(last[n_last++], m, sizeof last[0]);
            }
        }
        pose.ice_n = (uint8_t)n_last;
        memcpy(pose.ice, last, sizeof pose.ice);
    }
    local_map(c, pose.map);
    oa_boss_sample(c, player, &pose);
    oa_g_local_dead = !strncmp(pose.as[0], "Dead", 4);
    sample_shots(c, player, pose.as, &pose.shots);
    sample_held(player, pose.held);
    snprintf(pose.fx, sizeof pose.fx, "%s", g_fx_trail);  // (his weapon's swing trail: fx_update)
    memcpy(pose.fxev, g_fxev, sizeof pose.fxev);  // (his other effects: fx_update)
    memcpy(pose.ground_props, (const void *)g_rf_props, sizeof pose.ground_props);  // (fx_update: his ground's)
    pose.collide = g_online.collide;  // (the host's is the session's: players_collide)
    pose.revive = g_online.revive;    // (and rt_online_has_fairy's)
    pose.nopause = g_online.nopause;  // (and menus_no_pause's)
    {   float *bm = oa_bloody_moon_timer();  // (his Blood Moon timer: bloody_moon_follow)
        pose.bloody_moon = bm ? *bm : -1;
    }
    pose.weather = oa_weather_now();  // (the weather where he is: weather_follow)
    pose.pvp_seq = oa_g_pvp.seq, pose.pvp_target = oa_g_pvp.target, pose.pvp_damage = oa_g_pvp.damage;  // (pvp_update)
    pose.aggro_id = oa_g_aggro_id, pose.aggro_seq = oa_g_aggro_seq;
    memcpy(pose.objs, oa_g_objs_out, sizeof pose.objs);  // (objs_update)
    pose.objs_trunk = oa_g_objs_out_trunk;
    memcpy(pose.movers, oa_g_movers_out, sizeof pose.movers);  // (movers_update)
    memcpy(pose.broken, oa_g_broken, sizeof pose.broken);  // (breaks_update)
    memcpy(pose.trunks_gone, oa_g_trunks_gone, sizeof pose.trunks_gone);
    {   // the horse he rides: while his AS is a horse's (Horse*), the player's horse (GameRomHorse*) nearest him, looked
        // for among the actors a few times a second
        static uint64_t horse, tack[2];
        static Watch hw, tw[2];
        static int tick;
        const char *body = oa_playing(c, player, 0, 0);
        bool riding = strstr(body, "Horse") != NULL || strstr(body, "Motorcycle") != NULL;  // (a horse, the Master Cycle)
        if (!riding) horse = 0, tack[0] = tack[1] = 0;
        else if (!horse || !oa_alive(horse, hw) || tick++ % 20 == 0) {
            const float *pm = (const float *)(uintptr_t)(player + 0x398);
            int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
            float bd = 3.0f;
            uint64_t best = 0;
            for (int k = 0; k < na; k++) {
                char nm[48];
                oa_actor_name_of(oa_g_actors[k], nm);
                if ((strncmp(nm, "GameRomHorse", 12) && strcmp(nm, "GameROMMotorcycle")) || strstr(nm, "Reins") ||
                    strstr(nm, "Saddle")) continue;
                const float *m = (const float *)(uintptr_t)(oa_g_actors[k] + 0x398);
                float dx = m[3] - pm[3], dy = m[7] - pm[7], dz = m[11] - pm[11], d = sqrtf(dx * dx + dy * dy + dz * dz);
                if (d < bd) bd = d, best = oa_g_actors[k];
            }
            tack[0] = tack[1] = 0;  // (its saddle and reins: the nearest to it)
            float td[2] = {1.5f, 1.5f};
            const float *hm0 = best ? (const float *)(uintptr_t)(best + 0x398) : NULL;
            for (int k = 0; hm0 && k < na; k++) {
                char nm[48];
                oa_actor_name_of(oa_g_actors[k], nm);
                int which = !strncmp(nm, "GameRomHorseSaddle_", 19) ? 0 : !strncmp(nm, "GameRomHorseReins_", 18) ? 1 : -1;
                if (which < 0) continue;
                const float *m = (const float *)(uintptr_t)(oa_g_actors[k] + 0x398);
                float dx = m[3] - hm0[3], dy = m[7] - hm0[7], dz = m[11] - hm0[11], d = sqrtf(dx * dx + dy * dy + dz * dz);
                if (d < td[which]) td[which] = d, tack[which] = oa_g_actors[k], tw[which] = oa_watch(oa_g_actors[k]);
            }
            if (best != horse && best) {
                char nm[48];
                oa_actor_name_of(best, nm);
                LOG("[online] riding %s\n", nm);
            }
            horse = best, hw = best ? oa_watch(best) : hw;
        }
        const char *ht = getenv("SAVAGEWIND_ONLINE_HORSE_TEST");  // (test: that horse beside him, standing)
        if (ht && *ht && !horse) {
            const float *pm = (const float *)(uintptr_t)(player + 0x398);
            float hm[12] = {1, 0, 0, pm[3] + 2.5f, 0, 1, 0, pm[7], 0, 0, 1, pm[11]};
            snprintf(pose.horse, sizeof pose.horse, "%s", ht);
            memcpy(pose.horse_m, hm, sizeof hm);
            snprintf(pose.horse_as, sizeof pose.horse_as, "Wait");
        }
        if (horse && oa_alive(horse, hw)) {
            char nm[48];
            oa_actor_name_of(horse, nm);
            snprintf(pose.horse, sizeof pose.horse, "%s", nm);
            memcpy(pose.horse_m, (const float *)(uintptr_t)(horse + 0x398), sizeof pose.horse_m);
            snprintf(pose.horse_as, sizeof pose.horse_as, "%s", oa_playing(c, horse, 0, 0));
            for (int k = 0; k < 2; k++)
                if (tack[k] && oa_alive(tack[k], tw[k])) {
                    oa_actor_name_of(tack[k], nm);
                    snprintf(pose.tack[k], sizeof pose.tack[k], "%s", nm);
                    memcpy(pose.tack_m[k], (const float *)(uintptr_t)(tack[k] + 0x398), sizeof pose.tack_m[k]);
                }
        }
    }
    for (int k = 0; k < 3; k++) {  // (his armour's dyes: the player's armour code, 0x7000cdc43c, makes each piece with
        // its slot's at player +0x1114 / +0x1118 / +0x111c, ArmorDyeColor in its InstParamPack)
        int32_t d = VALID(player) ? *(int32_t *)(uintptr_t)(player + 0x1114 + 4 * k) : -1;
        pose.dye[k] = d >= 0 && d < 128 ? (int8_t)d : -1;
    }
    {   // his Link's ELink property values (its user instance +0x70, ONLINE_FX_PROPS words: the player's ELink,
        // Player_GameRom, has that many; its containers choose what to play by them, a charge's level and the like)
        uint64_t ui = oa_elink_ui(player), pv = VALID(ui) ? *(uint64_t *)(uintptr_t)(ui + 0x70) : 0;
        if (VALID(pv)) memcpy(pose.fx_props, (const void *)(uintptr_t)pv, sizeof pose.fx_props);
    }
    for (int k = 0; k < 3; k++) pose.fx_anim[k] = -1;
    if (pose.fx[0]) anim_slots(player, pose.fx_anim, pose.fx_frame);  // (and the attack it is the trail of)
    online_set_local_pose(&pose);
    if (getenv("SAVAGEWIND_ONLINE_POSE_LOG")) {
        static OnlinePose last;
        if (memcmp(last.as, pose.as, sizeof pose.as) || memcmp(last.gear_bone, pose.gear_bone, sizeof pose.gear_bone))
            LOG("[online] pose %s|%s|%s|%s gear %d %d %d %d %d %d\n", pose.as[0], pose.as[1], pose.as[2], pose.as[3],
                pose.gear_bone[0], pose.gear_bone[1], pose.gear_bone[2], pose.gear_bone[3], pose.gear_bone[4], pose.gear_bone[5]);
        last = pose;
    }
}

// The local Link's skeleton for the others: each bone's local rotation and translation, as his animation left them
void oa_sample_skeleton(Cpu *c, uint64_t pinfo) {
    uint64_t player = *(uint64_t *)(uintptr_t)(pinfo + 0x60);
    int n = 0;
    uint64_t unit = VALID(player) ? oa_skel_unit(c, player, &n) : 0;
    if (!unit || n <= 0) return;
    if (n > ONLINE_BONES) n = ONLINE_BONES;
    // and his bow's, after his
    uint64_t bow = oa_linked(player + 0xb98 + 0x18 * 2);
    int nbow = 0;
    uint64_t bow_unit = VALID(bow) ? oa_first_unit(bow, &nbow) : 0;
    if (!bow_unit || nbow < 0) nbow = 0;
    if (nbow > ONLINE_BOW_BONES) nbow = ONLINE_BOW_BONES;
    if (n > ONLINE_BONES - nbow) n = ONLINE_BONES - nbow;
    static float q[ONLINE_BONES][4], t[ONLINE_BONES][3];
    for (int b = 0; b < n + nbow; b++) {
        float m[12], sc[3];
        oa_call(c, FN_BONE_LOCAL_GET, b < n ? unit : bow_unit, (uint64_t)(uintptr_t)m, (uint64_t)(uintptr_t)sc, b < n ? b : b - n,
             0, 0);
        oa_mtx_to_quat(m, q[b]);
        t[b][0] = m[3], t[b][1] = m[7], t[b][2] = m[11];
    }
    online_set_local_skeleton(n, nbow, (const float (*)[4])q, (const float (*)[3])t);
}
