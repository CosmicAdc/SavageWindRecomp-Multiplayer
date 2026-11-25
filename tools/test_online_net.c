// Tests for the online client's handling of what comes from the server (runtime/online.c) without the game or a
// network: messages are built by hand and given to cl_message as the network thread would, then read back through the
// functions the game uses (online_players). No socket is opened, no thread is started.
//   make test-online-net
#include "../runtime/online.c"

// ---- stubs for what online.c takes from the rest of the runtime
GameSites g_site;  // (all zero: no game; sample_local reads nothing)
static uint64_t clock_ns = 100000000000ull;  // (a fake clock, well past 0: online_players looks INTERP_NS back)
uint64_t plat_now_ns(void) { return clock_ns; }

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static uint8_t msg[1 + sizeof(WireState)];  // (a message's payload: the sender's slot, then what he sent)

// A session as the network thread leaves it once the server said WELCOME: connected, this game in slot 0
static void set_connected(void) {
    stop_all();  // (nothing running: it only clears the session)
    pthread_mutex_lock(&g.lock);
    g.state = ONLINE_CONNECTED;
    g.id = 0;
    pthread_mutex_unlock(&g.lock);
}

static bool send_join(uint8_t slot, const char *name) {
    memset(msg, 0, 1 + ONLINE_NAME_MAX);
    msg[0] = slot;
    snprintf((char *)msg + 1, ONLINE_NAME_MAX, "%s", name);
    return cl_message(M_JOIN, msg, 1 + ONLINE_NAME_MAX);
}

static bool send_state(uint8_t slot, float x, float y, float z, float yaw) {
    WireState w;
    memset(&w, 0, sizeof w);
    w.pos[0] = x, w.pos[1] = y, w.pos[2] = z, w.yaw = yaw;
    w.anim = ONLINE_A_WALK;
    msg[0] = slot;
    memcpy(msg + 1, &w, sizeof w);
    return cl_message(M_STATE, msg, 1 + (int)sizeof w);
}

// A valid STATE from a player who joined reaches online_players: his name, where he is, what he does
static void test_state_reaches_players(void) {
    set_connected();
    CHECK(send_join(1, "Bot"));
    CHECK(send_state(1, 10.0f, 20.0f, -30.0f, 1.5f));
    OnlinePlayer out[ONLINE_MAX];
    CHECK(online_players(out));
    CHECK(out[1].used);
    CHECK(!strcmp(out[1].name, "Bot"));
    CHECK(out[1].pos[0] == 10.0f && out[1].pos[1] == 20.0f && out[1].pos[2] == -30.0f);  // (one snapshot: as sent)
    CHECK(out[1].yaw == 1.5f);
    CHECK(out[1].anim == ONLINE_A_WALK);
    CHECK(out[1].flags == 0);  // (not ONLINE_F_LOADING: the state counts)
    for (int i = 0; i < ONLINE_MAX; i++)
        if (i != 1) CHECK(!out[i].used);
}

// A STATE for a slot nobody joined is not a player
static void test_state_without_join_ignored(void) {
    set_connected();
    CHECK(send_state(2, 1.0f, 2.0f, 3.0f, 0.0f));
    OnlinePlayer out[ONLINE_MAX];
    CHECK(online_players(out));
    for (int i = 0; i < ONLINE_MAX; i++) CHECK(!out[i].used);
}

