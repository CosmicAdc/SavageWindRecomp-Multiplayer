// Tests for the online wire (runtime/online.c) without the game or a network: how a byte stream is cut into messages
// (next_msg, consumed) and how SKEL packs a bone (skel_encode, skel_decode). No socket is opened, no thread is started.
//   make test-online-wire
#include "../runtime/online.c"

// ---- stubs for what online.c takes from the rest of the runtime (as in tools/test_online_net.c)
GameSites g_site;  // (all zero: no game)
static uint64_t clock_ns = 100000000000ull;
uint64_t plat_now_ns(void) { return clock_ns; }

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

_Static_assert(SKEL_BONE_BYTES == 12, "a bone is 12 bytes on the wire");

// ---- framing: u16 length (what follows it), u8 type, payload

// put one message at the end of rb: its payload (n bytes, value from `fill` counting up) and type
static void put_msg(RecvBuf *rb, uint8_t type, int n, uint8_t fill) {
    int len = 1 + n;
    rb->data[rb->n++] = (uint8_t)len;
    rb->data[rb->n++] = (uint8_t)(len >> 8);
    rb->data[rb->n++] = type;
    for (int k = 0; k < n; k++) rb->data[rb->n++] = (uint8_t)(fill + k);
}

static bool payload_is(const uint8_t *p, int n, uint8_t fill) {
    for (int k = 0; k < n; k++)
        if (p[k] != (uint8_t)(fill + k)) return false;
    return true;
}

// (a) a message that arrives one byte at a time shows up when its last byte does, and not before
static void test_byte_by_byte(void) {
    static const int sizes[] = {0, 1, 9, 300, 4096};
    for (size_t s = 0; s < sizeof sizes / sizeof *sizes; s++) {
        static RecvBuf whole, rb;
        whole.n = 0;
        put_msg(&whole, M_CHAT, sizes[s], 7);
        memset(&rb, 0, sizeof rb);
        bool early = false;
        for (int k = 0; k < whole.n - 1; k++) {
            rb.data[rb.n++] = whole.data[k];
            int off = 0, len = -1;
            uint8_t type = 0;
            const uint8_t *p = NULL;
            if (next_msg(&rb, &off, &type, &p, &len)) early = true;
            CHECK(off == 0);  // (nothing taken)
        }
        CHECK(!early);
        rb.data[rb.n++] = whole.data[whole.n - 1];
        int off = 0, len = -1;
        uint8_t type = 0;
        const uint8_t *p = NULL;
        if (sizes[s] == 0) {  // (a message with no payload is complete at its 3rd byte: the loop above never saw less)
            CHECK(whole.n == 3);
        }
        CHECK(next_msg(&rb, &off, &type, &p, &len));
        CHECK(type == M_CHAT && len == sizes[s] && payload_is(p, len, 7));
        CHECK(off == rb.n);
        CHECK(!next_msg(&rb, &off, &type, &p, &len));  // (and nothing more)
    }
}

// (b) two messages stuck together both come out, in order, then none
static void test_two_glued(void) {
    static RecvBuf rb;
    memset(&rb, 0, sizeof rb);
    put_msg(&rb, M_STATE, 40, 1);
    put_msg(&rb, M_PING, 8, 100);
    int off = 0, len = -1;
    uint8_t type = 0;
    const uint8_t *p = NULL;
    CHECK(next_msg(&rb, &off, &type, &p, &len));
    CHECK(type == M_STATE && len == 40 && payload_is(p, len, 1));
    CHECK(next_msg(&rb, &off, &type, &p, &len));
    CHECK(type == M_PING && len == 8 && payload_is(p, len, 100));
    CHECK(off == rb.n);
    CHECK(!next_msg(&rb, &off, &type, &p, &len));
    CHECK(off == rb.n);
}

// (c) a length of 0 (not even the type byte) is never a message, however much follows it; nothing is taken
static void test_length_zero(void) {
    static RecvBuf rb;
    memset(&rb, 0, sizeof rb);
    rb.data[0] = 0, rb.data[1] = 0, rb.data[2] = M_STATE;
    rb.n = 3;
    for (int k = 3; k < 200; k++) rb.data[rb.n++] = (uint8_t)k;
    int off = 0, len = -1;
    uint8_t type = 0xee;
    const uint8_t *p = NULL;
    CHECK(!next_msg(&rb, &off, &type, &p, &len));
    CHECK(off == 0 && type == 0xee && p == NULL && len == -1);
}

