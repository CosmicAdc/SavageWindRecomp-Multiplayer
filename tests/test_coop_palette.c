// Tests for runtime/coop_palette.c (CU-14, ADR-009 decision 26): the eight colours against the three colour vision
// simulations of runtime/colour_vision.c, and the assignment by order of entry. No game and no UI.
//   clang -Wall -Iruntime tests/test_coop_palette.c runtime/coop_palette.c runtime/colour_vision.c -lm -o build/test_coop_palette
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "colour_vision.h"
#include "coop_palette.h"
#include "settings.h"

Settings g_settings;  // (colour_vision.c reads it for colour_vision_matrix, which this test does not call)

static int g_fail;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), g_fail++; \
    } while (0)

// Two colours are apart if, as the player sees them, their sRGB values (0..1) are at least this far in a straight line
// (the whole range is 1.73; 0.15 is a clear step on a screen, and the normal-vision pairs here are far above it).
// If a pair falls short, change the colour in coop_palette.c, not this number.
#define MIN_DISTANCE 0.15

static double to_linear(double v) { return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); }
static double to_srgb(double v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1 / 2.4) - 0.055;
}

// The colour as it looks with the simulation m (NULL: normal vision), as sRGB 0..1
static void seen(CoopRgb c, const float *m, double out[3]) {
    double lin[3] = {to_linear(c.r / 255.0), to_linear(c.g / 255.0), to_linear(c.b / 255.0)};
    for (int i = 0; i < 3; i++) {
        double v = m ? m[i * 3] * lin[0] + m[i * 3 + 1] * lin[1] + m[i * 3 + 2] * lin[2] : lin[i];
        out[i] = to_srgb(v);
    }
}

static void test_palette_apart(void) {
    static const char *const kinds[4] = {"normal vision", "protanopia", "deuteranopia", "tritanopia"};
    for (int kind = 0; kind < 4; kind++) {
        float m[9];
        if (kind) colour_vision_simulation(kind, 1.0f, m);
        double worst = 1e9;
        int wa = -1, wb = -1;
        for (int a = 0; a < COOP_PALETTE_COUNT; a++)
            for (int b = a + 1; b < COOP_PALETTE_COUNT; b++) {
                double va[3], vb[3];
                seen(coop_palette_color(a), kind ? m : NULL, va);
                seen(coop_palette_color(b), kind ? m : NULL, vb);
                double d = sqrt((va[0] - vb[0]) * (va[0] - vb[0]) + (va[1] - vb[1]) * (va[1] - vb[1]) + (va[2] - vb[2]) * (va[2] - vb[2]));
                if (d < worst) worst = d, wa = a, wb = b;
            }
        if (worst < MIN_DISTANCE) printf("  %s: colours %d and %d are only %.3f apart (need %.2f)\n", kinds[kind], wa, wb, worst, MIN_DISTANCE);
        else printf("  %s: closest pair (%d, %d) is %.3f apart\n", kinds[kind], wa, wb, worst);
        CHECK(worst >= MIN_DISTANCE);
    }
    puts("PASS: every pair of colours is apart under normal vision, protanopia, deuteranopia and tritanopia");
}

static void test_palette_basics(void) {
    for (int a = 0; a < COOP_PALETTE_COUNT; a++)
        for (int b = a + 1; b < COOP_PALETTE_COUNT; b++) {
            CoopRgb x = coop_palette_color(a), y = coop_palette_color(b);
            CHECK(x.r != y.r || x.g != y.g || x.b != y.b);
        }
    CoopRgb first = coop_palette_color(0), bad = coop_palette_color(-1), big = coop_palette_color(8);
    CHECK(bad.r == first.r && bad.g == first.g && bad.b == first.b);
    CHECK(big.r == first.r && big.g == first.g && big.b == first.b);
    CHECK(coop_palette_index_ok(0) && coop_palette_index_ok(7) && !coop_palette_index_ok(8) && !coop_palette_index_ok(-1));
    CHECK(COOP_SHAPE_RING == COOP_PALETTE_COUNT - 1);  // (eight shapes: circle to ring)
    puts("PASS: eight different colours, an index out of range gives the first");
}

static bool all_pairs_apart(const uint8_t *c, const uint8_t *s, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (c[i] == c[j] && s[i] == s[j]) return false;
    return true;
}

static void test_default_order(void) {
    uint8_t c[8], s[8];
    memset(c, 0xee, sizeof c), memset(s, 0xee, sizeof s);
    coop_palette_assign(NULL, 8, c, s);  // (nobody chose)
    for (int i = 0; i < 8; i++) CHECK(c[i] == i && s[i] == i);
    CoopPalettePref none[8];
    for (int i = 0; i < 8; i++) none[i].color = none[i].shape = -1;
    memset(c, 0xee, sizeof c), memset(s, 0xee, sizeof s);
    coop_palette_assign(none, 2, c, s);  // (two players: the first colour 0 shape 0, the second colour 1 shape 1)
    CHECK(c[0] == 0 && s[0] == 0 && c[1] == 1 && s[1] == 1 && c[2] == 0xee);  // (nothing past n)
    puts("PASS: by order of entry, colour i and shape i");
}

