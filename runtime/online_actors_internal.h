// Online play's actors, cut by module into runtime/oa_*.c (specs/decisiones/ADR-021-reparto-oa.md, ADR-023-modulos-oa.md): what
// the modules share. The game's 1.9.0 addresses (FN_*, P_*: all of them here, behind runtime/online_gate.h), the
// macros and types more than one module uses, and the state and functions a module keeps for the others. Only
// runtime/oa_*.c and runtime/online_actors.c (the lists of names from the network) include it.
#pragma once

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

// ---- oa_core.c

#define FN_CREATE 0x7001535900ull
#define ACTOR_HASH_ID 0x640  // (an actor's map object HashId: u32)
#define FN_SET_MTX 0x700152261cull
#define FN_ACTOR_WAKE 0x7001515988ull  // x0 an actor: woken (its state +0x64 to 1, its process manager told): take_out
#define FN_DELETE 0x7001514bacull
#define FN_AS_IN_SLOT 0x70014a0040ull  // x0 an AS manager, w1 slot, w2 bank -> the AS playing there (its name at +8;
                                       // the manager: slots at +0xc0, 0x50 each (+0x20 its banks), +0xb8 how many)
#define FN_PLAY_AS 0x700151e42cull  // an AI action's "play this AS" (x0 the action: its actor at +8; x1 the AS name, w2
                                    // not again if already playing, w3 the slot, w4 the bank, s0 the blend, -1 its
                                    // own); the AS manager is the actor's +0x560 (ForkASPlay's enter, 0x7000759e74)
#define FN_HIDE_GROUPS 0x700126ba4cull  // x1 a gsys model, x2 another (0: all), w3 visible: shows or hides x1's materials
                                       // in a group of x2's (the bfres' HideMaterialGroup: a material's groups, and
                                       // a model's, those it covers); the player's armour code (0x7001175324) calls it
                                       // for each piece's model (actor +0x4e0) with each other piece's
#define P_HIDE_GROUPS_X0 0x7001d66c98ull  // (its x0, unused: **this)
#define FN_FIND_BONE 0x7000f77260ull  // x0 a gsys model, x1 a SafeString -> the unit (low 16 bits, -1: none) and the
                                      // bone (high 16); unit->vtable[0x70](unit, Matrix34f *out, bone): its matrix in
                                      // the world (0x7000741454 reads "Weapon_L" so)
#define P_SAFE_STRING_VT 0x7001d66260ull  // (sead::SafeString's vtable, +0x10)
#define FN_REQUEST 0x7001535b10ull  // request an actor (its resources loaded first, on the game's own threads): x0 the
                                    // creator, x1 the name, x2 a heap, x3 a BaseProcHandle (unit, flag), x4 its params
                                    // or 0, x5 0, w6 1 -> true when asked; the player's armour code (0x70011750c8) then