// Slot 8 (ONLINE_MAX) and above don't exist: every message that names one is ignored, nothing changes
static void test_slot_out_of_range_ignored(void) {
    set_connected();
    CHECK(send_join(1, "Bot"));
    CHECK(send_state(1, 10.0f, 20.0f, -30.0f, 1.5f));
    static Remote before[ONLINE_MAX];
    memcpy(before, g.r, sizeof before);
    int nchat = g.nchat, nnotice = g.nnotice;
    const uint8_t slots[] = {ONLINE_MAX, ONLINE_MAX + 1, 0xff};
    for (size_t k = 0; k < sizeof slots; k++) {
        uint8_t s = slots[k];
        CHECK(send_join(s, "Intruder"));
        CHECK(send_state(s, 5.0f, 5.0f, 5.0f, 0.0f));
        memset(msg, 0, sizeof msg);
        msg[0] = s;
        CHECK(cl_message(M_LEAVE, msg, 1));
        CHECK(cl_message(M_EQUIP, msg, 1 + (int)sizeof(WireEquip)));
        msg[1] = 1;
        CHECK(cl_message(M_SKEL, msg, 3 + SKEL_BONE_BYTES));
        memcpy(msg + 1, "hi", 3);
        CHECK(cl_message(M_CHAT, msg, 4));
    }
    CHECK(!memcmp(before, g.r, sizeof before));
    CHECK(g.nchat == nchat);
    CHECK(g.nnotice == nnotice);
    OnlinePlayer out[ONLINE_MAX];
    CHECK(online_players(out));
    CHECK(out[1].used && out[1].pos[0] == 10.0f);  // (the real player is still there, as he was)
    for (int i = 0; i < ONLINE_MAX; i++)
        if (i != 1) CHECK(!out[i].used);
}

// What one session left must not reach the next: after stop_all the quests, loot, enemies and every queue are gone
static void test_stop_clears_session(void) {
    static uint8_t buf[13 + ONLINE_LOOT * sizeof(OnlineLoot) + ONLINE_ENEMIES * sizeof(OnlineEnemy)];
    uint8_t q[ONLINE_QUEST_BYTES];
    OnlineLoot loots[ONLINE_LOOT];
    static OnlineEnemy en[ONLINE_ENEMIES];
    float centre[3] = {0};
    uint64_t age;
    char pn[16][40];
    float pp[16][3];

    // a guest's game: what the host sends
    set_connected();
    memset(buf, 0, sizeof buf);
    buf[7] = 0x40;  // (a flag set)
    CHECK(cl_message(M_QUESTS, buf, ONLINE_QUEST_BYTES));
    memset(buf, 0, sizeof buf);
    buf[0] = 1;
    snprintf((char *)buf + 1 + offsetof(OnlineLoot, name), 40, "%s", "Item_Test");
    CHECK(cl_message(M_LOOTS, buf, 1 + (int)sizeof(OnlineLoot)));
    memset(buf, 0, sizeof buf);
    buf[12] = 1;
    snprintf((char *)buf + 13 + offsetof(OnlineEnemy, name), 40, "%s", "Enemy_Test");
    CHECK(cl_message(M_ENEMIES, buf, 13 + (int)sizeof(OnlineEnemy)));
    memset(buf, 0, sizeof buf);
    snprintf((char *)buf, 40, "%s", "Weapon_Test");
    CHECK(cl_message(M_PICK, buf, 52));
    memset(buf, 0, 4);
    CHECK(cl_message(M_TIME, buf, 4));
    CHECK(online_quests(q) && q[7] == 0x40);  // (they came)
    CHECK(online_loots(loots) == 1);
    CHECK(online_enemies(centre, en, &age) == 1);
    CHECK(g.npick_in == 1);
    stop_all();
    memset(q, 0xff, sizeof q);
    CHECK(!online_quests(q));
    CHECK(q[7] == 0);
    CHECK(online_loots(loots) == 0);
    CHECK(online_enemies(centre, en, &age) == -1);  // (none at all, as in a fresh process)
    CHECK(online_take_picks(pn, pp, 16) == 0);
    float hour;
    CHECK(!online_host_hour(&hour));

    // the host's game: what the others send
    set_connected();
    pthread_mutex_lock(&g.lock);
    g.hosting = true;
    pthread_mutex_unlock(&g.lock);
    CHECK(send_join(1, "Bot"));
    memset(buf, 0, sizeof buf);
    buf[0] = 1, buf[1 + 7] = 0x40;
    CHECK(cl_message(M_QUEST, buf, 1 + ONLINE_QUEST_BYTES));
    CHECK(online_take_quests(q) && q[7] == 0x40);
    memset(buf, 0, sizeof buf);
    buf[0] = 1, buf[1] = 1;
    CHECK(cl_message(M_ESTATE, buf, 2 + (int)sizeof(OnlineEnemy)));
    memset(buf, 0, sizeof buf);
    buf[0] = 1;
    CHECK(cl_message(M_LOOT_ADD, buf, 1 + (int)sizeof(OnlineLoot)));
    CHECK(cl_message(M_LOOT_TAKE, buf, 5));
    CHECK(cl_message(M_HIT, buf, 9));
    online_send_hit(1, 5, ONLINE_ELEM_NONE);  // (and what this game had queued to send)
    online_send_pick("Weapon_Test", centre);
    online_send_quests(q);
    CHECK(g.nest_in == 1 && g.nla_in == 1 && g.nlt_in == 1 && g.nhit_in == 1);
    stop_all();
    CHECK(!g.hosting && g.id == -1);
    CHECK(g.nest_in == 0 && g.nla_in == 0 && g.nlt_in == 0 && g.nhit_in == 0);
    CHECK(g.nhit_out == 0 && g.npick_out == 0 && g.nest_out == 0 && g.nla_out == 0 && g.nlt_out == 0 && g.nout == 0);
    CHECK(!g.quest_out_new && !g.quest_in_new && !g.quests_new && !g.est_new && !g.loots_new);
    static const uint8_t none[ONLINE_QUEST_BYTES];
    CHECK(!memcmp(g.quest_of[1], none, ONLINE_QUEST_BYTES));

    // a new game in a slot does not inherit the quests the last one there sent
    set_connected();
    pthread_mutex_lock(&g.lock);
    g.hosting = true;
    pthread_mutex_unlock(&g.lock);
    CHECK(send_join(1, "Bot"));
    memset(buf, 0, sizeof buf);
    buf[0] = 1, buf[1 + 7] = 0x40;
    CHECK(cl_message(M_QUEST, buf, 1 + ONLINE_QUEST_BYTES));
    msg[0] = 1;
    CHECK(cl_message(M_LEAVE, msg, 1));
    CHECK(send_join(1, "Other"));
    CHECK(!memcmp(g.quest_of[1], none, ONLINE_QUEST_BYTES));
}

