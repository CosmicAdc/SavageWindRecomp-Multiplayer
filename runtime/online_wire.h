// The wire format of runtime/online.c, in one place: runtime/online.c and tools/online_bot.c both include it.
// Messages are u16 length, u8 type, payload (online.c). A change here is a change of PROTOCOL.
#pragma once

#include "online.h"

#define MAGIC 0x4c4f5753u  // "SWOL"
#define PROTOCOL 38  // (38: the materials he holds or threw; 37: a cut tree's trunk among the objects moving near
                    // him; 36: STATE carries the level's mechanisms moving near him; 35: his rules, Blood Moon timer, last
                    // hit enemy, the objects moving near him)
enum { M_HELLO = 1, M_WELCOME, M_REJECT, M_STATE, M_JOIN, M_LEAVE, M_CHAT, M_PING, M_PONG, M_EQUIP, M_SKEL, M_TIME, M_ENEMIES, M_HIT, M_ESTATE, M_LOOT_ADD, M_LOOT_TAKE, M_LOOTS, M_QUEST, M_QUESTS, M_PICK };
// SKEL: u8 Link's bone count, u8 his bow's, then each bone's rotation (smallest three of its quaternion: 2 bits which
// is dropped, 3 x 15 bits; 6 bytes) and translation (3 x s16, 1/8192 m); 20 times a second
#define SKEL_BONE_BYTES 12
#define SKEL_BYTES (2 + ONLINE_BONES * SKEL_BONE_BYTES)
enum { R_FULL = 1, R_VERSION, R_PASSWORD, R_NAME };

#pragma pack(push, 1)
typedef struct {
    float pos[3], yaw;
    uint8_t flags;
    uint16_t ping_ms;
    uint8_t anim;
    char as[ONLINE_AS_CHANNELS][ONLINE_AS_MAX];  // OnlinePose ("" all: none; the others go by anim)
    uint8_t gear_bone[ONLINE_GEAR];
    float gear_rel[ONLINE_GEAR][12];
    OnlineShots shots;
    uint8_t effects;  // OnlinePose.effects
    OnlineGrab grab;  // OnlinePose.grab
    OnlineGrab stop;  // OnlinePose.stop
    uint8_t ice_n;    // OnlinePose.ice
    float ice[3][12];
    char map[32];     // OnlinePose.map
    struct { uint32_t hash; char name[28]; int32_t life; } boss[ONLINE_BOSSES];  // OnlinePose.boss
    char fx[ONLINE_FX_MAX];  // OnlinePose.fx
    int16_t fx_anim[3];      // OnlinePose.fx_anim
    float fx_frame[3];       // OnlinePose.fx_frame
    OnlineFxEvent fxev[ONLINE_FX_EVENTS];  // OnlinePose.fxev
    uint32_t fx_props[ONLINE_FX_PROPS];    // OnlinePose.fx_props
    uint8_t weather;                       // OnlinePose.weather
    int8_t dye[3];                         // OnlinePose.dye
    uint16_t pvp_seq;                      // OnlinePose.pvp_*
    uint8_t pvp_target;
    int16_t pvp_damage;
    char horse[24];                        // OnlinePose.horse*
    float horse_m[12];
    char horse_as[32];
    char tack[2][24];                      // OnlinePose.tack*
    float tack_m[2][12];
    uint32_t ground_props[16];             // OnlinePose.ground_props
    uint8_t collide;                       // OnlinePose.collide
    uint8_t revive;                        // OnlinePose.revive
    uint8_t nopause;                       // OnlinePose.nopause
    float bloody_moon;                     // OnlinePose.bloody_moon
    uint32_t aggro_id;                     // OnlinePose.aggro_*
    uint16_t aggro_seq;
    OnlineObj objs[ONLINE_OBJS];           // OnlinePose.objs
    uint32_t broken[8];                    // OnlinePose.broken
    uint32_t trunks_gone[4];               // OnlinePose.trunks_gone
    OnlineMover movers[ONLINE_MOVERS];     // OnlinePose.movers
    uint8_t objs_trunk;                    // OnlinePose.objs_trunk
    OnlineHeld held[ONLINE_HELD];          // OnlinePose.held
} WireState;
typedef struct { uint32_t magic; uint16_t version; char name[ONLINE_NAME_MAX], pass[ONLINE_NAME_MAX]; } WireHello;
typedef struct { char armor[3][48], gear[ONLINE_GEAR][48]; } WireEquip;  // EQUIP: what a player wears (head,
                                                                          // upper, lower) and carries (actor names)
#pragma pack(pop)

_Static_assert(sizeof(WireHello) == 54, "WireHello");
_Static_assert(sizeof(WireState) == 3375, "WireState");
_Static_assert(sizeof(WireEquip) == 432, "WireEquip");
_Static_assert(sizeof(OnlineEnemy) == 311, "OnlineEnemy");
_Static_assert(sizeof(OnlineLoot) == 97, "OnlineLoot");
