# ADR-022 · `online_actors.c` cortado en módulos `oa_*.c`, su cabecera interna y sus nombres (T207)

- Estado: propuesta del agente de T207 (pendiente de revisión del orquestador)
- Tarea: T207 de `fases/F2-cimientos/tasks.md`; sigue a [ADR-021](ADR-021-reparto-oa.md) (el reparto de T206)
- "Antes": `3370d18:runtime/online_actors.c` (6 632 líneas, agrupado por T206)

## Contexto

T206 dejó `online_actors.c` agrupado por módulos con separadores `// ==== oa_<módulo> ====`. T207 corta cada grupo a su
fichero y crea `runtime/online_actors_internal.h` con lo que comparten. `plan.md` §3.3 admite solo: quitar `static` a lo
que cruza de módulo y declararlo en la cabecera, añadir `#include`, y declarar `extern` el estado compartido en su
módulo dueño; el renombrado con `oa_` va en un commit aparte.

## Decisión

### Ficheros

- `runtime/oa_<módulo>.c`, uno por separador de T206 (core, gamedata, sample, puppet, runes, enemies, loot, world,
  flags, rules, control, frame), con el texto de su grupo en el mismo orden, sin los separadores. Cada uno empieza con
  un comentario de qué tiene y `#include "online_actors_internal.h"`. El bloque `// ==== prototipos ====` de T206 va con
  `oa_core` (sus tres funciones cruzan de módulo: quedan declaradas en la cabecera).
- `runtime/online_actors.c` **no se elimina** (decisión pedida en la sesión de T207 y respondida: "online_actors.c solo
  nombres"): queda con lo que T206 dejó delante de `#ifndef ONLINE_NAMES_ONLY` (comentario del fichero, `#include` y
  las listas de nombres de red, V-11), más `#include "online_actors_internal.h"`. Motivo: `tools/test_online_names.c`
  hace `#include "../runtime/online_actors.c"` y la regla `$(B)/test_online_names` del `Makefile` (regla existente,
  que las reglas de F2 no dejan editar) depende de ese fichero. El `#ifndef ONLINE_NAMES_ONLY` y su `#endif` se quitan
  (ya no guardan nada); el test sigue definiendo la macro, sin efecto.
- `runtime/online_actors_internal.h` (`#pragma once`).

### Qué va a la cabecera

1. Los `#include` del fichero original.
2. **Todas** las direcciones del juego `FN_*` / `P_*` (52; constitución, regla 13: "agrupadas en un único módulo"),
   cada una con su comentario pegado, que viaja con ella sin cambiar. Cuatro de esos comentarios explican sobre todo el
   código que las usa y ahora quedan lejos de él: `FN_SEND_MSG` (las explosiones de `oa_puppet`), `FN_GDT_SET_F32`
   (`bloody_moon_set`, `oa_flags`), `FN_GET_SPECIAL` (`give_heart`, `oa_gamedata`) y `FN_MAP_TYPE` (`local_map`,
   `oa_sample`). En la cabecera van bajo `// ---- oa_<módulo>.c`, el módulo que las usaba.
3. Las macros y los tipos que usa más de un módulo, con su comentario: `ACTOR_HASH_ID`, `ARMOR`, `VALID`, `Watch`,
   `Handle`, `Piece`, `MAGNE_BEAM`, `POOL`, `MapLine`, `InstParams`, `MAX_ACTORS`, `LINK_CARRIED`, `THEIRS` (core);
   `BONES`, `FX_EVENT_NS` (sample); `Refused`, `ENEMY_LIFE`, `Own`, `OWN_*`, `OWNS`, `STRIKE_FRAMES` (enemies);
   `UI_PAUSE_LOAD` (rules). Además, sin cruzar: `IN_IMAGE` (va con `VALID`, y `tasks.md` la nombra) y `MAGNE_AIM_OFF`
   (su comentario es la segunda línea del de `P_MAGNE_AIM`). El resto de macros y tipos sigue en su módulo.
4. **Cinco tipos con nombre nuevo** para las variables compartidas de `struct` anónimo, que no se pueden declarar
   `extern` sin nombrar su tipo: `OaAct` (`g_act`), `OaPvp` (`g_pvp`), `OaTheirs` (`g_theirs`), `OaBlast` (`g_blast`),
   `OaBlur` (`g_blur`). El cuerpo del `struct` pasa tal cual a `typedef struct {...} OaX;` en la cabecera, y la
   variable se queda en su módulo con su comentario: `static struct {...} g_x[...]` → `OaX g_x[...]`. Es el único
   cambio de texto aparte de `static`; `oa_check.py --modulos` vuelve a poner el `struct` en la variable para comparar.
   `g_act` no se parte (es T208).