#define FN_HANDLE_TAKE 0x7001516b74ull     // x0 the handle: its actor once its unit's state is 2 (ready), and wakes it
#define FN_HANDLE_RELEASE 0x70015168a8ull  // (a unit that failed, 3 or 5)
#define FN_ARMOR_BIND 0x700117363cull  // x0 an armour actor, x1 its wearer: the armour on his model's skeleton (+0x840)
#define FN_ARMOR_INIT 0x7001173550ull  // x0 the armour, once bound
#define P_CREATOR 0x7001d66550ull
#define P_HEAP_OWNER 0x7001d66558ull
#define ARMOR 3  // head, upper body, lower body
#define VALID(p) ((p) >= 0x7000000000ull && (p) < 0x7400000000ull)
// in the game's own image (code, vtables), not the heap (0x72...)
#define IN_IMAGE(p) ((p) >= 0x7000000000ull && (p) < 0x7004000000ull)
// a piece made by FN_REQUEST: asked for, then made once the game has loaded its resources
// An actor watched as the game's BaseProcLink does (0x7101517e60 sets one, 0x7101517e0c checks it): the actor's
// unit (+0x68) and id (+0x60); the unit holds the id (+0x40) and the actor (+0x48) while the actor lives. The game
// deletes actors of ours on its own (a real armour piece not the player's, when his equipment changes; the world's
// actors at a load): one that went is made again
typedef struct { uint64_t unit; uint32_t id; } Watch;
// A request's BaseProcHandle (the unit, a flag): the unit keeps the handle's address (+8) until the actor is taken
// or the handle released, so each request has one of its own, never moved, not used again for a long while (a ring)
typedef struct { uint64_t unit, flag; } Handle;
typedef struct {
    char name[48];  // the actor it is ("" none)
    uint64_t actor;
    Watch w;
    Handle *handle;  // (its request's, while pending)
    bool pending;
    int fails, wait;  // (wait: frames before asking again, after a failure)
} Piece;
#define MAGNE_BEAM 1
#define POOL 24  // (each player's pool of pieces: below)
// online_mod/armor.txt: each armour piece's online actor (OnlineArmor_<its model list>), "piece=actor" a line;
// online_mod/bones.txt: each of those actors' bone map, "actor=unit|unit..." (per unit of its model list, each bone's
// index in Link's skeleton, 255 for one Link hasn't: its cloth's, its support bones')
typedef struct { char key[48]; char *val; } MapLine;
// An InstParamPack (as 0x7000b1711c makes one to make an enemy with its weapon): +0 its vtable (the GOT's 0x7001d664b0
// + 0x10), +0x10 its buffer: s16 how many, s16 bytes used, then 0xc0 bytes of entries, each the key (a char *), its
// type (5 a string), its value (0x7001126168 adds one)
typedef struct { uint64_t vt, pad; int16_t n, used; uint8_t data[0xc0]; } InstParams;
// An actor's skeleton: the gsys model unit that has Link's bones (the one Skl_Root is in) and how many bones it has
// (its skeleton, unit +0x118: +0x18 the local matrices, 0x60 each; +0x50 the bone count)
#define FN_EFFECT_EMIT 0x7001100bd0ull  // x8 out {effect, id}, x0 an actor, x1 a name in its ELink user
#define FN_EFFECT_KILL 0x700056d010ull  // x0 such a handle: the effect ends
#define FN_XLINK_EMIT 0x700159f00cull  // x0 an actor, x1 an XLink keyword ("Disappear"), w2 2, x3 0x20 bytes of 0
#define FN_BONE_LOCAL_GET 0x7000f960f8ull  // x0 unit, x1 Matrix34f *out, x2 Vec3 *scale out, w3 bone
#define FN_BONE_LOCAL_SET 0x7000f95fa8ull  // x0 unit, x1 Matrix34f *, x2 Vec3 *scale, w3 bone
#define MAX_ACTORS 8192
#define LINK_CARRIED 6
#define FN_BODY_TO 0x70012d4e18ull       // x0 a rigid body, x1 a Matrix34f: its velocities set to reach it in a step
#define THEIRS ONLINE_LOOT

// ---- oa_gamedata.c

// An event started here, as an EventTag starts its own (1.9.0 0x70011662e0): its metadata (0x198 bytes: 0x7001112508
// makes it, 0x7001112794(meta, flow name, entry point name, "") names it, 0x70011128b4 frees it), then a request for
// the event manager (0x7001d66758's: 0x700110d590(mgr, request)): +0 a matrix (where), +0x30 0x101, +0x38 the
// metadata, +0x48 the actor it is for. true when the manager took it
#define FN_EVT_META_INIT 0x7001112508ull
#define FN_EVT_META_NAME 0x7001112794ull
#define FN_EVT_META_FREE 0x70011128b4ull
#define FN_EVT_CALL 0x700110d590ull
#define P_EVT_MGR 0x7001d66758ull
// A bool flag set as an event sets it (EventFlagONAction, 1.9.0 0x7001187758): its handle (0x7001143950(the
// manager's pending buffer, its name's crc32), the manager's +0xc28 in its top byte), then the manager's setter
// (0x7000730148(manager, value, handle)), which tells the rest of the game (the HUD wakes for the slate); a value
// written in memory alone is seen by nothing. The manager: 0x7001d66478's
#define FN_GDT_HANDLE 0x7001143950ull
#define FN_GDT_SET_BOOL 0x7000730148ull
#define P_GDT_MGR 0x7001d66478ull
// An item into the pouch, as an event's Demo_IncreasePorchItem does (EventIncreasePorchItemAction, 1.9.0 0x7001187e3c):
// 0x700120af80(name, count) whether it fits, then 0x700120af98(name, count) (x0 a sead::SafeString)
#define FN_POUCH_CAN_GET 0x700120af80ull
#define FN_POUCH_GET 0x700120af98ull
// a heart container, as a Divine Beast's gives it: the game's own for it (EventIncreasePorchItemAction,
// 0x7001187e3c, hands Obj_HeartUtuwa_A_01 / Obj_HeartKakera_A_01 to 0x7001211520, not to the pouch)
#define FN_GET_SPECIAL 0x7001211520ull  // x0 0, x1 a SafeString: the item's name