static void test_chosen_kept(void) {
    uint8_t c[4], s[4];
    // (the second player changed his colour and shape in Options; the others did not)
    CoopPalettePref p[4] = {{-1, -1}, {5, 6}, {-1, -1}, {-1, -1}};
    coop_palette_assign(p, 4, c, s);
    CHECK(c[0] == 0 && s[0] == 0);
    CHECK(c[1] == 5 && s[1] == 6);
    CHECK(c[2] == 2 && s[2] == 2 && c[3] == 3 && s[3] == 3);
    // (only the colour chosen: the shape is by his index; only the shape: the colour is)
    CoopPalettePref q[2] = {{4, -1}, {-1, 5}};
    coop_palette_assign(q, 2, c, s);
    CHECK(c[0] == 4 && s[0] == 0 && c[1] == 1 && s[1] == 5);
    // (out of range choices are no choice)
    CoopPalettePref r[3] = {{8, 8}, {-2, 100}, {127, -128}};
    coop_palette_assign(r, 3, c, s);
    for (int i = 0; i < 3; i++) CHECK(c[i] == i && s[i] == i);
    puts("PASS: a player's own choice is kept; a bad one counts as none");
}

static void test_clash(void) {
    uint8_t c[8], s[8];
    // (the first took colour 1 and shape 1 himself: the second, by order, would get exactly that, so he moves to
    // the next shape free with colour 1: shape 2)
    CoopPalettePref a[2] = {{1, 1}, {-1, -1}};
    coop_palette_assign(a, 2, c, s);
    CHECK(c[0] == 1 && s[0] == 1 && c[1] == 1 && s[1] == 2);
    // (the one with the higher index moves, whoever chose: both chose the same pair, the second goes on)
    CoopPalettePref b[3] = {{3, 4}, {3, 4}, {3, 4}};
    coop_palette_assign(b, 3, c, s);
    CHECK(c[0] == 3 && s[0] == 4 && s[1] == 5 && s[2] == 6 && c[1] == 3 && c[2] == 3);
    // (the shape wraps past the last: 7 -> 0; and skips a taken one)
    CoopPalettePref w[3] = {{2, 7}, {2, 7}, {2, 0}};
    coop_palette_assign(w, 3, c, s);
    CHECK(s[0] == 7 && s[1] == 0 && s[2] == 1);  // (the third asked for 0, taken by the second: goes to 1)
    // (a later player's choice never moves an earlier one)
    CoopPalettePref l[2] = {{-1, -1}, {0, 0}};
    coop_palette_assign(l, 2, c, s);
    CHECK(c[0] == 0 && s[0] == 0 && c[1] == 0 && s[1] == 1);
    // (a different colour with the same shape is no clash)
    CoopPalettePref d[2] = {{1, 3}, {2, 3}};
    coop_palette_assign(d, 2, c, s);
    CHECK(s[0] == 3 && s[1] == 3 && c[0] == 1 && c[1] == 2);
    // (eight players with a mess of choices still all differ)
    CoopPalettePref m[8] = {{0, 0}, {0, 0}, {0, 1}, {-1, -1}, {3, 3}, {3, -1}, {-1, 3}, {0, 2}};
    coop_palette_assign(m, 8, c, s);
    CHECK(all_pairs_apart(c, s, 8));
    puts("PASS: a clash moves the higher index to the next free shape");
}

static void test_many_and_edges(void) {
    uint8_t c[64], s[64];
    // (64 players: every colour-shape pair exactly once, all different)
    coop_palette_assign(NULL, 64, c, s);
    CHECK(all_pairs_apart(c, s, 64) == (64 <= COOP_PALETTE_COUNT * COOP_PALETTE_COUNT));
    for (int i = 0; i < 64; i++) CHECK(c[i] < 8 && s[i] < 8);
    // (past 8, wrapping would repeat colour i%8 and shape i%8: the shape moves on, so 16 players are still different)
    coop_palette_assign(NULL, 16, c, s);
    CHECK(all_pairs_apart(c, s, 16));
    // (nothing to write)
    uint8_t keep[2] = {0x55, 0x55};
    coop_palette_assign(NULL, 0, keep, keep);
    coop_palette_assign(NULL, -3, keep, keep);
    coop_palette_assign(NULL, 2, NULL, keep);
    coop_palette_assign(NULL, 2, keep, NULL);
    CHECK(keep[0] == 0x55 && keep[1] == 0x55);
    puts("PASS: many players, no players, NULL outputs");
}

int main(void) {
    test_palette_apart();
    test_palette_basics();
    test_default_order();
    test_chosen_kept();
    test_clash();
    test_many_and_edges();
    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    puts("PASS: coop_palette");
    return 0;
}
