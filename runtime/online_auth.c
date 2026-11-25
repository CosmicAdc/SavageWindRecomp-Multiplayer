// Password check for the online mode (runtime/online_auth.h).
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#endif
#include <string.h>

#include "online_auth.h"
#include "update_crypto.h"

enum { BLOCK = 64 };  // SHA-256 block size

// memset that the compiler can't drop because the buffer is dead afterwards
static void wipe(void *p, size_t n) {
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (n--) *v++ = 0;
}

// ------------------------------------------------------------------ HMAC-SHA256 (RFC 2104)
typedef struct {
    UcSha256 inner, outer;
} Hmac;

static void hmac_init(Hmac *h, const void *key, size_t n) {
    uint8_t k[BLOCK] = {0}, pad[BLOCK];
    if (n > BLOCK) {  // keys longer than a block are hashed first
        UcSha256 c;
        uc_sha256_init(&c);
        uc_sha256_update(&c, key, n);
        uc_sha256_final(&c, k);
        wipe(&c, sizeof c);
    } else if (n) {
        memcpy(k, key, n);
    }
    for (int i = 0; i < BLOCK; i++) pad[i] = k[i] ^ 0x36;
    uc_sha256_init(&h->inner);
    uc_sha256_update(&h->inner, pad, BLOCK);
    for (int i = 0; i < BLOCK; i++) pad[i] = k[i] ^ 0x5c;
    uc_sha256_init(&h->outer);
    uc_sha256_update(&h->outer, pad, BLOCK);
    wipe(k, sizeof k);
    wipe(pad, sizeof pad);
}

static void hmac_update(Hmac *h, const void *data, size_t n) {
    if (n) uc_sha256_update(&h->inner, data, n);
}

static void hmac_final(Hmac *h, uint8_t out[ONLINE_AUTH_MAC]) {
    uint8_t d[32];
    uc_sha256_final(&h->inner, d);
    uc_sha256_update(&h->outer, d, sizeof d);
    uc_sha256_final(&h->outer, out);
    wipe(d, sizeof d);
    wipe(h, sizeof *h);
}

void online_auth_hmac_sha256(const void *key, size_t key_len, const void *msg, size_t msg_len,
                             uint8_t out[ONLINE_AUTH_MAC]) {
    Hmac h;
    hmac_init(&h, key, key_len);
    hmac_update(&h, msg, msg_len);
    hmac_final(&h, out);
}

// ------------------------------------------------------------------ key and challenge response
void online_auth_key(const uint8_t salt[ONLINE_AUTH_SALT], const char *password, uint8_t key[ONLINE_AUTH_KEY]) {
    UcSha256 c;
    uc_sha256_init(&c);
    uc_sha256_update(&c, salt, ONLINE_AUTH_SALT);
    if (password && *password) uc_sha256_update(&c, password, strlen(password));
    uc_sha256_final(&c, key);
    wipe(&c, sizeof c);  // its buffer still holds password bytes
}

void online_auth_response(const uint8_t key[ONLINE_AUTH_KEY], const uint8_t challenge[ONLINE_AUTH_CHALLENGE],
                          const char *name, uint8_t out[ONLINE_AUTH_MAC]) {
    Hmac h;
    hmac_init(&h, key, ONLINE_AUTH_KEY);
    hmac_update(&h, challenge, ONLINE_AUTH_CHALLENGE);  // fixed length, so challenge || name is unambiguous
    if (name) hmac_update(&h, name, strlen(name));
    hmac_final(&h, out);
}

bool online_auth_equal(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    volatile uint8_t d = 0;  // no early exit: every byte is read whatever the earlier ones were
    for (size_t i = 0; i < n; i++) d |= x[i] ^ y[i];
    return d == 0;
}

// ------------------------------------------------------------------ system random source
#ifdef _WIN32
// bcrypt.dll is loaded at run time: neither the runtime link line nor the unit test links -lbcrypt.
typedef LONG(WINAPI *GenRandomFn)(void *alg, PUCHAR buf, ULONG n, ULONG flags);
#define GEN_RANDOM_SYSTEM_RNG 0x00000002u  // BCRYPT_USE_SYSTEM_PREFERRED_RNG
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800  // only System32: no bcrypt.dll planted next to the game
#endif

static bool system_random(uint8_t *p, size_t n) {
    HMODULE dll = LoadLibraryExW(L"bcrypt.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll) return false;
    GenRandomFn gen = (GenRandomFn)(void (*)(void))GetProcAddress(dll, "BCryptGenRandom");
    bool ok = gen != NULL;
    while (ok && n) {
        ULONG k = n > (1u << 20) ? (1u << 20) : (ULONG)n;
        ok = gen(NULL, p, k, GEN_RANDOM_SYSTEM_RNG) >= 0;  // NT_SUCCESS
        p += k, n -= k;
    }
    FreeLibrary(dll);
    return ok;
}
#else
static bool urandom(uint8_t *p, size_t n) {
    int fd;
    do fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC); while (fd < 0 && errno == EINTR);
    if (fd < 0) return false;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        p += r, n -= (size_t)r;
    }
    close(fd);
    return n == 0;
}

static bool system_random(uint8_t *p, size_t n) {
#if defined(__linux__)
    while (n) {
        ssize_t r = getrandom(p, n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && errno == ENOSYS) return urandom(p, n);  // kernel older than 3.17
        if (r <= 0) return false;
        p += r, n -= (size_t)r;
    }
    return true;
#else
    return urandom(p, n);
#endif
}
#endif

bool online_auth_random(void *buf, size_t n) {
    if (!n) return true;
    if (!buf) return false;
    return system_random((uint8_t *)buf, n);
}
