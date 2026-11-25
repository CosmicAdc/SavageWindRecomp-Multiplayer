// Reward policy and delivery ledger for co-op: see runtime/coop_rewards.h. Pure logic, tested by
// tests/test_coop_rewards.c.
#include "coop_rewards.h"

#include <stdio.h>
#include <string.h>

#define PATH_MAX_LEN 1024
#define MARK_CAUGHT_UP "caught-up"

// The slate's runes and their upgrades, as the flags the world sets for them
static const char *const k_runes[] = {"IsGet_Obj_RemoteBomb", "IsGet_Obj_RemoteBombCube", "IsGet_Obj_Magnetglove",
    "IsGet_Obj_StopTimer", "IsGet_Obj_IceMaker", "IsGet_Obj_Camera", "IsGet_Obj_RemoteBombLv2",
    "IsGet_Obj_RemoteBombCube2", "IsGet_Obj_StopTimerLv2", "IsGet_Obj_SheikSensor", "IsGet_Obj_SheikSensorLv2",
    "IsGet_Obj_SheikPadLv2", "IsGet_Obj_Motorcycle"};

static bool starts_with(const char *s, const char *prefix) { return !strncmp(s, prefix, strlen(prefix)); }

RewardClass coop_reward_class(const char *flag) {
    if (!flag || !*flag) return RW_NONE;
    if (!strcmp(flag, "IsGet_PortableUnit") || !strcmp(flag, "FindDungeon_Finish")) return RW_WORLD_ABILITY;
    for (size_t k = 0; k < sizeof k_runes / sizeof *k_runes; k++)
        if (!strcmp(flag, k_runes[k])) return RW_WORLD_ABILITY;
    if (starts_with(flag, "IsGet_Obj_HeroSoul_")) return RW_CHAMPION;
    if (starts_with(flag, "Clear_Dungeon") || starts_with(flag, "Get_HeartUtuwa_")) return RW_MILESTONE;
    if (starts_with(flag, "MainField_Npc_HiddenKorok")) return RW_COLLECTIBLE;
    if (strstr(flag, "_TBox_")) return RW_CHEST;
    return RW_NONE;
}

bool coop_reward_decide(RewardClass c, RewardMoment m) {
    switch (c) {
    case RW_WORLD_ABILITY:
    case RW_CHAMPION:
        return m == RW_AT_SESSION || m == RW_AT_FIRST_JOIN || m == RW_AT_REJOIN;
    case RW_MILESTONE:
        return m == RW_AT_SESSION || m == RW_AT_REJOIN;
    case RW_COLLECTIBLE:
        return m == RW_AT_SESSION;
    case RW_CHEST:
    case RW_NONE:
    default:
        return false;
    }
}

uint32_t coop_reward_hash(const char *flag) {
    if (!flag) return 0;
    uint32_t crc = 0xffffffffu;
    for (const unsigned char *p = (const unsigned char *)flag; *p; p++) {
        crc ^= *p;
        for (int i = 0; i < 8; i++) crc = crc >> 1 ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

// ---- The ledger

static uint32_t g_hash[COOP_LEDGER_MAX];
static int g_count;
static bool g_open, g_caught_up, g_need_newline;  // (the file's last line was cut short: end it before the next one)
static char g_path[PATH_MAX_LEN];

static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Takes one line (no newline); false if it is neither empty, a hash nor the mark, or the ledger is full
static bool take_line(const char *line, size_t n) {
    while (n && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) n--;
    if (n == 0) return true;
    if (n == sizeof MARK_CAUGHT_UP - 1 && !memcmp(line, MARK_CAUGHT_UP, n)) {
        g_caught_up = true;
        return true;
    }
    if (n != 8) return false;
    uint32_t h = 0;
    for (int i = 0; i < 8; i++) {
        int v = hex_val((unsigned char)line[i]);
        if (v < 0) return false;
        h = h << 4 | (uint32_t)v;
    }
    if (g_count >= COOP_LEDGER_MAX) return false;  // (a repeated line takes a place: only add writes, and it never repeats)
    g_hash[g_count++] = h;
    return true;
}

void coop_ledger_close(void) {
    g_open = g_caught_up = g_need_newline = false;
    g_count = 0;
    g_path[0] = 0;
}

bool coop_ledger_load(const char *path) {
    coop_ledger_close();
    if (!path || !*path || strlen(path) >= sizeof g_path) return false;
    strcpy(g_path, path);
    g_open = true;
    FILE *f = fopen(path, "rb");
    if (!f) return true;  // (none yet: a new ledger)
    bool ok = true, too_long = false;
    char line[64];
    size_t n = 0;
    int c, last = '\n';
    while ((c = fgetc(f)) != EOF) {
        last = c;
        if (c == '\n') {
            if (too_long || !take_line(line, n)) ok = false;
            n = 0, too_long = false;
        } else if (n < sizeof line) {
            line[n++] = (char)c;
        } else {
            too_long = true;
        }
    }
    if (ferror(f)) ok = false;
    fclose(f);
    if (last != '\n') {  // (the last line has no newline: take it, and end it before anything is added)
        if (too_long || !take_line(line, n)) ok = false;
        g_need_newline = true;
    }
    return ok;
}

// Appends text to the file and flushes it; the line the file may have been left with is ended first
static bool append(const char *text) {
    FILE *f = fopen(g_path, "ab");
    if (!f) return false;
    bool ok = !g_need_newline || fputc('\n', f) != EOF;
    ok = ok && fputs(text, f) >= 0;
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (ok) g_need_newline = false;
    return ok;
}

bool coop_ledger_has(uint32_t hash) {
    if (!g_open) return false;
    for (int i = 0; i < g_count; i++)
        if (g_hash[i] == hash) return true;
    return false;
}

bool coop_ledger_add(uint32_t hash) {
    if (!g_open) return false;
    if (coop_ledger_has(hash)) return true;
    if (g_count >= COOP_LEDGER_MAX) return false;
    char text[16];
    snprintf(text, sizeof text, "%08x\n", (unsigned)hash);
    if (!append(text)) return false;
    g_hash[g_count++] = hash;
    return true;
}

bool coop_ledger_caught_up(void) { return g_open && g_caught_up; }

bool coop_ledger_set_caught_up(void) {
    if (!g_open) return false;
    if (g_caught_up) return true;
    if (!append(MARK_CAUGHT_UP "\n")) return false;
    g_caught_up = true;
    return true;
}

int coop_ledger_count(void) { return g_open ? g_count : 0; }
