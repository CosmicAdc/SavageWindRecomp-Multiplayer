// Colour-safe palette and shapes for co-op players (runtime/coop_palette.c): eight colours that stay apart for a player
// with protanopia, deuteranopia or tritanopia (tests/test_coop_palette.c checks every pair against the simulations of
// runtime/colour_vision.h) and eight shapes, so that colour is never the only thing that tells one player from
// another. Pure logic: no game memory, no network and no UI (the overlay, T425, draws the shapes) and no settings
// (the players' choices come as arguments), so that the test can test it alone.
//
// The colours are the Okabe-Ito set (a palette made for colour blindness) with its black swapped for a light grey,
// which an overlay on a dark picture can show; the order puts the first two players, the usual pair, furthest apart.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_PALETTE_COUNT 8  // colours, and shapes

typedef struct {
    uint8_t r, g, b;  // sRGB
} CoopRgb;

typedef enum {
    COOP_SHAPE_CIRCLE,
    COOP_SHAPE_SQUARE,
    COOP_SHAPE_TRIANGLE,
    COOP_SHAPE_DIAMOND,
    COOP_SHAPE_STAR,
    COOP_SHAPE_CROSS,
    COOP_SHAPE_HEXAGON,
    COOP_SHAPE_RING,
} CoopShape;

// What a player chose in Options: -1 (or anything outside 0..7) is "nothing", and the order of entry decides
typedef struct {
    int8_t color;  // index into the palette, or -1
    int8_t shape;  // a CoopShape, or -1
} CoopPalettePref;

// Whether i is a colour or shape index (0..7): the check for what comes from the network
bool coop_palette_index_ok(int i);

// The colour of an index; an index out of range gives entry 0
CoopRgb coop_palette_color(int i);

// Colour and shape for n players, in order of entry (index 0 is the first in). Who chose nothing gets, by his index i,
// colour i and shape i (wrapping past 8); who chose only the colour, or only the shape, keeps it and gets the other
// by index; who chose both keeps both. If two players end with the same colour and shape, the one with the higher
// index gets the next shape free with that colour (the first in order, after his own, wrapping); the lower index never
// moves. With more players than colour-shape pairs (64) the last ones share. pref may be NULL (nobody chose); n <= 0,
// or out arrays NULL: nothing is written. out_color and out_shape get n entries each.
void coop_palette_assign(const CoopPalettePref *pref, int n, uint8_t *out_color, uint8_t *out_shape);

#ifdef __cplusplus
}
#endif
