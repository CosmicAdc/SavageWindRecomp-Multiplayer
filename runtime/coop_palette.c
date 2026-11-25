// Colour-safe palette and shapes for co-op players: see runtime/coop_palette.h. Pure logic, tested by
// tests/test_coop_palette.c.
#include "coop_palette.h"

// Okabe-Ito, black swapped for a light grey, and its blue made deeper: orange, sky blue, bluish green, yellow, deep
// blue, vermillion, reddish purple, grey. The stock blue (0,114,178) and bluish green (0,158,115) both turn the same
// teal under tritanopia (0.117 apart); the deep blue (0,85,170) goes darker there. Expected, worked by hand with the
// matrices of colour_vision.c (sRGB 0..1 as seen, severity 1), deep blue and bluish green:
//   tritanopia   (0.00,0.42,0.47) and (0.00,0.62,0.57): 0.22 apart (the closest other pair is about 0.18)
//   protanopia   about 0.4 or more from every other colour; deuteranopia and normal vision, the same or more
static const CoopRgb k_colors[COOP_PALETTE_COUNT] = {
    {230, 159, 0}, {86, 180, 233}, {0, 158, 115}, {240, 228, 66}, {0, 85, 170}, {213, 94, 0}, {204, 121, 167}, {230, 230, 230},
};

bool coop_palette_index_ok(int i) { return i >= 0 && i < COOP_PALETTE_COUNT; }

CoopRgb coop_palette_color(int i) { return k_colors[coop_palette_index_ok(i) ? i : 0]; }

// Whether an earlier player (0..i-1) already has this colour and shape
static bool taken(const uint8_t *color, const uint8_t *shape, int i, int c, int s) {
    for (int j = 0; j < i; j++)
        if (color[j] == c && shape[j] == s) return true;
    return false;
}

void coop_palette_assign(const CoopPalettePref *pref, int n, uint8_t *out_color, uint8_t *out_shape) {
    if (n <= 0 || !out_color || !out_shape) return;
    for (int i = 0; i < n; i++) {
        int c = i % COOP_PALETTE_COUNT, s = i % COOP_PALETTE_COUNT;
        if (pref && coop_palette_index_ok(pref[i].color)) c = pref[i].color;
        if (pref && coop_palette_index_ok(pref[i].shape)) s = pref[i].shape;
        for (int k = 0; k < COOP_PALETTE_COUNT && taken(out_color, out_shape, i, c, s); k++)
            s = (s + 1) % COOP_PALETTE_COUNT;  // (all eight taken with this colour: he shares the last one tried)
        out_color[i] = (uint8_t)c;
        out_shape[i] = (uint8_t)s;
    }
}
