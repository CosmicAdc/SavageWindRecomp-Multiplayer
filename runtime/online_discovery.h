// Discovery of games on the local network (T323): a UDP query and a fixed-size reply. Nothing here is the TCP
// protocol: PROTOCOL and runtime/online_wire.h are not touched; the packets carry the protocol number the caller
// gives. The packet functions are pure (no sockets); the host and scan functions use sockets and a thread.
// Thread rule: none of these may be called from the game thread or the presentation thread (they do I/O).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISCOVERY_PORT 52521      // UDP, the host listens here and the scan asks here (ONLINE_PORT + 1)
#define DISCOVERY_NONCE 8
#define DISCOVERY_VERSION 16      // summary of the version: opaque bytes the caller fills and compares
#define DISCOVERY_NAME 24         // bytes on the wire (0-terminated)
#define DISCOVERY_QUERY_SIZE 14   // "SWLQ" + u16 protocol + nonce
#define DISCOVERY_REPLY_SIZE 59   // "SWLR" + nonce + u16 protocol + u16 tcp port + players + max + password + version + name
#define DISCOVERY_MAX_RESULTS 16
#define DISCOVERY_MAX_PLAYERS 16  // a reply with a larger max is rejected
#define DISCOVERY_READ_PER_POLL 16
#define DISCOVERY_REPLIES_PER_SEC 4  // per source IP
#define DISCOVERY_SCAN_MS 5000
#define DISCOVERY_SCAN_STEP_MS 500

// What a host announces (and what a client learns). Integers are big-endian on the wire.
typedef struct {
    uint16_t protocol;                    // the PROTOCOL of the sender
    uint16_t tcp_port;                    // where to connect (never 0)
    uint8_t players, max_players;         // 0 <= players <= max_players, 1 <= max_players <= DISCOVERY_MAX_PLAYERS
    uint8_t password;                     // 0 or 1
    uint8_t version[DISCOVERY_VERSION];   // summary of the build/game version (opaque)
    char name[DISCOVERY_NAME + 1];        // always 0-terminated; no byte below 32 (see discovery_parse_reply)
} DiscoveryInfo;

typedef struct {
    DiscoveryInfo info;
    char address[64];   // textual IP the reply came from (IPv6 link-local: with %interface), ready for the invite/join code
} DiscoveryResult;

// ---- pure packets. Build: out must hold DISCOVERY_QUERY_SIZE / DISCOVERY_REPLY_SIZE; returns the size written, 0 if cap is too small.
size_t discovery_build_query(uint8_t *out, size_t cap, uint16_t protocol, const uint8_t nonce[DISCOVERY_NONCE]);
// Parse: the length must be exact, the magic must match. Hostile input: nothing else is trusted.
bool discovery_parse_query(const uint8_t *p, size_t n, uint16_t *protocol, uint8_t nonce[DISCOVERY_NONCE]);
// A name with a byte below 32 (or 127) gets that byte replaced by '?'; it is cut at the first 0. A reply with
// players > max_players, max_players 0 or > DISCOVERY_MAX_PLAYERS, tcp_port 0 or password > 1 is rejected.
size_t discovery_build_reply(uint8_t *out, size_t cap, const uint8_t nonce[DISCOVERY_NONCE], const DiscoveryInfo *info);
bool discovery_parse_reply(const uint8_t *p, size_t n, uint8_t nonce[DISCOVERY_NONCE], DiscoveryInfo *info);

// Per-IP limiter for the replies (pure: the clock is injected). One slot per source; a full table evicts the oldest.
typedef struct {
    uint8_t addr[17];        // [0] family 4 or 6, then the address bytes (IPv4 in the first 4)
    uint64_t window_ns;      // start of the one-second window
    uint32_t count;          // replies in this window
    bool used;
} DiscoveryLimitSlot;
typedef struct { DiscoveryLimitSlot slot[32]; } DiscoveryLimiter;
void discovery_limiter_init(DiscoveryLimiter *l);
// true: this reply may be sent (counted). false: the IP already had DISCOVERY_REPLIES_PER_SEC in this second.
bool discovery_limiter_allow(DiscoveryLimiter *l, int family, const void *addr, uint64_t now_ns);

// ---- host side (non-blocking; call from the network thread, never from the game thread)
// Opens the UDP sockets on port (v4 and, when available, v6 with the multicast group ff02::1). false: v4 failed.
bool discovery_host_open(uint16_t port);
// Answers the queries waiting (at most DISCOVERY_READ_PER_POLL datagrams per call, DISCOVERY_REPLIES_PER_SEC per IP)
// with *info (the caller keeps it up to date: players, password...). Returns the number of replies sent.
// A query with another protocol number than info->protocol is ignored. Does nothing if not open.
int discovery_host_poll(const DiscoveryInfo *info);
void discovery_host_close(void);

// ---- client side. Everything runs in its own thread; the caller only starts, reads and stops.
// protocol: the PROTOCOL the client speaks (replies of another protocol are kept: the caller sees info.protocol).
bool discovery_scan_start(uint16_t protocol);  // false: already scanning or the sockets/thread failed
// Copies up to max results (at most DISCOVERY_MAX_RESULTS) under the mutex; returns how many.
int discovery_scan_results(DiscoveryResult *list, int max);
bool discovery_scan_active(void);              // true until the 5 s are over or discovery_scan_stop
void discovery_scan_stop(void);                // asks the thread to end and joins it (it never waits more than ~100 ms)
