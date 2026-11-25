#include "online_discovery.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define _CRT_RAND_S
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#include <windows.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif
#include <pthread.h>

// ------------------------------------------------------------------ pure packets

static const uint8_t MAGIC_QUERY[4] = {'S', 'W', 'L', 'Q'};
static const uint8_t MAGIC_REPLY[4] = {'S', 'W', 'L', 'R'};

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v; }
static uint16_t get16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

size_t discovery_build_query(uint8_t *out, size_t cap, uint16_t protocol, const uint8_t nonce[DISCOVERY_NONCE]) {
    if (!out || !nonce || cap < DISCOVERY_QUERY_SIZE) return 0;
    memcpy(out, MAGIC_QUERY, 4);
    put16(out + 4, protocol);
    memcpy(out + 6, nonce, DISCOVERY_NONCE);
    return DISCOVERY_QUERY_SIZE;
}

bool discovery_parse_query(const uint8_t *p, size_t n, uint16_t *protocol, uint8_t nonce[DISCOVERY_NONCE]) {
    if (!p || n != DISCOVERY_QUERY_SIZE || memcmp(p, MAGIC_QUERY, 4)) return false;
    if (protocol) *protocol = get16(p + 4);
    if (nonce) memcpy(nonce, p + 6, DISCOVERY_NONCE);
    return true;
}

// The announced numbers must make sense; the same check on both sides.
static bool info_valid(const DiscoveryInfo *i) {
    return i->tcp_port != 0 && i->max_players >= 1 && i->max_players <= DISCOVERY_MAX_PLAYERS &&
           i->players <= i->max_players && i->password <= 1;
}

size_t discovery_build_reply(uint8_t *out, size_t cap, const uint8_t nonce[DISCOVERY_NONCE], const DiscoveryInfo *info) {
    if (!out || !nonce || !info || cap < DISCOVERY_REPLY_SIZE || !info_valid(info)) return 0;
    uint8_t *p = out;
    memcpy(p, MAGIC_REPLY, 4), p += 4;
    memcpy(p, nonce, DISCOVERY_NONCE), p += DISCOVERY_NONCE;
    put16(p, info->protocol), p += 2;
    put16(p, info->tcp_port), p += 2;
    *p++ = info->players, *p++ = info->max_players, *p++ = info->password;
    memcpy(p, info->version, DISCOVERY_VERSION), p += DISCOVERY_VERSION;
    // the name: cut at the first 0, control bytes replaced, zero padding up to the fixed size
    size_t len = 0;
    while (len < DISCOVERY_NAME && info->name[len]) len++;
    memset(p, 0, DISCOVERY_NAME);
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)info->name[i];
        p[i] = c < 32 || c == 127 ? '?' : c;
    }
    p += DISCOVERY_NAME;
    return (size_t)(p - out);
}

bool discovery_parse_reply(const uint8_t *p, size_t n, uint8_t nonce[DISCOVERY_NONCE], DiscoveryInfo *info) {
    if (!p || n != DISCOVERY_REPLY_SIZE || memcmp(p, MAGIC_REPLY, 4)) return false;
    DiscoveryInfo i;
    memset(&i, 0, sizeof i);
    const uint8_t *q = p + 4;
    uint8_t got[DISCOVERY_NONCE];
    memcpy(got, q, DISCOVERY_NONCE), q += DISCOVERY_NONCE;
    i.protocol = get16(q), q += 2;
    i.tcp_port = get16(q), q += 2;
    i.players = *q++, i.max_players = *q++, i.password = *q++;
    memcpy(i.version, q, DISCOVERY_VERSION), q += DISCOVERY_VERSION;
    // the name is hostile: it ends at the first 0 (or at the last byte, which is forced to 0) and has no control byte
    size_t len = 0;
    while (len < DISCOVERY_NAME && q[len]) len++;
    for (size_t k = 0; k < len; k++) i.name[k] = q[k] < 32 || q[k] == 127 ? '?' : (char)q[k];
    i.name[len] = 0;
    i.name[DISCOVERY_NAME] = 0;
    if (!info_valid(&i)) return false;
    if (nonce) memcpy(nonce, got, DISCOVERY_NONCE);
    if (info) *info = i;
    return true;
}

