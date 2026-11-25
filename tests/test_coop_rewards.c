// Tests for runtime/coop_rewards.c (CU-07, plan.md section D): the policy table cell by cell and the delivery ledger,
// with synthetic flag names and a ledger in a temporary folder; no game and no network.
//   clang -Wall -Iruntime tests/test_coop_rewards.c runtime/coop_rewards.c -o build/test_coop_rewards
// (run from the repository root: the folder it uses is relative, under build/)
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

#include "coop_rewards.h"

static int g_fail;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)

#define DIR "build/test_coop_rewards_tmp"
#define LEDGER DIR "/coop-ledger.txt"

static void put(const char *text) {
    FILE *f = fopen(LEDGER, "wb");
    if (f) fwrite(text, 1, strlen(text), f), fclose(f);
}

static void test_class(void) {
    CHECK(coop_reward_class("IsGet_PortableUnit") == RW_WORLD_ABILITY);
    CHECK(coop_reward_class("FindDungeon_Finish") == RW_WORLD_ABILITY);
    CHECK(coop_reward_class("IsGet_Obj_RemoteBomb") == RW_WORLD_ABILITY);
    CHECK(coop_reward_class("IsGet_Obj_Magnetglove") == RW_WORLD_ABILITY);
    CHECK(coop_reward_class("IsGet_Obj_SheikSensorLv2") == RW_WORLD_ABILITY);
    CHECK(coop_reward_class("IsGet_Obj_Motorcycle") == RW_WORLD_ABILITY);
    CHECK(coop_reward_class("IsGet_Obj_HeroSoul_Zora") == RW_CHAMPION);
    CHECK(coop_reward_class("IsGet_Obj_HeroSoul_Gerudo") == RW_CHAMPION);
    CHECK(coop_reward_class("Clear_Dungeon012") == RW_MILESTONE);
    CHECK(coop_reward_class("Clear_DungeonXyz") == RW_MILESTONE);
    CHECK(coop_reward_class("Get_HeartUtuwa_Dungeon_A") == RW_MILESTONE);
    CHECK(coop_reward_class("MainField_Npc_HiddenKorokGround_1") == RW_COLLECTIBLE);
    CHECK(coop_reward_class("MainField_Npc_HiddenKorokFly_2") == RW_COLLECTIBLE);
    CHECK(coop_reward_class("Dungeon001_TBox_01") == RW_CHEST);
    CHECK(coop_reward_class("MainField_A_TBox_Weapon") == RW_CHEST);
    // (a flag that hands out nothing, a near miss, and a bad pointer)
    CHECK(coop_reward_class("PortableUnit_PowerOn") == RW_NONE);  // (the slate's group, but no reward)
    CHECK(coop_reward_class("OP_DRC") == RW_NONE);
    CHECK(coop_reward_class("IsGet_Obj_Unknown") == RW_NONE);
    CHECK(coop_reward_class("IsGet_PortableUnitX") == RW_NONE);
    CHECK(coop_reward_class("isget_portableunit") == RW_NONE);  // (case matters)
    CHECK(coop_reward_class("XClear_Dungeon001") == RW_NONE);
    CHECK(coop_reward_class("IsGet_Obj_HeroSoul") == RW_NONE);  // (no trailing '_')
    CHECK(coop_reward_class("Rupee") == RW_NONE);
    CHECK(coop_reward_class("") == RW_NONE);
    CHECK(coop_reward_class(NULL) == RW_NONE);
    puts("PASS: class of each flag by name");
}

// One row of plan.md section D: the class and what it gives at each moment (session, first join, rejoin)
static void test_table(void) {
    static const struct {
        RewardClass c;
        bool at[3];
    } rows[] = {
        {RW_WORLD_ABILITY, {true, true, true}},   // always: without them the host cannot be followed
        {RW_CHAMPION, {true, true, true}},        // ADR-007, decision 2; ADR-008, decision 13
        {RW_MILESTONE, {true, false, true}},      // a first join is matched instead (T442); a rejoin gets what it missed
        {RW_COLLECTIBLE, {true, false, false}},   // ADR-011, decision 30: one seed for each, during the session
        {RW_CHEST, {false, false, false}},        // ADR-008, decision 14: it is whoever opens it
        {RW_NONE, {false, false, false}},
    };
    static const RewardMoment moments[3] = {RW_AT_SESSION, RW_AT_FIRST_JOIN, RW_AT_REJOIN};
    for (size_t r = 0; r < sizeof rows / sizeof *rows; r++)
        for (int m = 0; m < 3; m++) {
            bool got = coop_reward_decide(rows[r].c, moments[m]);
            if (got != rows[r].at[m]) printf("  class %d, moment %d: gave %d, wanted %d\n", (int)rows[r].c, m, got, rows[r].at[m]);
            CHECK(got == rows[r].at[m]);
        }
    // (out of range values give nothing)
    CHECK(!coop_reward_decide((RewardClass)99, RW_AT_SESSION));
    CHECK(!coop_reward_decide((RewardClass)-1, RW_AT_REJOIN));
    CHECK(!coop_reward_decide(RW_CHAMPION, (RewardMoment)7));
    puts("PASS: policy table, class by moment");
}

