// Reward policy and delivery ledger for co-op (runtime/coop_rewards.c): which flags the world sets that hand the
// player something (the slate, a champion's ability, a spirit orb, a heart container, a Korok seed...), whether
// each one is handed out at a given moment, and a ledger of the ones already handed out. The table is the owner's,
// as plan.md section D writes it (ADR-007, decisions 2 and 5; ADR-008, decisions 13, 14 and 19; ADR-011, decision
// 30) and is not decided here. Pure logic: no game memory, no network, no clock and no random numbers, so that
// tests/test_coop_rewards.c can test it alone. The integration (quests_update) asks coop_reward_decide and the
// ledger before every give_item / give_heart.
//
// Rupees are not a class: they are each player's own (ADR-008, decision 19) and no flag hands them out.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RW_NONE,           // a flag that hands out nothing (or NULL)
    RW_WORLD_ABILITY,  // the slate, its runes and upgrades, the paraglider: without them the host cannot be followed
    RW_CHAMPION,       // a champion's ability (IsGet_Obj_HeroSoul_*)
    RW_MILESTONE,      // a spirit orb (Clear_Dungeon*) or a heart container (Get_HeartUtuwa_*)
    RW_COLLECTIBLE,    // a Korok seed
    RW_CHEST,          // what a chest held (*_TBox_*): belongs to whoever opens it
} RewardClass;

// When the flag is seen
typedef enum {
    RW_AT_SESSION,     // it comes while both play together
    RW_AT_FIRST_JOIN,  // it is in the first message of a guest whose co-op profile is new (its ledger has no
                       // "caught up" mark yet): the guest gets the host's hearts and stamina matched instead (T442)
    RW_AT_REJOIN,      // it is in the first message of a guest who comes back to a world that went on without him
} RewardMoment;

// The class of a flag by its name (the world's flag names: "IsGet_PortableUnit", "Clear_Dungeon012"...). NULL, "" and
// names of no class: RW_NONE. Prefix and exact names, case-sensitive, as the game writes them.
RewardClass coop_reward_class(const char *flag);

// Whether a flag of class c is handed out at moment m. Not the ledger: a flag already delivered is asked of
// coop_ledger_has apart. The table (give: yes, no):
//                   session  first join  rejoin
//   ability           yes       yes        yes
//   champion          yes       yes        yes
//   milestone         yes       no         yes   (a first join is matched instead; P4-11: a rejoin gets what it missed)
//   collectible       yes       no         no    (one seed for each, during the session; P4-11 does not name it)
//   chest             no        no         no    (it is whoever opens it, and may drop it marked)
//   none              no        no         no
bool coop_reward_decide(RewardClass c, RewardMoment m);

// The hash the ledger keeps for a flag: its CRC-32 (the one the game keys its flags by). NULL: 0.
uint32_t coop_reward_hash(const char *flag);

// ---- The ledger: one file per co-op profile (coop-ledger.txt), one line per delivered flag hash (8 hexadecimal
// digits) and the line "caught-up" once the guest has been matched to the host. One ledger is open at a time; the
// integration calls coop_ledger_close from RT_ON_RESET.

#define COOP_LEDGER_MAX 65536  // entries kept; a ledger past this refuses more (coop_ledger_add: false)

// Opens the ledger at path and reads it. A missing file is a new, empty ledger (true; the file is made by the first
// add). A line that is not a hash, "caught-up" or empty is ignored, and at most COOP_LEDGER_MAX entries are read:
// either one makes it false, with the ledger still open and usable with what could be read. false too (and no
// ledger open) for a NULL or empty path, or one that does not fit. Replaces any ledger that was open.
bool coop_ledger_load(const char *path);

// Whether hash was added. Nothing open: false.
bool coop_ledger_has(uint32_t hash);

// Adds hash and writes its line at once (append, then flush). Already there: true, nothing written. false: no
// ledger open, it is full, or the line could not be written (then it is not added either).
bool coop_ledger_add(uint32_t hash);

// The mark that this profile has been matched to the host (T442), which tells a first join from a later one. It
// survives a reload. Nothing open: false and set refuses.
bool coop_ledger_caught_up(void);
bool coop_ledger_set_caught_up(void);

// Entries in the open ledger (0 if none)
int coop_ledger_count(void);

// Forgets the open ledger (the file stays as it is)
void coop_ledger_close(void);

#ifdef __cplusplus
}
#endif