// ------------------------------------------------------------------ per-IP limiter (pure)

void discovery_limiter_init(DiscoveryLimiter *l) { memset(l, 0, sizeof *l); }

bool discovery_limiter_allow(DiscoveryLimiter *l, int family, const void *addr, uint64_t now_ns) {
    if (!l || !addr || (family != 4 && family != 6)) return false;
    uint8_t key[17] = {0};
    key[0] = (uint8_t)family;
    memcpy(key + 1, addr, family == 4 ? 4 : 16);
    DiscoveryLimitSlot *hit = NULL, *free_slot = NULL, *oldest = &l->slot[0];
    for (int i = 0; i < 32; i++) {
        DiscoveryLimitSlot *s = &l->slot[i];
        if (!s->used) {
            if (!free_slot) free_slot = s;
            continue;
        }
        if (!memcmp(s->addr, key, sizeof key)) {
            hit = s;
            break;
        }
        if (!oldest->used || s->window_ns < oldest->window_ns) oldest = s;
    }
    if (!hit) {
        hit = free_slot ? free_slot : oldest;
        memset(hit, 0, sizeof *hit);
        memcpy(hit->addr, key, sizeof key);
        hit->used = true;
        hit->window_ns = now_ns;
    }
    if (now_ns < hit->window_ns || now_ns - hit->window_ns >= 1000000000ull) {  // new window (or the clock went back)
        hit->window_ns = now_ns;
        hit->count = 0;
    }
    if (hit->count >= DISCOVERY_REPLIES_PER_SEC) return false;
    hit->count++;
    return true;
}

// ------------------------------------------------------------------ sockets (small portability layer)

#ifdef _WIN32
typedef SOCKET dsock;
#define DS_INVALID INVALID_SOCKET
#define ds_close(s) closesocket(s)
#else
typedef int dsock;
#define DS_INVALID (-1)
#define ds_close(s) close(s)
#endif

static uint64_t now_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (uint64_t)((double)t.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static void sleep_ms(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
#endif
}

static bool net_init(void) {
#ifdef _WIN32
    static bool done;
    if (!done) {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) return false;
        done = true;
    }
#endif
    return true;
}

static bool set_nonblocking(dsock s) {
#ifdef _WIN32
    u_long on = 1;
    return ioctlsocket(s, FIONBIO, &on) == 0;
#else
    int f = fcntl(s, F_GETFL, 0);
    return f >= 0 && fcntl(s, F_SETFL, f | O_NONBLOCK) == 0;
#endif
}

static dsock udp_socket(int family) {
    dsock s = socket(family, SOCK_DGRAM, 0);
    if (s == DS_INVALID) return DS_INVALID;
    if (!set_nonblocking(s)) {
        ds_close(s);
        return DS_INVALID;
    }
#ifdef _WIN32
    BOOL no = FALSE;
    DWORD got = 0;  // ICMP "port unreachable" must not look like a read error
    WSAIoctl(s, SIO_UDP_CONNRESET, &no, sizeof no, NULL, 0, &got, NULL, NULL);
#endif
    return s;
}

// One datagram, non-blocking: its length, 0 nothing waiting (or a harmless error), -1 the socket is unusable.
// from: the source (sockaddr_storage). A datagram longer than size is reported as size + 1 (it can't be valid).
static long recv_one(dsock s, uint8_t *buf, size_t size, struct sockaddr_storage *from, socklen_t *fromlen) {
    *fromlen = sizeof *from;
#ifdef _WIN32
    int n = recvfrom(s, (char *)buf, (int)size, 0, (struct sockaddr *)from, fromlen);
    if (n >= 0) return n;
    int e = WSAGetLastError();
    if (e == WSAEMSGSIZE) return (long)size + 1;
    return e == WSAEWOULDBLOCK || e == WSAECONNRESET || e == WSAEINTR ? 0 : -1;
#else
    ssize_t n = recvfrom(s, buf, size, MSG_DONTWAIT | MSG_TRUNC, (struct sockaddr *)from, fromlen);
    if (n >= 0) return (long)n;
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNREFUSED || errno == EINTR ? 0 : -1;
#endif
}

