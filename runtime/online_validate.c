// The network validation layer (runtime/online_validate.h): its functions are inline there, so that every program that
// includes runtime/online.c has them without a rule of its own. What is left here is the guard: the sizes of every
// structure those functions walk, field by field, as they were when the walk was written. A field added, removed or
// resized stops the build here, before a number or a string of it can go by unchecked (the forgotten shield of F1):
// walk it in online_validate_state / _equip / _enemy / _loot and online_terminate_*, then change the size here.
#include "online_validate.h"

_Static_assert(sizeof(WireState) == 3375, "WireState changed: walk its new fields in online_validate_state");
_Static_assert(sizeof(WireEquip) == 432, "WireEquip changed: walk its new fields in online_validate_equip");
_Static_assert(sizeof(OnlineEnemy) == 311, "OnlineEnemy changed: walk its new fields in online_validate_enemy");
_Static_assert(sizeof(OnlineLoot) == 97, "OnlineLoot changed: walk its new fields in online_validate_loot");
_Static_assert(sizeof(OnlineShots) == 1 + ONLINE_SHOTS * 51 + ONLINE_BOOMS * 15, "OnlineShots changed: online_validate_state");
_Static_assert(sizeof(OnlineGrab) == 100, "OnlineGrab changed: online_validate_grab");
_Static_assert(sizeof(OnlineObj) == 52, "OnlineObj changed: online_validate_state");
_Static_assert(sizeof(OnlineMover) == 80, "OnlineMover changed: online_validate_state");
_Static_assert(sizeof(OnlineFxEvent) == 24, "OnlineFxEvent changed: online_validate_state");
_Static_assert(sizeof(OnlineHeld) == 81, "OnlineHeld changed: online_validate_state");
_Static_assert(offsetof(WireState, held) + sizeof(((WireState *)0)->held) == sizeof(WireState),
               "WireState has a field after held: walk it in online_validate_state");
