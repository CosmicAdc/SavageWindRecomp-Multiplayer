// Online play's actors, oa_flags: the flags the players share: the quests, the slate, the Koroks, the Blood Moon, the
// weather. What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"

// The weather: the host's where the others are near him (WEATHER_NEAR: the game's weather is a region's, and each game
// draws its own; far from him each one keeps his own). ksys::world::Manager's current weather (+0x7d1) is set as
// runtime/weather.c holds it (its timer +0x794 at 4: taken up), not in a cutscene (+0x7d6: its own) nor while the
// player holds one himself (Cheats > Weather)
#define WEATHER_NEAR 500.0f
uint8_t oa_weather_now(void) {
    uint64_t wm = g_site.world_mgr ? *(uint64_t *)(uintptr_t)g_site.world_mgr : 0;
    if (!VALID(wm) || !*(uint32_t *)(uintptr_t)(wm + 0x5c0)) return 255;
    uint8_t w = *(volatile uint8_t *)(uintptr_t)(wm + 0x7d1);
    return w < 9 ? w : 255;
}

// The Blood Moon: the time manager (ksys::world::Manager +0x5c8, its first) counts it at +0xc8 in degrees of the day
// (360 a day; the time of day at +0xb8), from WM_BloodyMoonTimer at load; past 2520 (7 days), at midnight, there is
// one (1.9.0 0x7001425fa8), and it starts again at 0. The others keep the host's (his time of day is theirs already),
// so the Blood Moon comes for everyone the same night, and the world's items taken (quests.txt) come back for all
float *oa_bloody_moon_timer(void) {
    uint64_t wm = g_site.world_mgr ? *(uint64_t *)(uintptr_t)g_site.world_mgr : 0;
    if (!VALID(wm) || !*(uint32_t *)(uintptr_t)(wm + 0x5c0)) return NULL;
    uint64_t mgrs = *(uint64_t *)(uintptr_t)(wm + 0x5c8), tm = VALID(mgrs) ? *(uint64_t *)(uintptr_t)mgrs : 0;
    return VALID(tm) ? (float *)(uintptr_t)(tm + 0xc8) : NULL;
}

static void bloody_moon_set(Cpu *c, float v) {
    float *bm = oa_bloody_moon_timer();
    uint64_t mgr = test_warp_gdt_mgr();
    if (!bm || !VALID(mgr)) return;
    uint32_t handle = *(uint32_t *)(uintptr_t)((uint64_t)(uintptr_t)bm - 0xc8 + 0x120);
    if (handle != 0xffffffffu) oa_call_f(c, FN_GDT_SET_F32, mgr, handle, 0, 0, 0, 0, 0, v);
    *bm = v;
}

void oa_bloody_moon_follow(Cpu *c, const OnlinePlayer *pl, int me) {
    const char *test = getenv("SAVAGEWIND_ONLINE_BM_TEST");  // (test: the host's timer set to that once, in the world)
    static bool tested;
    if (test && me == 0 && !tested && oa_bloody_moon_timer()) {
        static int wait;
        if (++wait > 300) bloody_moon_set(c, strtof(test, NULL)), tested = true, LOG("[online] Blood Moon test: %s\n", test);
    }
    const OnlinePlayer *h = &pl[0];  // (the host: slot 0)
    float *bm = oa_bloody_moon_timer();
    if (me <= 0 || !bm || !h->used || !h->pose.valid || h->pose.bloody_moon < 0 || h->pose.bloody_moon > 4000 ||
        (h->flags & ONLINE_F_LOADING))
        return;
    float d = h->pose.bloody_moon - *bm;
    if (d < 30 && d > -30) return;  // (two game hours: his comes a little before or after ours, the same night)
    static int logged;
    if (logged++ < 10) LOG("[online] the host's Blood Moon timer: %.1f (was %.1f)\n", h->pose.bloody_moon, *bm);
    bloody_moon_set(c, h->pose.bloody_moon);
}

