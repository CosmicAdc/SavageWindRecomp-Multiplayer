// Tests for the online server's session state machine (runtime/online.c: srv_message, srv_drop) without the game or a
// network: each simulated client is one end of a socketpair, the server writes to the other end exactly as it writes to
// a real connection, and the test reads what the client would have received. No port is opened, no thread is started,
// nothing waits on a clock. POSIX only (socketpair): the Makefile target only says so on Windows.
//   make test-online-session
//
// A full session (R_FULL) and the drop of a client that says nothing for TIMEOUT_NS live in srv_step (one round of
// srv_loop: accept, select, the timeouts; T218). Those cases give it a listening socket of their own, a Unix one in a
// directory of its own (no port), call it with no wait (or with a limit it never reaches: what it waits for is already
// there), and move the test's clock instead of waiting.
#ifdef _WIN32
#include <stdio.h>
int main(void) {
    printf("test_online_session: POSIX only (socketpair): nothing to do on Windows\n");
    return 0;
}
#else
#include "../runtime/online.c"
#include <sys/un.h>

// ---- stubs for what online.c takes from the rest of the runtime
GameSites g_site;  // (all zero: no game)
static uint64_t clock_ns = 100000000000ull;
uint64_t plat_now_ns(void) { return clock_ns; }

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

// ---- the simulated clients
static Server sv;
static int peer[ONLINE_MAX];  // (the test's end of each client's socketpair; -1: none)
static uint8_t inbuf[ONLINE_MAX][16384];  // (what came from the server and has not been taken yet)
static int innb[ONLINE_MAX];

static void reset(void) {
    for (int i = 0; i < ONLINE_MAX; i++) {
        if (sv.c[i].used) sock_close(sv.c[i].s);
        if (peer[i] >= 0) close(peer[i]);
        peer[i] = -1;
        innb[i] = 0;
    }
    memset(&sv, 0, sizeof sv);
    for (int i = 0; i < ONLINE_MAX; i++) sv.c[i].s = SOCK_BAD;
    sv.listener = SOCK_BAD;
}

// a connection in slot i, as srv_loop leaves it after accept: used, nothing said yet
static void connect_client(int i) {
    int fd[2];
    int r = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    CHECK(r == 0);
    if (r != 0) return;
    sv.c[i] = (SClient){.s = fd[0], .used = true, .heard = clock_ns};
    peer[i] = fd[1];
    innb[i] = 0;
}

// the first message of client i: HELLO. magic / version / name / pass as given (the server's view of a client)
static bool say_hello(int i, uint32_t magic, uint16_t version, const char *name, const char *pass) {
    WireHello h;
    memset(&h, 0, sizeof h);
    h.magic = magic, h.version = version;
    snprintf(h.name, sizeof h.name, "%s", name);
    snprintf(h.pass, sizeof h.pass, "%s", pass);
    return srv_message(&sv, i, M_HELLO, (const uint8_t *)&h, sizeof h);
}

static bool join_ok(int i, const char *name) { return say_hello(i, MAGIC, PROTOCOL, name, ""); }

// take what the server has written to client i's socket into its buffer (without waiting)
static void drain(int i) {
    for (;;) {
        int k = (int)recv(peer[i], inbuf[i] + innb[i], (size_t)(sizeof inbuf[i] - (size_t)innb[i]), MSG_DONTWAIT);
        if (k <= 0) break;
        innb[i] += k;
    }
}

// the next whole message the server sent to client i (false: none): its type, payload and length
static bool take(int i, uint8_t *type, uint8_t *pay, int *len) {
    drain(i);
    if (innb[i] < 3) return false;
    int l = inbuf[i][0] | inbuf[i][1] << 8;  // (u16 length of type + payload, then the type)
    if (l < 1 || innb[i] < 2 + l) return false;
    *type = inbuf[i][2], *len = l - 1;
    memcpy(pay, inbuf[i] + 3, (size_t)(l - 1));
    memmove(inbuf[i], inbuf[i] + 2 + l, (size_t)(innb[i] - 2 - l));
    innb[i] -= 2 + l;
    return true;
}

// nothing (more) for client i
static bool silent(int i) {
    drain(i);
    return innb[i] == 0;
}

