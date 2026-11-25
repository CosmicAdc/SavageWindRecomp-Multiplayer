// Password check for the online mode: salted key, HMAC-SHA256 challenge response and constant-time comparison.
// Pure apart from online_auth_random (the system's random source). SHA-256 comes from runtime/update_crypto.c.
//
//   key      = SHA-256(salt[16] || password)                 (the host stores salt + key, never the password)
//   response = HMAC-SHA256(key, challenge[16] || name)       (what the client sends back for a CHALLENGE)
//
// This proves knowledge of the password without sending it; it does not encrypt anything else.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ONLINE_AUTH_SALT 16
#define ONLINE_AUTH_CHALLENGE 16
#define ONLINE_AUTH_KEY 32
#define ONLINE_AUTH_MAC 32

#if defined(__GNUC__) || defined(__clang__)
#define ONLINE_AUTH_MUST_CHECK __attribute__((warn_unused_result))
#else
#define ONLINE_AUTH_MUST_CHECK
#endif

// HMAC-SHA256 (RFC 2104 / RFC 4231) of any key and message. key or msg may be NULL when their length is 0.
void online_auth_hmac_sha256(const void *key, size_t key_len, const void *msg, size_t msg_len,
                             uint8_t out[ONLINE_AUTH_MAC]);

// key = SHA-256(salt || password). password: NUL-terminated; NULL is the empty password.
void online_auth_key(const uint8_t salt[ONLINE_AUTH_SALT], const char *password, uint8_t key[ONLINE_AUTH_KEY]);

// out = HMAC-SHA256(key, challenge || name). name: NUL-terminated (a name read from the network must have been
// checked for its terminator on receive); NULL is the empty name.
void online_auth_response(const uint8_t key[ONLINE_AUTH_KEY], const uint8_t challenge[ONLINE_AUTH_CHALLENGE],
                          const char *name, uint8_t out[ONLINE_AUTH_MAC]);

// true if the n bytes of a and b are equal. Time depends only on n, never on where they differ.
bool online_auth_equal(const void *a, const void *b, size_t n);

// Fills buf with n bytes from the system's cryptographic random source (BCryptGenRandom on Windows, getrandom or
// /dev/urandom elsewhere). false: the source failed and buf must not be used (it is not a usable fallback).
ONLINE_AUTH_MUST_CHECK bool online_auth_random(void *buf, size_t n);

#ifdef __cplusplus
}
#endif