// (d) a length of 65535 (more than the buffer holds) is never complete, whatever the buffer holds, and next_msg
// reads nothing past what is in it: guard bytes behind the buffer are not touched, and neither are the results
typedef struct { RecvBuf rb; uint8_t guard[64]; } Guarded;
static void test_length_max(void) {
    static Guarded gd;
    memset(&gd, 0, sizeof gd);
    memset(gd.guard, 0xa5, sizeof gd.guard);
    gd.rb.data[0] = 0xff, gd.rb.data[1] = 0xff, gd.rb.data[2] = M_STATE;
    for (int k = 3; k < (int)sizeof gd.rb.data; k++) gd.rb.data[k] = (uint8_t)k;
    gd.rb.n = (int)sizeof gd.rb.data;  // (the buffer full: the most it can ever hold)
    int off = 0, len = -1;
    uint8_t type = 0xee;
    const uint8_t *p = NULL;
    CHECK(!next_msg(&gd.rb, &off, &type, &p, &len));
    CHECK(off == 0 && type == 0xee && p == NULL && len == -1);
    for (size_t k = 0; k < sizeof gd.guard; k++) CHECK(gd.guard[k] == 0xa5);
    CHECK(gd.rb.n == (int)sizeof gd.rb.data);

    // the largest length the buffer can hold is still taken: 32768 - 2
    memset(&gd.rb, 0, sizeof gd.rb);
    int l = (int)sizeof gd.rb.data - 2;
    gd.rb.data[0] = (uint8_t)l, gd.rb.data[1] = (uint8_t)(l >> 8), gd.rb.data[2] = M_STATE;
    gd.rb.n = (int)sizeof gd.rb.data;
    off = 0;
    CHECK(next_msg(&gd.rb, &off, &type, &p, &len));
    CHECK(type == M_STATE && len == l - 1 && off == (int)sizeof gd.rb.data);
    CHECK(p + len == gd.rb.data + sizeof gd.rb.data);  // (its payload ends where the buffer does)
    for (size_t k = 0; k < sizeof gd.guard; k++) CHECK(gd.guard[k] == 0xa5);
}

// (e) consumed drops what was taken and keeps the rest (a message still arriving included) at the front
static void test_consumed_keeps_rest(void) {
    static RecvBuf whole, rb;
    memset(&whole, 0, sizeof whole);
    put_msg(&whole, M_CHAT, 20, 1);
    put_msg(&whole, M_TIME, 4, 50);
    int two = whole.n;                  // (the two whole messages)
    put_msg(&whole, M_EQUIP, 60, 90);   // (a third, of which only a part comes first)
    int part = two + 17;

    memset(&rb, 0, sizeof rb);
    memcpy(rb.data, whole.data, part);
    rb.n = part;
    int off = 0, len = -1;
    uint8_t type = 0;
    const uint8_t *p = NULL;
    CHECK(next_msg(&rb, &off, &type, &p, &len) && type == M_CHAT && len == 20);
    CHECK(next_msg(&rb, &off, &type, &p, &len) && type == M_TIME && len == 4);
    CHECK(off == two);
    CHECK(!next_msg(&rb, &off, &type, &p, &len));
    CHECK(off == two);
    consumed(&rb, off);
    CHECK(rb.n == 17);
    CHECK(!memcmp(rb.data, whole.data + two, 17));  // (the start of the third, now at the front)

    // the rest of the third comes: it completes
    memcpy(rb.data + rb.n, whole.data + part, whole.n - part);
    rb.n += whole.n - part;
    off = 0;
    CHECK(next_msg(&rb, &off, &type, &p, &len));
    CHECK(type == M_EQUIP && len == 60 && payload_is(p, len, 90));
    CHECK(off == rb.n);
    consumed(&rb, off);
    CHECK(rb.n == 0);

    // taking nothing changes nothing
    memcpy(rb.data, whole.data, two);
    rb.n = two;
    consumed(&rb, 0);
    CHECK(rb.n == two && !memcmp(rb.data, whole.data, two));
}