// the next message for client i is `type`, one slot byte (and, if name, the name after it): CHECKs and returns the slot
static int expect_slot_msg(int i, uint8_t type, const char *name) {
    static uint8_t pay[4096];
    uint8_t t = 0;
    int len = 0;
    bool got = take(i, &t, pay, &len);
    CHECK(got);
    if (!got) return -1;
    CHECK(t == type);
    CHECK(len >= 1);
    if (name) {
        CHECK(len == 1 + ONLINE_NAME_MAX);
        CHECK(!strncmp((char *)pay + 1, name, ONLINE_NAME_MAX));
    }
    return len >= 1 ? pay[0] : -1;
}

// a REJECT with this reason, nothing after it
static void expect_reject(int i, uint8_t why) {
    static uint8_t pay[64];
    uint8_t t = 0;
    int len = 0;
    CHECK(take(i, &t, pay, &len));
    CHECK(t == M_REJECT);
    CHECK(len == 1 && pay[0] == why);
    CHECK(silent(i));
}

// the server's end of client i is closed: the test's end reads end-of-file
static bool closed_by_server(int i) {
    char c;
    return recv(peer[i], &c, 1, MSG_DONTWAIT) == 0;
}

static void begin(int n, const char *what) {
    printf("caso %d: %s\n", n, what);
    reset();
}

// (1) a correct HELLO: WELCOME with the slot, then the JOIN the new player gets of itself
static void test_hello_ok(void) {
    begin(1, "HELLO correcto -> WELCOME con la ranura");
    connect_client(2);  // (the slot is what srv_loop picked: the lowest free; here the test picks 2)
    CHECK(join_ok(2, "Link"));
    CHECK(sv.c[2].hello);
    CHECK(!strcmp(sv.c[2].name, "Link"));
    CHECK(expect_slot_msg(2, M_WELCOME, NULL) == 2);
    CHECK(expect_slot_msg(2, M_JOIN, "Link") == 2);  // (everyone gets the JOIN, the new player too)
    CHECK(silent(2));
}

// (2) another version: REJECT R_VERSION and the message returns false (srv_loop drops the connection); the same for a
// bad magic
static void test_reject_version(void) {
    begin(2, "version distinta -> REJECT R_VERSION");
    connect_client(0);
    CHECK(!say_hello(0, MAGIC, PROTOCOL - 1, "Link", ""));
    CHECK(!sv.c[0].hello);
    expect_reject(0, R_VERSION);
    srv_drop(&sv, 0);
    CHECK(closed_by_server(0));
    connect_client(1);
    CHECK(!say_hello(1, MAGIC ^ 1u, PROTOCOL, "Link", ""));
    expect_reject(1, R_VERSION);
}

// (3) a wrong password (the server has one): REJECT R_PASSWORD; the right one gets in
static void test_reject_password(void) {
    begin(3, "contrasena mala -> REJECT R_PASSWORD");
    snprintf(sv.password, sizeof sv.password, "%s", "secret");
    connect_client(0);
    CHECK(!say_hello(0, MAGIC, PROTOCOL, "Link", "wrong"));
    expect_reject(0, R_PASSWORD);
    connect_client(1);
    CHECK(!say_hello(1, MAGIC, PROTOCOL, "Link", ""));  // (none given: also wrong)
    expect_reject(1, R_PASSWORD);
    connect_client(2);
    CHECK(say_hello(2, MAGIC, PROTOCOL, "Link", "secret"));
    CHECK(expect_slot_msg(2, M_WELCOME, NULL) == 2);
}

// (4) an empty name: REJECT R_NAME
static void test_reject_name(void) {
    begin(4, "nombre vacio -> REJECT R_NAME");
    connect_client(0);
    CHECK(!say_hello(0, MAGIC, PROTOCOL, "", ""));
    CHECK(!sv.c[0].hello);
    expect_reject(0, R_NAME);
}

// (5) a first message that is not a HELLO (or a HELLO too short): the connection is refused, nothing is sent, and the
// others hear nothing (it never joined: no LEAVE)
static void test_first_not_hello(void) {
    begin(5, "primer mensaje que no es HELLO -> conexion rechazada");
    connect_client(0);
    CHECK(join_ok(0, "Link"));
    drain(0);
    innb[0] = 0;
    connect_client(1);
    static uint8_t st[sizeof(WireState)];
    memset(st, 0, sizeof st);
    CHECK(!srv_message(&sv, 1, M_STATE, st, sizeof st));
    CHECK(!sv.c[1].hello);
    CHECK(silent(1));
    srv_drop(&sv, 1);
    CHECK(closed_by_server(1));
    CHECK(!sv.c[1].used);
    connect_client(2);
    WireHello h = {.magic = MAGIC, .version = PROTOCOL};
    CHECK(!srv_message(&sv, 2, M_HELLO, (const uint8_t *)&h, sizeof h - 1));  // (a HELLO cut short)
    CHECK(silent(2));
    srv_drop(&sv, 2);
    CHECK(silent(0));  // (neither JOIN nor LEAVE for the one that was refused)
    CHECK(sv.c[0].hello);
}

