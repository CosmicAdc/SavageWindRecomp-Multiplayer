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

int main(void) {
    test_state_reaches_players();
    test_state_without_join_ignored();
    test_slot_out_of_range_ignored();
    test_stop_clears_session();
    stop_all();
    printf("test_online_net: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
