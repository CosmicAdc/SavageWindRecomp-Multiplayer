// Online play: the other players as actors in the world (runtime/online.h). Once per frame, on the game's frame loop
// (tools/patches.py _vfr_ratio, ksys::VFR::updateInterval on the main thread, before the actors' update):
//   - the local Link's pose is read for the others (the AS his Link plays, where his gear is, his heading);
//   - each remote player gets a Link of his own (OnlinePlayer, tools/online_mod.py: created the way the game creates
//     the Remote Bomb rune's bomb) where he is, playing the AS his Link plays; wearing his armour, the game's own
//     pieces worn on that Link's skeleton as the player's are; carrying his gear (weapon, shield, bow, sheath,
//     quiver: model-only actors, OnlineGear_*) on the bones it is on in his game.
//
// 1.9.0 (no symbols; read from the code):
//   0x7001535900  create an actor now, by name: x0 the creator (**0x7001d66550), x1 the actor's name, x2 a heap
//                 (*(**0x7001d66558 + 0x28)), x3 0, w4, w5 0 -> the actor or 0 (its resources aren't loaded). The
//                 Remote Bomb rune (0x7000b3bb40) calls it with "RemoteBomb" / "RemoteBombCube" and then
//   0x700152261c  set an actor's matrix (x0 actor, x1 Matrix34f *, x2 scale or 0): the 3x4 at actor +0x398
//   0x7001514bac  delete an actor (x0 actor, w1 0), as 0x7001535900 does with one that failed its check
//
// Off unless online_mod is the RomFS overlay (or SAVAGEWIND_ONLINE_ACTOR=<name> picks an actor: docs/ONLINE.md, what
// was tried). Without it the other players are only the overlay's figures (runtime/ui/online_ui.mm).
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "control_cells.h"
#include "game_sites.h"
#include "online.h"
#include "online_gate.h"
#include "rt.h"
#include "settings.h"
#include "online_actors_internal.h"  // (and the rest of online play's actors: runtime/oa_*.c)

// ---- Names from the network that would make an actor here (specs/contratos/validacion-red.md V-11). The enemies
// are enemies.txt's (enemy_name_ok); what a host's enemy holds and what falls as loot, each kind on a list of its own
// in online_mod (tools/online_mod.py, from the player's own ActorInfo), "name=its profile" a line: weapons.txt (the
// Weapon_* an enemy holds or drops: not the sheaths nor the quivers), items.txt (Item_*), rupees.txt (PutRupee*).
// A list is read once, at the first name of its kind; one that isn't there (an overlay older than tools/online_mod.py
// VERSION 43) or is empty refuses every name of its kind, said once in the log. tools/test_online_names.c builds this
// part alone (ONLINE_NAMES_ONLY), with lists of its own.
#define NAME_LEN 48  // (a name with its 0: the game's actor names are shorter; a longer line is no name)
typedef struct { const char *file, *kind; char (*name)[NAME_LEN]; int n; bool read; } NameList;
static NameList g_weapon_names = {"weapons.txt", "weapon"}, g_item_names = {"items.txt", "item"},
                g_rupee_names = {"rupees.txt", "rupee"};

static int name_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static void names_read(NameList *l) {
    l->read = true;
    const char *ov = getenv("SAVAGEWIND_ROMFS_OVERLAY");
    char p[1100], line[256];
    snprintf(p, sizeof p, "%s/%s", ov ? ov : "", l->file);
    FILE *f = ov && *ov ? fopen(p, "r") : NULL;
    int cap = 0;
    while (f && fgets(line, sizeof line, f)) {
        if (!strchr(line, '\n') && !feof(f)) {  // (longer than the buffer: none of it is a name)
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '\n') {}
            continue;
        }
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (eq) *eq = 0;
        size_t len = strlen(line);
        if (!len || len >= NAME_LEN) continue;
        if (l->n == cap) {
            void *m = realloc(l->name, sizeof *l->name * (cap ? cap * 2 : 256));
            if (!m) break;
            l->name = m, cap = cap ? cap * 2 : 256;
        }
        memcpy(l->name[l->n++], line, len + 1);
    }
    if (f) fclose(f);
    if (l->n > 1) qsort(l->name, (size_t)l->n, NAME_LEN, name_cmp);
    if (l->n) LOG("[online] %d %s names allowed from the network\n", l->n, l->kind);
    else LOG("[online] no %s names (online_mod/%s): every %s name from the network refused\n", l->kind, l->file, l->kind);
}

// a name in a field of cap bytes as it came: on the list when it ends within them and the list has it
static bool name_listed(NameList *l, const char *name, size_t cap) {
    if (!l->read) names_read(l);
    const char *end = memchr(name, 0, cap);
    if (!end || end == name || end - name >= NAME_LEN || !l->n) return false;
    return bsearch(name, l->name, (size_t)l->n, NAME_LEN, name_cmp) != NULL;
}

// what a host's enemy holds (its EquipItem1 / EquipItem2: a weapon, a shield), on weapons.txt
bool weapon_name_listed(const char *name, size_t cap) { return name_listed(&g_weapon_names, name, cap); }

// loot: an item, a weapon or a rupee, each on its kind's list (anything else is on none)
bool loot_name_listed(const char *name, size_t cap) {
    return !strncmp(name, "Item_", 5)      ? name_listed(&g_item_names, name, cap)
         : !strncmp(name, "Weapon_", 7)    ? name_listed(&g_weapon_names, name, cap)
         : !strncmp(name, "PutRupee", 8)   ? name_listed(&g_rupee_names, name, cap)
                                           : false;
}