// (6) the second client gets the JOIN of the first, and the first gets the JOIN of the second
static void test_join_both_ways(void) {
    begin(6, "JOIN cruzado entre el primero y el segundo");
    connect_client(0);
    CHECK(join_ok(0, "Link"));
    CHECK(expect_slot_msg(0, M_WELCOME, NULL) == 0);
    CHECK(expect_slot_msg(0, M_JOIN, "Link") == 0);
    CHECK(silent(0));
    connect_client(1);
    CHECK(join_ok(1, "Zelda"));
    CHECK(expect_slot_msg(1, M_WELCOME, NULL) == 1);
    CHECK(expect_slot_msg(1, M_JOIN, "Link") == 0);   // (the one already there)
    CHECK(expect_slot_msg(1, M_JOIN, "Zelda") == 1);  // (its own, back)
    CHECK(silent(1));
    CHECK(expect_slot_msg(0, M_JOIN, "Zelda") == 1);  // (the first hears of the second)
    CHECK(silent(0));
}

// (7) srv_drop of a player that had joined: the others get a LEAVE with its slot, the dropped one's socket is closed
static void test_drop_leave(void) {
    begin(7, "srv_drop -> LEAVE con esa ranura para los demas");
    connect_client(0);
    CHECK(join_ok(0, "A"));
    connect_client(1);
    CHECK(join_ok(1, "B"));
    connect_client(2);
    CHECK(join_ok(2, "C"));
    for (int i = 0; i < 3; i++) drain(i), innb[i] = 0;  // (the joins are not what is looked at here)
    srv_drop(&sv, 1);
    CHECK(expect_slot_msg(0, M_LEAVE, NULL) == 1);
    CHECK(expect_slot_msg(2, M_LEAVE, NULL) == 1);
    CHECK(silent(0) && silent(2));
    CHECK(closed_by_server(1));
    CHECK(!sv.c[1].used && !sv.c[1].hello);
    CHECK(sv.c[0].hello && sv.c[2].hello);
}

// (8) the slot freed is reused: a new client there gets WELCOME with that slot, and the clean state of its place (nothing
// of the one that left: not its name, not its last STATE)
static void test_slot_reused(void) {
    begin(8, "la ranura liberada se reutiliza sin restos del anterior");
    static WireState w;
    connect_client(0);
    CHECK(join_ok(0, "A"));
    connect_client(1);
    CHECK(join_ok(1, "Old"));
    memset(&w, 0, sizeof w);
    w.pos[0] = 123.0f;
    CHECK(srv_message(&sv, 1, M_STATE, (const uint8_t *)&w, sizeof w));
    CHECK(sv.c[1].have_last);
    srv_drop(&sv, 1);
    CHECK(!sv.c[1].have_last && !sv.c[1].have_equip && sv.c[1].rb.n == 0);
    for (int i = 0; i < 2; i++) drain(i), innb[i] = 0;
    connect_client(1);  // (srv_loop gives the lowest free slot: this one)
    CHECK(join_ok(1, "New"));
    CHECK(expect_slot_msg(1, M_WELCOME, NULL) == 1);
    CHECK(expect_slot_msg(1, M_JOIN, "A") == 0);
    CHECK(expect_slot_msg(1, M_JOIN, "New") == 1);
    CHECK(silent(1));  // (no STATE of the old occupant)
    CHECK(expect_slot_msg(0, M_JOIN, "New") == 1);
    CHECK(silent(0));
    CHECK(!strcmp(sv.c[1].name, "New"));
}