static void send_to(dsock s, const uint8_t *p, size_t n, const struct sockaddr *to, socklen_t len) {
    // Best effort: a full buffer or an unreachable network just loses this datagram; the next round tries again.
#ifdef _WIN32
    (void)sendto(s, (const char *)p, (int)n, 0, to, len);
#else
    (void)sendto(s, p, n, MSG_DONTWAIT | MSG_NOSIGNAL, to, len);
#endif
}

// Textual source address; family 4 or 6 and the raw bytes for the limiter. false: unknown family.
static bool source_of(const struct sockaddr_storage *a, char *text, size_t cap, int *family, uint8_t raw[16]) {
    if (a->ss_family == AF_INET) {
        const struct sockaddr_in *s = (const struct sockaddr_in *)a;
        *family = 4;
        memcpy(raw, &s->sin_addr, 4);
        return inet_ntop(AF_INET, &s->sin_addr, text, (socklen_t)cap) != NULL;
    }
    if (a->ss_family == AF_INET6) {
        const struct sockaddr_in6 *s = (const struct sockaddr_in6 *)a;
        *family = 6;
        memcpy(raw, &s->sin6_addr, 16);
        if (!inet_ntop(AF_INET6, &s->sin6_addr, text, (socklen_t)cap)) return false;
        if (s->sin6_scope_id) {  // fe80::1%3: the interface is part of the address
            size_t l = strlen(text);
            if (l + 12 < cap) snprintf(text + l, cap - l, "%%%lu", (unsigned long)s->sin6_scope_id);
        }
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ host side

static struct {
    dsock s4, s6;
    bool open;
    DiscoveryLimiter limiter;
} H = {DS_INVALID, DS_INVALID, false, {{{{0}}}}};

bool discovery_host_open(uint16_t port) {
    if (H.open) return true;
    if (!port || !net_init()) return false;
    dsock s4 = udp_socket(AF_INET);
    if (s4 == DS_INVALID) return false;
    struct sockaddr_in a4;
    memset(&a4, 0, sizeof a4);
    a4.sin_family = AF_INET, a4.sin_port = htons(port), a4.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s4, (struct sockaddr *)&a4, sizeof a4)) {
        ds_close(s4);
        return false;
    }
    dsock s6 = udp_socket(AF_INET6);  // optional: no IPv6 on the machine is not an error
    if (s6 != DS_INVALID) {
        int on = 1;
        struct sockaddr_in6 a6;
        memset(&a6, 0, sizeof a6);
        a6.sin6_family = AF_INET6, a6.sin6_port = htons(port), a6.sin6_addr = in6addr_any;
        struct ipv6_mreq mr;
        memset(&mr, 0, sizeof mr);
        inet_pton(AF_INET6, "ff02::1", &mr.ipv6mr_multiaddr);  // all nodes, on the default interface
        if (setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&on, sizeof on) ||
            bind(s6, (struct sockaddr *)&a6, sizeof a6)) {
            ds_close(s6);
            s6 = DS_INVALID;
        } else {
            (void)setsockopt(s6, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&mr, sizeof mr);  // failure: unicast only
        }
    }
    H.s4 = s4, H.s6 = s6, H.open = true;
    discovery_limiter_init(&H.limiter);
    return true;
}

void discovery_host_close(void) {
    if (!H.open) return;
    ds_close(H.s4);
    if (H.s6 != DS_INVALID) ds_close(H.s6);
    H.s4 = H.s6 = DS_INVALID;
    H.open = false;
}

