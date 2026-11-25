#include "online_addr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "online_invite.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#define MAX_TEXT 255  // characters of input accepted; longer text is not an address
#define MAX_NAME 253  // DNS name limit

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// s[0..n) must be exactly "a.b.c.d", each 0..255 with no leading zeros.
static bool parse_v4(const char *s, size_t n, uint8_t out[4]) {
    size_t i = 0;
    for (int part = 0; part < 4; part++) {
        size_t start = i;
        unsigned v = 0;
        while (i < n && is_digit(s[i]) && i - start < 3) v = v * 10 + (unsigned)(s[i++] - '0');
        size_t len = i - start;
        if (len == 0 || v > 255 || (len > 1 && s[start] == '0')) return false;
        out[part] = (uint8_t)v;
        if (part < 3) {
            if (i >= n || s[i] != '.') return false;
            i++;
        }
    }
    return i == n;
}

// s[0..n) is an IPv6 literal without brackets or zone: hex groups, one "::" at most, optional IPv4 tail.
static bool parse_v6(const char *s, size_t n, uint8_t out[16]) {
    uint16_t g[8];
    int ng = 0, gap = -1;
    size_t i = 0;
    if (n < 2) return false;
    if (s[0] == ':') {
        if (s[1] != ':') return false;
        gap = 0;
        i = 2;
    }
    while (i < n) {
        size_t e = i;
        bool dot = false;
        while (e < n && s[e] != ':') {
            if (s[e] == '.') dot = true;
            e++;
        }
        if (e == i) return false;
        if (dot) {
            uint8_t v4[4];
            if (e != n || ng > 6 || !parse_v4(s + i, e - i, v4)) return false;
            g[ng++] = (uint16_t)((v4[0] << 8) | v4[1]);
            g[ng++] = (uint16_t)((v4[2] << 8) | v4[3]);
            i = n;
            break;
        }
        if (e - i > 4 || ng >= 8) return false;
        unsigned v = 0;
        for (size_t k = i; k < e; k++) {
            int h = hex_value(s[k]);
            if (h < 0) return false;
            v = (v << 4) | (unsigned)h;
        }
        g[ng++] = (uint16_t)v;
        i = e;
        if (i == n) break;
        i++;                        // the ':'
        if (i == n) return false;   // single colon at the end
        if (s[i] == ':') {
            if (gap >= 0) return false;
            gap = ng;
            i++;
        }
    }
    if (gap < 0 ? ng != 8 : ng > 7) return false;
    int zeros = 8 - ng;
    int o = 0;
    for (int k = 0; k < 8; k++) {
        uint16_t v = 0;
        if (gap >= 0 && k >= gap && k < gap + zeros) v = 0;
        else v = g[k < gap || gap < 0 ? k : k - zeros];
        out[o++] = (uint8_t)(v >> 8);
        out[o++] = (uint8_t)v;
    }
    return true;
}

// Literal with an optional "%zone" at the end. Returns false if it is not a valid IPv6.
static bool valid_v6_zone(const char *s, size_t n) {
    size_t lit = n;
    for (size_t i = 0; i < n; i++)
        if (s[i] == '%') { lit = i; break; }
    if (lit < n) {
        if (lit + 1 == n) return false;
        for (size_t i = lit + 1; i < n; i++) {
            char c = s[i];
            if (!(is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || c == '-'))
                return false;
        }
    }
    uint8_t tmp[16];
    return parse_v6(s, lit, tmp);
}

// "" -> default; ":digits" -> that port (1..65535). Anything else is false.
static bool parse_port_suffix(const char *s, size_t n, uint16_t *port, uint16_t def) {
    if (n == 0) { *port = def; return true; }
    if (s[0] != ':' || n < 2 || n > 6) return false;
    unsigned v = 0;
    for (size_t i = 1; i < n; i++) {
        if (!is_digit(s[i])) return false;
        v = v * 10 + (unsigned)(s[i] - '0');
    }
    if (v < 1 || v > 65535) return false;
    *port = (uint16_t)v;
    return true;
}

static bool copy_host(char *host, size_t cap, const char *s, size_t n) {
    if (n + 1 > cap) return false;
    memcpy(host, s, n);
    host[n] = 0;
    return true;
}