// (9) a STATE of one client reaches the others with its slot in front, whole, and does not return to the sender; a
// player who joins later gets the last one
static void test_state_relayed(void) {
    begin(9, "STATE llega a los demas con su ranura delante y no vuelve al emisor");
    static WireState w;
    static uint8_t pay[4096];
    uint8_t t = 0;
    int len = 0;
    for (int i = 0; i < 3; i++) {
        connect_client(i);
        CHECK(join_ok(i, i == 0 ? "A" : i == 1 ? "B" : "C"));
    }
    for (int i = 0; i < 3; i++) drain(i), innb[i] = 0;
    memset(&w, 0, sizeof w);
    w.pos[0] = 10.0f, w.pos[1] = 20.0f, w.pos[2] = -30.0f, w.yaw = 1.5f;
    w.anim = ONLINE_A_WALK;
    CHECK(srv_message(&sv, 1, M_STATE, (const uint8_t *)&w, sizeof w));
    CHECK(silent(1));  // (not back to the sender)
    static const int to[] = {0, 2};
    for (int k = 0; k < 2; k++) {
        CHECK(take(to[k], &t, pay, &len));
        CHECK(t == M_STATE);
        CHECK(len == 1 + (int)sizeof w);
        CHECK(pay[0] == 1);  // (the sender's slot, in front)
        CHECK(len == 1 + (int)sizeof w && !memcmp(pay + 1, &w, sizeof w));
        CHECK(silent(to[k]));
    }
    connect_client(3);  // (a late one: it is told where the others are)
    CHECK(join_ok(3, "D"));
    CHECK(expect_slot_msg(3, M_WELCOME, NULL) == 3);
    bool seen = false;
    while (take(3, &t, pay, &len))
        if (t == M_STATE && len == 1 + (int)sizeof w && pay[0] == 1 && !memcmp(pay + 1, &w, sizeof w)) seen = true;
    CHECK(seen);
}

// ---- srv_step (T218): a listening socket of the test's, a Unix one in a directory of its own (no port)
static char ldir[64], lpath[96];

static void open_listener(void) {
    snprintf(ldir, sizeof ldir, "%s", "/tmp/swsessXXXXXX");
    CHECK(mkdtemp(ldir) != NULL);
    snprintf(lpath, sizeof lpath, "%s/s", ldir);
    struct sockaddr_un a;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", lpath);
    int l = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(l >= 0);
    CHECK(bind(l, (struct sockaddr *)&a, sizeof a) == 0);
    CHECK(listen(l, 8) == 0);
    sv.listener = l;
}

static void close_listener(void) {
    if (sv.listener != SOCK_BAD) sock_close(sv.listener);
    sv.listener = SOCK_BAD;
    unlink(lpath);
    rmdir(ldir);
}

// a connection from outside to the listener, as a new player's (srv_step accepts it)
static int dial(void) {
    struct sockaddr_un a;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", lpath);
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(s >= 0);
    CHECK(connect(s, (struct sockaddr *)&a, sizeof a) == 0);
    return s;
}

static void join_named(int n) {
    for (int i = 0; i < n; i++) {
        char name[8];
        snprintf(name, sizeof name, "P%d", i);
        connect_client(i);
        CHECK(join_ok(i, name));
    }
    for (int i = 0; i < n; i++) drain(i), innb[i] = 0;  // (the joins are not what is looked at here)
}

// (10) a full session: with the eight slots taken, a ninth connection gets REJECT R_FULL and is closed, and the eight
// stay, hearing nothing of it; once a slot is freed, the next connection takes it
static void test_full(void) {
    begin(10, "sesion llena -> REJECT R_FULL");
    open_listener();
    join_named(ONLINE_MAX);
    int s = dial();
    srv_step(&sv, 1000000);  // (the connection is already there: select returns at once)
    uint8_t got[16];
    int k = (int)recv(s, got, sizeof got, MSG_DONTWAIT);
    CHECK(k == 4);  // (u16 length 2, the type, the reason)
    CHECK(k == 4 && got[0] == 2 && got[1] == 0 && got[2] == M_REJECT && got[3] == R_FULL);
    char c;
    CHECK(recv(s, &c, 1, MSG_DONTWAIT) == 0);  // (and closed by the server)
    close(s);
    for (int i = 0; i < ONLINE_MAX; i++) CHECK(sv.c[i].used && sv.c[i].hello && silent(i));
    srv_drop(&sv, 3);
    for (int i = 0; i < ONLINE_MAX; i++)
        if (i != 3) drain(i), innb[i] = 0;
    s = dial();
    srv_step(&sv, 1000000);
    CHECK(sv.c[3].used && !sv.c[3].hello);  // (in, waiting for its HELLO)
    CHECK(recv(s, got, sizeof got, MSG_DONTWAIT) < 0);  // (nothing said to it yet, not closed)
    close(s);
    close_listener();
}

