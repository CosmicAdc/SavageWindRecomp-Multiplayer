#include "online_netstat.h"

#include <math.h>
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define MS 1000000ull
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))

static OnlineNetStat ns;

int main(void) {
    float ping, p95, jit, loss;
    int level;

    // no data: nothing to judge, no division by zero
    online_netstat_init(&ns);
    online_netstat_get(&ns, 1000 * MS, &ping, &p95, &jit, &loss, &level);
    CHECK(ping == 0 && p95 == 0 && jit == 0 && loss == 0 && level == ONLINE_NETSTAT_GOOD);
    online_netstat_get(&ns, 0, NULL, NULL, NULL, NULL, NULL);  // all outputs optional

    // perfect flow: 40 states/s, ping 40 ms
    online_netstat_init(&ns);
    uint16_t seq = 0;
    uint64_t t = 0;
    for (int i = 0; i < 400; i++) {
        t = (uint64_t)i * 25 * MS;
        online_netstat_state(&ns, seq++, t);
        if (i % 20 == 0) online_netstat_ping(&ns, 40, t);
    }
    online_netstat_get(&ns, t, &ping, &p95, &jit, &loss, &level);
    CHECK(NEAR(ping, 40, 0.01) && NEAR(p95, 40, 0.01));
    CHECK(NEAR(loss, 0, 0.001) && NEAR(jit, 0, 0.01));
    CHECK(level == ONLINE_NETSTAT_GOOD);

    // jitter: intervals alternate 10 and 30 ms -> mean 20, mean deviation 10
    online_netstat_init(&ns);
    t = 0;
    for (int i = 0; i < 100; i++) {
        t += (i % 2 ? 30 : 10) * MS;
        online_netstat_state(&ns, (uint16_t)i, t);
    }
    online_netstat_get(&ns, t, NULL, NULL, &jit, &loss, NULL);
    CHECK(NEAR(jit, 10, 0.5) && NEAR(loss, 0, 0.001));

    // percentile: pings 1..20 -> mean 10.5, p95 = 19
    online_netstat_init(&ns);
    for (int i = 1; i <= 20; i++) online_netstat_ping(&ns, (float)i, (uint64_t)i * 10 * MS);
    online_netstat_state(&ns, 0, 200 * MS);
    online_netstat_get(&ns, 300 * MS, &ping, &p95, NULL, NULL, NULL);
    CHECK(NEAR(ping, 10.5, 0.001) && NEAR(p95, 19, 0.001));
    // pings leave the window; the ring keeps only its capacity
    for (int i = 0; i < 100; i++) online_netstat_ping(&ns, 5, (uint64_t)(400 + i) * MS);
    online_netstat_ping(&ns, -1, 500 * MS);  // invalid: ignored
    online_netstat_get(&ns, 500 * MS, &ping, NULL, NULL, NULL, NULL);
    CHECK(NEAR(ping, 5, 0.001));
    online_netstat_get(&ns, 20000 * MS, &ping, &p95, NULL, NULL, NULL);
    CHECK(ping == 0 && p95 == 0);

    // every 5th sequence number missing: 5 received of 6 expected = 16.7 % loss, BAD only after the 2 s hysteresis
    online_netstat_init(&ns);
    seq = 0;
    uint64_t first_bad = 0;
    for (int i = 0; i < 600; i++) {  // 15 s
        t = (uint64_t)i * 25 * MS;
        seq++;
        if (i % 5 == 4) seq++;  // one number never arrives
        online_netstat_state(&ns, seq, t);
        online_netstat_get(&ns, t, NULL, NULL, NULL, &loss, &level);
        if (level == ONLINE_NETSTAT_BAD && !first_bad) first_bad = t;
    }
    CHECK(NEAR(loss, 16.7, 1.0));
    CHECK(level == ONLINE_NETSTAT_BAD);
    CHECK(first_bad >= 2000 * MS);  // not instant: the candidate level waits 2 s

    // 10 received of 11 expected = 9.1 % is not "bad" (> 10) but not good (>= 2): FAIR
    online_netstat_init(&ns);
    seq = 0;
    for (int i = 0; i < 600; i++) {
        t = (uint64_t)i * 25 * MS;
        seq++;
        if (i % 10 == 9) seq++;
        online_netstat_state(&ns, seq, t);
        online_netstat_get(&ns, t, NULL, NULL, NULL, &loss, &level);
    }
    CHECK(NEAR(loss, 9.1, 1.0));
    CHECK(level == ONLINE_NETSTAT_FAIR);

    // hysteresis on ping: 40 ms until 6 s, then 400 ms
    online_netstat_init(&ns);
    seq = 0;
    first_bad = 0;
    for (int i = 0; i <= 120; i++) {  // 100 ms steps, 12 s
        t = (uint64_t)i * 100 * MS;
        for (int k = 0; k < 4; k++) online_netstat_state(&ns, seq++, t + (uint64_t)k * 25 * MS);
        if (i % 5 == 0) online_netstat_ping(&ns, t < 6000 * MS ? 40.0f : 400.0f, t);  // 2 per second, like the real PING
        online_netstat_get(&ns, t + 75 * MS, NULL, NULL, NULL, NULL, &level);
        if (t == 7000 * MS) CHECK(level == ONLINE_NETSTAT_GOOD);
        if (level == ONLINE_NETSTAT_BAD && !first_bad) first_bad = t;
    }
    // 10 pings in the window: the mean passes 250 when 6 of them are 400 ms (t = 8.5 s); FAIR was pending since 6.5 s
    // but the raw level changed to BAD before FAIR was shown, so BAD waits 2 s from 8.5 s: 10.5 s
    CHECK(first_bad >= 10400 * MS && first_bad <= 10700 * MS);

    // sequence wrap: 65530 ... 65535, 0 ... 10 with nothing missing
    online_netstat_init(&ns);
    t = 0;
    for (uint32_t i = 0; i < 17; i++) {
        t += 25 * MS;
        online_netstat_state(&ns, (uint16_t)(65530 + i), t);
    }
    online_netstat_get(&ns, t, NULL, NULL, NULL, &loss, &level);
    CHECK(NEAR(loss, 0, 0.001));
    // and with a real hole across the wrap (65535 and 0 missing): 2 missing out of 5 expected -> 40 %
    online_netstat_init(&ns);
    online_netstat_state(&ns, 65533, 10 * MS);
    online_netstat_state(&ns, 65534, 20 * MS);
    online_netstat_state(&ns, 1, 30 * MS);
    online_netstat_get(&ns, 40 * MS, NULL, NULL, NULL, &loss, NULL);
    CHECK(NEAR(loss, 100.0 * 2 / 5, 0.01));  // gaps 1,1,3 -> expected 5, received 3

    // repeats and out-of-order numbers are ignored (not loss, not extra receipts)
    online_netstat_init(&ns);
    online_netstat_state(&ns, 5, 10 * MS);
    online_netstat_state(&ns, 5, 20 * MS);
    online_netstat_state(&ns, 4, 30 * MS);
    online_netstat_state(&ns, 6, 40 * MS);
    online_netstat_get(&ns, 50 * MS, NULL, NULL, NULL, &loss, NULL);
    CHECK(NEAR(loss, 0, 0.001));

    // 6 s without data after having data: BAD at once, loss 0, no division by zero
    online_netstat_init(&ns);
    for (int i = 0; i < 100; i++) online_netstat_state(&ns, (uint16_t)i, (uint64_t)i * 25 * MS);
    online_netstat_get(&ns, 2475 * MS, NULL, NULL, NULL, &loss, &level);
    CHECK(level == ONLINE_NETSTAT_GOOD);
    online_netstat_get(&ns, 2475 * MS + 6000 * MS, &ping, &p95, &jit, &loss, &level);
    CHECK(level == ONLINE_NETSTAT_BAD && loss == 0 && jit == 0);
    // and it recovers only after the hysteresis once data comes back
    t = 2475 * MS + 6000 * MS;
    seq = 100;
    int back = 0;
    for (int i = 0; i < 400; i++) {
        t += 25 * MS;
        online_netstat_state(&ns, seq++, t);
        online_netstat_get(&ns, t, NULL, NULL, NULL, NULL, &level);
        if (!back && level != ONLINE_NETSTAT_BAD) back = i;
    }
    CHECK(level == ONLINE_NETSTAT_GOOD);
    CHECK(back >= 80);  // 2 s at 25 ms per step

    // clock going back does not break anything: samples "from the future" are not counted
    online_netstat_init(&ns);
    online_netstat_state(&ns, 0, 5000 * MS);
    online_netstat_state(&ns, 1, 5025 * MS);
    online_netstat_get(&ns, 1000 * MS, &ping, &p95, &jit, &loss, &level);
    CHECK(loss == 0 && jit == 0);

    if (fails) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("test_online_netstat: ok\n");
    return 0;
}
