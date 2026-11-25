#include "online_version.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static OnlineBuildInfo make(void) {
    OnlineBuildInfo b;
    online_build_reset();
    OnlineBuildSources s = {38, "9bc886f8314dd42b", "1.9.0", 43, 1};
    online_build_init(&s);
    online_build_info(&b);
    return b;
}

int main(void) {
    OnlineBuildInfo a, b;
    uint8_t da[16], db[16];

    // before init: only the co-op version, DLC unknown, nothing else
    online_build_reset();
    online_build_info(&a);
    CHECK(strcmp(a.coop, ONLINE_COOP_VERSION) == 0 && !a.gamelib[0] && !a.game[0] && a.protocol == 0);
    CHECK(a.dlc == ONLINE_BUILD_DLC_UNKNOWN && online_build_valid(&a));

    // init fills the four sources and the co-op version; the first call wins
    a = make();
    CHECK(a.protocol == 38 && a.overlay == 43 && a.dlc == 1);
    CHECK(!strcmp(a.gamelib, "9bc886f8314dd42b") && !strcmp(a.game, "1.9.0") && !strcmp(a.coop, ONLINE_COOP_VERSION));
    OnlineBuildSources other = {99, "other", "2.0.0", 1, 0};
    online_build_init(&other);
    online_build_info(&b);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    online_build_init(NULL);
    online_build_reset();
    online_build_info(&b);
    CHECK(b.protocol == 0);  // reset forgets the sources

    // identical builds
    a = make();
    b = a;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_OK);
    CHECK(online_build_valid(&a));
    online_build_digest(&a, da);
    online_build_digest(&b, db);
    CHECK(memcmp(da, db, 16) == 0);

    // one difference per field, with its reason and a different digest
    b = a; b.protocol++;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_PROTOCOL);
    CHECK(online_build_compare(&b, &a) == ONLINE_BUILD_PROTOCOL);
    online_build_digest(&b, db); CHECK(memcmp(da, db, 16) != 0);
    b = a; strcpy(b.coop, "coop-2");
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_COOP);
    online_build_digest(&b, db); CHECK(memcmp(da, db, 16) != 0);
    b = a; b.gamelib[0] = '0';
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_GAMELIB);
    online_build_digest(&b, db); CHECK(memcmp(da, db, 16) != 0);
    b = a; strcpy(b.game, "1.8.0");
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_GAME);
    online_build_digest(&b, db); CHECK(memcmp(da, db, 16) != 0);
    b = a; b.overlay = 44;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_OVERLAY);
    online_build_digest(&b, db); CHECK(memcmp(da, db, 16) != 0);
    b = a; b.dlc = 0;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_DLC);
    online_build_digest(&b, db); CHECK(memcmp(da, db, 16) != 0);

    // order of the checks: the first field that differs decides
    b = a; b.dlc = 0; b.overlay = 1; strcpy(b.game, "x"); b.gamelib[1] = '!'; strcpy(b.coop, "z"); b.protocol = 1;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_PROTOCOL);
    b.protocol = a.protocol;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_COOP);
    strcpy(b.coop, a.coop);
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_GAMELIB);
    b.gamelib[1] = a.gamelib[1];
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_GAME);
    strcpy(b.game, a.game);
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_OVERLAY);
    b.overlay = a.overlay;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_DLC);

    // a relay without a game: the empty game version is not a difference, on either side
    b = a; b.game[0] = 0;
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_OK);
    CHECK(online_build_compare(&b, &a) == ONLINE_BUILD_OK);
    b.overlay = 7;  // ...but the other fields still count
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_OVERLAY);

    // text that came from the network without a terminator: different, never read past the array
    b = a; memset(b.coop, 'A', sizeof b.coop);
    CHECK(!online_build_valid(&b));
    CHECK(online_build_compare(&a, &b) == ONLINE_BUILD_COOP);
    CHECK(online_build_compare(&b, &a) == ONLINE_BUILD_COOP);
    b = a; memset(b.gamelib, 'A', sizeof b.gamelib);
    CHECK(!online_build_valid(&b) && online_build_compare(&a, &b) == ONLINE_BUILD_GAMELIB);
    b = a; memset(b.game, 'A', sizeof b.game);
    CHECK(!online_build_valid(&b) && online_build_compare(&a, &b) == ONLINE_BUILD_GAME);  // not "empty"
    CHECK(online_build_compare(&b, &b) == ONLINE_BUILD_GAME);
    online_build_digest(&b, db);  // no overrun on unterminated text

    // identical unterminated fields are still not accepted
    b = a; memset(b.coop, 'A', sizeof b.coop);
    CHECK(online_build_compare(&b, &b) == ONLINE_BUILD_COOP);

    // validity of the other bytes
    b = a; b.dlc = 8;
    CHECK(!online_build_valid(&b));
    b.dlc = ONLINE_BUILD_DLC_UNKNOWN;
    CHECK(online_build_valid(&b));
    b = a; b.pad[1] = 1;
    CHECK(!online_build_valid(&b));
    CHECK(!online_build_valid(NULL));
    CHECK(online_build_compare(NULL, &a) != ONLINE_BUILD_OK && online_build_compare(&a, NULL) != ONLINE_BUILD_OK);

    // digest ignores the bytes after a terminator
    b = a; b.game[sizeof b.game - 1] = 'Z';  // after the terminator of "1.9.0"
    online_build_digest(&a, da);
    online_build_digest(&b, db);
    CHECK(memcmp(da, db, 16) == 0);
    online_build_digest(NULL, db);
    for (int i = 0; i < 16; i++) CHECK(db[i] == 0);
    online_build_digest(&a, NULL);

    // long sources are cut, not overrun, and stay terminated
    online_build_reset();
    char big[200];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = 0;
    OnlineBuildSources lng = {1, big, big, 2, 3};
    online_build_init(&lng);
    online_build_info(&b);
    CHECK(online_build_valid(&b));
    CHECK(strlen(b.gamelib) == ONLINE_BUILD_GAMELIB_CAP - 1 && strlen(b.game) == ONLINE_BUILD_GAME_CAP - 1);
    online_build_reset();
    OnlineBuildSources nul = {1, NULL, NULL, 0, 0};
    online_build_init(&nul);
    online_build_info(&b);
    CHECK(!b.gamelib[0] && !b.game[0] && online_build_valid(&b));
    online_build_info(NULL);

    CHECK(!strcmp(online_build_reason_name(ONLINE_BUILD_OK), "ok"));
    CHECK(!strcmp(online_build_reason_name(ONLINE_BUILD_DLC), "dlc"));
    CHECK(!strcmp(online_build_reason_name(99), "unknown"));

    if (fails) {
        printf("online_version: %d failure(s)\n", fails);
        return 1;
    }
    printf("online_version: OK\n");
    return 0;
}