int discovery_host_poll(const DiscoveryInfo *info) {
    if (!H.open || !info) return 0;
    int replies = 0, reads = 0;
    dsock socks[2] = {H.s4, H.s6};
    for (int k = 0; k < 2 && reads < DISCOVERY_READ_PER_POLL; k++) {
        if (socks[k] == DS_INVALID) continue;
        while (reads < DISCOVERY_READ_PER_POLL) {
            uint8_t buf[64];
            struct sockaddr_storage from;
            socklen_t fl;
            long n = recv_one(socks[k], buf, sizeof buf, &from, &fl);
            if (n <= 0) break;  // nothing waiting, or this socket failed: try again at the next poll
            reads++;
            uint16_t protocol;
            uint8_t nonce[DISCOVERY_NONCE];
            if ((size_t)n != DISCOVERY_QUERY_SIZE || !discovery_parse_query(buf, (size_t)n, &protocol, nonce)) continue;
            if (protocol != info->protocol) continue;
            char text[64];
            int family;
            uint8_t raw[16];
            if (!source_of(&from, text, sizeof text, &family, raw)) continue;
            if (!discovery_limiter_allow(&H.limiter, family, raw, now_ns())) continue;
            uint8_t reply[DISCOVERY_REPLY_SIZE];
            size_t len = discovery_build_reply(reply, sizeof reply, nonce, info);
            if (!len) continue;
            send_to(socks[k], reply, len, (struct sockaddr *)&from, fl);
            replies++;
        }
    }
    return replies;
}

// ------------------------------------------------------------------ client side

typedef struct {
    struct sockaddr_storage addr;
    socklen_t len;
} Target;

#define MAX_TARGETS 40

static struct {
    pthread_mutex_t lock;
    DiscoveryResult list[DISCOVERY_MAX_RESULTS];
    int count;
    pthread_t thread;
    bool thread_valid;
    atomic_bool running, stop;
    uint16_t protocol;
    uint8_t nonce[DISCOVERY_NONCE];
} C = {.lock = PTHREAD_MUTEX_INITIALIZER};

static void fill_random(uint8_t *p, size_t n) {
    // Only used to tell our replies from somebody else's: unpredictability is welcome, not a security boundary.
    size_t i = 0;
#ifdef _WIN32
    for (; i + 4 <= n; i += 4) {
        unsigned int v;
        if (rand_s(&v)) break;
        memcpy(p + i, &v, 4);
    }
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        i = fread(p, 1, n, f);
        fclose(f);
    }
#endif
    uint64_t t = now_ns() ^ ((uint64_t)(uintptr_t)p << 17);
    for (; i < n; i++) {
        t = t * 6364136223846793005ull + 1442695040888963407ull;
        p[i] = (uint8_t)(t >> 56);
    }
}

static void add_v4(Target *t, int *n, uint32_t addr_be) {
    if (*n >= MAX_TARGETS) return;
    for (int i = 0; i < *n; i++) {
        const struct sockaddr_in *s = (const struct sockaddr_in *)&t[i].addr;
        if (s->sin_family == AF_INET && s->sin_addr.s_addr == addr_be) return;
    }
    struct sockaddr_in *s = (struct sockaddr_in *)&t[*n].addr;
    memset(&t[*n].addr, 0, sizeof t[*n].addr);
    s->sin_family = AF_INET, s->sin_port = htons(DISCOVERY_PORT), s->sin_addr.s_addr = addr_be;
    t[*n].len = sizeof *s;
    (*n)++;
}

