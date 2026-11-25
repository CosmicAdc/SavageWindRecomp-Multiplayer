// Tests for runtime/coop_world.c (ADR-011, decision 29): a world id in a temporary folder, no game and no network.
//   clang -Wall -Iruntime tests/test_coop_world.c runtime/coop_world.c -o build/test_coop_world
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

#include "coop_world.h"

static int g_fail;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)

#define DIR "build/test_coop_world_tmp"
#define FILE_PATH DIR "/coop-world.txt"
#define TEMP_PATH DIR "/coop-world.txt.tmp"

#define K_HEX "0001abcdef102030405060708090feff"  // k_rnd as 32 digits

static const uint8_t k_rnd[16] = {0x00, 0x01, 0xab, 0xcd, 0xef, 0x10, 0x20, 0x30,
                                  0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xfe, 0xff};

static void put(const char *text) {
    FILE *f = fopen(FILE_PATH, "wb");
    if (f) fwrite(text, 1, strlen(text), f), fclose(f);
}

static bool exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void test_create_load(void) {
    uint8_t id[16], back[16];
    memset(id, 0, sizeof id);
    remove(FILE_PATH);
    CHECK(!coop_world_load(DIR, back));  // (no file yet)
    CHECK(coop_world_create(DIR, k_rnd, id));
    CHECK(memcmp(id, k_rnd, 16) == 0);
    CHECK(!exists(TEMP_PATH));
    memset(back, 0x55, sizeof back);
    CHECK(coop_world_load(DIR, back) && memcmp(back, k_rnd, 16) == 0);
    // (the file is 32 lowercase digits and a newline)
    char text[64] = {0};
    FILE *f = fopen(FILE_PATH, "rb");
    CHECK(f != NULL);
    size_t n = 0;
    if (f) n = fread(text, 1, sizeof text - 1, f), fclose(f);
    CHECK(n == 33 && strncmp(text, K_HEX, 32) == 0 && text[32] == '\n');
    // (creating again replaces it; id may be NULL)
    uint8_t other[16];
    memset(other, 0x42, sizeof other);
    CHECK(coop_world_create(DIR, other, NULL));
    CHECK(coop_world_load(DIR, back) && memcmp(back, other, 16) == 0);
    CHECK(!exists(TEMP_PATH));
    puts("PASS: create and load, lowercase digits, replace");
}

static void test_corrupt(void) {
    uint8_t back[16];
    memset(back, 0x77, sizeof back);
    const char *bad[] = {
        "",
        "\n",
        "0001abcdef102030405060708090fe",           // short
        K_HEX "00",                                 // long
        "0001abcdef102030405060708090fefg",         // not hex
        K_HEX " x",                                 // text after
        " 0001abcdef10203040506070" "8090feff",     // leading space
        K_HEX "\nextra\n",
        "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        put(bad[i]);
        CHECK(!coop_world_load(DIR, back));
    }
    for (int i = 0; i < 16; i++) CHECK(back[i] == 0x77);  // (never written on failure)
    // (upper case and a CRLF are accepted, and so is no newline at all)
    put("0001ABCDEF102030405060708090FEFF\r\n");
    CHECK(coop_world_load(DIR, back) && memcmp(back, k_rnd, 16) == 0);
    put(K_HEX);
    CHECK(coop_world_load(DIR, back) && memcmp(back, k_rnd, 16) == 0);
    puts("PASS: corrupt files are refused, id untouched");
}

static void test_bad_args(void) {
    uint8_t id[16] = {0};
    CHECK(!coop_world_load(NULL, id));
    CHECK(!coop_world_load("", id));
    CHECK(!coop_world_load(DIR, NULL));
    CHECK(!coop_world_create(NULL, k_rnd, id));
    CHECK(!coop_world_create("", k_rnd, id));
    CHECK(!coop_world_create(DIR, NULL, id));
    CHECK(!coop_world_create(DIR "/no-such-folder", k_rnd, id));  // (the folder must exist)
    char big[2048];
    memset(big, 'a', sizeof big - 1), big[sizeof big - 1] = 0;
    CHECK(!coop_world_load(big, id));
    CHECK(!coop_world_create(big, k_rnd, id));
    puts("PASS: bad arguments");
}

static void test_name(void) {
    char a[COOP_WORLD_NAME_SIZE], b[COOP_WORLD_NAME_SIZE];
    coop_world_profile_name(k_rnd, a);
    CHECK(strcmp(a, "coop-0001abcdef102030") == 0);
    coop_world_profile_name(k_rnd, b);
    CHECK(strcmp(a, b) == 0);  // (stable)
    CHECK(coop_world_valid_name(a));
    // (only the first 8 bytes count; the other 8 do not change the name)
    uint8_t id[16];
    memcpy(id, k_rnd, 16);
    id[8] ^= 0xff, id[15] ^= 0xff;
    coop_world_profile_name(id, b);
    CHECK(strcmp(a, b) == 0);
    id[7] ^= 1;
    coop_world_profile_name(id, b);
    CHECK(strcmp(a, b) != 0);
    memset(id, 0, sizeof id);
    coop_world_profile_name(id, a);
    CHECK(strcmp(a, "coop-0000000000000000") == 0 && coop_world_valid_name(a));
    memset(id, 0xff, sizeof id);
    coop_world_profile_name(id, a);
    CHECK(strcmp(a, "coop-ffffffffffffffff") == 0 && coop_world_valid_name(a));
    coop_world_profile_name(NULL, a);
    CHECK(a[0] == 0);
    puts("PASS: profile name is stable and valid");
}

static void test_valid_name(void) {
    CHECK(coop_world_valid_name("coop-0123456789abcdef"));
    CHECK(coop_world_valid_name("a"));
    CHECK(coop_world_valid_name("a-b-9"));
    char s[64];
    memset(s, 'a', 31), s[31] = 0;
    CHECK(coop_world_valid_name(s));  // (31)
    memset(s, 'a', 32), s[32] = 0;
    CHECK(!coop_world_valid_name(s));  // (32)
    CHECK(!coop_world_valid_name(NULL));
    CHECK(!coop_world_valid_name(""));
    CHECK(!coop_world_valid_name("Coop-1"));  // (upper case)
    CHECK(!coop_world_valid_name("coop_1"));
    CHECK(!coop_world_valid_name("coop 1"));
    CHECK(!coop_world_valid_name("."));
    CHECK(!coop_world_valid_name(".."));
    CHECK(!coop_world_valid_name("../x"));
    CHECK(!coop_world_valid_name("a/b"));
    CHECK(!coop_world_valid_name("a\\b"));
    CHECK(!coop_world_valid_name("C:"));
    CHECK(!coop_world_valid_name("a.b"));
    CHECK(!coop_world_valid_name("caf\xc3\xa9"));
    puts("PASS: valid names are [a-z0-9-], 1 to 31 characters");
}

int main(void) {
    MKDIR("build");
    MKDIR(DIR);  // (it may already be there)
    test_create_load();
    test_corrupt();
    test_bad_args();
    test_name();
    test_valid_name();
    remove(FILE_PATH);
    remove(TEMP_PATH);
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    puts("PASS: coop_world");
    return 0;
}
