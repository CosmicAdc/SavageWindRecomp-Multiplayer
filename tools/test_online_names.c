// Tests for the lists of names online play accepts from the network for an actor made here (runtime/online_actors.c
// NameList: what a host's enemy holds, what falls as loot), without the game: the part of online_actors.c before the
// game's (ONLINE_NAMES_ONLY) is built alone and reads lists this test writes, synthetic names in the lists' format
// (tools/online_mod.py: "name=its profile" a line), into the folder given, served as the overlay would be.
//   make test-online-names
#define ONLINE_NAMES_ONLY
#include "../runtime/online_actors.c"

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static char dir[900];

static void write_list(const char *file, const char *text) {
    char p[1100];
    snprintf(p, sizeof p, "%s/%s", dir, file);
    FILE *f = fopen(p, "wb");
    if (!f) { printf("can't write %s\n", p); exit(2); }
    fputs(text, f);
    fclose(f);
}

static void remove_list(const char *file) {
    char p[1100];
    snprintf(p, sizeof p, "%s/%s", dir, file);
    remove(p);
}

// a name as it comes in a 40-byte field (OnlineEnemy.weapon, OnlineLoot.name)
static bool loot(const char *n) {
    char f[40] = {0};
    snprintf(f, sizeof f, "%s", n);
    return oa_loot_name_listed(f, sizeof f);
}
static bool weapon(const char *n) {
    char f[40] = {0};
    snprintf(f, sizeof f, "%s", n);
    return oa_weapon_name_listed(f, sizeof f);
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: test_online_names <a folder to write the lists in>\n"); return 2; }
    snprintf(dir, sizeof dir, "%s", argv[1]);
    static char env[1000];
    snprintf(env, sizeof env, "SAVAGEWIND_ROMFS_OVERLAY=%s", dir);
    putenv(env);

    // weapons.txt: CRLF lines as Python writes them on Windows, a line with no profile, one with no name, the longest
    // name there is room for (47 characters) and one longer (48), a line longer than the reader's buffer (255
    // characters a read): none of it a name, not its tail either
    char long47[48], long48[49], longline[400];
    snprintf(long47, sizeof long47, "Weapon_Long_%035d", 0);
    snprintf(long48, sizeof long48, "Weapon_Long_%036d", 1);
    memset(longline, 'x', sizeof longline);
    memcpy(longline + 255, "Weapon_Tail_001=x\n", 19);  // (its tail, the buffer's next read, looks like a name)
    char text[2000];
    snprintf(text, sizeof text,
             "Weapon_Sword_001=WeaponSmallSword\r\n"
             "Weapon_Shield_001=WeaponShield\r\n"
             "Weapon_Bow_001\n"
             "=WeaponBow\n"
             "%s=x\n"
             "%s=x\n"
             "%s"
             "Weapon_Spear_001=WeaponSpear\n", long47, long48, longline);
    write_list("weapons.txt", text);
    write_list("items.txt", "Item_Fruit_A=Item\nItem_Enemy_00=Item\n");
    remove_list("rupees.txt");  // (not there: every rupee refused)

    // ---- weapons: on the list, or not
    CHECK(weapon("Weapon_Sword_001"));
    CHECK(weapon("Weapon_Shield_001"));
    CHECK(weapon("Weapon_Spear_001"));          // (after the long line: read on)
    CHECK(weapon("Weapon_Bow_001"));            // (no profile: still a name)
    CHECK(!weapon("Weapon_Sword_999"));         // (the prefix alone is not enough)
    CHECK(!weapon("Weapon_Sword_001 "));
    CHECK(!weapon("Weapon_Sword_00"));
    CHECK(!weapon("Weapon_Tail_001"));          // (the long line's tail)
    CHECK(!weapon(""));
    CHECK(!weapon("GameROMPlayer"));
    CHECK(!weapon("Item_Fruit_A"));             // (an item is no weapon)
    CHECK(strlen(long47) == 47 && strlen(long48) == 48);
    CHECK(name_listed(&g_weapon_names, long47, sizeof long47));
    CHECK(!name_listed(&g_weapon_names, long48, sizeof long48));  // (not on it: too long to be a name)
    CHECK(!weapon(long47));                     // (in a 40-byte field it is cut: another name, not on the list)
    CHECK(g_weapon_names.n == 5);               // (Sword_001, Shield_001, Bow_001, Spear_001, the 47)

    // ---- loot: each kind on its own list
    CHECK(loot("Item_Fruit_A"));
    CHECK(loot("Item_Enemy_00"));
    CHECK(loot("Weapon_Sword_001"));
    CHECK(!loot("Item_Invented"));
    CHECK(!loot("Weapon_Invented"));
    CHECK(!loot("Enemy_Bokoblin_Junior"));
    CHECK(!loot("GameROMPlayer"));
    CHECK(!loot(""));
    CHECK(!loot("PutRupee"));                   // (rupees.txt not there: all refused)
    CHECK(!loot("PutRupee_Blue"));
    CHECK(g_rupee_names.read && g_rupee_names.n == 0);

    // ---- a field not ended within its bytes is no name
    char full[16];
    memcpy(full, "Item_Fruit_A....", sizeof full);
    CHECK(!oa_loot_name_listed(full, sizeof full));
    memcpy(full, "Item_Fruit_A\0...", sizeof full);
    CHECK(oa_loot_name_listed(full, sizeof full));
    CHECK(!oa_loot_name_listed("Item_Fruit_A", 12));  // (its 0 just past the field's bytes)

    // ---- read once: the files changing afterwards change nothing
    write_list("items.txt", "Item_Other=Item\n");
    write_list("rupees.txt", "PutRupee_Blue=Item\n");
    CHECK(loot("Item_Fruit_A"));
    CHECK(!loot("Item_Other"));
    CHECK(!loot("PutRupee_Blue"));

    // ---- a list with nothing on it refuses everything; no overlay, nothing read
    write_list("empty.txt", "\n\n=x\n");
    NameList empty = {"empty.txt", "test"};
    CHECK(!name_listed(&empty, "x", 2));
    CHECK(empty.read && empty.n == 0);
    NameList missing = {"missing.txt", "test"};
    CHECK(!name_listed(&missing, "Weapon_Sword_001", 17));
    CHECK(missing.read && missing.n == 0);

    // ---- many names: sorted once, each found
    char *many = malloc(600 * 32);
    int at = 0;
    for (int k = 599; k >= 0; k--) at += sprintf(many + at, "Item_Many_%03d=Item\n", k);  // (backwards: sorted here)
    write_list("many.txt", many);
    free(many);
    NameList big = {"many.txt", "test"};
    int found = 0;
    for (int k = 0; k < 600; k++) {
        char n[32];
        snprintf(n, sizeof n, "Item_Many_%03d", k);
        found += name_listed(&big, n, sizeof n);
    }
    CHECK(big.n == 600 && found == 600);
    CHECK(!name_listed(&big, "Item_Many_600", 14));

    remove_list("weapons.txt"), remove_list("items.txt"), remove_list("rupees.txt");
    remove_list("empty.txt"), remove_list("many.txt");
    printf("test_online_names: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
