// Network quality statistics for one player over a 5 s window. Pure: injected clock, no sockets, no allocation.
#pragma once

#include <stdint.h>

#define ONLINE_NETSTAT_WINDOW_NS 5000000000ull
#define ONLINE_NETSTAT_HYST_NS 2000000000ull
#define ONLINE_NETSTAT_PINGS 32
#define ONLINE_NETSTAT_STATES 256

enum { ONLINE_NETSTAT_GOOD = 0, ONLINE_NETSTAT_FAIR, ONLINE_NETSTAT_BAD };

typedef struct {
    uint64_t t_ns;
    float ms;
} OnlineNetStatPing;

typedef struct {
    uint64_t t_ns;
    uint64_t dt_ns;   // since the previous accepted STATE (0 for the first)
    uint16_t gap;     // seq distance to the previous one: 1 = none missing
} OnlineNetStatState;

typedef struct {
    OnlineNetStatPing pings[ONLINE_NETSTAT_PINGS];
    OnlineNetStatState states[ONLINE_NETSTAT_STATES];
    int ping_head, ping_count, state_head, state_count;
    int have_seq, have_data;
    uint16_t last_seq;
    uint64_t last_state_ns;
    int level, pending;          // shown level; candidate level waiting out the hysteresis
    uint64_t pending_since_ns;
} OnlineNetStat;

void online_netstat_init(OnlineNetStat *ns);
// A round-trip time measured at now_ns.
void online_netstat_ping(OnlineNetStat *ns, float ms, uint64_t now_ns);
// A STATE received at now_ns carrying sequence number seq (16 bits, wraps). Repeats and old ones are ignored.
void online_netstat_state(OnlineNetStat *ns, uint16_t seq, uint64_t now_ns);
// Outputs may be NULL. ping_ms: mean, ping_p95: 95th percentile, jitter_ms: mean absolute deviation of the
// per-state interval, loss_pct: missing/expected sequence numbers in the window (0..100), level: ONLINE_NETSTAT_*.
// With no STATE ever received there is nothing to judge: all zero, GOOD. If STATEs were received before but none
// in the window, the level is BAD at once. Other level changes wait 2 s (hysteresis).
void online_netstat_get(OnlineNetStat *ns, uint64_t now_ns, float *ping_ms, float *ping_p95, float *jitter_ms,
                        float *loss_pct, int *level);