void oa_weather_follow(const OnlinePlayer *pl, int me, const float at[3]) {
    if (g_settings.weather > 0) return;
    uint64_t wm = g_site.world_mgr ? *(uint64_t *)(uintptr_t)g_site.world_mgr : 0;
    if (!VALID(wm) || !*(uint32_t *)(uintptr_t)(wm + 0x5c0) || *(volatile uint8_t *)(uintptr_t)(wm + 0x7d6)) return;
    // (whose: of the players within WEATHER_NEAR, the first in the session - the host when he is there - so players
    // together anywhere on the map share one sky, ours when we are the first of them)
    const OnlinePlayer *h = NULL;
    for (int i = 0; i < me && !h; i++) {
        const OnlinePlayer *p = &pl[i];
        if (!p->used || !p->pose.valid || p->pose.weather > 8 || (p->flags & ONLINE_F_LOADING) || !oa_same_map(p)) continue;
        float dx = p->pos[0] - at[0], dy = p->pos[1] - at[1], dz = p->pos[2] - at[2];
        if (dx * dx + dy * dy + dz * dz <= WEATHER_NEAR * WEATHER_NEAR) h = p;
    }
    if (!h) return;
    volatile uint8_t *now = (volatile uint8_t *)(uintptr_t)(wm + 0x7d1);
    if (*now == h->pose.weather) return;
    static int logged;
    if (logged++ < 20) LOG("[online] %s's weather: %d (was %d)\n", h->name, h->pose.weather, *now);
    *now = h->pose.weather;
    *(volatile int32_t *)(uintptr_t)(wm + 0x794) = 4;
}

// the Sheikah Slate, another player having taken it: its pedestal's own event here (Obj_DRStone_A_01<Talk>: the
// cutscene Demo700_0, which sets its flags, gives the slate, wakes the HUD and opens the first door), when this Link is
// near its pedestal and in no event; true once started (its flags are then left to it)
bool oa_slate_event(Cpu *c, uint64_t player) {
    static int tries, wait;
    if (wait > 0) { wait--; return false; }
    extern bool framepace_in_event(void);
    if (framepace_in_event() || tries > 5) return false;
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    float dx = pm[3] + 1125.0f, dz = pm[11] - 1903.2f;
    if (dx * dx + dz * dz > 40 * 40) return false;  // (not in the Shrine of Resurrection: the flags alone)
    int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
    uint64_t stone = 0;
    for (int k = 0; k < na && !stone; k++) {
        char n[48];
        oa_actor_name_of(oa_g_actors[k], n);
        if (!strcmp(n, "Obj_DRStone_A_01")) stone = oa_g_actors[k];
    }
    tries++, wait = 90;
    return oa_start_event(c, stone ? stone : player, "Obj_DRStone_A_01", "Talk");
}

static bool slate_group(const char *n) {
    return !strcmp(n, "IsGet_PortableUnit") || !strcmp(n, "PortableUnit_PowerOn") || !strcmp(n, "OP_DRC") ||
           !strcmp(n, "Open_FirstDoor") || !strcmp(n, "IsPlayed_Demo700_0");
}

// The slate's runes and their upgrades (IsGet_<item>): the flag alone leaves the slate's runes as they were (its rune
// list is made when an item is had: 0x700120af98), so the item is given too, as the shrine's pedestal does
static bool slate_rune(const char *n) {
    static const char *const runes[] = {"IsGet_Obj_RemoteBomb", "IsGet_Obj_RemoteBombCube", "IsGet_Obj_Magnetglove",
        "IsGet_Obj_StopTimer", "IsGet_Obj_IceMaker", "IsGet_Obj_Camera", "IsGet_Obj_RemoteBombLv2",
        "IsGet_Obj_RemoteBombCube2", "IsGet_Obj_StopTimerLv2", "IsGet_Obj_SheikSensor", "IsGet_Obj_SheikSensorLv2",
        "IsGet_Obj_SheikPadLv2", "IsGet_Obj_Motorcycle"};
    for (size_t k = 0; k < sizeof runes / sizeof *runes; k++)
        if (!strcmp(n, runes[k])) return true;
    return false;
}

static bool union_has(const uint8_t *all, char (*names)[48], int n, const char *want) {
    for (int k = 0; k < n; k++)
        if (!strcmp(names[k], want)) return all[k >> 3] >> (k & 7) & 1;
    return false;
}

