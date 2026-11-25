#include "online_auth.h"
#include "update_crypto.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// out = bytes of the hex string; returns the byte count
static size_t unhex(const char *s, uint8_t *out) {
    size_t n = 0;
    for (; s[0] && s[1]; s += 2) {
        unsigned v;
        sscanf(s, "%2x", &v);
        out[n++] = (uint8_t)v;
    }
    return n;
}

static bool same_hex(const uint8_t *got, size_t n, const char *hex) {
    uint8_t want[64];
    return unhex(hex, want) == n && memcmp(got, want, n) == 0;
}

static void sha256(const void *p, size_t n, uint8_t out[32]) {
    UcSha256 c;
    uc_sha256_init(&c);
    uc_sha256_update(&c, p, n);
    uc_sha256_final(&c, out);
}

static bool all_zero(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (p[i]) return false;
    return true;
}

// RFC 4231 section 4: HMAC-SHA-256 test cases 1-7
static void rfc4231(void) {
    static struct {
        int key_byte, key_len;  // key_byte < 0: key 0x01 0x02 ... key_len
        const char *data;       // text, or NULL for data_byte * data_len
        int data_byte, data_len;
        size_t mac_len;         // 16 for case 5 (truncated output)
        const char *mac;
    } cases[] = {
        {0x0b, 20, "Hi There", 0, 0, 32, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
        {0, 0, "what do ya want for nothing?", 0, 0, 32,
         "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"},
        {0xaa, 20, NULL, 0xdd, 50, 32, "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"},
        {-1, 25, NULL, 0xcd, 50, 32, "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b"},
        {0x0c, 20, "Test With Truncation", 0, 0, 16, "a3b6167473100ee06e0c796c2955552b"},
        {0xaa, 131, "Test Using Larger Than Block-Size Key - Hash Key First", 0, 0, 32,
         "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"},
        {0xaa, 131,
         "This is a test using a larger than block-size key and a larger than block-size data. The key needs to be "
         "hashed before being used by the HMAC algorithm.",
         0, 0, 32, "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"},
    };
    for (size_t t = 0; t < sizeof cases / sizeof cases[0]; t++) {
        uint8_t key[131], data[64], mac[32];
        size_t key_len = (size_t)cases[t].key_len, data_len;
        if (t == 1) {
            memcpy(key, "Jefe", 4);  // case 2: the key is text
            key_len = 4;
        } else {
            for (size_t i = 0; i < key_len; i++)
                key[i] = cases[t].key_byte < 0 ? (uint8_t)(i + 1) : (uint8_t)cases[t].key_byte;
        }
        const void *msg = data;
        if (cases[t].data) {
            msg = cases[t].data;
            data_len = strlen(cases[t].data);
        } else {
            data_len = (size_t)cases[t].data_len;
            memset(data, cases[t].data_byte, data_len);
        }
        online_auth_hmac_sha256(key, key_len, msg, data_len, mac);
        if (!same_hex(mac, cases[t].mac_len, cases[t].mac)) {
            printf("FAIL RFC 4231 test case %d\n", (int)t + 1);
            fails++;
        }
    }
}

// FIPS 180-4 SHA-256 examples published by NIST (empty, "abc", two blocks, one million 'a'): checks the SHA-256
// reused from update_crypto.c
static void fips180(void) {
    uint8_t d[32];
    sha256("", 0, d);
    CHECK(same_hex(d, 32, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    sha256("abc", 3, d);
    CHECK(same_hex(d, 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256(two, strlen(two), d);
    CHECK(same_hex(d, 32, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    static uint8_t a[1000];
    memset(a, 'a', sizeof a);
    UcSha256 c;
    uc_sha256_init(&c);
    for (int i = 0; i < 1000; i++) uc_sha256_update(&c, a, sizeof a);  // one million 'a'
    uc_sha256_final(&c, d);
    CHECK(same_hex(d, 32, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

int main(void) {
    fips180();
    rfc4231();

    uint8_t salt[ONLINE_AUTH_SALT], challenge[ONLINE_AUTH_CHALLENGE];
    for (int i = 0; i < 16; i++) salt[i] = (uint8_t)(0xa0 + i), challenge[i] = (uint8_t)(0x10 * i + 3);

    // key = SHA-256(salt || password)
    uint8_t key[32], want[32], buf[64];
    online_auth_key(salt, "hunter2", key);
    memcpy(buf, salt, 16);
    memcpy(buf + 16, "hunter2", 7);
    sha256(buf, 23, want);
    CHECK(memcmp(key, want, 32) == 0);

    // response = HMAC-SHA256(key, challenge || name)
    uint8_t mac[32], mac2[32];
    online_auth_response(key, challenge, "Link", mac);
    memcpy(buf, challenge, 16);
    memcpy(buf + 16, "Link", 4);
    online_auth_hmac_sha256(key, 32, buf, 20, want);
    CHECK(memcmp(mac, want, 32) == 0);

    // round trip: the host keeps salt + key, the client derives the same key from the password it typed
    uint8_t host_key[32], client_key[32];
    online_auth_key(salt, "correct horse", host_key);
    online_auth_key(salt, "correct horse", client_key);
    online_auth_response(client_key, challenge, "Zelda", mac);
    online_auth_response(host_key, challenge, "Zelda", mac2);
    CHECK(online_auth_equal(mac, mac2, 32));
    // wrong password, other salt, other challenge, other name: all rejected
    online_auth_key(salt, "correct horsf", client_key);
    online_auth_response(client_key, challenge, "Zelda", mac);
    CHECK(!online_auth_equal(mac, mac2, 32));
    uint8_t salt2[16];
    memcpy(salt2, salt, 16);
    salt2[15] ^= 1;
    online_auth_key(salt2, "correct horse", client_key);
    online_auth_response(client_key, challenge, "Zelda", mac);
    CHECK(!online_auth_equal(mac, mac2, 32));
    uint8_t challenge2[16];
    memcpy(challenge2, challenge, 16);
    challenge2[0] ^= 0x80;
    online_auth_response(host_key, challenge2, "Zelda", mac);
    CHECK(!online_auth_equal(mac, mac2, 32));
    online_auth_response(host_key, challenge, "zelda", mac);
    CHECK(!online_auth_equal(mac, mac2, 32));

    // empty password: key = SHA-256(salt); NULL means the same; it differs from any non-empty password
    uint8_t empty_key[32];
    online_auth_key(salt, "", empty_key);
    sha256(salt, 16, want);
    CHECK(memcmp(empty_key, want, 32) == 0);
    online_auth_key(salt, NULL, key);
    CHECK(memcmp(empty_key, key, 32) == 0);
    online_auth_key(salt, "a", key);
    CHECK(memcmp(empty_key, key, 32) != 0);
    online_auth_response(empty_key, challenge, "Link", mac);
    online_auth_response(empty_key, challenge, "Link", mac2);
    CHECK(online_auth_equal(mac, mac2, 32));

    // 23-character name (the longest that fits the 24-byte wire field): all of it counts
    const char *name23 = "ABCDEFGHIJKLMNOPQRSTUVW";
    CHECK(strlen(name23) == 23);
    online_auth_response(host_key, challenge, name23, mac);
    memcpy(buf, challenge, 16);
    memcpy(buf + 16, name23, 23);
    online_auth_hmac_sha256(host_key, 32, buf, 39, want);
    CHECK(memcmp(mac, want, 32) == 0);
    online_auth_response(host_key, challenge, "ABCDEFGHIJKLMNOPQRSTUVX", mac2);  // last character differs
    CHECK(!online_auth_equal(mac, mac2, 32));
    online_auth_response(host_key, challenge, "ABCDEFGHIJKLMNOPQRSTUV", mac2);  // 22-character prefix
    CHECK(!online_auth_equal(mac, mac2, 32));
    // empty and NULL names are the same message (the challenge alone)
    online_auth_response(host_key, challenge, "", mac);
    online_auth_response(host_key, challenge, NULL, mac2);
    CHECK(online_auth_equal(mac, mac2, 32));
    online_auth_hmac_sha256(host_key, 32, challenge, 16, want);
    CHECK(memcmp(mac, want, 32) == 0);

    // constant-time comparison: equal, first byte, last byte, every bit, n = 0
    uint8_t x[32], y[32];
    for (int i = 0; i < 32; i++) x[i] = y[i] = (uint8_t)(i * 7);
    CHECK(online_auth_equal(x, y, 32));
    y[0] ^= 1;
    CHECK(!online_auth_equal(x, y, 32));
    y[0] ^= 1;
    y[31] ^= 0x80;
    CHECK(!online_auth_equal(x, y, 32));
    CHECK(online_auth_equal(x, y, 31));
    y[31] ^= 0x80;
    for (int bit = 0; bit < 8; bit++) {
        y[17] ^= (uint8_t)(1 << bit);
        CHECK(!online_auth_equal(x, y, 32));
        y[17] ^= (uint8_t)(1 << bit);
    }
    CHECK(online_auth_equal(x, y, 32));
    CHECK(online_auth_equal(x, y, 0));

    // system random source: works, fills the whole buffer, two draws differ
    uint8_t r1[32], r2[32];
    memset(r1, 0, sizeof r1);
    memset(r2, 0, sizeof r2);
    CHECK(online_auth_random(r1, sizeof r1));
    CHECK(online_auth_random(r2, sizeof r2));
    CHECK(!all_zero(r1, sizeof r1));
    CHECK(memcmp(r1, r2, sizeof r1) != 0);
    static uint8_t big[1 << 16];
    CHECK(online_auth_random(big, sizeof big));
    CHECK(!all_zero(big + sizeof big - 32, 32));  // the tail was written too
    CHECK(online_auth_random(NULL, 0));
    CHECK(!online_auth_random(NULL, 16));

    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("test_online_auth: ok\n");
    return 0;
}
