#include "online_netstat.h"

#include <math.h>
#include <string.h>

#define RESYNC_GAP 1000  // a larger jump is a restart or a long silence, not loss

void online_netstat_init(OnlineNetStat *ns) {
    memset(ns, 0, sizeof *ns);
    ns->level = ns->pending = ONLINE_NETSTAT_GOOD;
}

void online_netstat_ping(OnlineNetStat *ns, float ms, uint64_t now_ns) {
    if (!(ms >= 0))  // also drops NaN
        return;
    OnlineNetStatPing *p = &ns->pings[ns->ping_head];
    p->t_ns = now_ns;
    p->ms = ms;
    ns->ping_head = (ns->ping_head + 1) % ONLINE_NETSTAT_PINGS;
    if (ns->ping_count < ONLINE_NETSTAT_PINGS)
        ns->ping_count++;
}

void online_netstat_state(OnlineNetStat *ns, uint16_t seq, uint64_t now_ns) {
    uint16_t gap = 1;
    if (ns->have_seq) {
        uint16_t d = (uint16_t)(seq - ns->last_seq);
        if (d == 0 || d >= 0x8000)  // repeat or arrived out of order
            return;
        gap = d < RESYNC_GAP ? d : 1;
    }
    OnlineNetStatState *s = &ns->states[ns->state_head];
    s->t_ns = now_ns;
    s->dt_ns = ns->have_seq && now_ns > ns->last_state_ns ? now_ns - ns->last_state_ns : 0;
    if (s->dt_ns >= ONLINE_NETSTAT_WINDOW_NS)  // a long silence is not jitter
        s->dt_ns = 0;
    s->gap = gap;
    ns->state_head = (ns->state_head + 1) % ONLINE_NETSTAT_STATES;
    if (ns->state_count < ONLINE_NETSTAT_STATES)
        ns->state_count++;
    ns->have_seq = ns->have_data = 1;
    ns->last_seq = seq;
    ns->last_state_ns = now_ns;
}

// A sample counts if it is not from the future (clock went back) and not older than the window.
static int in_window(uint64_t t, uint64_t now_ns) {
    return t <= now_ns && now_ns - t <= ONLINE_NETSTAT_WINDOW_NS;
}

void online_netstat_get(OnlineNetStat *ns, uint64_t now_ns, float *ping_ms, float *ping_p95, float *jitter_ms,
                        float *loss_pct, int *level) {
    // ping: mean and p95 over the samples in the window
    float v[ONLINE_NETSTAT_PINGS];
    int n = 0;
    for (int i = 0; i < ns->ping_count; i++)
        if (in_window(ns->pings[i].t_ns, now_ns))
            v[n++] = ns->pings[i].ms;
    float mean = 0, p95 = 0;
    if (n) {
        double sum = 0;
        for (int i = 1; i < n; i++) {  // insertion sort
            float x = v[i];
            int j = i - 1;
            while (j >= 0 && v[j] > x) {
                v[j + 1] = v[j];
                j--;
            }
            v[j + 1] = x;
        }
        for (int i = 0; i < n; i++)
            sum += v[i];
        mean = (float)(sum / n);
        int idx = (int)ceil(0.95 * n) - 1;
        p95 = v[idx < 0 ? 0 : idx];
    }

    // states: loss and jitter over the window
    int count = 0, with_dt = 0;
    double expected = 0, per[ONLINE_NETSTAT_STATES];
    for (int i = 0; i < ns->state_count; i++) {
        const OnlineNetStatState *s = &ns->states[i];
        if (!in_window(s->t_ns, now_ns))
            continue;
        count++;
        expected += s->gap;
        if (s->dt_ns)
            per[with_dt++] = (double)s->dt_ns / 1e6 / s->gap;
    }
    float loss = 0, jitter = 0;
    if (expected > 0) {
        loss = (float)(100.0 * (expected - count) / expected);
        if (loss < 0)
            loss = 0;
    }
    if (with_dt > 1) {
        double m = 0, dev = 0;
        for (int i = 0; i < with_dt; i++)
            m += per[i];
        m /= with_dt;
        for (int i = 0; i < with_dt; i++)
            dev += fabs(per[i] - m);
        jitter = (float)(dev / with_dt);
    }

    // level
    int lvl = ns->level;
    if (!ns->have_data) {
        lvl = ns->level = ns->pending = ONLINE_NETSTAT_GOOD;
    } else if (count == 0) {  // heard from him before, silent for the whole window
        lvl = ns->level = ns->pending = ONLINE_NETSTAT_BAD;
    } else {
        int raw = ONLINE_NETSTAT_FAIR;
        if (mean > 250 || loss > 10)
            raw = ONLINE_NETSTAT_BAD;
        else if (mean < 100 && loss < 2)
            raw = ONLINE_NETSTAT_GOOD;
        if (raw == ns->level) {
            ns->pending = raw;
        } else if (raw != ns->pending) {
            ns->pending = raw;
            ns->pending_since_ns = now_ns;
        } else if (now_ns >= ns->pending_since_ns && now_ns - ns->pending_since_ns >= ONLINE_NETSTAT_HYST_NS) {
            ns->level = raw;
        }
        lvl = ns->level;
    }

    if (ping_ms) *ping_ms = mean;
    if (ping_p95) *ping_p95 = p95;
    if (jitter_ms) *jitter_ms = jitter;
    if (loss_pct) *loss_pct = loss;
    if (level) *level = lvl;
}