static int korok_event(Cpu *c, uint64_t player, const char *flag) {
    extern bool framepace_in_event(void);
    if (!VALID(player) || getenv("SAVAGEWIND_ONLINE_NO_KOROK_EVENT")) return 0;
    bool fly = strstr(flag, "KorokFly") != NULL;
    const char *want = fly ? "Npc_HiddenKorokFly" : "Npc_HiddenKorokGround";
    uint32_t id = (uint32_t)strtoul(strrchr(flag, '_') + 1, NULL, 10);  // (its map object's HashId)
    const float *pm = (const float *)(uintptr_t)(player + 0x398);
    int na = oa_all_actors(*(uint64_t *)(uintptr_t)(player + 0x68), oa_g_actors, MAX_ACTORS);
    uint64_t found = 0;
    for (int k = 0; k < na && !found; k++) {
        char n[48];
        oa_actor_name_of(oa_g_actors[k], n);
        if (strcmp(n, want)) continue;
        const float *m = (const float *)(uintptr_t)(oa_g_actors[k] + 0x398);
        float d = (m[3] - pm[3]) * (m[3] - pm[3]) + (m[7] - pm[7]) * (m[7] - pm[7]) + (m[11] - pm[11]) * (m[11] - pm[11]);
        if (d > 60 * 60) continue;
        static int off = -1;  // (where an actor keeps its HashId: looked for once)
        if (off < 0)
            for (int o = 0x100; o < 0x900 && off < 0; o += 4)
                if (*(uint32_t *)(uintptr_t)(oa_g_actors[k] + o) == id) off = o, LOG("[online] an actor's HashId is at +%#x\n", o);
        if (off >= 0 && *(uint32_t *)(uintptr_t)(oa_g_actors[k] + off) == id) found = oa_g_actors[k];
    }
    if (!found) return 0;
    if (framepace_in_event()) return -1;  // (an event plays: later)
    LOG("[online] %s: its Korok is here, its cutscene\n", flag);
    uint64_t mgr = *(uint64_t *)(uintptr_t)P_KOROK_EVT_MGR;
    if (!VALID(mgr)) return 0;
    static uint8_t meta[0x200] __attribute__((aligned(16)));
    memset(meta, 0, sizeof meta);
    oa_call(c, FN_EVT_META_INIT, (uint64_t)(uintptr_t)meta, 0, 0, 0, 0, 0);
    oa_call(c, FN_EVT_META_NAME, (uint64_t)(uintptr_t)meta, (uint64_t)(uintptr_t)"Demo017_0",
         (uint64_t)(uintptr_t)(fly ? "HiddenKorok_Air" : "HiddenKorok_Ground"), (uint64_t)(uintptr_t)"", 0, 0);
    bool ok = oa_call(c, FN_EVT_START, mgr, (uint64_t)(uintptr_t)meta, found, 0, 0, 0) & 1;
    oa_call(c, FN_EVT_META_FREE, (uint64_t)(uintptr_t)meta, 0, 0, 0, 0, 0);
    LOG("[online] Demo017_0 for it: %d\n", ok);
    return ok ? 1 : -1;
}

volatile int oa_g_fake_blood_moon;  // (`bloodmoon`, developer mode: the world's flags cleared here, as one does)