5. `extern` de cada variable y prototipo de cada función que cruza de módulo, agrupados por el módulo que la define,
   generados de su definición. `k_bones` se declara con su tamaño (`k_bones[8]`): `BONES` es un `sizeof` y lo usa
   `oa_frame`.
6. Las declaraciones de funciones de otros ficheros del runtime que el original ya tenía (`framepace_title_ui`,
   `framepace_paused`, `test_warp_hour`, `test_warp_set_hour`, `test_warp_gdt_mgr`): repetidas en la cabecera porque las
   usa otro módulo; la original se queda en `oa_core` (es texto comparado).

### Lo que cruza de módulo

145 funciones y variables pierden `static` y pasan a exportarse: 144 sin prefijo (entre ellas `weapon_name_listed` y
`loot_name_listed`, de `online_actors.c`, que usan `oa_enemies` y `oa_loot`) y `online_actors_on`, que ya empieza por
`online_`. Se encontraron con los usos de nombres entre grupos (la heurística de `oa_check.py --usos`) y se
confirmaron compilando cada objeto suelto con `clang -c` (cero errores, cero avisos nuevos) y con `nm`: cada símbolo
exportado nuevo es indefinido (`U`) en al menos otro objeto, salvo tres que van en la misma declaración que uno que sí
cruza y que no se separan para no cambiar el texto: `g_rf_ui` (con `g_rf`), `g_refused_enemy` y `g_refused_weapon`
(con `g_refused_loot`).

