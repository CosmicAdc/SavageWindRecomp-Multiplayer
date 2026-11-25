#include "online_ratelimit.h"

void online_ratelimit_init(OnlineRateBucket *b, double per_second, double burst) {
    b->rate = per_second > 0 ? per_second : 0;
    b->burst = burst > 0 ? burst : 0;
    b->tokens = b->burst;
    b->last_ns = 0;
    b->started = false;
}

bool online_ratelimit_take(OnlineRateBucket *b, uint64_t now_ns) {
    if (!b->started) {
        b->started = true;
        b->last_ns = now_ns;
    } else if (now_ns > b->last_ns) {
        b->tokens += b->rate * (double)(now_ns - b->last_ns) / 1e9;
        if (b->tokens > b->burst)
            b->tokens = b->burst;
        b->last_ns = now_ns;
    } else {
        b->last_ns = now_ns;  // standing still or going back: no tokens, re-base
    }
    if (b->tokens >= 1.0 - 1e-9) {  // tolerance for floating-point drift
        b->tokens -= 1.0;
        if (b->tokens < 0) b->tokens = 0;
        return true;
    }
    return false;
}
