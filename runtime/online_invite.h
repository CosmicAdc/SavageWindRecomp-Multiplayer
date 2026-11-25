// Invitation code: (family, address, port) <-> short text a player can copy and paste. Pure: no sockets, no
// allocation. The code carries no password and no name, only where to connect.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ONLINE_INVITE_V4 4
#define ONLINE_INVITE_V6 6
// Text size including the NUL: IPv4 "XXXX-XXXX-XXXX-XXX" (15 symbols) and IPv6 (34 symbols, 9 groups).
#define ONLINE_INVITE_TEXT_V4 19
#define ONLINE_INVITE_TEXT_V6 43

// family: ONLINE_INVITE_V4 (addr = 4 bytes) or ONLINE_INVITE_V6 (addr = 16 bytes), network order.
// port: 1..65535. Writes a NUL-terminated text; false if the input is invalid or cap is too small.
bool online_invite_encode(int family, const uint8_t *addr, uint16_t port, char *out, size_t cap);
// Accepts upper/lower case, ignores '-' and spaces, reads O as 0 and I/L as 1. addr must hold 16 bytes (only
// 4 are used for IPv4). Returns false for anything that is not a well-formed code with a valid checksum.
bool online_invite_decode(const char *text, int *family, uint8_t *addr, uint16_t *port);