// An ESTATE whose strings fill their fields with no NUL comes out terminated, shield too
static void test_estate_strings_terminated(void) {
    static uint8_t buf[2 + sizeof(OnlineEnemy)];
    static OnlineEnemy e[64];
    uint8_t from[64];
    set_connected();
    pthread_mutex_lock(&g.lock);
    g.hosting = true;  // (ESTATE goes to the host's game)
    pthread_mutex_unlock(&g.lock);
    memset(buf, 0, sizeof buf);
    buf[0] = 1, buf[1] = 1;
    memset(buf + 2 + offsetof(OnlineEnemy, name), 'A', sizeof e[0].name);
    memset(buf + 2 + offsetof(OnlineEnemy, as), 'B', sizeof e[0].as);
    memset(buf + 2 + offsetof(OnlineEnemy, weapon), 'C', sizeof e[0].weapon);
    memset(buf + 2 + offsetof(OnlineEnemy, shield), 'D', sizeof e[0].shield);
    CHECK(cl_message(M_ESTATE, buf, 2 + (int)sizeof(OnlineEnemy)));
    CHECK(online_take_estates(e, from, 64) == 1);
    CHECK(from[0] == 1);
    CHECK(strlen(e[0].shield) <= 39);
    CHECK(strlen(e[0].name) <= 39 && strlen(e[0].as) <= 31 && strlen(e[0].weapon) <= 39);
}