// (11) a client that says nothing for TIMEOUT_NS is dropped (the others get its LEAVE); TIMEOUT_NS itself is not yet
// too long, and any message resets the count
static void test_silence(void) {
    static uint8_t pay[4096];
    uint8_t t = 0;
    int len = 0;
    begin(11, "expulsion por silencio (TIMEOUT_NS)");
    open_listener();
    join_named(3);
    clock_ns += TIMEOUT_NS;  // (exactly the limit: all stay)
    srv_step(&sv, 0);
    for (int i = 0; i < 3; i++) CHECK(sv.c[i].used && silent(i));
    uint8_t ping[3 + 8] = {9, 0, M_PING, 1, 2, 3, 4, 5, 6, 7, 8};  // (1 just spoke: a PING)
    CHECK(send(peer[1], ping, sizeof ping, 0) == (ssize_t)sizeof ping);
    clock_ns += 1;
    srv_step(&sv, 1000000);  // (the PING is already there: select returns at once)
    CHECK(!sv.c[0].used && !sv.c[2].used);
    CHECK(sv.c[1].used && sv.c[1].hello && sv.c[1].heard == clock_ns);
    CHECK(expect_slot_msg(1, M_LEAVE, NULL) == 0);
    CHECK(take(1, &t, pay, &len));
    CHECK(t == M_PONG && len == 8 && !memcmp(pay, ping + 3, 8));
    CHECK(expect_slot_msg(1, M_LEAVE, NULL) == 2);
    CHECK(silent(1));
    CHECK(closed_by_server(0));
    drain(2), innb[2] = 0;  // (the LEAVE of 0 it got before going)
    CHECK(closed_by_server(2));
    clock_ns += TIMEOUT_NS + 1;  // (and 1, silent from then on, goes too)
    srv_step(&sv, 0);
    CHECK(!sv.c[1].used);
    CHECK(closed_by_server(1));
    close_listener();
}

// TIME from the server to client i: the host's hour
static void expect_time(int i, float hour) {
    static uint8_t pay[64];
    uint8_t t = 0;
    int len = 0;
    float h = -1;
    CHECK(take(i, &t, pay, &len));
    CHECK(t == M_TIME && len == 4);
    memcpy(&h, pay, 4);
    CHECK(h == hour);
}

// (12) each server keeps its own time for the next TIME: a second server in the same process sends the host's hour
// at once, as the first did (before T218 it was a static of srv_loop: the second waited for the first's time)
static void test_time_each_server(void) {
    begin(12, "un servidor nuevo en el mismo proceso manda TIME enseguida");
    online_set_local_hour(12.5f);
    open_listener();
    join_named(1);
    srv_step(&sv, 0);
    expect_time(0, 12.5f);
    clock_ns += TIME_EVERY_NS / 2;
    srv_step(&sv, 0);
    CHECK(silent(0));  // (one each TIME_EVERY_NS)
    close_listener();
    reset();  // (a new server: srv_open clears it all, as reset does)
    open_listener();
    join_named(1);
    srv_step(&sv, 0);
    expect_time(0, 12.5f);  // (at once)
    close_listener();
    online_set_local_hour(-1);
}

int main(void) {
    for (int i = 0; i < ONLINE_MAX; i++) peer[i] = -1;
    memset(&sv, 0, sizeof sv);
    sv.listener = SOCK_BAD;
    int cases = 0, bad = 0;
    void (*const t[])(void) = {test_hello_ok, test_reject_version, test_reject_password, test_reject_name,
                               test_first_not_hello, test_join_both_ways, test_drop_leave, test_slot_reused,
                               test_state_relayed, test_full, test_silence, test_time_each_server};
    for (size_t k = 0; k < sizeof t / sizeof *t; k++) {
        int before = failures;
        t[k]();
        cases++;
        if (failures != before) bad++;
        printf("  %s\n", failures == before ? "ok" : "FALLA");
    }
    reset();
    printf("test_online_session: %d cases, %d failed, %d checks, %d check failures\n", cases, bad, checks, failures);
    return failures ? 1 : 0;
}
#endif