// ---- oa_sample.c

#define BONES (int)(sizeof oa_k_bones / sizeof *oa_k_bones)
// His Link's and his weapon's other effects (a spin attack's charge and its spin, a heavy blow on the ground, ...): each one
// as it is played goes to the others as an event (OnlinePose.fxev, the last ONLINE_FX_EVENTS by number), and they play
// it by its name on his puppet or the weapon in its hand (its ELink is the player's / the weapon's), ended after
// FX_EVENT_NS (one that loops would stay)
#define FX_EVENT_NS 2000000000ull
// The map the local Link is on: the game's own (0x7001208304: its type, 0x70012082f8: its name; each a SafeString,
// its chars at +8): "MainField/", "CDungeon/Dungeon000", "MainFieldDungeon/RemainsWind", ...
#define FN_MAP_TYPE 0x7001208304ull
#define FN_MAP_NAME 0x70012082f8ull

// ---- oa_puppet.c

// Explosions of the others' bombs: an OnlineBoom actor asked for at that place (its resources load meanwhile), then
// sent the Remote Bomb rune's "go off" message (0x800000a: 0x7000b3b66c sends it to each of its bombs) through a
// message transceiver of the local player's (his armour manager's, player +0x2500 +0xd8: the armour code sends its
// messages so); its own AI goes off (shock wave, effects, sound) and deletes it. Sent again while it is there
#define FN_SEND_MSG 0x700154c61cull  // x0 the sender's transceiver, x1 the receiver (actor +0x5f0), x2 the message, w4 1

// ---- oa_runes.c

// What the others hold with the Magnesis rune: the same object here (the one of that name nearest where it was when he
// caught it) where it is in his game, each frame while he holds it (then let go there: it falls as it does here), with
// the rune's glow on it ("MagneCatchMoving", the player's own effect: 0x7000b998fc plays it on the player with the
// held actor's place, effect +0x98 / +0x9a bit 8 and +0xbc..c4)
#define P_MAGNE 0x7001e15f18ull
#define FN_XLINK_EFFECT 0x7001395354ull  // x8 out {effect, id}, x0 an actor, x1 a keyword of its XLink, w2 0
#define P_MAGNE_FX 0x7001d66ab8ull       // (**this +0x30: the actor the rune's effects are played on)
#define FN_ACTOR_LOOK_ON 0x700152a08cull   // x0 an actor, w1 bits of its +0x710 to set (how it's drawn: 1 the
#define FN_ACTOR_LOOK_OFF 0x700152a0b4ull  // Magnesis rune's aim on it, 2 held by it: 0x7000b98c40), and to clear
#define FN_SPECIAL_FX_B 0x700124d220ull    // s0 a value, x1 a model: its materials' uk_special_effect_B
#define FN_MAGNE_BEAM_SET 0x7000d40bb8ull  // x0 a holder (+0x48 the effect's handle, +0x50 its id), x1 Matrix34f, s0 a length
#define P_MAGNE_AIM 0x7001e58720ull        // (*this + MAGNE_AIM_OFF: two Vector3f, the rune's aim when last raised,
#define MAGNE_AIM_OFF 0x192ccull           // the camera's forward; MagneConnect's strands follow it, zero: not drawn)
// the materials' uk_special_effect_A of a model's units (how the Stasis rune draws what it stopped: 0x700124ca00 sets
// it each frame, pulsing; 0 when it's over)
#define FN_SPECIAL_FX_A 0x700124ca00ull       // x1 a model, w2 0: its pulse now
#define FN_SPECIAL_FX_A_UNIT 0x700124ca7cull  // s0 the value, x0 a model unit

// ---- oa_enemies.c

