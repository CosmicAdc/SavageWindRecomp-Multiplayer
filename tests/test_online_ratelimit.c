#include "online_ratelimit.h"

#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define SEC 1000000000ull

int main(void) {
    OnlineRateBucket b;

    // burst: starts full, then empty
    online_ratelimit_init(&b, 10, 5);
    int n = 0;
    for (int i = 0; i < 20; i++) n += online_ratelimit_take(&b, 100 * SEC);
    CHECK(n == 5);

    // exact rate: 10/s, one take per 100 ms after draining -> every take succeeds
    online_ratelimit_init(&b, 10, 1);
    CHECK(online_ratelimit_take(&b, 0));
    int ok = 0;
    for (int i = 1; i <= 100; i++) ok += online_ratelimit_take(&b, (uint64_t)i * SEC / 10);
    CHECK(ok == 100);
    // twice as fast as the rate: half get through
    online_ratelimit_init(&b, 10, 1);
    online_ratelimit_take(&b, 0);
    ok = 0;
    for (int i = 1; i <= 200; i++) ok += online_ratelimit_take(&b, (uint64_t)i * SEC / 20);
    CHECK(ok == 100);

    // clock that stands still: no refill
    online_ratelimit_init(&b, 10, 2);
    CHECK(online_ratelimit_take(&b, 5 * SEC));
    CHECK(online_ratelimit_take(&b, 5 * SEC));
    CHECK(!online_ratelimit_take(&b, 5 * SEC));
    CHECK(!online_ratelimit_take(&b, 5 * SEC));

    // clock that goes back: no free tokens, no negative or infinite ones
    CHECK(!online_ratelimit_take(&b, 1 * SEC));
    CHECK(b.tokens >= 0 && b.tokens < 1);
    CHECK(!online_ratelimit_take(&b, 1 * SEC + SEC / 20));  // 50 ms after the re-base: 0.5 token
    CHECK(online_ratelimit_take(&b, 1 * SEC + SEC / 10));   // 100 ms: 1 token

    // long idle: refill is capped at burst
    online_ratelimit_init(&b, 10, 3);
    for (int i = 0; i < 3; i++) online_ratelimit_take(&b, 0);
    CHECK(!online_ratelimit_take(&b, 0));
    n = 0;
    for (int i = 0; i < 10; i++) n += online_ratelimit_take(&b, 100000ull * SEC);
    CHECK(n == 3);

    // zero rate: only the burst, never refills
    online_ratelimit_init(&b, 0, 1);
    CHECK(online_ratelimit_take(&b, 0));
    CHECK(!online_ratelimit_take(&b, 1000 * SEC));

    if (fails) { printf("%d failure(s)\n", fails); return 1; }
    printf("test_online_ratelimit: ok\n");
    return 0;
}
