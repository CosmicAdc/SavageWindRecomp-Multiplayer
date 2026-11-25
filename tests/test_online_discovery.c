// Tests for runtime/online_discovery.c (T323): the pure packets and the limiter, then a real UDP round trip on the
// loopback (skipped, and said so, when the machine will not open the socket). No game files, no TCP protocol.
#include "online_discovery.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <time.h>
#define SLEEP_MS(ms) do { struct timespec ts_ = {0, (long)(ms) * 1000000L}; nanosleep(&ts_, NULL); } while (0)
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define SEC 1000000000ull

static DiscoveryInfo sample(void) {
    DiscoveryInfo i;
    memset(&i, 0, sizeof i);
    i.protocol = 38, i.tcp_port = 52520, i.players = 1, i.max_players = 2, i.password = 1;
    for (int k = 0; k < DISCOVERY_VERSION; k++) i.version[k] = (uint8_t)(0xA0 + k);
    snprintf(i.name, sizeof i.name, "Cosmic");
    return i;
}

static void test_query(void) {
    uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8}, got[8], q[DISCOVERY_QUERY_SIZE + 4];
    CHECK(discovery_build_query(q, sizeof q, 38, nonce) == DISCOVERY_QUERY_SIZE);
    CHECK(!memcmp(q, "SWLQ", 4));
    uint16_t proto = 0;
    CHECK(discovery_parse_query(q, DISCOVERY_QUERY_SIZE, &proto, got) && proto == 38 && !memcmp(got, nonce, 8));
    // exact length only
    for (size_t n = 0; n <= sizeof q; n++)
        if (n != DISCOVERY_QUERY_SIZE) CHECK(!discovery_parse_query(q, n, &proto, got));
    // wrong magic, in each byte; a reply is not a query
    for (int k = 0; k < 4; k++) {
        uint8_t bad[DISCOVERY_QUERY_SIZE];
        memcpy(bad, q, sizeof bad);
        bad[k] ^= 0x20;
        CHECK(!discovery_parse_query(bad, sizeof bad, &proto, got));
    }
    // too small a buffer writes nothing
    uint8_t small[DISCOVERY_QUERY_SIZE - 1];
    CHECK(discovery_build_query(small, sizeof small, 38, nonce) == 0);
    CHECK(!discovery_parse_query(NULL, DISCOVERY_QUERY_SIZE, &proto, got));
}