bool online_addr_format(int family, const uint8_t *addr, char *out, size_t cap) {
    if (!addr || !out || cap == 0) return false;
    char buf[ONLINE_ADDR_TEXT_MAX];
    if (family == ONLINE_INVITE_V4) {
        snprintf(buf, sizeof buf, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
    } else if (family == ONLINE_INVITE_V6) {
        uint16_t g[8];
        for (int k = 0; k < 8; k++) g[k] = (uint16_t)((addr[2 * k] << 8) | addr[2 * k + 1]);
        int best = -1, best_len = 0;  // longest run of zero groups, at least 2; the first wins a tie
        for (int k = 0; k < 8;) {
            if (g[k]) { k++; continue; }
            int s = k;
            while (k < 8 && !g[k]) k++;
            if (k - s > best_len) { best = s; best_len = k - s; }
        }
        if (best_len < 2) best = -1;
        size_t o = 0;
        for (int k = 0; k < 8; k++) {
            if (k == best) {
                buf[o++] = ':';
                buf[o++] = ':';
                k += best_len - 1;
                continue;
            }
            if (o && buf[o - 1] != ':') buf[o++] = ':';
            o += (size_t)snprintf(buf + o, sizeof buf - o, "%x", g[k]);
        }
        buf[o] = 0;
    } else {
        return false;
    }
    size_t len = strlen(buf);
    if (len + 1 > cap) return false;
    memcpy(out, buf, len + 1);
    return true;
}

bool online_addr_parse(const char *text, char *host, size_t cap, uint16_t *port, uint16_t default_port) {
    if (!text || !host || !port || cap == 0) return false;
    host[0] = 0;
    // trim spaces and tabs; bound the length before looking at anything else
    size_t len = 0;
    while (text[len]) {
        if (len > MAX_TEXT + 64) return false;  // cheap bound on how much is read even if it is mostly blanks
        len++;
    }
    size_t a = 0, b = len;
    while (a < b && (text[a] == ' ' || text[a] == '\t')) a++;
    while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t')) b--;
    const char *s = text + a;
    size_t n = b - a;
    if (n == 0 || n > MAX_TEXT) return false;

    bool has_sep = false;
    for (size_t i = 0; i < n; i++)
        if (s[i] == '.' || s[i] == ':' || s[i] == '[' || s[i] == ']' || s[i] == '%') has_sep = true;

    // invitation code: only letters, digits, '-' and blanks; the checksum tells it from a name
    if (!has_sep) {
        char copy[MAX_TEXT + 1];
        memcpy(copy, s, n);
        copy[n] = 0;
        int fam;
        uint8_t addr[16];
        uint16_t p;
        if (online_invite_decode(copy, &fam, addr, &p)) {
            char tmp[ONLINE_ADDR_TEXT_MAX];
            if (!online_addr_format(fam, addr, tmp, sizeof tmp) || strlen(tmp) + 1 > cap) return false;
            strcpy(host, tmp);
            *port = p;
            return true;
        }
    }

    for (size_t i = 0; i < n; i++)  // no blanks or control characters from here on
        if ((unsigned char)s[i] <= ' ' || (unsigned char)s[i] == 0x7f) return false;

    if (s[0] == '[') {
        size_t close = 1;
        while (close < n && s[close] != ']') close++;
        if (close >= n || close == 1) return false;
        if (!valid_v6_zone(s + 1, close - 1)) return false;
        uint16_t p;
        if (!parse_port_suffix(s + close + 1, n - close - 1, &p, default_port)) return false;
        if (!copy_host(host, cap, s + 1, close - 1)) return false;
        *port = p;
        return true;
    }

    size_t colons = 0, first_colon = n;
    for (size_t i = 0; i < n; i++)
        if (s[i] == ':') {
            if (!colons) first_colon = i;
            colons++;
        }
    if (colons >= 2) {  // IPv6 without brackets: no port
        if (!valid_v6_zone(s, n)) return false;
        if (!copy_host(host, cap, s, n)) return false;
        *port = default_port;
        return true;
    }

    uint16_t p;
    if (!parse_port_suffix(s + first_colon, n - first_colon, &p, default_port)) return false;
    size_t hn = first_colon;
    if (hn == 0 || hn > MAX_NAME) return false;
    bool numeric = true;
    for (size_t i = 0; i < hn; i++) {
        char c = s[i];
        if (!is_digit(c) && c != '.') numeric = false;
        if (!(is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.'))
            return false;
    }
    if (numeric) {
        uint8_t v4[4];
        if (!parse_v4(s, hn, v4)) return false;
    } else {
        if (s[0] == '.') return false;
        for (size_t i = 1; i < hn; i++)
            if (s[i] == '.' && s[i - 1] == '.') return false;
    }
    if (!copy_host(host, cap, s, hn)) return false;
    *port = p;
    return true;
}

int online_addr_classify(int family, const uint8_t *addr) {
    if (!addr) return -1;
    if (family == ONLINE_INVITE_V4) {
        uint8_t a = addr[0], b = addr[1];
        if (a == 0 || a == 127 || a >= 224) return -1;  // unspecified, loopback, multicast and reserved
        if (a == 169 && b == 254) return -1;            // IPv4 link-local
        if (a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127))
            return ONLINE_ADDR_V4_PRIVATE;
        return ONLINE_ADDR_V4_PUBLIC;
    }
    if (family == ONLINE_INVITE_V6) {
        if ((addr[0] & 0xe0) == 0x20) return ONLINE_ADDR_V6_GLOBAL;                       // 2000::/3
        if (addr[0] == 0xfe && (addr[1] & 0xc0) == 0x80) return ONLINE_ADDR_V6_LINK_LOCAL;  // fe80::/10
    }
    return -1;
}

static bool add_local(OnlineLocalAddr *list, int *count, int max, int family, const uint8_t *addr, uint32_t scope) {
    if (*count >= max) return false;
    int kind = online_addr_classify(family, addr);
    if (kind < 0) return true;
    for (int i = 0; i < *count; i++)  // the same address can show up once per adapter entry
        if (list[i].family == family && list[i].scope == (kind == ONLINE_ADDR_V6_LINK_LOCAL ? scope : 0) &&
            !memcmp(list[i].addr, addr, family == ONLINE_INVITE_V4 ? 4 : 16))
            return true;
    OnlineLocalAddr *e = &list[*count];
    memset(e, 0, sizeof *e);
    e->family = family;
    e->kind = (OnlineAddrKind)kind;
    memcpy(e->addr, addr, family == ONLINE_INVITE_V4 ? 4 : 16);
    e->scope = kind == ONLINE_ADDR_V6_LINK_LOCAL ? scope : 0;
    if (!online_addr_format(family, e->addr, e->text, sizeof e->text)) return true;
    (*count)++;
    return true;
}

#ifdef _WIN32

typedef ULONG(WINAPI *GetAdaptersAddressesFn)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);

int online_addr_local(OnlineLocalAddr *list, int max) {
    if (!list || max <= 0) return 0;
    HMODULE dll = LoadLibraryA("iphlpapi.dll");
    if (!dll) return 0;
    GetAdaptersAddressesFn get = (GetAdaptersAddressesFn)(void *)GetProcAddress(dll, "GetAdaptersAddresses");
    int count = 0;
    if (get) {
        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        ULONG size = 16 * 1024;
        PIP_ADAPTER_ADDRESSES buf = NULL;
        ULONG rc = ERROR_BUFFER_OVERFLOW;
        for (int tries = 0; tries < 4 && rc == ERROR_BUFFER_OVERFLOW; tries++) {
            free(buf);
            buf = (PIP_ADAPTER_ADDRESSES)malloc(size);
            if (!buf) break;
            rc = get(AF_UNSPEC, flags, NULL, buf, &size);
        }
        if (buf && rc == NO_ERROR) {
            for (PIP_ADAPTER_ADDRESSES ad = buf; ad; ad = ad->Next) {
                if (ad->OperStatus != IfOperStatusUp || ad->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                for (PIP_ADAPTER_UNICAST_ADDRESS u = ad->FirstUnicastAddress; u; u = u->Next) {
                    const SOCKADDR *sa = u->Address.lpSockaddr;
                    if (!sa) continue;
                    if (sa->sa_family == AF_INET) {
                        const SOCKADDR_IN *in = (const SOCKADDR_IN *)sa;
                        add_local(list, &count, max, ONLINE_INVITE_V4, (const uint8_t *)&in->sin_addr, 0);
                    } else if (sa->sa_family == AF_INET6) {
                        const SOCKADDR_IN6 *in6 = (const SOCKADDR_IN6 *)sa;
                        uint32_t scope = in6->sin6_scope_id ? in6->sin6_scope_id : ad->Ipv6IfIndex;
                        add_local(list, &count, max, ONLINE_INVITE_V6, in6->sin6_addr.s6_addr, scope);
                    }
                }
            }
        }
        free(buf);
    }
    FreeLibrary(dll);
    return count;
}

#else

int online_addr_local(OnlineLocalAddr *list, int max) {
    if (!list || max <= 0) return 0;
    struct ifaddrs *ifa = NULL;
    if (getifaddrs(&ifa) != 0) return 0;
    int count = 0;
    for (struct ifaddrs *p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || !(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
        if (p->ifa_addr->sa_family == AF_INET) {
            const struct sockaddr_in *in = (const struct sockaddr_in *)(const void *)p->ifa_addr;
            add_local(list, &count, max, ONLINE_INVITE_V4, (const uint8_t *)&in->sin_addr, 0);
        } else if (p->ifa_addr->sa_family == AF_INET6) {
            const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)(const void *)p->ifa_addr;
            uint32_t scope = in6->sin6_scope_id ? in6->sin6_scope_id : if_nametoindex(p->ifa_name);
            add_local(list, &count, max, ONLINE_INVITE_V6, in6->sin6_addr.s6_addr, scope);
        }
    }
    freeifaddrs(ifa);
    return count;
}

#endif