// A name from the network that would make an actor here is one of a list first (specs/contratos/validacion-red.md
// V-11): one that isn't makes nothing, that entry alone. Each kind's refusals are counted, said at most once a second
typedef struct { uint64_t last; unsigned n; } Refused;
#define ENEMY_LIFE 0x848   // (an actor's life, s32: its General Life to start with; seen going down as it is hit)
// Each of the host's enemies is had for real by one game, its owner's: the player nearest it (another one only when
// he is clearly nearer, and not again for 3 s), whose game runs its AI (it sees him, chases him, is hit as any enemy);
// the others show it (OnlineEnemy_*). The owner's game makes it when it comes to have it (from the state it had),
// and sends its state to the host's game (ESTATE); the one that no longer has it deletes it. The enemies the host's
// game has itself are found there; their ids are everyone's
typedef struct {
    uint32_t gid; int state; uint64_t actor; Watch w; Piece made; InstParams params;
    bool died; float last_pos[3];  // (loot: died here, where it was)
    bool marked;  // (the host's game: its dead flag set, as the game sets it only for the map's own, not one made again)
    bool remade;  // (made by us from the list, not the map's own)
    uint64_t held[2]; Watch held_w[2];  // (loot: its weapon and shield while it lived: they fall when it dies)
    uint64_t guard_until; int32_t real_life;  // (just made here from another's: own_guard)
} Own;  // (state: OWN_*)
// (OWN_GONE: gone here, died or removed, and told to the host's game, whose list still has it as ours until that
// comes: neither made again nor shown as his until his list no longer has it ours)
enum { OWN_NONE, OWN_MAKING, OWN_HAVE, OWN_GONE };
#define OWNS 64
// The host's enemies hitting the local Link: his enemy starting an attack (an AS of its with Attack or Kick in its name)
// lands STRIKE_FRAMES later on the local Link when he is within its reach in front of it, his shield not up: his life
// (player +0x848, quarter hearts) goes down by its weapon's attack power, its own when it has none
#define STRIKE_FRAMES 10

// ---- oa_world.c

// A map object broken here for another (a tree cut, a crate that didn't break): taken out of the world as the game
// takes out what a blow broke (1.9.0 0x7001524598, mode 0: called so for each one at 0x7001492680) - its actor deleted
// (FN_DELETE) and its map placement told (0x7001526bd0)
// Before it, as a tree's own cut does (1.9.0 0x7000951ae8): its map object marked gone (0x7000bdc1f0: the actor's
// map object, +0x430 of its vtable, its flags +0xc | 0x80, x1 12 bytes for the place it copies, x2 the place) - else
// the game drew the tree again from its placement (the model it shows for trees far away), standing beside its stump
#define FN_TAKE_OUT 0x7001524598ull
#define FN_MAPOBJ_GONE 0x7000bdc1f0ull

// ---- oa_flags.c

// (set as the game sets it: its game data value WM_BloodyMoonTimer (the time manager's handle to it at +0x120), with
// the game data manager's f32 setter (1.9.0 0x7000b36c80: x0 the manager, w1 the handle, s0 the value; the time
// manager writes it there each frame and takes it back from there), and its own copy; a write to the copy alone
// was undone the same frame)
#define FN_GDT_SET_F32 0x7000b36c80ull
// A Korok another player found, when its Korok is here (Npc_HiddenKorokGround / Fly within 50 m of this Link): its
// cutscene as the finder sees it (Demo017_0<HiddenKorok_Ground / _Air> with the Korok: 0x70008f682c starts it so),
// which makes it appear, gives the seed and sets its flag; false when it isn't here or an event plays
#define P_KOROK_EVT_MGR 0x7001e3a638ull  // (the event manager, as 0x70008f682c reads it)
#define FN_EVT_START 0x700110d500ull     // x0 it, x1 an event's metadata, x2 its actor, x3 0 -> started

// ---- oa_rules.c

enum { UI_PAUSE_LOAD = 0x200 };  // (the UI's pause state while a load runs: the world waits for it)

// ---- The types of state shared that were anonymous: the variable is in its module (oa_check.py --modulos
// puts the type back on it)

