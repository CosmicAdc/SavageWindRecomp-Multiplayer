// Enemy threat for co-op: see runtime/coop_threat.h. Pure logic, tested by tests/test_coop_threat.c.
#include "coop_threat.h"

#include <math.h>
#include <string.h>

static const CoopThreatCfg k_default = {8000000000ull, 1.5f, 2000000000ull};

CoopThreatCfg coop_threat_cfg_default(void) { return k_default; }

static const CoopThreatCfg *cfg_of(const CoopThreatCfg *cfg) { return cfg ? cfg : &k_default; }

static bool slot_ok(int slot) { return slot >= 0 && slot < COOP_THREAT_SLOTS; }

static void add_threat(CoopThreat *t, int slot, double amount) {
    double v = (double)t->threat[slot] + amount;
    t->threat[slot] = v > COOP_THREAT_MAX ? COOP_THREAT_MAX : (float)v;
}

// (the pending damage whose time is over explains nothing any more)
static void expire_pending(CoopThreat *t, uint64_t now_ns) {
    int n = 0;
    for (int i = 0; i < t->npending; i++)
        if (now_ns < t->pending[i].until_ns && t->pending[i].damage > 0) t->pending[n++] = t->pending[i];
    t->npending = n;
}

void coop_threat_reset(CoopThreat *t) {
    if (t) memset(t, 0, sizeof *t);
}

void coop_threat_decay(CoopThreat *t, const CoopThreatCfg *cfg, uint64_t now_ns) {
    if (!t) return;
    cfg = cfg_of(cfg);
    if (!t->timed) {
        t->timed = true, t->decay_ns = now_ns;
        return;
    }
    if (now_ns <= t->decay_ns) return;  // (the same time, or one before: a monotonic clock gives neither twice)
    uint64_t dt = now_ns - t->decay_ns;
    t->decay_ns = now_ns;
    if (!cfg->half_life_ns) return;
    double k = exp2(-(double)dt / (double)cfg->half_life_ns);
    for (int i = 0; i < COOP_THREAT_SLOTS; i++) {
        float v = (float)(t->threat[i] * k);
        t->threat[i] = v < COOP_THREAT_FLOOR ? 0.0f : v;
    }
}

void coop_threat_blow(CoopThreat *t, const CoopThreatCfg *cfg, int slot, int32_t damage, int32_t life_now,
                      uint64_t now_ns) {
    if (!t) return;
    cfg = cfg_of(cfg);
    coop_threat_decay(t, cfg, now_ns);
    expire_pending(t, now_ns);
    if (!slot_ok(slot) || damage <= 0 || life_now <= 0) return;
    // (effective damage: no more than the life it has left, less what earlier blows will still take from it)
    int64_t pending = 0;
    for (int i = 0; i < t->npending; i++) pending += t->pending[i].damage;
    int64_t left = (int64_t)life_now - pending;
    int64_t eff = damage < left ? damage : left;
    if (eff <= 0) return;
    add_threat(t, slot, (double)eff);
    uint64_t until = now_ns + cfg->pending_ns;
    if (t->npending < COOP_THREAT_PENDING) {
        t->pending[t->npending].damage = (int32_t)eff, t->pending[t->npending].until_ns = until;
        t->npending++;
    } else {  // (full: into the newest, which now lasts as long as this one; eff + pending <= life_now, an int32)
        CoopThreatPending *p = &t->pending[COOP_THREAT_PENDING - 1];
        p->damage = (int32_t)(p->damage + eff), p->until_ns = until;
    }
}

void coop_threat_life(CoopThreat *t, const CoopThreatCfg *cfg, int owner, int32_t life, uint64_t now_ns) {
    if (!t) return;
    cfg = cfg_of(cfg);
    coop_threat_decay(t, cfg, now_ns);
    expire_pending(t, now_ns);
    if (life < 0) return;  // (unknown, or gone from that game: no life seen)
    if (!t->seen) {
        t->seen = true, t->life = life;
        return;
    }
    int64_t drop = (int64_t)t->life - life;
    t->life = life;
    if (drop <= 0) return;  // (a rise is no one's damage)
    int n = 0;
    for (int i = 0; i < t->npending; i++) {
        CoopThreatPending p = t->pending[i];
        int64_t take = drop < p.damage ? drop : p.damage;
        p.damage -= (int32_t)take, drop -= take;
        if (p.damage > 0) t->pending[n++] = p;
    }
    t->npending = n;
    if (drop > 0 && slot_ok(owner)) add_threat(t, owner, (double)drop);
}

void coop_threat_forget_slot(CoopThreat *t, int slot) {
    if (t && slot_ok(slot)) t->threat[slot] = 0.0f;
}

CoopThreatPick coop_threat_pick(const CoopThreat *t, const CoopThreatCfg *cfg, const bool can[COOP_THREAT_SLOTS],
                                int owner, int *to) {
    if (to) *to = -1;
    if (!t) return COOP_THREAT_NEAREST;
    cfg = cfg_of(cfg);
    float margin = cfg->margin >= 1.0f ? cfg->margin : 1.0f;  // (NaN too)
    int lead = 0;
    float top = t->threat[0], second = 0.0f;
    for (int i = 1; i < COOP_THREAT_SLOTS; i++) {
        float v = t->threat[i];
        if (v > top) second = top, top = v, lead = i;
        else if (v > second) second = v;
    }
    // (a tie is no leader, whatever the margin)
    if (!(top > 0.0f) || !(top > second) || !((double)top >= (double)margin * second))
        return COOP_THREAT_NEAREST;
    if (!can || !can[lead]) return COOP_THREAT_NEAREST;  // (beyond 80 m, say: it counts, but nearness decides)
    if (!slot_ok(owner)) owner = -1;
    if (to) *to = lead;
    return lead == owner ? COOP_THREAT_KEEP : COOP_THREAT_TO;
}