// ---- SKEL's bones

// a small deterministic generator (the same numbers on every system and compiler)
static uint64_t rng_state = 0x9e3779b97f4a7c15ull;
static uint64_t rng(void) {
    rng_state ^= rng_state << 13, rng_state ^= rng_state >> 7, rng_state ^= rng_state << 17;
    return rng_state;
}
static double rng_unit(void) { return (double)(rng() >> 11) / 9007199254740992.0 * 2.0 - 1.0; }  // [-1, 1)

// the angle (radians) between the rotations two quaternions (x, y, z, w) stand for (q and -q are the same one)
static double quat_angle(const double a[4], const double b[4]) {
    double w = a[3] * b[3] + a[0] * b[0] + a[1] * b[1] + a[2] * b[2];  // (a times the conjugate of b)
    double x = -a[3] * b[0] + b[3] * a[0] - (a[1] * b[2] - a[2] * b[1]);
    double y = -a[3] * b[1] + b[3] * a[1] - (a[2] * b[0] - a[0] * b[2]);
    double z = -a[3] * b[2] + b[3] * a[2] - (a[0] * b[1] - a[1] * b[0]);
    return 2.0 * atan2(sqrt(x * x + y * y + z * z), fabs(w));
}

// What the round trip of a unit quaternion may be off by, in radians. Each of the three kept components is rounded to
// 1/16383/sqrt(2) steps (half a step at most: 2.2e-5), so the angle is off by at most 2 * sqrt(3) * 2.2e-5 = 7.5e-5.
// POR MEDIR: the test prints the largest error it saw; the orchestrator sets this to that, with a little room.
#define SKEL_ANGLE_TOL 1e-4

static void test_skel_roundtrip(void) {
    uint8_t o[16];
    double worst = 0;
    int used[4] = {0};
    for (int n = 0; n < 1000; n++) {
        double q[4], l;
        do {
            for (int k = 0; k < 4; k++) q[k] = rng_unit();
            l = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        } while (l < 0.1 || l > 1.0);  // (uniform in the ball, so the directions are uniform)
        for (int k = 0; k < 4; k++) q[k] /= l;
        float qf[4] = {(float)q[0], (float)q[1], (float)q[2], (float)q[3]}, t[3] = {0, 0, 0}, qd[4], td[3];
        memset(o, 0xa5, sizeof o);
        skel_encode(o, qf, t);
        for (int k = 4 * 3; k < 16; k++) CHECK(o[k] == 0xa5);  // (a bone is 12 bytes: nothing past them)
        skel_decode(o, qd, td);
        used[o[0] & 3]++;
        double d[4] = {qd[0], qd[1], qd[2], qd[3]};
        double dl = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + d[3] * d[3]);
        CHECK(fabs(dl - 1.0) < 1e-5);  // (what comes out is a unit quaternion)
        double ang = quat_angle(d, q);
        if (ang > worst) worst = ang;
    }
    printf("test_online_wire: skel rotation, 1000 unit quaternions: largest angle error %.3g rad (tolerance %.3g)\n",
        worst, SKEL_ANGLE_TOL);
    CHECK(worst < SKEL_ANGLE_TOL);
    for (int k = 0; k < 4; k++) CHECK(used[k] > 0);  // (each of the four "largest component" cases came up)
}