static void test_reply(void) {
    uint8_t nonce[8] = {9, 8, 7, 6, 5, 4, 3, 2}, got[8], r[DISCOVERY_REPLY_SIZE + 4];
    DiscoveryInfo in = sample(), out;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &in) == DISCOVERY_REPLY_SIZE);
    CHECK(!memcmp(r, "SWLR", 4));
    CHECK(discovery_parse_reply(r, DISCOVERY_REPLY_SIZE, got, &out));
    CHECK(!memcmp(got, nonce, 8));
    CHECK(out.protocol == 38 && out.tcp_port == 52520 && out.players == 1 && out.max_players == 2 && out.password == 1);
    CHECK(!memcmp(out.version, in.version, DISCOVERY_VERSION));
    CHECK(!strcmp(out.name, "Cosmic"));
    // exact length only
    for (size_t n = 0; n <= sizeof r; n++)
        if (n != DISCOVERY_REPLY_SIZE) CHECK(!discovery_parse_reply(r, n, got, &out));
    // the fixed offsets, so that a change of layout is noticed (big-endian numbers)
    CHECK(r[12] == 0 && r[13] == 38 && r[14] == 0xCD && r[15] == 0x28);  // 52520 = 0xCD28
    CHECK(r[16] == 1 && r[17] == 2 && r[18] == 1);
    // a query is not a reply
    uint8_t q[DISCOVERY_QUERY_SIZE];
    discovery_build_query(q, sizeof q, 38, nonce);
    CHECK(!discovery_parse_reply(q, sizeof q, got, &out));

    // hostile name: control bytes become '?', the string always ends
    uint8_t h[DISCOVERY_REPLY_SIZE];
    memcpy(h, r, sizeof h);
    uint8_t *name = h + DISCOVERY_REPLY_SIZE - DISCOVERY_NAME;
    memset(name, 'x', DISCOVERY_NAME);  // no terminator at all
    name[0] = 7, name[1] = 31, name[2] = 127, name[3] = 'A', name[4] = 0x1B;
    CHECK(discovery_parse_reply(h, sizeof h, got, &out));
    CHECK(strlen(out.name) == DISCOVERY_NAME && out.name[DISCOVERY_NAME] == 0);
    CHECK(out.name[0] == '?' && out.name[1] == '?' && out.name[2] == '?' && out.name[3] == 'A' && out.name[4] == '?');
    for (size_t k = 0; k < strlen(out.name); k++) CHECK((unsigned char)out.name[k] >= 32 && (unsigned char)out.name[k] != 127);
    // text after a 0 is ignored
    memset(name, 0, DISCOVERY_NAME);
    memcpy(name, "ab", 2), name[3] = 'z';
    CHECK(discovery_parse_reply(h, sizeof h, got, &out) && !strcmp(out.name, "ab"));
    // an empty name is allowed
    memset(name, 0, DISCOVERY_NAME);
    CHECK(discovery_parse_reply(h, sizeof h, got, &out) && out.name[0] == 0);

    // numbers that make no sense are rejected (parse) or not built (build)
    DiscoveryInfo bad;
    bad = in, bad.tcp_port = 0;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &bad) == 0);
    bad = in, bad.max_players = 0;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &bad) == 0);
    bad = in, bad.max_players = DISCOVERY_MAX_PLAYERS + 1;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &bad) == 0);
    bad = in, bad.players = 3;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &bad) == 0);
    bad = in, bad.password = 2;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &bad) == 0);
    discovery_build_reply(r, sizeof r, nonce, &in);
    uint8_t b[DISCOVERY_REPLY_SIZE];
    memcpy(b, r, sizeof b), b[14] = b[15] = 0;  // port 0
    CHECK(!discovery_parse_reply(b, sizeof b, got, &out));
    memcpy(b, r, sizeof b), b[17] = 0;          // max 0
    CHECK(!discovery_parse_reply(b, sizeof b, got, &out));
    memcpy(b, r, sizeof b), b[16] = 255;        // players above max
    CHECK(!discovery_parse_reply(b, sizeof b, got, &out));
    memcpy(b, r, sizeof b), b[18] = 2;          // password flag
    CHECK(!discovery_parse_reply(b, sizeof b, got, &out));
    memcpy(b, r, sizeof b), b[17] = 200;        // absurd maximum
    CHECK(!discovery_parse_reply(b, sizeof b, got, &out));
    // the name given to build is also cleaned and cut
    in = sample();
    memset(in.name, 'n', DISCOVERY_NAME), in.name[DISCOVERY_NAME] = 0, in.name[2] = 9;
    CHECK(discovery_build_reply(r, sizeof r, nonce, &in) == DISCOVERY_REPLY_SIZE);
    CHECK(discovery_parse_reply(r, DISCOVERY_REPLY_SIZE, got, &out) && out.name[2] == '?' && out.name[3] == 'n');
    // a failed parse leaves the output alone
    DiscoveryInfo keep = sample(), probe = keep;
    CHECK(!discovery_parse_reply(b, 10, got, &probe) && !memcmp(&probe, &keep, sizeof keep));
}

static void test_limiter(void) {
    DiscoveryLimiter l;
    discovery_limiter_init(&l);
    uint8_t a[4] = {192, 168, 1, 10}, b[4] = {192, 168, 1, 11};
    int ok = 0;
    for (int i = 0; i < 10; i++) ok += discovery_limiter_allow(&l, 4, a, 5 * SEC + (uint64_t)i * 10000000ull);
    CHECK(ok == DISCOVERY_REPLIES_PER_SEC);                       // 4 in the first second, the rest refused
    CHECK(discovery_limiter_allow(&l, 4, b, 5 * SEC + 200000000ull));  // another IP has its own allowance
    CHECK(!discovery_limiter_allow(&l, 4, a, 5 * SEC + 900000000ull));
    ok = 0;
    for (int i = 0; i < 6; i++) ok += discovery_limiter_allow(&l, 4, a, 6 * SEC + (uint64_t)i);  // a new second
    CHECK(ok == DISCOVERY_REPLIES_PER_SEC);
    // a clock that goes back starts a new window (it must not lock an IP out)
    CHECK(discovery_limiter_allow(&l, 4, a, 1 * SEC));
    // an IPv4 address and an IPv6 one that start with the same bytes are different sources
    uint8_t v6[16] = {192, 168, 1, 10};
    for (int i = 0; i < 4; i++) CHECK(discovery_limiter_allow(&l, 6, v6, 100 * SEC));
    CHECK(!discovery_limiter_allow(&l, 6, v6, 100 * SEC));
    // bad input
    CHECK(!discovery_limiter_allow(&l, 5, a, 0) && !discovery_limiter_allow(&l, 4, NULL, 0) && !discovery_limiter_allow(NULL, 4, a, 0));
    // a flood of different sources does not break it: the table evicts the oldest and every new IP gets its replies
    discovery_limiter_init(&l);
    int firsts = 0;
    for (int i = 0; i < 1000; i++) {
        uint8_t src[4] = {10, (uint8_t)(i >> 8), (uint8_t)i, 1};
        firsts += discovery_limiter_allow(&l, 4, src, 200 * SEC + (uint64_t)i * 1000ull);
    }
    CHECK(firsts == 1000);
}

