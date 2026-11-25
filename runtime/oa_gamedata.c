// Online play's actors, oa_gamedata: the game's data: its flags (GameDataMgr), events started, items and hearts given.
// What the modules share: runtime/online_actors_internal.h
#include "online_actors_internal.h"
#include "hle.h"  // (RT_ON_RESET)

// ---- The quests (protocol 17): the flags of online_mod/quests.txt (the main, shrine and side quests' steps and the bool flags
// they depend on) are everyone's: each game sends which it has (every 2 s), the host's game ORs them all with its own
// and sends the union, and each game sets the ones it lacks. A bool flag's place in GameDataMgr (test_warp_gdt_mgr):
// its pending-write buffer (+0xbd8) +8 a count and +0x10 an array of proxies (0x18 each: +8 the flag's definition,
// its name's crc32 at +0xc; +0x10 the value), sorted by that hash
uint32_t oa_crc32_str(const char *s) {
    uint32_t h = 0xFFFFFFFFu;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        for (int b = 0; b < 8; b++) h = h >> 1 ^ (0xEDB88320u & -(h & 1));
    }
    return ~h;
}

// the value of the bool flag with that hash (NULL: none)
uint32_t *oa_gdt_value(int off, uint32_t h);
uint32_t *oa_bool_flag(uint32_t h) { return oa_gdt_value(0xbd8, h); }
// a GameData value by its name's hash: its value in the buffer at mgr+off (+0xbd8 the bool flags), NULL none
uint32_t *oa_gdt_value(int off, uint32_t h) {
    uint64_t mgr = test_warp_gdt_mgr();
    uint64_t pb = VALID(mgr) ? *(uint64_t *)(uintptr_t)(mgr + off) : 0;
    uint64_t buf = VALID(pb) ? *(uint64_t *)(uintptr_t)pb : 0;
    if (!VALID(buf)) return NULL;
    uint32_t n = *(uint32_t *)(uintptr_t)(buf + 8);
    uint64_t arr = *(uint64_t *)(uintptr_t)(buf + 0x10);
    if (!n || n > 200000 || !VALID(arr)) return NULL;
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint64_t f = *(uint64_t *)(uintptr_t)(arr + 8 * mid), pr = VALID(f) ? *(uint64_t *)(uintptr_t)(f + 8) : 0;
        uint32_t mh = VALID(pr) ? *(uint32_t *)(uintptr_t)(pr + 0xc) : 0;
        if (mh < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= n) return NULL;
    uint64_t f = *(uint64_t *)(uintptr_t)(arr + 8 * lo), pr = VALID(f) ? *(uint64_t *)(uintptr_t)(f + 8) : 0;
    return VALID(pr) && *(uint32_t *)(uintptr_t)(pr + 0xc) == h ? (uint32_t *)(uintptr_t)(f + 0x10) : NULL;
}

// oa_start_event's state, at file level so that a reboot clears it (RT_ON_RESET at the end of the file)
static uint8_t meta[0x200] __attribute__((aligned(16)));
static struct { float m[12]; uint32_t flags, pad; uint64_t meta, zero, actor; } req __attribute__((aligned(16)));

bool oa_start_event(Cpu *c, uint64_t actor, const char *flow, const char *entry) {
    uint64_t mgr = *(uint64_t *)(uintptr_t)P_EVT_MGR;
    mgr = VALID(mgr) ? *(uint64_t *)(uintptr_t)mgr : 0;
    if (!VALID(mgr) || !VALID(actor)) return false;
    memset(meta, 0, sizeof meta);
    oa_call(c, FN_EVT_META_INIT, (uint64_t)(uintptr_t)meta, 0, 0, 0, 0, 0);
    oa_call(c, FN_EVT_META_NAME, (uint64_t)(uintptr_t)meta, (uint64_t)(uintptr_t)flow, (uint64_t)(uintptr_t)entry,
         (uint64_t)(uintptr_t)"", 0, 0);
    memset(&req, 0, sizeof req);
    memcpy(req.m, (const float *)(uintptr_t)(actor + 0x398), sizeof req.m);
    req.flags = 0x101, req.meta = (uint64_t)(uintptr_t)meta, req.actor = actor;
    bool ok = oa_call(c, FN_EVT_CALL, mgr, (uint64_t)(uintptr_t)&req, 0, 0, 0, 0) & 1;
    oa_call(c, FN_EVT_META_FREE, (uint64_t)(uintptr_t)meta, 0, 0, 0, 0, 0);
    LOG("[online] event %s<%s> started: %d\n", flow, entry, ok);
    return ok;
}

void oa_give_item(Cpu *c, const char *name, int count) {
    uint64_t ss[2] = {*(uint64_t *)(uintptr_t)P_SAFE_STRING_VT + 0x10, (uint64_t)(uintptr_t)name};
    if (!(oa_call(c, FN_POUCH_CAN_GET, (uint64_t)(uintptr_t)ss, (uint64_t)count, 0, 0, 0, 0) & 1)) {
        LOG("[online] %s doesn't fit in the pouch\n", name);
        return;
    }
    oa_call(c, FN_POUCH_GET, (uint64_t)(uintptr_t)ss, (uint64_t)count, 0, 0, 0, 0);
    LOG("[online] %s into the pouch\n", name);
}

void oa_give_heart(Cpu *c) {
    static const char name[] = "Obj_HeartUtuwa_A_01";
    uint64_t ss[2] = {*(uint64_t *)(uintptr_t)P_SAFE_STRING_VT + 0x10, (uint64_t)(uintptr_t)name};
    oa_call(c, FN_GET_SPECIAL, 0, (uint64_t)(uintptr_t)ss, 0, 0, 0, 0);
    LOG("[online] a heart container\n");
}

bool oa_set_bool_flag(Cpu *c, uint32_t h) {
    uint64_t mgr = *(uint64_t *)(uintptr_t)P_GDT_MGR;
    mgr = VALID(mgr) ? *(uint64_t *)(uintptr_t)mgr : 0;
    uint64_t pb = VALID(mgr) ? *(uint64_t *)(uintptr_t)(mgr + 0xbd8) : 0;
    uint64_t buf = VALID(pb) ? *(uint64_t *)(uintptr_t)pb : 0;
    if (!VALID(buf)) return false;
    uint32_t handle = (uint32_t)oa_call(c, FN_GDT_HANDLE, buf, h, 0, 0, 0, 0);
    handle |= (uint32_t)*(int32_t *)(uintptr_t)(mgr + 0xc28) << 24;
    if (handle == 0xffffffffu) return false;
    return oa_call(c, FN_GDT_SET_BOOL, mgr, 1, handle, 0, 0, 0) & 1;
}

// A reboot (runtime/main.c teardown): the event's work memory held the last boot's addresses (ADR-026, resolution 1)
RT_ON_RESET(oa_gamedata_reset) {
    memset(meta, 0, sizeof meta);
    memset(&req, 0, sizeof req);
}