// A remote player's puppet here: his Link and its pieces (the table, g_pup, is oa_puppet's; oa_puppet_frame and
// rt_online_frame's steps 3 and 4 work on an entry through oa_pup. His runes: oa_runes' g_rune, g_ice)
typedef struct {
    uint64_t link;  // OnlinePlayer; 0: none
    Watch link_w;
    int link_fails;
    Piece armor[ARMOR], gear[ONLINE_GEAR];
    Piece horse;  // (the horse he rides: OnlineHorse_*, playing his horse's AS)
    Piece tack[2];  // (its saddle and reins)
    char horse_as[32];
    Piece shot[ONLINE_SHOTS];      // what he holds, threw or shot (bombs, arrows: OnlineShots)
    Piece held[ONLINE_HELD];       // the materials he holds over his head or threw (OnlinePose.held: OnlineGear_*)
    uint16_t shot_id[ONLINE_SHOTS];  // (its id; 0: unused, out of sight)
    Piece nock;                    // the arrow on his bow's string while he draws it (it's no actor in his game)
    struct { uint64_t p; uint32_t id, pad; } kago[2];  // Daruk's Protection on his Link: its effects' handles
    struct { uint64_t p; uint32_t id, pad; } gale;  // Revali's Gale on his Link: its charge's effect handle
    struct { uint64_t p; uint32_t id, pad; } trail;  // his weapon's swing trail on it (OnlinePose.fx), its handle
    char trail_name[ONLINE_FX_MAX];
    int8_t dye[3];  // (the dyes his armour pieces here were made with)
    uint16_t fxev_seen;  // (his last effect played here, by its number)
    struct { uint64_t p; uint32_t id, pad; uint64_t t; char name[24]; } fxev[ONLINE_FX_EVENTS];  // (those playing)
    int kago_n;                    // (the ring's charges it was made for)
    uint16_t boom_seen;              // (the last explosion played)
    char name[ONLINE_NAME_MAX];
    int anim;                                     // the one-clip AS played (no pose: ONLINE_A_*; -1 none yet)
    char as[ONLINE_AS_CHANNELS][ONLINE_AS_MAX];   // the player's AS played on each channel
    int covered;                                  // the pieces whose cover is applied (a bit each; -1: none yet)
    int frames;                                   // frames since his Link was made
    bool skeleton;                                // his skeleton has come: his Link is posed by it, plays no AS
} OaPup;

typedef struct { uint16_t seq; uint8_t target; int16_t damage; } OaPvp;

typedef struct { uint32_t gid; Piece p; bool placed, gone; float pos[3]; int tries; uint64_t retry_t, made_t; uint8_t origin; } OaTheirs;

typedef struct { float pos[3]; int from; bool fresh; } OaBlast;

typedef struct { uint64_t actor; int16_t anim[3]; float frame[3]; } OaBlur;

// ---- Elsewhere in the runtime (each also declared where it was, in its module)
bool framepace_title_ui(void);
bool framepace_paused(void);
float test_warp_hour(bool *held);
void test_warp_set_hour(float hour);
uint64_t test_warp_gdt_mgr(void);

// ---- What a module keeps or does for the others, by module

// runtime/online_actors.c
bool oa_weapon_name_listed(const char *name, size_t cap);
bool oa_loot_name_listed(const char *name, size_t cap);

