// Token bucket with an injected clock. Pure: no sockets, no globals, no clock of its own.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    double tokens, rate, burst;  // tokens now, refill per second, capacity
    uint64_t last_ns;            // clock of the last refill
    bool started;                // false until the first take: the bucket starts full at that moment
} OnlineRateBucket;

// per_second: refill rate; burst: capacity (also the initial fill). Both are clamped to >= 0.
void online_ratelimit_init(OnlineRateBucket *b, double per_second, double burst);
// Refills for the time since the last call (capped at burst) and takes one token if there is one.
// A clock that stands still gives nothing; a clock that goes back gives nothing and is re-based.
bool online_ratelimit_take(OnlineRateBucket *b, uint64_t now_ns);
