// Shrine invitation, pure logic (runtime/coop_shrine_invite.h; T465a).
#include "coop_shrine_invite.h"

#include <string.h>

#define PREFIX "CDungeon/Dungeon"
#define PREFIX_LEN 16

CoopSiMapKind coop_si_classify(const char *map, size_t cap, int *shrine) {
    if (shrine) *shrine = -1;
    if (!map || cap == 0) return COOP_SI_MAP_MALFORMED;
    size_t n = 0;
    while (n < cap && map[n]) n++;
    if (n == cap) return COOP_SI_MAP_MALFORMED;   // no NUL
    if (n == 0) return COOP_SI_MAP_EMPTY;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)map[i] < 0x20 || (unsigned char)map[i] >= 0x7f) return COOP_SI_MAP_MALFORMED;
    if (strncmp(map, "CDungeon/", 9) != 0) return COOP_SI_MAP_OTHER;
    if (n != PREFIX_LEN + 3 || strncmp(map, PREFIX, PREFIX_LEN) != 0) return COOP_SI_MAP_MALFORMED;
    int v = 0;
    for (int i = 0; i < 3; i++) {
        char c = map[PREFIX_LEN + i];
        if (c < '0' || c > '9') return COOP_SI_MAP_MALFORMED;
        v = v * 10 + (c - '0');
    }
    if (v >= COOP_SI_SHRINES) return COOP_SI_MAP_MALFORMED;
    if (shrine) *shrine = v;
    return COOP_SI_MAP_SHRINE;
}

static void player_reset(CoopSiPlayer *p) {
    memset(p, 0, sizeof *p);
    p->shrine = -1, p->cand = -1, p->pending = -1, p->repeat_n = -1;
}

void coop_si_reset(CoopSi *s) {
    memset(s, 0, sizeof *s);
    for (int i = 0; i < COOP_SI_SLOTS; i++) player_reset(&s->p[i]);
}

void coop_si_leave(CoopSi *s, int slot) {
    if (slot < 0 || slot >= COOP_SI_SLOTS) return;
    player_reset(&s->p[slot]);
    if (s->queued && s->qslot == slot) s->queued = false;
    // (an open notice from him closes at the next poll: he is no longer in the shrine)
}

static uint64_t since(uint64_t now, uint64_t then) { return now > then ? now - then : 0; }

void coop_si_state(CoopSi *s, int slot, const char *map, size_t cap, uint64_t now_ns) {
    if (slot < 0 || slot >= COOP_SI_SLOTS) return;
    int want;
    CoopSiMapKind k = coop_si_classify(map, cap, &want);
    if (k == COOP_SI_MAP_EMPTY || k == COOP_SI_MAP_MALFORMED) return;
    CoopSiPlayer *p = &s->p[slot];
    if (!p->known) {   // the first map we see of him is where he already was: no invitation (ADR-025 n.º 17)
        p->known = true, p->shrine = (int16_t)want, p->ncand = 0;
        return;
    }
    if (want == p->shrine) {
        p->ncand = 0;
        return;
    }
    if (p->ncand && p->cand == want) {
        if (p->ncand < 255) p->ncand++;
    } else {
        p->cand = (int16_t)want, p->ncand = 1;
    }
    if (p->ncand < COOP_SI_CONFIRM) return;
    p->shrine = (int16_t)want, p->ncand = 0;
    if (want < 0) {   // he left
        p->pending = -1;
        return;
    }
    if (p->repeat_set && p->repeat_n == want && since(now_ns, p->repeat_ns) < COOP_SI_REPEAT_NS) {
        p->pending = -1;
        return;
    }
    p->repeat_set = true, p->repeat_n = (int16_t)want, p->repeat_ns = now_ns;
    p->pending = (int16_t)want;
}

static CoopSiWhy hard_block(const CoopSiCtx *c, int shrine) {
    if (c->flags & COOP_SI_F_DEAD) return COOP_SI_WHY_DEAD;
    if (c->own_shrine == shrine) return COOP_SI_WHY_SAME_SHRINE;
    if (c->flags & COOP_SI_F_OTHER_MAP) return COOP_SI_WHY_OTHER_MAP;
    if (c->flags & COOP_SI_F_CHALLENGE) return COOP_SI_WHY_CHALLENGE;
    return COOP_SI_WHY_NONE;
}