// the null quaternion, and ones that are not numbers, come out as the identity (x, y, z, w) = (0, 0, 0, 1)
static void test_skel_null_quaternion(void) {
    uint8_t o[SKEL_BONE_BYTES];
    float qd[4], td[3], t[3] = {0, 0, 0};
    const float zero[4] = {0, 0, 0, 0};
    skel_encode(o, zero, t);
    skel_decode(o, qd, td);
    CHECK(qd[0] == 0 && qd[1] == 0 && qd[2] == 0 && qd[3] == 1.0f);
    const float nanq[4] = {NAN, 0, 0, 1};
    skel_encode(o, nanq, t);
    skel_decode(o, qd, td);
    CHECK(qd[0] == 0 && qd[1] == 0 && qd[2] == 0 && qd[3] == 1.0f);
    // axis-aligned ones (each component the largest in turn), and the same rotation with every sign flipped
    for (int k = 0; k < 4; k++) {
        for (int sg = -1; sg <= 1; sg += 2) {
            float q[4] = {0, 0, 0, 0};
            q[k] = (float)sg;
            skel_encode(o, q, t);
            skel_decode(o, qd, td);
            double a[4] = {qd[0], qd[1], qd[2], qd[3]}, b[4] = {q[0], q[1], q[2], q[3]};
            CHECK(quat_angle(a, b) < 1e-6);
        }
    }
    // a quaternion that is not unit is normalised first: scaled copies come out the same
    const float big[4] = {0, 3.0f, 0, 4.0f}, unit[4] = {0, 0.6f, 0, 0.8f};
    uint8_t o2[SKEL_BONE_BYTES];
    skel_encode(o, big, t);
    skel_encode(o2, unit, t);
    CHECK(!memcmp(o, o2, 6));
}

// translations: 1/8192 m in an s16, so +-3.99987 m; further out saturates there, never wraps round
static void test_skel_translation(void) {
    uint8_t o[SKEL_BONE_BYTES];
    const float q[4] = {0, 0, 0, 1};
    float qd[4], td[3];
    const float max = 32767.0f / 8192.0f;

    // in range: off by at most half a step
    const float in[][3] = {{0, 0, 0}, {1.5f, -2.25f, 3.0f}, {-3.9f, 3.9f, 0.123456f}, {max, -max, 0.0001f}};
    for (size_t k = 0; k < sizeof in / sizeof *in; k++) {
        skel_encode(o, q, in[k]);
        skel_decode(o, qd, td);
        for (int c = 0; c < 3; c++) CHECK(fabsf(td[c] - in[k][c]) <= 0.5f / 8192.0f + 1e-6f);
    }
    // out of range: the end of the range, with its own sign
    const float out[][3] = {{4.0f, -4.0f, 100.0f}, {-100.0f, 1e6f, -1e6f}, {1e30f, -1e30f, 3.4e38f},
                            {INFINITY, -INFINITY, 32768.0f}};
    for (size_t k = 0; k < sizeof out / sizeof *out; k++) {
        skel_encode(o, q, out[k]);
        skel_decode(o, qd, td);
        for (int c = 0; c < 3; c++) {
            float want = out[k][c] > 0 ? max : -max;
            CHECK(td[c] == want);
        }
        CHECK(qd[3] == 1.0f);  // (and the rotation beside it is not touched by it)
    }
    // exactly where the range ends, and one step inside it
    const float edge[3] = {max, -max, 32766.0f / 8192.0f};
    skel_encode(o, q, edge);
    skel_decode(o, qd, td);
    CHECK(td[0] == max && td[1] == -max && td[2] == edge[2]);
}

// What arrives is decoded whatever it holds: every bone of random bytes gives numbers, and the translation stays in range
static void test_skel_decode_garbage(void) {
    for (int n = 0; n < 1000; n++) {
        uint8_t o[SKEL_BONE_BYTES];
        for (int k = 0; k < SKEL_BONE_BYTES; k++) o[k] = (uint8_t)rng();
        float q[4], t[3];
        skel_decode(o, q, t);
        for (int k = 0; k < 4; k++) CHECK(isfinite(q[k]) && fabsf(q[k]) <= 1.0001f);
        for (int k = 0; k < 3; k++) CHECK(isfinite(t[k]) && fabsf(t[k]) <= 4.0f);
    }
    const uint8_t ff[SKEL_BONE_BYTES] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    float q[4], t[3];
    skel_decode(ff, q, t);
    for (int k = 0; k < 4; k++) CHECK(isfinite(q[k]));
    CHECK(t[0] == -1.0f / 8192.0f);  // (0xffff as an s16)
}

int main(void) {
    test_byte_by_byte();
    test_two_glued();
    test_length_zero();
    test_length_max();
    test_consumed_keeps_rest();
    test_skel_roundtrip();
    test_skel_null_quaternion();
    test_skel_translation();
    test_skel_decode_garbage();
    printf("test_online_wire: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