// 255.255.255.255, 127.0.0.1 (a host on this same machine) and the broadcast of each interface that has one.
static int collect_v4_targets(Target *t) {
    int n = 0;
    add_v4(t, &n, htonl(INADDR_BROADCAST));
    add_v4(t, &n, htonl(INADDR_LOOPBACK));
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s != INVALID_SOCKET) {
        INTERFACE_INFO info[32];
        DWORD got = 0;
        if (!WSAIoctl(s, SIO_GET_INTERFACE_LIST, NULL, 0, info, sizeof info, &got, NULL, NULL)) {
            for (DWORD i = 0; i < got / sizeof info[0]; i++) {
                if (!(info[i].iiFlags & IFF_UP) || !(info[i].iiFlags & IFF_BROADCAST) ||
                    info[i].iiAddress.Address.sa_family != AF_INET)
                    continue;
                uint32_t a = info[i].iiAddress.AddressIn.sin_addr.s_addr;
                uint32_t m = info[i].iiNetmask.AddressIn.sin_addr.s_addr;
                add_v4(t, &n, a | ~m);
            }
        }
        closesocket(s);
    }
#else
    struct ifaddrs *list = NULL;
    if (!getifaddrs(&list)) {
        for (struct ifaddrs *i = list; i; i = i->ifa_next) {
            if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP) ||
                !(i->ifa_flags & IFF_BROADCAST) || !i->ifa_broadaddr)
                continue;
            add_v4(t, &n, ((struct sockaddr_in *)i->ifa_broadaddr)->sin_addr.s_addr);
        }
        freeifaddrs(list);
    }
#endif
    return n;
}

// Keeps one entry per (address, tcp port); the newest reply refreshes it.
static void result_add(const char *address, const DiscoveryInfo *info) {
    pthread_mutex_lock(&C.lock);
    int at = -1;
    for (int i = 0; i < C.count; i++)
        if (C.list[i].info.tcp_port == info->tcp_port && !strcmp(C.list[i].address, address)) at = i;
    if (at < 0 && C.count < DISCOVERY_MAX_RESULTS) at = C.count++;
    if (at >= 0) {
        C.list[at].info = *info;
        snprintf(C.list[at].address, sizeof C.list[at].address, "%s", address);
    }
    pthread_mutex_unlock(&C.lock);
}

static void drain(dsock s) {
    for (int i = 0; i < 64; i++) {
        uint8_t buf[128];
        struct sockaddr_storage from;
        socklen_t fl;
        long n = recv_one(s, buf, sizeof buf, &from, &fl);
        if (n <= 0) return;
        uint8_t nonce[DISCOVERY_NONCE];
        DiscoveryInfo info;
        if ((size_t)n != DISCOVERY_REPLY_SIZE || !discovery_parse_reply(buf, (size_t)n, nonce, &info)) continue;
        if (memcmp(nonce, C.nonce, DISCOVERY_NONCE)) continue;  // an answer to somebody else's question
        char text[64];
        int family;
        uint8_t raw[16];
        if (!source_of(&from, text, sizeof text, &family, raw)) continue;
        result_add(text, &info);
    }
}

// Waits up to ms for data on either socket (in 50 ms slices so a stop is seen quickly), draining what arrives.
static void listen_for(dsock s4, dsock s6, unsigned ms) {
    uint64_t end = now_ns() + (uint64_t)ms * 1000000ull;
    for (;;) {
        uint64_t now = now_ns();
        if (now >= end || atomic_load(&C.stop)) return;
        uint64_t left_ms = (end - now) / 1000000ull;
        struct timeval tv = {0, (int)(left_ms > 50 ? 50 : left_ms) * 1000};
        fd_set rd;
        FD_ZERO(&rd);
        int maxfd = 0;
        FD_SET(s4, &rd);
        maxfd = (int)s4;
        if (s6 != DS_INVALID) {
            FD_SET(s6, &rd);
            if ((int)s6 > maxfd) maxfd = (int)s6;
        }
        int r = select(maxfd + 1, &rd, NULL, NULL, &tv);
        if (r < 0) {
            sleep_ms(10);  // a transient failure must not spin
            continue;
        }
        if (FD_ISSET(s4, &rd)) drain(s4);
        if (s6 != DS_INVALID && FD_ISSET(s6, &rd)) drain(s6);
    }
}