Ya eran globales antes de T207: `g_online_actor`, `g_online_block_a` y `g_game_buttons`. Los dos últimos los declara
`runtime/hle/input.c` (`extern`), fuera de los ficheros de la tarea: **conservan su nombre** (excepción a "ningún
símbolo exportado sin prefijo"). `g_online_actor` solo lo usan `oa_core` y `oa_frame`: se renombra.

Los prototipos `static` sueltos que ya no hacen falta (su función cruza de módulo o está definida más arriba) se
quitan; los que hacen falta se conservan donde estaban, y se añade uno: `static void animate(...)` al principio de
`oa_puppet.c` (estaba en `oa_core`, que ya no la ve). Los prototipos con comentario se conservan (son texto
comparado), sin `static` cuando su función cruza.

### Comprobación (`oa_check.py --modulos`)

Une la cabecera, `online_actors.c` y los `oa_*.c` y compara su multiconjunto de elementos con el del "antes", texto
exacto, con solo estas diferencias admitidas: `static` quitado (nunca añadido) al principio de una definición; en la
cabecera se ignoran sus comentarios, `#include`, `#pragma`, prototipos y `extern`; en cada `.c`, su comentario inicial y
el `#include` de la cabecera; los prototipos `static` sueltos; el `#ifndef ONLINE_NAMES_ONLY` / `#endif` del antes; y
los cinco `typedef` del punto 4 (se vuelven a poner en su variable). Además: `online_actors.c` es lo de antes del
`#ifndef` en el mismo orden; ningún fichero cortado tiene `#if`; y, por unidad de compilación (cabecera + cada `.c`),
cada elemento expande las mismas macros del fichero que en el "antes" (las que nombra definidas antes de él, y las que
nombran esas al expandirse). `--autoprueba` lo prueba con un corte sintético (bien → 0; una línea cambiada, un
`static` añadido, un tipo anónimo con otro cuerpo, una macro que otro módulo deja de ver, `online_actors.c` en otro
orden, un `#if`, un módulo que falta → 1).

    PY=tools/venv/Scripts/python.exe
    $PY tools/coop/oa_check.py --autoprueba
    $PY tools/coop/oa_check.py --antes 3370d18:runtime/online_actors.c --modulos "runtime/oa_*.c"

## Consecuencias

- El ejecutable exporta 145 nombres más. Hasta el commit de nombres, uno sin prefijo puede chocar con otro del runtime
  al enlazar (no se ha enlazado: lo hace el orquestador). Una búsqueda con `grep` de definiciones globales con esos
  nombres en `runtime/` no encontró ninguna [sin verificar enlazando].
- Una variable que antes era `static` ahora se lee con una indirección (`.refptr` en Windows): a `-O0 -fno-inline`,
  205 funciones de 205 siguen existiendo y 195 miden lo mismo que antes; las 10 que no (`break_here`, `breaks_update`,
  `cmd_bloodmoon`, `enemies_host`, `enemies_show`, `objs_update`, `pvp_update`, `rt_online_frame`, `sample_pose`,
  `world_rotate`) leen o escriben variables que pasaron a `extern`. Con `-O2` cambian además las decisiones de
  integrar en línea: no hay igualdad bit a bit (`plan.md` §3.4).
- Los comentarios de otros ficheros que citan `runtime/online_actors.c` (por ejemplo `runtime/online.h`,
  `tools/patches.py`, `docs/ONLINE.md`) no se tocan aquí: quedan fuera de los ficheros de T207.

## Líneas por fichero (commit "cortar")

| Fichero | Líneas |
|---|---|
| `oa_core.c` | 645 |
| `oa_gamedata.c` | 90 |
| `oa_sample.c` | 868 |
| `oa_puppet.c` | 208 |
| `oa_runes.c` | 379 |
| `oa_enemies.c` | 1 139 |
| `oa_loot.c` | 546 |
| `oa_world.c` | 945 |
| `oa_flags.c` | 312 |
| `oa_rules.c` | 225 |
| `oa_control.c` | 166 |
| `oa_frame.c` | 777 |
| `online_actors.c` (nombres) | 94 |
| `online_actors_internal.h` | 452 |

## Nombres (commit "prefijos")

Todo nombre que quedó exportado sin empezar por `rt_`, `online_` u `oa_` lleva delante `oa_` (`call` → `oa_call`,
`g_act` → `oa_g_act`): las 144 definiciones que perdieron `static` en el commit "cortar" y `g_online_actor`, que ya era
global (solo la usan `oa_core` y `oa_frame`). No cambian `g_online_block_a` ni `g_game_buttons` (los declara
`runtime/hle/input.c`, fuera de esta tarea), ni `online_actors_on` (ya tiene prefijo), ni las macros y tipos (no son
símbolos).

La sustitución es mecánica, `oa_check.py --renombrar` (la misma función que usa `--nombres` al comparar): palabra
completa, solo en el código; no en comentarios, cadenas ni caracteres, ni tras `.` o `->`, ni dentro del cuerpo de un
`struct`/`union`. Así no cambian el texto de los mensajes del log (`LOG(...)`: la firma de T201 no se ve afectada), los
comentarios que nombran una función, ni el campo `int strike;` del `struct` de `g_puppet`, que se llama como la función
`strike`. Ningún `oa_<nombre>` existía antes, y ningún nombre de la tabla es una variable local ni un parámetro
(búsqueda en las firmas y cuerpos, y `clang -Wshadow` sin avisos de una local que tape una global). Se aplica a
`runtime/oa_*.c`, `runtime/online_actors.c`, `runtime/online_actors_internal.h` y a `tools/test_online_names.c`, que
llama a las dos funciones de nombres (`weapon_name_listed`, `loot_name_listed`: cinco llamadas), aprobado en la
sesión de T207. Tras el renombrado, los objetos solo exportan nombres con `rt_`, `online_` u `oa_`, más los dos de
`input.c`, y piden fuera exactamente los mismos símbolos que el objeto de `online_actors.c` antes de T207 (`nm`).

    $PY tools/coop/oa_check.py --renombrar specs/decisiones/ADR-022-modulos-oa.md \
        runtime/oa_*.c runtime/online_actors.c runtime/online_actors_internal.h tools/test_online_names.c
    $PY tools/coop/oa_check.py --antes 3370d18:runtime/online_actors.c --modulos "runtime/oa_*.c" \
        --nombres specs/decisiones/ADR-022-modulos-oa.md

Tabla (`viejo nuevo`, por el módulo que lo define):

```
oa-nombres:
# online_actors.c
weapon_name_listed     oa_weapon_name_listed
loot_name_listed       oa_loot_name_listed
# oa_core.c
has_body               oa_has_body
watch                  oa_watch
alive                  oa_alive
alive_rewatch          oa_alive_rewatch
new_handle             oa_new_handle
g_act                  oa_g_act
g_local_dead           oa_g_local_dead
g_aggro_id             oa_g_aggro_id
g_aggro_seq            oa_g_aggro_seq
g_objs_out             oa_g_objs_out
g_objs_out_trunk       oa_g_objs_out_trunk
g_movers_out           oa_g_movers_out
g_broken               oa_g_broken
g_trunks_gone          oa_g_trunks_gone
g_online_actor         oa_g_online_actor
actor_name             oa_actor_name
is_online_player       oa_is_online_player
call_f                 oa_call_f
call_sret              oa_call_sret
call                   oa_call
playing                oa_playing
play_as                oa_play_as
g_pool                 oa_g_pool
orphan                 oa_orphan
orphans_update         oa_orphans_update
delete_piece           oa_delete_piece
model_visible          oa_model_visible
g_pool_at              oa_g_pool_at
pool_update            oa_pool_update
drop                   oa_drop
gear_actor             oa_gear_actor
map_load               oa_map_load
map_find               oa_map_find
armor_actor            oa_armor_actor
armor_bones            oa_armor_bones
create                 oa_create
request                oa_request
request_p              oa_request_p
inst_params_s32        oa_inst_params_s32
inst_params_string     oa_inst_params_string
g_dist                 oa_g_dist
g_pvp                  oa_g_pvp
g_piece_params         oa_g_piece_params
piece_update           oa_piece_update
running                oa_running
skel_unit              oa_skel_unit
linked                 oa_linked
equipment              oa_equipment
actor_name_of          oa_actor_name_of
first_unit             oa_first_unit
bone_mtx               oa_bone_mtx
mtx_mul                oa_mtx_mul
mtx_inv                oa_mtx_inv
g_actors               oa_g_actors
mtx_to_quat            oa_mtx_to_quat
quat_to_mtx            oa_quat_to_mtx
all_actors             oa_all_actors
g_theirs               oa_g_theirs
# oa_gamedata.c
crc32_str              oa_crc32_str
bool_flag              oa_bool_flag
gdt_value              oa_gdt_value
start_event            oa_start_event
give_item              oa_give_item
give_heart             oa_give_heart
set_bool_flag          oa_set_bool_flag
# oa_sample.c
k_channel              oa_k_channel
k_bones                oa_k_bones
k_shot_names           oa_k_shot_names
g_fx_trail_t           oa_g_fx_trail_t
g_rf                   oa_g_rf
g_rf_ui                oa_g_rf_ui
g_rf_watch             oa_g_rf_watch
slink_ui               oa_slink_ui
elink_ui               oa_elink_ui
fx_update              oa_fx_update
elem_near              oa_elem_near
g_map                  oa_g_map
same_map               oa_same_map
sample_pose            oa_sample_pose
sample_skeleton        oa_sample_skeleton
# oa_puppet.c
link_live              oa_link_live
g_blast                oa_g_blast
cover                  oa_cover
g_blur                 oa_g_blur
boom_start             oa_boom_start
booms_forget           oa_booms_forget
booms_update           oa_booms_update
pose_link              oa_pose_link
pose_armor             oa_pose_armor
# oa_runes.c
magne_held             oa_magne_held
stop_update            oa_stop_update
ice_update             oa_ice_update
grab_update            oa_grab_update
# oa_enemies.c
g_refused_enemy        oa_g_refused_enemy
g_refused_weapon       oa_g_refused_weapon
g_refused_loot         oa_g_refused_loot
name_refused           oa_name_refused
enemy_state            oa_enemy_state
g_own                  oa_g_own
owns_left              oa_owns_left
own_guard              oa_own_guard
g_reg_out              oa_g_reg_out
g_reg_n                oa_g_reg_n
owns_forget            oa_owns_forget
enemies_host           oa_enemies_host
attack_power           oa_attack_power
our_defense            oa_our_defense
strike                 oa_strike
puppets_forget         oa_puppets_forget
enemies_client         oa_enemies_client
enemies_show           oa_enemies_show
boss_sample            oa_boss_sample
boss_update            oa_boss_update
# oa_loot.c
players_own_item       oa_players_own_item
g_carried_now          oa_g_carried_now
drop_hush              oa_drop_hush
loot_forget            oa_loot_forget
loot_mine_add          oa_loot_mine_add
loot_mine_thrown       oa_loot_mine_thrown
link_carried           oa_link_carried
carries                oa_carries
loot_update            oa_loot_update
ground_update          oa_ground_update
# oa_world.c
objs_update            oa_objs_update
movers_update          oa_movers_update
movers_forget          oa_movers_forget
breaks_update          oa_breaks_update
# oa_flags.c
weather_now            oa_weather_now
bloody_moon_timer      oa_bloody_moon_timer
bloody_moon_follow     oa_bloody_moon_follow
weather_follow         oa_weather_follow
slate_event            oa_slate_event
g_fake_blood_moon      oa_g_fake_blood_moon
quests_update          oa_quests_update
# oa_rules.c
g_nopause_now          oa_g_nopause_now
g_nopause_frames       oa_g_nopause_frames
own_strikes_in_menu    oa_own_strikes_in_menu
g_nopause_rule         oa_g_nopause_rule
players_collide        oa_players_collide
pvp_update             oa_pvp_update
# oa_control.c
flag_query             oa_flag_query
event_query            oa_event_query
find_actors            oa_find_actors
fin
```