// The flags that today are handed out one by one (quests_update), by what the table says on a first join
static void test_flow_by_flag(void) {
    // (first join to an advanced world: the slate and the champions' abilities come, orbs and hearts do not, nor seeds and chests)
    CHECK(coop_reward_decide(coop_reward_class("IsGet_PortableUnit"), RW_AT_FIRST_JOIN));
    CHECK(coop_reward_decide(coop_reward_class("IsGet_Obj_HeroSoul_Rito"), RW_AT_FIRST_JOIN));
    CHECK(!coop_reward_decide(coop_reward_class("Clear_Dungeon030"), RW_AT_FIRST_JOIN));
    CHECK(!coop_reward_decide(coop_reward_class("Get_HeartUtuwa_Dungeon_B"), RW_AT_FIRST_JOIN));
    CHECK(!coop_reward_decide(coop_reward_class("MainField_Npc_HiddenKorokGround_9"), RW_AT_FIRST_JOIN));
    CHECK(!coop_reward_decide(coop_reward_class("Dungeon002_TBox_03"), RW_AT_FIRST_JOIN));
    // (during the session the orb comes, the chest never)
    CHECK(coop_reward_decide(coop_reward_class("Clear_Dungeon030"), RW_AT_SESSION));
    CHECK(!coop_reward_decide(coop_reward_class("Dungeon002_TBox_03"), RW_AT_SESSION));
    puts("PASS: table applied by flag name");
}

static void test_hash(void) {
    CHECK(coop_reward_hash("123456789") == 0xcbf43926u);  // (the standard CRC-32 check value)
    CHECK(coop_reward_hash("") == 0);
    CHECK(coop_reward_hash(NULL) == 0);
    CHECK(coop_reward_hash("Clear_Dungeon001") != coop_reward_hash("Clear_Dungeon002"));
    CHECK(coop_reward_hash("IsGet_PortableUnit") == coop_reward_hash("IsGet_PortableUnit"));
    puts("PASS: flag hash is CRC-32");
}

static void test_ledger(void) {
    remove(LEDGER);
    CHECK(!coop_ledger_has(1) && !coop_ledger_add(1) && !coop_ledger_caught_up() && coop_ledger_count() == 0);  // (none open)
    CHECK(!coop_ledger_set_caught_up());
    CHECK(coop_ledger_load(LEDGER));  // (no file: a new ledger)
    CHECK(coop_ledger_count() == 0 && !coop_ledger_has(0x12345678u) && !coop_ledger_caught_up());
    CHECK(coop_ledger_add(0x12345678u) && coop_ledger_has(0x12345678u));
    CHECK(coop_ledger_add(0xdeadbeefu) && coop_ledger_add(0));
    CHECK(coop_ledger_add(0x12345678u));  // (again: nothing more)
    CHECK(coop_ledger_count() == 3);
    CHECK(!coop_ledger_has(0x12345679u));
    // (reload: what was added is there, and the mark is not)
    coop_ledger_close();
    CHECK(!coop_ledger_has(0x12345678u) && coop_ledger_count() == 0);
    CHECK(coop_ledger_load(LEDGER));
    CHECK(coop_ledger_count() == 3 && coop_ledger_has(0x12345678u) && coop_ledger_has(0xdeadbeefu) && coop_ledger_has(0));
    CHECK(!coop_ledger_caught_up());
    // (the mark survives a reload; setting it twice writes once)
    CHECK(coop_ledger_set_caught_up() && coop_ledger_caught_up() && coop_ledger_set_caught_up());
    CHECK(coop_ledger_count() == 3);  // (the mark is no entry)
    CHECK(coop_ledger_add(0xcafef00du));
    CHECK(coop_ledger_load(LEDGER));
    CHECK(coop_ledger_caught_up() && coop_ledger_count() == 4 && coop_ledger_has(0xcafef00du));
    // (the file is 8 lowercase digits a line, and the mark once)
    char text[256] = {0};
    FILE *f = fopen(LEDGER, "rb");
    CHECK(f != NULL);
    if (f) fread(text, 1, sizeof text - 1, f), fclose(f);
    CHECK(!strcmp(text, "12345678\ndeadbeef\n00000000\ncaught-up\ncafef00d\n"));
    // (a ledger of its own is replaced by the next load)
    CHECK(coop_ledger_load(DIR "/other-ledger.txt") && coop_ledger_count() == 0 && !coop_ledger_caught_up());
    remove(DIR "/other-ledger.txt");
    puts("PASS: ledger add, reload, duplicate, the caught-up mark");
}

