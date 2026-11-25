// Revive policy for co-op: see runtime/coop_revive.h. Pure logic, tested by tests/test_coop_revive.c.
#include "coop_revive.h"

#include <math.h>
#include <string.h>

static const CoopReviveCfg k_default = {30000000000ull, 3000000000ull, 2.0f, 3};

CoopReviveCfg coop_revive_cfg_default(void) { return k_default; }

static const CoopReviveCfg *cfg_of(const CoopReviveCfg *cfg) { return cfg ? cfg : &k_default; }

static uint64_t gap(uint64_t a, uint64_t b) { return a > b ? a - b : b - a; }

bool coop_revive_allowed(const CoopReviveIn *in) {
    if (!in || !in->rule_on || !in->peers) return false;
    int n = in->npeers < 0 ? 0 : in->npeers > COOP_REVIVE_MAX_PEERS ? COOP_REVIVE_MAX_PEERS : in->npeers;
    for (int i = 0; i < n; i++) {
        const CoopRevivePeer *p = &in->peers[i];
        if (!p->present || !p->alive || p->loading) continue;
        if (p->fell && gap(p->fell_ns, in->my_fell_ns) <= in->both_window_ns) continue;  // (down at once with him)
        return true;
    }
    return false;
}

void coop_revive_reset(CoopReviveMachine *m) {
    if (m) memset(m, 0, sizeof *m);
}

void coop_revive_fall(CoopReviveMachine *m, uint64_t now_ns) {
    if (!m || m->state != COOP_REVIVE_UP) return;
    memset(m, 0, sizeof *m);
    m->state = COOP_REVIVE_DOWN;
    m->down_ns = now_ns;
}

bool coop_revive_helper_ok(const CoopReviveCfg *cfg, float distance_m, bool holding) {
    return holding && isfinite(distance_m) && distance_m >= 0.0f && distance_m < cfg_of(cfg)->range_m;
}

// (the countdown is over when now is countdown_ns or more after the fall; a clock before the fall leaves it running)
static bool timed_out(const CoopReviveMachine *m, const CoopReviveCfg *cfg, uint64_t now_ns) {
    return now_ns >= m->down_ns && now_ns - m->down_ns >= cfg->countdown_ns;
}

CoopReviveEvent coop_revive_tick(CoopReviveMachine *m, const CoopReviveCfg *cfg, uint64_t now_ns, bool helper_ok) {
    if (!m || m->state == COOP_REVIVE_UP) return COOP_REVIVE_EV_NONE;
    cfg = cfg_of(cfg);
    if (m->state == COOP_REVIVE_REVIVING) {
        if (now_ns < m->last_ns) now_ns = m->last_ns;  // (a clock that goes back passes nothing)
        if (helper_ok) {
            m->progress_ns += now_ns - m->last_ns;
            m->last_ns = now_ns;
            if (m->progress_ns >= cfg->hold_ns) {
                coop_revive_reset(m);
                return COOP_REVIVE_EV_RESCUED;
            }
        } else {
            m->state = COOP_REVIVE_DOWN, m->progress_ns = 0;  // (he let go or moved away: it starts again, the countdown on)
        }
    }
    if (timed_out(m, cfg, now_ns)) {
        coop_revive_reset(m);
        return COOP_REVIVE_EV_TIMED_OUT;
    }
    if (m->state == COOP_REVIVE_DOWN && helper_ok) {
        m->state = COOP_REVIVE_REVIVING;
        m->progress_ns = 0;
        m->last_ns = now_ns;
    }
    return COOP_REVIVE_EV_NONE;
}

int coop_revive_hearts(CoopReviveEvent ev, const CoopReviveCfg *cfg) {
    return ev == COOP_REVIVE_EV_TIMED_OUT ? cfg_of(cfg)->timeout_hearts : 0;
}

uint64_t coop_revive_remaining_ns(const CoopReviveMachine *m, const CoopReviveCfg *cfg, uint64_t now_ns) {
    if (!m || m->state == COOP_REVIVE_UP) return 0;
    cfg = cfg_of(cfg);
    uint64_t used = now_ns > m->down_ns ? now_ns - m->down_ns : 0;
    return used >= cfg->countdown_ns ? 0 : cfg->countdown_ns - used;
}

float coop_revive_progress(const CoopReviveMachine *m, const CoopReviveCfg *cfg) {
    if (!m || m->state != COOP_REVIVE_REVIVING) return 0.0f;
    cfg = cfg_of(cfg);
    if (cfg->hold_ns == 0 || m->progress_ns >= cfg->hold_ns) return 1.0f;
    return (float)((double)m->progress_ns / (double)cfg->hold_ns);
}