static void *scan_thread(void *arg) {
    (void)arg;
    dsock s4 = udp_socket(AF_INET), s6 = udp_socket(AF_INET6);
    if (s4 != DS_INVALID) {
        int on = 1;
        (void)setsockopt(s4, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof on);
        struct sockaddr_in any;  // an explicit port-0 bind: replies come back to this socket
        memset(&any, 0, sizeof any);
        any.sin_family = AF_INET, any.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s4, (struct sockaddr *)&any, sizeof any)) {
            ds_close(s4);
            s4 = DS_INVALID;
        }
    }
    if (s6 != DS_INVALID) {
        int on = 1;
        struct sockaddr_in6 any6;
        memset(&any6, 0, sizeof any6);
        any6.sin6_family = AF_INET6, any6.sin6_addr = in6addr_any;
        if (setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&on, sizeof on) ||
            bind(s6, (struct sockaddr *)&any6, sizeof any6)) {
            ds_close(s6);
            s6 = DS_INVALID;
        }
    }
    if (s4 == DS_INVALID) {  // listen_for needs the v4 socket; without it there is nothing to scan with
        if (s6 != DS_INVALID) ds_close(s6);
        atomic_store(&C.running, false);
        return NULL;
    }

    Target v4[MAX_TARGETS];
    int nv4 = collect_v4_targets(v4);
    struct sockaddr_in6 mc;
    memset(&mc, 0, sizeof mc);
    mc.sin6_family = AF_INET6, mc.sin6_port = htons(DISCOVERY_PORT);
    inet_pton(AF_INET6, "ff02::1", &mc.sin6_addr);

    uint8_t query[DISCOVERY_QUERY_SIZE];
    size_t qn = discovery_build_query(query, sizeof query, C.protocol, C.nonce);
    uint64_t start = now_ns();
    for (int round = 0; round < DISCOVERY_SCAN_MS / DISCOVERY_SCAN_STEP_MS && !atomic_load(&C.stop); round++) {
        for (int i = 0; i < nv4; i++) send_to(s4, query, qn, (struct sockaddr *)&v4[i].addr, v4[i].len);
        if (s6 != DS_INVALID) send_to(s6, query, qn, (struct sockaddr *)&mc, sizeof mc);
        uint64_t next = start + (uint64_t)(round + 1) * DISCOVERY_SCAN_STEP_MS * 1000000ull;
        uint64_t now = now_ns();
        if (next > now) listen_for(s4, s6, (unsigned)((next - now) / 1000000ull));
    }
    ds_close(s4);
    if (s6 != DS_INVALID) ds_close(s6);
    atomic_store(&C.running, false);
    return NULL;
}

bool discovery_scan_start(uint16_t protocol) {
    if (!net_init()) return false;
    if (atomic_load(&C.running)) return false;
    if (C.thread_valid) {  // a finished scan not yet joined
        pthread_join(C.thread, NULL);
        C.thread_valid = false;
    }
    pthread_mutex_lock(&C.lock);
    C.count = 0;
    pthread_mutex_unlock(&C.lock);
    C.protocol = protocol;
    fill_random(C.nonce, DISCOVERY_NONCE);
    atomic_store(&C.stop, false);
    atomic_store(&C.running, true);
    if (pthread_create(&C.thread, NULL, scan_thread, NULL)) {
        atomic_store(&C.running, false);
        return false;
    }
    C.thread_valid = true;
    return true;
}

int discovery_scan_results(DiscoveryResult *list, int max) {
    if (!list || max <= 0) return 0;
    pthread_mutex_lock(&C.lock);
    int n = C.count < max ? C.count : max;
    memcpy(list, C.list, (size_t)n * sizeof *list);
    pthread_mutex_unlock(&C.lock);
    return n;
}

bool discovery_scan_active(void) { return atomic_load(&C.running); }

void discovery_scan_stop(void) {
    atomic_store(&C.stop, true);
    if (C.thread_valid) {
        pthread_join(C.thread, NULL);
        C.thread_valid = false;
    }
}