// A STATE whose position or heading is not finite, or beyond any world, is dropped whole: the player stays as he was
static void test_state_not_finite_dropped(void) {
    set_connected();
    CHECK(send_join(1, "Bot"));
    CHECK(send_state(1, 10.0f, 20.0f, -30.0f, 1.5f));
    const float bad[] = {NAN, INFINITY, -INFINITY, 1e30f, -100001.0f};
    unsigned before = g_drop_n[D_STATE];
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        CHECK(send_state(1, bad[k], 20.0f, -30.0f, 1.5f));
        CHECK(send_state(1, 10.0f, bad[k], -30.0f, 1.5f));
        CHECK(send_state(1, 10.0f, 20.0f, bad[k], 1.5f));
        CHECK(send_state(1, 99.0f, 99.0f, 99.0f, bad[k]));
    }
    CHECK(g_drop_n[D_STATE] - before == 4 * (sizeof bad / sizeof *bad));
    OnlinePlayer out[ONLINE_MAX];
    CHECK(online_players(out));
    CHECK(out[1].pos[0] == 10.0f && out[1].pos[1] == 20.0f && out[1].pos[2] == -30.0f && out[1].yaw == 1.5f);
    CHECK(send_state(1, 100000.0f, -100000.0f, 0.0f, 0.0f));  // (the limit itself is fine)
    CHECK(g_drop_n[D_STATE] - before == 4 * (sizeof bad / sizeof *bad));
}

// TIME is an hour, 0..24; anything else is not delivered
static void test_time_range(void) {
    float hour;
    set_connected();
    const float bad[] = {99.0f, -0.5f, 24.5f, NAN, INFINITY};
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        memcpy(msg, &bad[k], 4);
        CHECK(cl_message(M_TIME, msg, 4));
        CHECK(!online_host_hour(&hour));
    }
    const float good[] = {0.0f, 12.5f, 24.0f};
    for (size_t k = 0; k < sizeof good / sizeof *good; k++) {
        memcpy(msg, &good[k], 4);
        CHECK(cl_message(M_TIME, msg, 4));
        CHECK(online_host_hour(&hour) && hour == good[k]);
    }
}

// A HIT with negative damage would heal the enemy: it never reaches online_take_hits
static void test_hit_negative_dropped(void) {
    uint32_t id[4];
    int32_t damage[4];
    uint8_t element[4];
    set_connected();
    pthread_mutex_lock(&g.lock);
    g.hosting = true;
    pthread_mutex_unlock(&g.lock);
    uint32_t eid = 7;
    int32_t d = -50;
    memcpy(msg, &eid, 4);
    memcpy(msg + 4, &d, 4);
    msg[8] = ONLINE_ELEM_NONE;
    CHECK(cl_message(M_HIT, msg, 9));
    CHECK(online_take_hits(id, damage, element, 4) == 0);
    d = 50;
    memcpy(msg + 4, &d, 4);
    CHECK(cl_message(M_HIT, msg, 9));
    CHECK(online_take_hits(id, damage, element, 4) == 1 && id[0] == 7 && damage[0] == 50);
}