static void test_ledger_corrupt(void) {
    // (garbage lines are ignored, the rest is read, and the answer says it was not clean)
    put("12345678\nnot a hash\n\ndeadbeef\n123456789\n1234567\nzzzzzzzz\ncaught-up\n");
    CHECK(!coop_ledger_load(LEDGER));
    CHECK(coop_ledger_count() == 2 && coop_ledger_has(0x12345678u) && coop_ledger_has(0xdeadbeefu) && coop_ledger_caught_up());
    CHECK(coop_ledger_add(0x1u));  // (still usable)
    // (upper case and CRLF are fine; empty lines are not an error)
    put("ABCDEF01\r\n\r\n00FF00FF\r\n");
    CHECK(coop_ledger_load(LEDGER) && coop_ledger_count() == 2 && coop_ledger_has(0xabcdef01u) && coop_ledger_has(0x00ff00ffu));
    // (binary junk and a very long line)
    char junk[400];
    memset(junk, 'x', sizeof junk - 1), junk[sizeof junk - 1] = 0;
    put(junk);
    CHECK(!coop_ledger_load(LEDGER) && coop_ledger_count() == 0);
    put("\x01\x02\x03\n\xff\xfe\n12345678\n");
    CHECK(!coop_ledger_load(LEDGER) && coop_ledger_count() == 1 && coop_ledger_has(0x12345678u));
    // (a last line cut short by a crash: ignored, and the next add starts a line of its own)
    put("12345678\ndead");
    CHECK(!coop_ledger_load(LEDGER) && coop_ledger_count() == 1);
    CHECK(coop_ledger_add(0xbeef0001u));
    CHECK(coop_ledger_load(LEDGER) == false);  // ("dead" is still there, alone on its line)
    CHECK(coop_ledger_count() == 2 && coop_ledger_has(0xbeef0001u) && !coop_ledger_has(0xdeadbeefu));
    // (a last full line with no newline is taken, and the next add does not glue to it)
    put("12345678\n87654321");
    CHECK(coop_ledger_load(LEDGER) && coop_ledger_count() == 2);
    CHECK(coop_ledger_add(0x11111111u));
    CHECK(coop_ledger_load(LEDGER) && coop_ledger_count() == 3 && coop_ledger_has(0x87654321u) && coop_ledger_has(0x11111111u));
    puts("PASS: corrupt ledger lines are ignored, a cut last line is not glued");
}

static void test_ledger_bad_path(void) {
    CHECK(!coop_ledger_load(NULL) && coop_ledger_count() == 0 && !coop_ledger_has(1));
    CHECK(!coop_ledger_load(""));
    char big[2048];
    memset(big, 'a', sizeof big - 1), big[sizeof big - 1] = 0;
    CHECK(!coop_ledger_load(big) && !coop_ledger_add(1));
    // (a folder that is not there: it loads as new, but nothing can be written, and then nothing is added)
    CHECK(coop_ledger_load(DIR "/no-such-folder/ledger.txt"));
    CHECK(!coop_ledger_add(0x5u) && !coop_ledger_has(0x5u) && coop_ledger_count() == 0);
    CHECK(!coop_ledger_set_caught_up() && !coop_ledger_caught_up());
    puts("PASS: bad paths, and a failed write adds nothing");
}

static void test_ledger_full(void) {
    // (a full ledger written in one go: one file open per add would be slow here)
    FILE *f = fopen(LEDGER, "wb");
    CHECK(f != NULL);
    if (!f) return;
    for (uint32_t i = 0; i < COOP_LEDGER_MAX; i++) fprintf(f, "%08x\n", (unsigned)(i + 1));
    fclose(f);
    CHECK(coop_ledger_load(LEDGER) && coop_ledger_count() == COOP_LEDGER_MAX && coop_ledger_has(COOP_LEDGER_MAX));
    CHECK(!coop_ledger_add(0x7fffffffu) && !coop_ledger_has(0x7fffffffu));  // (full: refused)
    CHECK(coop_ledger_add(1));                                              // (one it has: still fine)
    // (a file with more than the cap: it reads the first ones and says it was not clean)
    f = fopen(LEDGER, "ab");
    if (f) fputs("7fffffff\n7ffffffe\n", f), fclose(f);
    CHECK(!coop_ledger_load(LEDGER) && coop_ledger_count() == COOP_LEDGER_MAX && !coop_ledger_has(0x7fffffffu));
    puts("PASS: ledger tops out at 65536 entries");
}

int main(void) {
    MKDIR("build");
    MKDIR(DIR);  // (it may already be there)
    test_class();
    test_table();
    test_flow_by_flag();
    test_hash();
    test_ledger();
    test_ledger_corrupt();
    test_ledger_bad_path();
    test_ledger_full();
    coop_ledger_close();
    remove(LEDGER);
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    puts("PASS: coop_rewards");
    return 0;
}