CoopSiWhy coop_si_soft_block(const CoopSiCtx *c) {
    if (c->flags & COOP_SI_F_PAUSED) return COOP_SI_WHY_PAUSED;
    if (c->flags & COOP_SI_F_SCENE) return COOP_SI_WHY_SCENE;
    if (c->flags & COOP_SI_F_LOADING) return COOP_SI_WHY_LOADING;
    if (c->flags & COOP_SI_F_MOUNTED) return COOP_SI_WHY_MOUNTED;
    if (c->flags & COOP_SI_F_GLIDING) return COOP_SI_WHY_GLIDING;
    if (c->flags & COOP_SI_F_SWIMMING) return COOP_SI_WHY_SWIMMING;
    if (c->flags & COOP_SI_F_CLIMBING) return COOP_SI_WHY_CLIMBING;
    return COOP_SI_WHY_NONE;
}

static bool inside(const CoopSi *s, int slot, int shrine) {
    return slot >= 0 && slot < COOP_SI_SLOTS && s->p[slot].shrine == shrine;
}

static CoopSiAct emit(CoopSiOut *o, CoopSiAct act, CoopSiWhy why, int slot, int shrine) {
    if (o) o->act = act, o->why = why, o->slot = slot, o->shrine = shrine;
    return act;
}

// Closes the open notice if it should be: returns the action to give, or NONE
static CoopSiAct check_open(CoopSi *s, const CoopSiCtx *c, uint64_t now, CoopSiOut *o) {
    if (!s->open) return COOP_SI_NONE;
    CoopSiWhy why;
    if (!c->enabled) why = COOP_SI_WHY_SETTING_OFF;
    else if (since(now, s->opened_ns) >= COOP_SI_ASK_NS) why = COOP_SI_WHY_EXPIRED;
    else if (!inside(s, s->slot, s->shrine)) why = COOP_SI_WHY_LEFT;
    else why = hard_block(c, s->shrine);
    if (why == COOP_SI_WHY_NONE) return COOP_SI_NONE;
    int sl = s->slot, sh = s->shrine;
    s->open = false;
    return emit(o, COOP_SI_CLOSE, why, sl, sh);
}

CoopSiAct coop_si_poll(CoopSi *s, const CoopSiCtx *c, uint64_t now, CoopSiOut *o) {
    emit(o, COOP_SI_NONE, COOP_SI_WHY_NONE, -1, -1);
    CoopSiAct a = check_open(s, c, now, o);
    if (a != COOP_SI_NONE) return a;
    if (!c->enabled) {   // nothing is kept while it is off
        s->queued = false;
        for (int i = 0; i < COOP_SI_SLOTS; i++) s->p[i].pending = -1;
        return COOP_SI_NONE;
    }
    if (s->queued && (!inside(s, s->qslot, s->qshrine) || hard_block(c, s->qshrine) != COOP_SI_WHY_NONE))
        s->queued = false;
    if (!s->open && s->queued) {
        s->open = true, s->slot = s->qslot, s->shrine = s->qshrine, s->opened_ns = now, s->queued = false;
        return emit(o, COOP_SI_SHOW, COOP_SI_WHY_NONE, s->slot, s->shrine);
    }
    for (int i = 0; i < COOP_SI_SLOTS; i++) {
        CoopSiPlayer *p = &s->p[i];
        if (p->pending < 0) continue;
        int n = p->pending;
        p->pending = -1;
        if (hard_block(c, n) != COOP_SI_WHY_NONE) continue;
        if (!s->open || s->slot == i) {   // free, or the same player's newer entry replaces his old notice
            s->open = true, s->slot = i, s->shrine = n, s->opened_ns = now;
            return emit(o, COOP_SI_SHOW, COOP_SI_WHY_NONE, i, n);
        }
        if (!s->queued || s->qslot == i) s->queued = true, s->qslot = i, s->qshrine = n;   // else dropped
    }
    return COOP_SI_NONE;
}

CoopSiAct coop_si_accept(CoopSi *s, const CoopSiCtx *c, uint64_t now, CoopSiOut *o) {
    emit(o, COOP_SI_NONE, COOP_SI_WHY_NO_NOTICE, -1, -1);
    if (!s->open) return COOP_SI_NONE;
    CoopSiAct a = check_open(s, c, now, o);
    if (a != COOP_SI_NONE) return a;
    CoopSiWhy soft = coop_si_soft_block(c);
    if (soft != COOP_SI_WHY_NONE) return emit(o, COOP_SI_NONE, soft, s->slot, s->shrine);
    int sl = s->slot, sh = s->shrine;
    s->open = false;
    return emit(o, COOP_SI_TRAVEL, COOP_SI_WHY_NONE, sl, sh);
}

bool coop_si_decline(CoopSi *s) {
    bool was = s->open;
    s->open = false;
    return was;
}