void oa_quests_update(Cpu *c, uint64_t player, bool host) {
    static MapLine *m;
    static char names[ONLINE_QUEST_BYTES * 8][48];
    static uint32_t hash[ONLINE_QUEST_BYTES * 8];
    // (the world's items and chests, the flags a Blood Moon clears: after one here, those the others' union still has
    // from before it aren't set here again (until the union drops them, every game past its own); those taken after are)
    static uint8_t revival[ONLINE_QUEST_BYTES], held_off[ONLINE_QUEST_BYTES], had[ONLINE_QUEST_BYTES];
    static int n = -1, tick;
    if (n < 0) {  // (quests.txt: a name a line; map_load wants key=value)
        n = 0;
        const char *ov = getenv("SAVAGEWIND_ROMFS_OVERLAY");
        char p[1100], line[128];
        snprintf(p, sizeof p, "%s/quests.txt", ov ? ov : "");
        FILE *f = ov && *ov ? fopen(p, "r") : NULL;
        while (f && n < ONLINE_QUEST_BYTES * 8 && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!line[0]) continue;
            snprintf(names[n], sizeof names[n], "%s", line);
            static const char *const kinds[] = {"_TBox_", "_Item_", "_Obj_", "_Weapon_", "_Barrel_", "_Kibako",
                                                "_LinkTag", "_WallCrack", "_IceWall", "_Pot", "_BeeHome", "_OreDrop",
                                                "_TreasureSpot", "_FldObj", "_TwnObj", "_DgnObj", "_PutRupee", "_Necklace"};
            for (size_t q = 0; q < sizeof kinds / sizeof *kinds; q++)
                if (strstr(line, kinds[q])) revival[n >> 3] |= (uint8_t)(1 << (n & 7));
            hash[n] = oa_crc32_str(line), n++;
        }
        if (f) fclose(f);
        LOG("[online] %d quest flags shared\n", n);
        (void)m;
    }
    if (!n || tick++ % 60) return;
    static bool counted;
    if (!counted && test_warp_gdt_mgr()) {  // (how many aren't bool flags)
        int miss = 0;
        for (int k = 0; k < n; k++)
            if (!oa_bool_flag(hash[k])) {
                if (miss < 5) LOG("[online] quest flag %s: no bool flag\n", names[k]);
                miss++;
            }
        LOG("[online] %d of the quest flags aren't bool flags\n", miss);
        if (getenv("SAVAGEWIND_ONLINE_GDT_PROBE"))  // (test: where MaxHeartValue is)
            for (int off = 0xb80; off < 0xc40; off += 8) {
                uint32_t *v = oa_gdt_value(off, oa_crc32_str("MaxHeartValue"));
                if (v) LOG("[online] MaxHeartValue at mgr+%#x: %d\n", off, (int)*v);
            }
        const char *ql = getenv("SAVAGEWIND_ONLINE_QUEST_LOG");
        for (int k = 0; ql && k < n; k++)
            if (strstr(names[k], ql)) {
                uint32_t *v = oa_bool_flag(hash[k]);
                LOG("[online] quest flag %s = %d\n", names[k], v ? (int)*v : -1);
            }
        counted = true;
    }
    uint8_t mine[ONLINE_QUEST_BYTES] = {0}, all[ONLINE_QUEST_BYTES];
    if (__atomic_exchange_n(&oa_g_fake_blood_moon, 0, __ATOMIC_ACQ_REL))
        for (int k = 0; k < n; k++) {
            uint32_t *v = revival[k >> 3] >> (k & 7) & 1 ? oa_bool_flag(hash[k]) : NULL;
            if (v) *v &= ~1u;
        }
    for (int k = 0; k < n; k++) {
        uint32_t *v = oa_bool_flag(hash[k]);
        if (v && (*v & 1)) mine[k >> 3] |= (uint8_t)(1 << (k & 7));
    }
    {   // a Blood Moon here: many of the revival flags this game had are clear at once
        int cleared = 0;
        for (int b = 0; b < ONLINE_QUEST_BYTES; b++) cleared += __builtin_popcount(had[b] & ~mine[b] & revival[b]);
        static bool had_once;
        if (had_once && cleared >= 64) {
            uint8_t u[ONLINE_QUEST_BYTES];
            bool got = host ? (online_take_quests(u), true) : online_quests(u);
            for (int b = 0; got && b < ONLINE_QUEST_BYTES; b++) held_off[b] |= u[b] & revival[b];
            LOG("[online] a Blood Moon here: %d of the world's flags cleared; the others' from before aren't set again\n", cleared);
        }
        memcpy(had, mine, sizeof had), had_once = true;
    }
    bool have;
    if (host) {
        static uint8_t last[ONLINE_QUEST_BYTES];
        static bool sent;
        online_take_quests(all);
        for (int k = 0; k < ONLINE_QUEST_BYTES; k++) all[k] |= mine[k];
        if (!sent || memcmp(all, last, sizeof all)) online_set_quests(all), memcpy(last, all, sizeof last), sent = true;
        have = true;
    } else {
        static uint8_t last[ONLINE_QUEST_BYTES];
        static int every;
        if (memcmp(mine, last, sizeof mine) || every++ % 5 == 0) online_send_quests(mine), memcpy(last, mine, sizeof last);
        have = online_quests(all);
    }
    // (the slate: near its pedestal without it, by its own event once the other has it, whose flags come with it; none of
    // them set meanwhile (with some set, its pedestal is half done); elsewhere its flags alone)
    static bool slate_started;
    bool by_event = slate_started;
    for (int k = 0; have && k < n && VALID(player); k++) {
        if (strcmp(names[k], "IsGet_PortableUnit") || (mine[k >> 3] >> (k & 7) & 1)) continue;
        const float *pm = (const float *)(uintptr_t)(player + 0x398);
        float dx = pm[3] + 1125.0f, dz = pm[11] - 1903.2f;
        if (dx * dx + dz * dz < 40 * 40 && getenv("SAVAGEWIND_ONLINE_SLATE_EVENT")) by_event = true;
        if (by_event && !slate_started && (all[k >> 3] >> (k & 7) & 1) && oa_slate_event(c, player)) slate_started = true;
    }
    extern bool framepace_in_event(void);
    bool korok_started = false;
    for (int b = 0; have && b < ONLINE_QUEST_BYTES; b++) held_off[b] &= all[b];  // (gone from the union: set again later)
    for (int k = 0; have && k < n; k++) {
        if (!(all[k >> 3] >> (k & 7) & 1) || (mine[k >> 3] >> (k & 7) & 1)) continue;
        if (held_off[k >> 3] >> (k & 7) & 1) continue;  // (from before this game's Blood Moon)
        if (by_event && (!strcmp(names[k], "IsGet_PortableUnit") || !strcmp(names[k], "PortableUnit_PowerOn") ||
                         !strcmp(names[k], "OP_DRC") || !strcmp(names[k], "Open_FirstDoor") ||
                         !strcmp(names[k], "IsPlayed_Demo700_0"))) continue;
        uint32_t *v = oa_bool_flag(hash[k]);
        if (!v) continue;
        // (the slate's flags once his cutscene is over: its last one, IsPlayed_Demo700_0, has come)
        if (slate_group(names[k]) && !union_has(all, names, n, "IsPlayed_Demo700_0")) continue;
        // (a Korok found near here: its own cutscene, which gives the seed and sets its flag; once started, the seed isn't
        // given again when its flag is set afterwards)
        bool korok = !strncmp(names[k], "MainField_Npc_HiddenKorok", 25), played = false;
        static uint32_t korok_played[16];
        static int korok_np;
        for (int q = 0; korok && q < 16; q++) played |= korok_played[q] == hash[k];
        if (korok && !played && !korok_started) {
            int r = korok_event(c, player, names[k]);
            static uint32_t wait_h[16];
            static int wait_n[16];
            if (r == 1) {
                korok_played[korok_np++ % 16] = hash[k], korok_started = true;
                continue;
            }
            if (r < 0) {  // (its Korok here but no cutscene yet, another event playing: again, 30 times at most)
                int q = 0;
                while (q < 16 && wait_h[q] != hash[k]) q++;
                if (q == 16) q = (int)(hash[k] % 16), wait_h[q] = hash[k], wait_n[q] = 0;
                korok_started = true;  // (one at a time)
                if (++wait_n[q] < 30) continue;
            }
        }
        if (korok && played && framepace_in_event()) continue;  // (its cutscene playing: its flag comes from it)
        if (!oa_set_bool_flag(c, hash[k])) *v |= 1;  // (the game's setter; else in memory)
        if (!strcmp(names[k], "IsGet_PortableUnit")) oa_give_item(c, "Obj_DRStone_Get", 1);  // (the slate in his key items)
        if (slate_rune(names[k])) oa_give_item(c, names[k] + 6, 1);  // (a rune: the slate's runes as the game gives it)
        // (the Great Plateau done: the king's paraglider, which he gives at that step. Without it a player who wasn't
        // there had the quest finished and no way down from the plateau. Its IsGet_PlayerStole2 flag is no sign: the
        // game has it set long before, in both games, the paraglider or not)
        if (!strcmp(names[k], "FindDungeon_Finish")) oa_give_item(c, "PlayerStole2", 1);
        if (korok && !played) oa_give_item(c, "Obj_KorokNuts", 1);  // (a Korok found far from here)
        if (!strncmp(names[k], "Clear_Dungeon", 13)) oa_give_item(c, "Obj_DungeonClearSeal", 1);  // (a shrine: its orb)
        // (a Divine Beast: its champion's ability, as the game gives it after the beast; its heart container)
        if (!strncmp(names[k], "IsGet_Obj_HeroSoul_", 19)) oa_give_item(c, names[k] + 6, 1);
        if (!strncmp(names[k], "Get_HeartUtuwa_", 15)) oa_give_heart(c);
        LOG("[online] quest flag %s: set (another player did it)\n", names[k]);
    }
}

// rt_online_frame, step 7:
// the time of day: the host's for everyone (his game sends it; the others follow when theirs is off by more than
// 3 game minutes, a few seconds: the two clocks run at the same speed)
void oa_flags_hour_frame(void) {
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