// ENEMIES / ESTATE drop the entry whose matrices are bad, not the message; LOOTS the same; LOOT_ADD and PICK drop whole
static void test_lists_not_finite_dropped(void) {
    static uint8_t buf[13 + ONLINE_LOOT * sizeof(OnlineLoot) + ONLINE_ENEMIES * sizeof(OnlineEnemy)];
    static OnlineEnemy en[ONLINE_ENEMIES], in[3];
    static OnlineLoot loots[ONLINE_LOOT];
    float centre[3];
    uint64_t age;
    uint8_t from[8];
    char pn[16][40];
    float pp[16][3];

    // ENEMIES (a guest's game): three entries, the middle one with NAN in its weapon matrix
    set_connected();
    memset(buf, 0, sizeof buf);
    buf[12] = 3;
    for (int k = 0; k < 3; k++) snprintf((char *)buf + 13 + k * sizeof(OnlineEnemy) + offsetof(OnlineEnemy, name), 40, "Enemy_%d", k);
    float fnan = NAN;
    memcpy(buf + 13 + 1 * sizeof(OnlineEnemy) + offsetof(OnlineEnemy, wm) + 5 * 4, &fnan, 4);
    unsigned before = g_drop_n[D_ENEMIES];
    CHECK(cl_message(M_ENEMIES, buf, 13 + 3 * (int)sizeof(OnlineEnemy)));
    CHECK(online_enemies(centre, en, &age) == 2);
    CHECK(!strcmp(en[0].name, "Enemy_0") && !strcmp(en[1].name, "Enemy_2"));
    CHECK(g_drop_n[D_ENEMIES] - before == 1);
    memcpy(buf, &fnan, 4);  // (a centre that is not finite: the whole message goes)
    CHECK(cl_message(M_ENEMIES, buf, 13 + 3 * (int)sizeof(OnlineEnemy)));
    CHECK(online_enemies(centre, en, &age) == 2);

    // LOOTS: the one with a huge position goes, the others stay
    set_connected();
    memset(buf, 0, sizeof buf);
    buf[0] = 3;
    for (int k = 0; k < 3; k++) snprintf((char *)buf + 1 + k * sizeof(OnlineLoot) + offsetof(OnlineLoot, name), 40, "Item_%d", k);
    float huge = 1e30f;
    memcpy(buf + 1 + 0 * sizeof(OnlineLoot) + offsetof(OnlineLoot, pos) + 8, &huge, 4);
    memcpy(buf + 1 + 2 * sizeof(OnlineLoot) + offsetof(OnlineLoot, rot) + 4, &fnan, 4);
    CHECK(cl_message(M_LOOTS, buf, 1 + 3 * (int)sizeof(OnlineLoot)));
    CHECK(online_loots(loots) == 1 && !strcmp(loots[0].name, "Item_1"));

    // the host's game: ESTATE with the first of three entries bad, then LOOT_ADD and PICK
    set_connected();
    pthread_mutex_lock(&g.lock);
    g.hosting = true;
    pthread_mutex_unlock(&g.lock);
    memset(buf, 0, sizeof buf);
    buf[0] = 1, buf[1] = 3;
    for (int k = 0; k < 3; k++) snprintf((char *)buf + 2 + k * sizeof(OnlineEnemy) + offsetof(OnlineEnemy, name), 40, "Enemy_%d", k);
    memcpy(buf + 2 + 0 * sizeof(OnlineEnemy) + offsetof(OnlineEnemy, m), &fnan, 4);
    memcpy(buf + 2 + 1 * sizeof(OnlineEnemy) + offsetof(OnlineEnemy, sm) + 44, &huge, 4);
    CHECK(cl_message(M_ESTATE, buf, 2 + 3 * (int)sizeof(OnlineEnemy)));
    CHECK(online_take_estates(in, from, 8) == 1);
    CHECK(!strcmp(in[0].name, "Enemy_2") && from[0] == 1);

    memset(buf, 0, sizeof buf);
    buf[0] = 1;
    snprintf((char *)buf + 1 + offsetof(OnlineLoot, name), 40, "Item_Test");
    memcpy(buf + 1 + offsetof(OnlineLoot, pos), &fnan, 4);
    CHECK(cl_message(M_LOOT_ADD, buf, 1 + (int)sizeof(OnlineLoot)));
    CHECK(g.nla_in == 0);
    memset(buf + 1 + offsetof(OnlineLoot, pos), 0, 4);
    CHECK(cl_message(M_LOOT_ADD, buf, 1 + (int)sizeof(OnlineLoot)));
    CHECK(g.nla_in == 1);

    memset(buf, 0, sizeof buf);
    snprintf((char *)buf, 40, "%s", "Weapon_Test");
    memcpy(buf + 40 + 4, &huge, 4);
    CHECK(cl_message(M_PICK, buf, 52));
    CHECK(online_take_picks(pn, pp, 16) == 0);
    memset(buf + 40, 0, 12);
    CHECK(cl_message(M_PICK, buf, 52));
    CHECK(online_take_picks(pn, pp, 16) == 1);
    stop_all();
}

int main(void) {
    test_state_reaches_players();
    test_state_without_join_ignored();
    test_slot_out_of_range_ignored();
    test_stop_clears_session();
    test_estate_strings_terminated();
    test_state_not_finite_dropped();
    test_time_range();
    test_hit_negative_dropped();
    test_lists_not_finite_dropped();
    stop_all();
    printf("test_online_net: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