// runtime/oa_core.c
bool oa_has_body(uint64_t a);
Watch oa_watch(uint64_t actor);
bool oa_alive(uint64_t actor, Watch w);
bool oa_alive_rewatch(uint64_t actor, Watch *w);
Handle *oa_new_handle(void);
extern bool oa_g_local_dead;
extern uint32_t oa_g_aggro_id;
extern uint16_t oa_g_aggro_seq;
extern OnlineObj oa_g_objs_out[ONLINE_OBJS];
extern uint8_t oa_g_objs_out_trunk;
extern OnlineMover oa_g_movers_out[ONLINE_MOVERS];
extern uint32_t oa_g_broken[8], oa_g_trunks_gone[4];
extern volatile bool oa_g_online_actor[ONLINE_MAX];
const char *oa_actor_name(void);
bool oa_is_online_player(void);
bool online_actors_on(void);
uint64_t oa_call_f(Cpu *c, uint64_t fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, float s0);
void oa_call_sret(Cpu *c, uint64_t fn, uint64_t out, uint64_t a0, uint64_t a1);
uint64_t oa_call(Cpu *c, uint64_t fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
const char *oa_playing(Cpu *c, uint64_t actor, int slot, int bank);
bool oa_play_as(Cpu *c, uint64_t actor, const char *name, int slot, int bank, float blend, bool again);
extern Piece oa_g_pool[ONLINE_MAX][POOL];
void oa_orphan(Piece *p);
void oa_orphans_update(Cpu *c);
void oa_delete_piece(Cpu *c, Piece *p);
void oa_model_visible(Cpu *c, uint64_t actor, bool visible);
extern float oa_g_pool_at[3];
void oa_pool_update(Cpu *c);
void oa_drop(Cpu *c, int i, bool left);
const char *oa_gear_actor(const char *piece);
int oa_map_load(const char *file, MapLine **out);
const char *oa_map_find(MapLine *m, int n, const char *key);
const char *oa_armor_actor(const char *piece);
const uint8_t *oa_armor_bones(const char *actor, int u, int *len);
uint64_t oa_create(Cpu *c, const char *name);
bool oa_request(Cpu *c, const char *name, Handle *handle);
bool oa_request_p(Cpu *c, const char *name, Handle *handle, uint64_t params);
void oa_inst_params_s32(InstParams *p, const char *key, int32_t value);
void oa_inst_params_string(InstParams *p, const char *key, const char *value);
extern float oa_g_dist;
extern OaPvp oa_g_pvp;
extern InstParams *oa_g_piece_params;
bool oa_piece_update(Cpu *c, int i, Piece *p, const char *want, const char *who);
bool oa_running(uint64_t actor);
uint64_t oa_skel_unit(Cpu *c, uint64_t actor, int *bones);
uint64_t oa_linked(uint64_t at);
void oa_equipment(uint64_t player, uint64_t armor[ARMOR], uint64_t gear[ONLINE_GEAR]);
void oa_actor_name_of(uint64_t a, char out[48]);
uint64_t oa_first_unit(uint64_t actor, int *bones);
bool oa_bone_mtx(Cpu *c, uint64_t actor, const char *bone, float out[12]);
void oa_mtx_mul(const float a[12], const float b[12], float out[12]);
void oa_mtx_inv(const float a[12], float out[12]);
extern uint64_t oa_g_actors[MAX_ACTORS];
void oa_mtx_to_quat(const float m[12], float q[4]);
void oa_quat_to_mtx(const float q[4], float m[12]);
int oa_all_actors(uint64_t from, uint64_t *out, int max);
extern OaTheirs oa_g_theirs[THEIRS];

// runtime/oa_gamedata.c
uint32_t oa_crc32_str(const char *s);
uint32_t *oa_bool_flag(uint32_t h);
uint32_t *oa_gdt_value(int off, uint32_t h);
bool oa_start_event(Cpu *c, uint64_t actor, const char *flow, const char *entry);
void oa_give_item(Cpu *c, const char *name, int count);
void oa_give_heart(Cpu *c);
bool oa_set_bool_flag(Cpu *c, uint32_t h);

// runtime/oa_sample.c
extern const int oa_k_channel[ONLINE_AS_CHANNELS][2];
extern const char *const oa_k_bones[8];
extern const char *const oa_k_shot_names[ONLINE_K_COUNT];
extern uint64_t oa_g_fx_trail_t;
extern uint64_t volatile oa_g_rf, oa_g_rf_ui;
extern Watch oa_g_rf_watch;
uint64_t oa_slink_ui(uint64_t actor);
uint64_t oa_elink_ui(uint64_t actor);
void oa_fx_update(uint64_t player);
uint8_t oa_elem_near(const float p[3], float r);
extern char oa_g_map[32];
bool oa_same_map(const OnlinePlayer *p);
void oa_sample_pose(Cpu *c, uint64_t pinfo);
void oa_sample_skeleton(Cpu *c, uint64_t pinfo);
void oa_sample_frame(Cpu *c, uint64_t pinfo);
void oa_sample_why_frame(void);

// runtime/oa_puppet.c
uint64_t oa_link_live(int i);
extern OaBlast oa_g_blast[4];
void oa_cover(Cpu *c, int i);
extern OaBlur volatile oa_g_blur[ONLINE_MAX];
void oa_boom_start(Cpu *c, const char *name, const float pos[3]);
void oa_booms_forget(void);
void oa_booms_update(Cpu *c, uint64_t player);
void oa_pose_link(Cpu *c, int i, const OnlinePlayer *p);
void oa_pose_armor(Cpu *c, const Piece *p);
OaPup *oa_pup(int i);
uint64_t oa_puppet_link(int i);
const char *oa_puppet_name(int i);
void oa_puppet_pieces(Cpu *c, int i, void (*each)(Cpu *c, int i, Piece *p));
void oa_puppet_drop(Cpu *c, int i);
void oa_puppet_forget(void);
bool oa_puppets_validate(void);
bool oa_puppet_frame(Cpu *c, const float link[3], int me);

// runtime/oa_runes.c
uint64_t oa_magne_held(void);
void oa_stop_update(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me);
void oa_ice_update(Cpu *c, const OnlinePlayer *pl, int me);
void oa_grab_update(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me);
uint64_t oa_rune_grab(int i);
uint64_t oa_rune_released(int i);
uint64_t oa_rune_released_t(int i);
bool oa_rune_holds(uint64_t a);
bool oa_rune_stopped(uint64_t a);
bool oa_rune_ice(uint64_t a);
void oa_runes_pieces(Cpu *c, int i, void (*each)(Cpu *c, int i, Piece *p));
void oa_runes_drop(int i);
void oa_runes_forget(void);

// runtime/oa_enemies.c
extern Refused oa_g_refused_enemy, oa_g_refused_weapon, oa_g_refused_loot;
void oa_name_refused(Refused *r, const char *what, const char *name);
void oa_enemy_state(Cpu *c, uint64_t a, OnlineEnemy *e);
extern Own oa_g_own[OWNS];
void oa_owns_left(Cpu *c);
void oa_own_guard(uint64_t player);
extern OnlineEnemy oa_g_reg_out[ONLINE_ENEMIES];
extern int oa_g_reg_n;
void oa_owns_forget(void);
void oa_enemies_host(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me);
int oa_attack_power(const char *name);
int oa_our_defense(uint64_t player);
void oa_strike(Cpu *c, uint64_t player, const OnlineEnemy *e);
void oa_puppets_forget(void);
void oa_enemies_client(Cpu *c, uint64_t player, int me);
void oa_enemies_show(Cpu *c, uint64_t player, OnlineEnemy *e, int n, bool fresh, const float centre[3], int me, bool suppress);
void oa_boss_sample(Cpu *c, uint64_t player, OnlinePose *pose);
void oa_boss_update(Cpu *c, const OnlinePlayer *pl, int me);
void oa_enemies_frame(Cpu *c, uint64_t player, const float link[3], int me);

// runtime/oa_loot.c
bool oa_players_own_item(const char *n);
extern uint64_t oa_g_carried_now[LINK_CARRIED];
void oa_drop_hush(const float p[3]);
void oa_loot_forget(void);
void oa_loot_mine_add(uint64_t a, const char *n, int me);
void oa_loot_mine_thrown(uint64_t a, const char *n);
void oa_link_carried(uint64_t player, uint64_t out[LINK_CARRIED]);
bool oa_carries(const uint64_t carried[LINK_CARRIED], uint64_t a);
void oa_loot_update(Cpu *c, uint64_t player, const float link[3], int me, bool host);
void oa_ground_update(Cpu *c, uint64_t player, const float link[3]);

// runtime/oa_world.c
void oa_objs_update(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me);
void oa_movers_update(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me);
void oa_movers_forget(void);
void oa_breaks_update(Cpu *c, uint64_t player, const float link[3], const OnlinePlayer *pl, int me);
void oa_world_frame(Cpu *c, uint64_t player, const float link[3], int me);

// runtime/oa_flags.c
uint8_t oa_weather_now(void);
float *oa_bloody_moon_timer(void);
void oa_bloody_moon_follow(Cpu *c, const OnlinePlayer *pl, int me);
void oa_weather_follow(const OnlinePlayer *pl, int me, const float at[3]);
bool oa_slate_event(Cpu *c, uint64_t player);
extern volatile int oa_g_fake_blood_moon;
void oa_quests_update(Cpu *c, uint64_t player, bool host);
void oa_flags_hour_frame(void);

// runtime/oa_rules.c
extern volatile int oa_g_nopause_now;
extern volatile int oa_g_nopause_frames;
void oa_own_strikes_in_menu(Cpu *c, uint64_t player);
extern volatile int oa_g_nopause_rule;
void oa_players_collide(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me);
void oa_pvp_update(Cpu *c, uint64_t player, const OnlinePlayer *pl, int me);
extern volatile int g_online_block_a;
extern volatile uint64_t g_game_buttons;
void oa_rules_nopause_frame(Cpu *c, int me);
void oa_rules_block_a_frame(const float link[3], int me);

// runtime/oa_control.c
void oa_flag_query(Cpu *c);
void oa_event_query(Cpu *c, uint64_t pinfo);
void oa_find_actors(uint64_t pinfo);
void oa_control_frame(Cpu *c, uint64_t pinfo);
void oa_control_tests_frame(Cpu *c, uint64_t pinfo, const float link[3]);
