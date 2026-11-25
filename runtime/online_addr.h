// Addresses for the online tab: one parser for what the player types in "Address" (name, IPv4, IPv6, port,
// invitation code) and the list of this PC's own addresses with their kind. The parser is pure: no sockets, no
// name resolution, no allocation. online_addr_local only asks the OS for the interfaces.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Kind of a local address, from the point of view of "who can reach it".
typedef enum {
    ONLINE_ADDR_V4_PRIVATE = 0,  // 10/8, 172.16/12, 192.168/16 and 100.64/10 (carrier NAT): same network only
    ONLINE_ADDR_V4_PUBLIC,       // any other unicast IPv4
    ONLINE_ADDR_V6_GLOBAL,       // 2000::/3
    ONLINE_ADDR_V6_LINK_LOCAL,   // fe80::/10: only the same link, needs the interface (scope)
} OnlineAddrKind;

#define ONLINE_ADDR_HOST_MAX 256   // size (with NUL) that always holds what online_addr_parse can return
#define ONLINE_ADDR_TEXT_MAX 46    // size (with NUL) of an address text: longest IPv6 "xxxx:...:255.255.255.255"

typedef struct {
    int family;                       // ONLINE_INVITE_V4 (4) or ONLINE_INVITE_V6 (6), from online_invite.h
    OnlineAddrKind kind;
    uint8_t addr[16];                 // network order; IPv4 uses the first 4 bytes, the rest is zero
    uint32_t scope;                   // interface index for IPv6 link-local, 0 otherwise
    char text[ONLINE_ADDR_TEXT_MAX];  // address as text, IPv6 without brackets or zone
} OnlineLocalAddr;

// Reads what the player typed. Accepted (surrounding spaces and tabs are ignored):
//   name        name:port        1.2.3.4        1.2.3.4:port
//   [v6]        [v6]:port        v6 (no brackets, so no port)        an invitation code (see online_invite.h)
// An IPv6 may end in "%zone" (letters, digits, '_', '.', '-'). A text made only of digits and dots must be a
// well-formed IPv4. On success *host holds the host without brackets (for an invitation code, the address as text),
// *port the port found, or default_port when there is none. The port of a text must be 1..65535.
// Returns false for empty or longer-than-255 text, a '[' without ']', a bad address or port, a host that does not
// fit in cap (with NUL) or a name with characters that are not letters, digits, '-', '_' or '.'.
// Does not resolve names: the caller passes host to getaddrinfo.
bool online_addr_parse(const char *text, char *host, size_t cap, uint16_t *port, uint16_t default_port);

// Kind of an address, or -1 if it is not worth listing: loopback, unspecified, multicast, IPv4 link-local
// (169.254/16), IPv6 unique-local (fc00::/7), IPv4-mapped and anything else outside 2000::/3 and fe80::/10.
// family: ONLINE_INVITE_V4 (4 bytes) or ONLINE_INVITE_V6 (16 bytes).
int online_addr_classify(int family, const uint8_t *addr);

// Writes the address as text: dotted IPv4, or IPv6 in lower case with the longest run of zero groups as "::".
// Needs ONLINE_ADDR_TEXT_MAX bytes at most. False for a bad family, a null pointer or a cap that is too small.
bool online_addr_format(int family, const uint8_t *addr, char *out, size_t cap);

// Fills list with this PC's addresses that are up and worth listing (see online_addr_classify), at most max.
// Uses getifaddrs on POSIX and GetAdaptersAddresses (loaded at run time, no extra library) on Windows.
// Returns how many were written; 0 if the OS call fails.
int online_addr_local(OnlineLocalAddr *list, int max);