static void test_sockets(void) {
    DiscoveryInfo info = sample();
    CHECK(discovery_host_poll(&info) == 0);  // not open: nothing happens
    if (!discovery_host_open(DISCOVERY_PORT)) {
        printf("skip: the UDP socket on port %d cannot be opened here (socket part not run)\n", DISCOVERY_PORT);
        return;
    }
    CHECK(discovery_host_open(DISCOVERY_PORT));  // already open: fine
    DiscoveryResult res[DISCOVERY_MAX_RESULTS];

    CHECK(discovery_scan_start(38));
    CHECK(!discovery_scan_start(38));  // already scanning
    int n = 0;
    for (int t = 0; t < 300 && n == 0; t++) {  // up to 3 s; the host answers as the polls come
        discovery_host_poll(&info);
        SLEEP_MS(10);
        n = discovery_scan_results(res, DISCOVERY_MAX_RESULTS);
    }
    CHECK(n >= 1);
    int loop = 0;
    for (int i = 0; i < n; i++) {
        CHECK(res[i].info.tcp_port == 52520 && res[i].info.protocol == 38 && !strcmp(res[i].info.name, "Cosmic"));
        CHECK(res[i].info.password == 1 && res[i].info.players == 1 && res[i].info.max_players == 2);
        loop += !strcmp(res[i].address, "127.0.0.1");
    }
    printf("socket: %d result(s) in the first poll window, loopback seen: %d\n", n, loop);
    for (int t = 0; t < 50; t++) {  // let the rest of the scan arrive (other interfaces)
        discovery_host_poll(&info);
        SLEEP_MS(10);
    }
    n = discovery_scan_results(res, DISCOVERY_MAX_RESULTS);
    loop = 0;
    for (int i = 0; i < n; i++) loop += !strcmp(res[i].address, "127.0.0.1");
    CHECK(loop == 1);  // one entry per (address, port), however many replies
    CHECK(discovery_scan_results(res, 0) == 0 && discovery_scan_results(NULL, 4) == 0);
    discovery_scan_stop();
    CHECK(!discovery_scan_active());
    CHECK(discovery_scan_results(res, DISCOVERY_MAX_RESULTS) == n);  // results stay after the stop

    // a host with another protocol number does not answer; the scan finds nothing and a restart clears the list
    CHECK(discovery_scan_start(99));
    CHECK(discovery_scan_results(res, DISCOVERY_MAX_RESULTS) == 0);
    for (int t = 0; t < 120; t++) {
        discovery_host_poll(&info);
        SLEEP_MS(10);
    }
    CHECK(discovery_scan_results(res, DISCOVERY_MAX_RESULTS) == 0);
    discovery_scan_stop();

    // stopping must be quick
    CHECK(discovery_scan_start(38));
    discovery_scan_stop();
    CHECK(!discovery_scan_active());
    discovery_host_close();
    CHECK(discovery_host_poll(&info) == 0);
    discovery_host_close();  // twice: harmless
}

int main(void) {
    test_query();
    test_reply();
    test_limiter();
    test_sockets();
    if (fails) {
        printf("test_online_discovery: %d failure(s)\n", fails);
        return 1;
    }
    printf("test_online_discovery: OK\n");
    return 0;
}
