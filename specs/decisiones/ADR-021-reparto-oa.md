# ADR-021 · Reparto de `online_actors.c` en módulos (T206)

- Estado: propuesta del agente de T206 (pendiente de revisión del orquestador)
- Tarea: T206 de `fases/F2-cimientos/tasks.md`, paso 2; base de T207
- "Antes": `7b53d18:runtime/online_actors.c` (6 603 líneas; la etiqueta `f1-cerrada` aún no existe)

## Contexto

`plan.md` §3.1 y `mapa/07` §8.1 proponen 11 módulos (12 contando `oa_control` aparte) con rangos de líneas
aproximados del commit `350c5f8`. T206 los agrupa dentro del mismo fichero, sin cambiar un carácter de ninguna
definición, y fija el reparto real, definición a definición. Este ADR es ese reparto. `tools/coop/oa_check.py`
lo lee de aquí (bloque `oa-reparto`) para escribir el fichero reordenado, y su modo de comparación demuestra que no
cambió nada.

## Decisión

### Orden de los módulos en el fichero

core, gamedata, sample, puppet, runes, enemies, loot, world, flags, rules, control, frame (el de `tasks.md`), cada
uno tras un separador `// ==== oa_<módulo> ====`. Tras `oa_core` va `// ==== prototipos ====` con los prototipos
`static` que el nuevo orden necesita (generados por `oa_check.py` a partir de la cabecera de cada definición).

Lo que va antes de `#ifndef ONLINE_NAMES_ONLY` (las listas de nombres de red, que `tools/test_online_names.c`
compila solo) y el `#endif` final no se mueven.

### Reglas del reparto

1. **Unidad que se mueve**: una definición con todo lo que la precede desde la anterior (líneas en blanco,
   comentario pegado, comentarios sueltos y prototipos originales). Por eso un comentario de sección
   (`// ---- ...`) viaja con la primera definición de su sección.
2. **Las direcciones `FN_*` / `P_*` se quedan junto a la función que las usa** cuando solo las usa un módulo. Su
   comentario de encima describe esa función (por ejemplo el de `FN_GDT_SET_F32` explica `bloody_moon_set`):
   llevarlas a `oa_core` separaría la explicación de su código. Van a `oa_core` las que usan varios módulos (las del
   principio del fichero, `FN_EFFECT_*`, `FN_XLINK_EMIT`, `FN_BONE_LOCAL_*`, `FN_BODY_TO`). Pasarlas todas a la
   cabecera interna es de T207 (paso 1). Esto se aparta de `plan.md` §3.1 ("todas las direcciones y offsets" en
   core), que daba el reparto como aproximado.
3. **Estado compartido usado antes de su módulo dueño**: en un solo fichero, una variable debe definirse antes de su
   primer uso y no se puede añadir una declaración previa sin cambiar el texto comparado. Las variables que escribe
   un módulo y lee otro anterior en el orden quedan en `oa_core`, con su **dueño lógico** anotado abajo; T207
   decide si las lleva a su dueño (con `extern` en la cabecera interna).
4. Un bloque cuyo comentario describe varias definiciones se mantiene junto (por ejemplo `MENU_LIVE_AFTER`,
   `UI_PAUSE_LOAD`, `g_nopause_frames` y `own_strikes_in_menu`, que comparten comentario, van todos a `oa_rules`).

### Diferencias con `plan.md` §3.1 / `mapa/07` §8.1

| Qué | Propuesta | Aquí | Por qué |
|---|---|---|---|
| `enemy_actor`, `weapon_scale`, `attack_power` | core (carga de `.txt`) | enemies | Solo los usan enemies y rules (posteriores); `enemy_actor` lleva el comentario de sección de los enemigos |
| `boss_sample` (+ `g_boss`, `BOSS_TRACK`, `g_btrack`, `btrack`) | sample | enemies, con `boss_update` | Comparten el estado y su comentario; `sample_pose` la llama con el prototipo que ya existía |
| `magne_held` y `P_MAGNE`... `MAGNE_AIM_OFF` | sample | runes | El comentario de esas macros describe Magnesis; `sample_pose` llama a `magne_held` con un prototipo |
| `own_strikes_in_menu` | enemies | rules | Comparte comentario con el estado de menús sin pausa (regla 4) |
| `g_nopause_now` | (junto a `sample_held`) | rules | Solo la usan las funciones de menús sin pausa y `rt_online_frame` |
| `MAGNET_TIP` | (junto a `g_act`) | runes | Solo la usa `grab_update` |
| `forget_all` | core (`mapa/07`) / frame (`plan.md`) | frame | `plan.md` §3.1; solo la llama `rt_online_frame` |
| `cover` | puppet | puppet | — |
| `crc32_str` | gamedata | gamedata | Lleva encima el comentario de sección de las misiones (`---- The quests`): viaja con ella |
| `slate_event`, `slate_group`, `slate_rune`, `union_has`, `korok_event` | flags | flags | — |
| `mtx_to_quat`, `quat_to_mtx` | sample / puppet | core | Utilidades de matrices, con `mtx_mul` y `mtx_inv` |
| `g_online_block_a`, `g_game_buttons` | rules (bloqueo de A) | rules | — |

### Estado compartido que queda en `oa_core` (dueño lógico)

| Variable | Dueño lógico | La leen antes |
|---|---|---|
| `g_act` | puppet / runes (la parte T208) | core (`drop`), sample |
| `g_local_dead` | frame | sample, enemies |
| `g_aggro_id`, `g_aggro_seq` | enemies | sample |
| `g_objs_out`, `g_objs_out_trunk`, `g_movers_out`, `g_broken`, `g_trunks_gone` | world | sample |
| `g_pvp` | rules | sample |
| `THEIRS`, `g_theirs` | loot | runes (`grab_update`) |
| `g_dist`, `g_piece_params`, `g_actors`, `MAX_ACTORS`, `LINK_CARRIED` | core | (compartidos de verdad) |

### Bloque que lee `oa_check.py`

Una orden por línea. Una orden por nombre manda sobre cualquier rango; entre órdenes de la misma clase, la
posterior manda. Los números son la línea del **código** de cada definición (sin su comentario) en el "antes".

```
oa-reparto:
orden core gamedata sample puppet runes enemies loot world flags rules control frame
prototipos tras core

core     96-273
puppet   link_live k_anims
sample   k_channel k_bones BONES
frame    g_last_link g_settle
puppet   g_blast
world    g_break_test_t
runes    MAGNET_TIP
core     275-873
frame    forget_all
puppet   cover
sample   875-929
core     931-935
sample   939-1009
puppet   1011-1025
flags    1027-1104
sample   1106-1455
rules    g_nopause_now
puppet   1457-1527
runes    1529-1549
sample   1550-1566
core     1570-1570
sample   1571-1973
core     mtx_to_quat quat_to_mtx
puppet   1975-2065
enemies  2067-2836
core     all_actors
rules    2838-3054
enemies  3056-3347
loot     3349-3772
core     THEIRS g_theirs
gamedata 3774-3933
flags    slate_event slate_group slate_rune union_has P_KOROK_EVT_MGR FN_EVT_START
flags    3934-4123
loot     4125-4129
runes    4131-4285
enemies  4287-4411
runes    4413-4624
loot     4626-4634
world    4636-5582
loot     5584-5686
rules    5688-5689
control  5691-5852
frame    5854-6601
fin
```

## Consecuencias

- T207 corta por los separadores; cada módulo ya está contiguo.
- Las variables de la tabla "estado compartido" cruzan de módulo: en T207 serán `extern` en la cabecera interna
  (o se definen en su dueño lógico).
- `rt_online_frame` (750 líneas) sigue entera en `oa_frame`: dentro tiene el código de marionetas, desenfoque,
  bloqueo de A, hora, etc. Partirla es de T209, no de aquí.
- El "antes" tenía 23 prototipos `static` sueltos (sin comentario) y otros con comentario; todos se conservan, cada
  uno delante de la definición que lo seguía. Algunos quedan redundantes (su función ya está definida más arriba);
  `-Wall` no incluye `-Wredundant-decls`, así que no deberían dar avisos nuevos (sin compilar: lo comprueba el
  orquestador). Quitarlos es trabajo de T207.

## Cómo se comprueba

    PY=tools/venv/Scripts/python.exe
    $PY tools/coop/oa_check.py --autoprueba
    $PY tools/coop/oa_check.py --antes 7b53d18:runtime/online_actors.c --despues runtime/online_actors.c
    $PY tools/coop/oa_check.py --orden runtime/online_actors.c
    # el fichero sale igual al regenerarlo desde este ADR:
    $PY tools/coop/oa_check.py --reordenar specs/decisiones/ADR-021-reparto-oa.md \
        --antes 7b53d18:runtime/online_actors.c --salida /tmp/oa.c && cmp /tmp/oa.c runtime/online_actors.c

`--orden` es una heurística (no un compilador): lo que cuenta es `make -j8` sin avisos nuevos.

## Líneas por módulo tras T206

Separador, líneas en blanco y comentarios incluidos; fichero de 6 632 líneas (6 603 antes + 13 separadores, 13
líneas en blanco delante de ellos y 3 prototipos). Las líneas 1-96 (listas de nombres, fuera de
`#ifndef ONLINE_NAMES_ONLY`) y el `#endif` final no son de ningún módulo.

| Módulo | Líneas | Nº de líneas | Definiciones |
|---|---|---|---|
| `oa_core` | 97-874 | 778 | 115 |
| prototipos | 875-879 | 5 | 3 |
| `oa_gamedata` | 880-991 | 112 | 19 |
| `oa_sample` | 992-1870 | 879 | 53 |
| `oa_puppet` | 1871-2080 | 210 | 18 |
| `oa_runes` | 2081-2474 | 394 | 22 |
| `oa_enemies` | 2475-3643 | 1 169 | 65 |
| `oa_loot` | 3644-4188 | 545 | 31 |
| `oa_world` | 4189-5139 | 951 | 58 |
| `oa_flags` | 5140-5461 | 322 | 16 |
| `oa_rules` | 5462-5686 | 225 | 17 |
| `oa_control` | 5687-5851 | 165 | 15 |
| `oa_frame` | 5852-6631 | 780 | 4 |

## Tabla completa (generada)

Salida de `tools/coop/oa_check.py --tabla specs/decisiones/ADR-021-reparto-oa.md --antes 7b53d18:runtime/online_actors.c`.
Una fila por unidad movida (su definición); "declaración" es un prototipo con comentario que viaja con lo que le
sigue. Las variables declaradas juntas (`g_broken, g_trunks_gone`) son una sola fila.

| Modulo | Elemento | Clase | Linea en el antes |
|---|---|---|---|
| core | `FN_CREATE` | macro | 97 |
| core | `ACTOR_HASH_ID` | macro | 98 |
| core | `FN_SET_MTX` | macro | 99 |
| core | `FN_ACTOR_WAKE` | macro | 100 |
| core | `FN_DELETE` | macro | 101 |
| core | `FN_AS_IN_SLOT` | macro | 102 |
| core | `FN_PLAY_AS` | macro | 104 |
| core | `FN_HIDE_GROUPS` | macro | 107 |
| core | `P_HIDE_GROUPS_X0` | macro | 111 |
| core | `FN_FIND_BONE` | macro | 112 |
| core | `P_SAFE_STRING_VT` | macro | 115 |
| core | `FN_REQUEST` | macro | 116 |
| core | `FN_HANDLE_TAKE` | macro | 119 |
| core | `FN_HANDLE_RELEASE` | macro | 120 |
| core | `FN_ARMOR_BIND` | macro | 121 |
| core | `FN_ARMOR_INIT` | macro | 122 |
| core | `P_CREATOR` | macro | 123 |
| core | `P_HEAP_OWNER` | macro | 124 |
| core | `framepace_title_ui` | declaracion | 126 |
| core | `framepace_paused` | declaracion | 127 |
| core | `ARMOR` | macro | 129 |
| core | `VALID` | macro | 130 |
| core | `IN_IMAGE` | macro | 132 |
| core | `has_body` | funcion | 140 |
| core | `Watch` | variable/tipo | 151 |
| core | `watch` | funcion | 153 |
| core | `alive` | funcion | 158 |
| core | `alive_rewatch` | funcion | 165 |
| core | `Handle` | variable/tipo | 176 |
| core | `HANDLES` | macro | 177 |
| core | `g_handles` | variable/tipo | 178 |
| core | `g_next_handle` | variable/tipo | 179 |
| core | `new_handle` | funcion | 180 |
| core | `Piece` | variable/tipo | 186 |
| core | `MAGNE_BEAM` | macro | 195 |
| runes | `MAGNET_TIP` | macro | 196 |
| core | `g_act` | variable/tipo | 198 |
| puppet | `link_live` | funcion | 251 |
| puppet | `k_anims` | variable/tipo | 254 |
| sample | `k_channel` | variable/tipo | 258 |
| sample | `k_bones` | variable/tipo | 260 |
| sample | `BONES` | macro | 261 |
| frame | `g_last_link` | variable/tipo | 263 |
| core | `g_local_dead` | variable/tipo | 264 |
| puppet | `g_blast` | variable/tipo | 265 |
| core | `g_aggro_id` | variable/tipo | 266 |
| core | `g_aggro_seq` | variable/tipo | 267 |
| core | `g_objs_out` | variable/tipo | 268 |
| world | `g_break_test_t` | variable/tipo | 269 |
| core | `g_objs_out_trunk` | variable/tipo | 270 |
| core | `g_movers_out` | variable/tipo | 271 |
| core | `g_broken`, `g_trunks_gone` | variable/tipo | 272 |
| core | `g_online_actor` | variable/tipo | 273 |
| core | `online_has_actor` | funcion | 275 |
| frame | `g_settle` | variable/tipo | 276 |
| core | `actor_name` | funcion | 279 |
| core | `is_online_player` | funcion | 294 |
| core | `online_actors_on` | funcion | 297 |
| core | `call_f` | funcion | 309 |
| core | `call_sret` | funcion | 324 |
| core | `call` | funcion | 334 |
| core | `playing` | funcion | 339 |
| core | `play_as` | funcion | 348 |
| core | `POOL` | macro | 357 |
| core | `g_pool` | variable/tipo | 358 |
| core | `ORPHANS` | macro | 365 |
| core | `g_orphan` | variable/tipo | 366 |
| core | `orphan` | funcion | 367 |
| core | `orphans_update` | funcion | 373 |
| frame | `forget_all` | funcion | 393 |
| core | `delete_piece` | funcion | 420 |
| core | `model_visible` | funcion | 433 |
| core | `hide_piece` | funcion | 448 |
| core | `g_pool_at` | variable/tipo | 454 |
| core | `pool_piece` | funcion | 456 |
| core | `from_pool` | funcion | 468 |
| core | `pool_update` | funcion | 481 |
| core | `drop` | funcion | 500 |
| core | `g_gear_map` | variable/tipo | 520 |
| core | `g_gear_n` | variable/tipo | 521 |
| core | `gear_actor` | funcion | 522 |
| core | `MapLine` | variable/tipo | 551 |
| core | `map_load` | funcion | 552 |
| core | `map_find` | funcion | 573 |
| core | `armor_actor` | funcion | 579 |
| core | `ARMOR_BONES` | macro | 587 |
| core | `armor_bones` | funcion | 588 |
| core | `create` | funcion | 620 |
| core | `request` | funcion | 630 |
| core | `request_p` | funcion | 633 |
| core | `InstParams` | variable/tipo | 646 |
| core | `inst_params_s32` | funcion | 647 |
| core | `inst_params_string` | funcion | 657 |
| core | `test_warp_hour` | declaracion | 668 |
| core | `test_warp_set_hour` | declaracion | 669 |
| core | `test_warp_gdt_mgr` | declaracion | 670 |
| core | `g_dist` | variable/tipo | 671 |
| core | `g_pvp` | variable/tipo | 672 |
| core | `g_piece_params` | variable/tipo | 673 |
| core | `piece_update` | funcion | 676 |
| puppet | `cover` | funcion | 723 |
| core | `running` | funcion | 739 |
| core | `FN_EFFECT_EMIT` | macro | 743 |
| core | `FN_EFFECT_KILL` | macro | 744 |
| core | `FN_XLINK_EMIT` | macro | 745 |
| core | `FN_BONE_LOCAL_GET` | macro | 746 |
| core | `FN_BONE_LOCAL_SET` | macro | 747 |
| core | `skel_unit` | funcion | 748 |
| core | `linked` | funcion | 769 |
| core | `glider_of` | funcion | 783 |
| core | `equipment` | funcion | 805 |
| core | `actor_name_of` | funcion | 816 |
| core | `first_unit` | funcion | 827 |
| core | `bone_mtx` | funcion | 839 |
| core | `mtx_mul` | funcion | 859 |
| core | `mtx_inv` | funcion | 868 |
| sample | `k_shot_names` | variable/tipo | 876 |
| sample | `IS_BOMB` | macro | 879 |
| sample | `ARROW_RING` | macro | 883 |
| sample | `g_arrow_ring` | variable/tipo | 884 |
| sample | `g_arrow_next` | variable/tipo | 885 |
| sample | `g_bomb_made` | variable/tipo | 888 |
| sample | `g_bomb_next` | variable/tipo | 889 |
| sample | `rt_online_bomb` | funcion | 891 |
| sample | `rt_online_arrow` | funcion | 897 |
| sample | `FX_WATCH` | macro | 913 |
| sample | `FX_RING` | macro | 914 |
| sample | `FX_TRAIL_NS` | macro | 915 |
| sample | `g_fx_ui` | variable/tipo | 916 |
| sample | `g_fx_ring` | variable/tipo | 917 |
| sample | `g_fx_next` | variable/tipo | 918 |
| sample | `g_fx_taken` | variable/tipo | 919 |
| sample | `FX_EVENT_NS` | macro | 924 |
| sample | `g_fxev` | variable/tipo | 925 |
| sample | `g_kago_wind` | variable/tipo | 926 |
| sample | `g_fxev_seq` | variable/tipo | 927 |
| sample | `g_fx_trail` | variable/tipo | 928 |
| sample | `g_fx_trail_t` | variable/tipo | 929 |
| core | `MAX_ACTORS` | macro | 931 |
| core | `g_actors` | variable/tipo | 932 |
| core | `loot_mine_add` | declaracion | 933 |
| core | `loot_mine_thrown` | declaracion | 934 |
| core | `LINK_CARRIED` | macro | 935 |
| sample | `g_rf`, `g_rf_ui` | variable/tipo | 939 |
| sample | `g_rf_watch` | variable/tipo | 940 |
| sample | `g_rf_ring` | variable/tipo | 941 |
| sample | `g_rf_props` | variable/tipo | 942 |
| sample | `g_rf_next` | variable/tipo | 943 |
| sample | `g_rf_taken` | variable/tipo | 944 |
| sample | `rt_online_xlink_emit` | funcion | 946 |
| sample | `rt_game_fmt` | funcion | 979 |
| sample | `anim_slots` | funcion | 997 |
| puppet | `g_blur` | variable/tipo | 1011 |
| puppet | `rt_blur_anim` | funcion | 1013 |
| puppet | `rt_blur_frame` | funcion | 1020 |
| flags | `WEATHER_NEAR` | macro | 1031 |
| flags | `weather_now` | funcion | 1032 |
| flags | `bloody_moon_timer` | funcion | 1043 |
| flags | `FN_GDT_SET_F32` | macro | 1054 |
| flags | `bloody_moon_set` | funcion | 1055 |
| flags | `bloody_moon_follow` | funcion | 1064 |
| flags | `weather_follow` | funcion | 1084 |
| sample | `slink_ui` | funcion | 1106 |
| sample | `elink_ui` | funcion | 1112 |
| sample | `CONT_SOUND_NS` | macro | 1123 |
| sample | `continuous_sound` | funcion | 1125 |
| sample | `fx_update` | funcion | 1149 |
| sample | `ELEM_SEEN` | macro | 1241 |
| sample | `g_elem_seen` | variable/tipo | 1242 |
| sample | `elem_of_kind` | funcion | 1244 |
| sample | `elem_near` | funcion | 1250 |
| sample | `sample_shots` | funcion | 1263 |
| sample | `held_kind` | funcion | 1384 |
| rules | `g_nopause_now` | variable/tipo | 1389 |
| sample | `sample_held` | funcion | 1391 |
| puppet | `FN_SEND_MSG` | macro | 1461 |
| puppet | `BOOMS_MAX` | macro | 1462 |
| puppet | `g_boom` | variable/tipo | 1463 |
| puppet | `boom_start` | funcion | 1465 |
| puppet | `booms_forget` | funcion | 1478 |
| puppet | `booms_update` | funcion | 1480 |
| runes | `P_MAGNE` | macro | 1533 |
| runes | `FN_XLINK_EFFECT` | macro | 1534 |
| runes | `P_MAGNE_FX` | macro | 1535 |
| runes | `FN_ACTOR_LOOK_ON` | macro | 1536 |
| runes | `FN_ACTOR_LOOK_OFF` | macro | 1537 |
| runes | `FN_SPECIAL_FX_B` | macro | 1538 |
| runes | `FN_MAGNE_BEAM_SET` | macro | 1539 |
| runes | `P_MAGNE_AIM` | macro | 1540 |
| runes | `MAGNE_AIM_OFF` | macro | 1541 |
| runes | `magne_held` | funcion | 1544 |
| sample | `FN_MAP_TYPE` | macro | 1552 |
| sample | `FN_MAP_NAME` | macro | 1553 |
| sample | `g_map` | variable/tipo | 1554 |
| sample | `local_map` | funcion | 1555 |
| sample | `same_map` | funcion | 1564 |
| core | `FN_BODY_TO` | macro | 1570 |
| sample | `sample_pose` | funcion | 1573 |
| core | `mtx_to_quat` | funcion | 1926 |
| core | `quat_to_mtx` | funcion | 1943 |
| sample | `sample_skeleton` | funcion | 1951 |
| puppet | `g_pose_q`, `g_pose_t` | variable/tipo | 1978 |
| puppet | `g_pose_n` | variable/tipo | 1979 |
| puppet | `pose_link` | funcion | 1981 |
| puppet | `pose_armor` | funcion | 2025 |
| puppet | `animate` | funcion | 2051 |
| enemies | `enemy_actor` | funcion | 2073 |
| core | `all_actors` | funcion | 2082 |
| enemies | `is_boss` | funcion | 2105 |
| enemies | `is_enemy` | funcion | 2109 |
| enemies | `Refused` | variable/tipo | 2116 |
| enemies | `g_refused_enemy`, `g_refused_weapon`, `g_refused_loot` | variable/tipo | 2117 |
| enemies | `name_refused` | funcion | 2118 |
| enemies | `enemy_name_ok` | funcion | 2127 |
| enemies | `enemy_weapon` | funcion | 2133 |
| enemies | `ENEMY_LIFE` | macro | 2155 |
| enemies | `ENEMY_NEAR` | macro | 2156 |
| enemies | `ENEMY_AREA` | macro | 2157 |
| enemies | `OWN_SAFE` | macro | 2161 |
| enemies | `OWNER_NONE` | macro | 2165 |
| enemies | `k_hands` | variable/tipo | 2170 |
| enemies | `enemy_held` | funcion | 2171 |
| enemies | `own_guarded_life` | declaracion | 2193 |
| enemies | `enemy_state` | funcion | 2194 |
| enemies | `Own` | variable/tipo | 2211 |
| enemies | `OWN_NONE`, `OWN_MAKING`, `OWN_HAVE`, `OWN_GONE` | variable/tipo | 2221 |
| enemies | `OWNS` | macro | 2222 |
| enemies | `GUARD_LIFE` | macro | 2223 |
| enemies | `g_own` | variable/tipo | 2224 |
| enemies | `g_own_left` | variable/tipo | 2225 |
| enemies | `own_find` | funcion | 2227 |
| enemies | `owns_left` | funcion | 2235 |
| enemies | `own_is` | funcion | 2242 |
| enemies | `own_add` | funcion | 2248 |
| enemies | `enemy_delete` | funcion | 2259 |
| enemies | `own_drop` | funcion | 2271 |
| enemies | `own_guarded_life` | funcion | 2282 |
| enemies | `own_guard` | funcion | 2287 |
| enemies | `own_ready` | funcion | 2315 |
| enemies | `ELEM_SOURCES` | macro | 2359 |
| enemies | `g_esrc` | variable/tipo | 2360 |
| enemies | `k_elem_source` | variable/tipo | 2361 |
| enemies | `elem_start` | funcion | 2364 |
| enemies | `elem_update` | funcion | 2375 |
| enemies | `own_hits` | funcion | 2402 |
| enemies | `REG` | macro | 2421 |
| enemies | `g_reg` | variable/tipo | 2424 |
| enemies | `g_reg_out` | variable/tipo | 2425 |
| enemies | `g_reg_n` | variable/tipo | 2426 |
| enemies | `owns_forget` | funcion | 2430 |
| enemies | `crc32_str` | declaracion | 2446 |
| enemies | `mark_dead` | funcion | 2449 |
| enemies | `dist3` | funcion | 2466 |
| enemies | `can_own` | funcion | 2473 |
| enemies | `enemies_host` | funcion | 2478 |
| enemies | `PUPPETS` | macro | 2748 |
| enemies | `g_puppet` | variable/tipo | 2749 |
| enemies | `weapon_scale` | funcion | 2762 |
| enemies | `attack_power` | funcion | 2774 |
| enemies | `our_defense` | funcion | 2784 |
| enemies | `STRIKE_FRAMES` | macro | 2804 |
| enemies | `strike` | funcion | 2805 |
| rules | `MENU_LIVE_AFTER` | variable/tipo | 2842 |
| rules | `UI_PAUSE_LOAD` | variable/tipo | 2843 |
| rules | `g_nopause_frames` | variable/tipo | 2844 |
| rules | `own_strikes_in_menu` | funcion | 2845 |
| rules | `COLLIDE_R` | macro | 2899 |
| rules | `COLLIDE_H` | macro | 2900 |
| rules | `g_revive_rule` | variable/tipo | 2905 |
| rules | `g_nopause_rule` | variable/tipo | 2906 |
| rules | `rt_scene_state` | funcion | 2907 |
| rules | `rt_online_menu_live` | funcion | 2912 |
| rules | `rt_menu_draw_pause` | funcion | 2922 |
| rules | `rt_online_has_fairy` | funcion | 2936 |
| rules | `players_collide` | funcion | 2945 |
| rules | `pvp_update` | funcion | 2986 |
| enemies | `puppets_forget` | funcion | 3056 |
| enemies | `enemies_client` | funcion | 3062 |
| enemies | `enemies_show` | funcion | 3108 |
| loot | `players_own_item` | funcion | 3355 |
| loot | `is_loot` | funcion | 3358 |
| loot | `loot_name_ok` | funcion | 3362 |
| loot | `DEATHS` | macro | 3364 |
| loot | `g_death` | variable/tipo | 3365 |
| loot | `MINE` | macro | 3366 |
| loot | `g_mine` | variable/tipo | 3369 |
| core | `THEIRS` | macro | 3370 |
| core | `g_theirs` | variable/tipo | 3371 |
| loot | `g_carried_now` | variable/tipo | 3372 |
| loot | `g_loot_list` | variable/tipo | 3373 |
| loot | `g_nloot_list` | variable/tipo | 3374 |
| loot | `g_next_gid` | variable/tipo | 3375 |
| loot | `DROP_SEEN` | macro | 3382 |
| loot | `DROP_SETTLE_NS` | macro | 3383 |
| loot | `DROP_HUSH_NS` | macro | 3384 |
| loot | `g_drop_seen` | variable/tipo | 3385 |
| loot | `g_drop_hush` | variable/tipo | 3386 |
| loot | `drop_hush` | funcion | 3387 |
| loot | `loot_forget` | funcion | 3394 |
| loot | `loot_death` | funcion | 3401 |
| loot | `loot_mine_add` | funcion | 3418 |
| loot | `loot_mine_thrown` | funcion | 3433 |
| loot | `near_link` | funcion | 3440 |
| loot | `link_carried` | funcion | 3448 |
| loot | `carries` | funcion | 3454 |
| loot | `loot_host_take` | funcion | 3461 |
| loot | `loot_host_add` | funcion | 3470 |
| loot | `loot_update` | funcion | 3484 |
| gamedata | `crc32_str` | funcion | 3779 |
| gamedata | `gdt_value` | declaracion | 3789 |
| gamedata | `bool_flag` | funcion | 3790 |
| gamedata | `gdt_value` | funcion | 3792 |
| gamedata | `FN_EVT_META_INIT` | macro | 3817 |
| gamedata | `FN_EVT_META_NAME` | macro | 3818 |
| gamedata | `FN_EVT_META_FREE` | macro | 3819 |
| gamedata | `FN_EVT_CALL` | macro | 3820 |
| gamedata | `P_EVT_MGR` | macro | 3821 |
| gamedata | `start_event` | funcion | 3822 |
| flags | `slate_event` | funcion | 3844 |
| gamedata | `FN_GDT_HANDLE` | macro | 3867 |
| gamedata | `FN_GDT_SET_BOOL` | macro | 3868 |
| gamedata | `P_GDT_MGR` | macro | 3869 |
| gamedata | `FN_POUCH_CAN_GET` | macro | 3872 |
| gamedata | `FN_POUCH_GET` | macro | 3873 |
| gamedata | `give_item` | funcion | 3874 |
| gamedata | `FN_GET_SPECIAL` | macro | 3886 |
| gamedata | `give_heart` | funcion | 3887 |
| flags | `slate_group` | funcion | 3894 |
| flags | `slate_rune` | funcion | 3901 |
| flags | `union_has` | funcion | 3911 |
| gamedata | `set_bool_flag` | funcion | 3917 |
| flags | `P_KOROK_EVT_MGR` | macro | 3932 |
| flags | `FN_EVT_START` | macro | 3933 |
| flags | `korok_event` | funcion | 3934 |
| flags | `g_fake_blood_moon` | variable/tipo | 3972 |
| flags | `quests_update` | funcion | 3974 |
| loot | `GROUND` | macro | 4128 |
| loot | `g_ground` | variable/tipo | 4129 |
| runes | `FN_SPECIAL_FX_A` | macro | 4133 |
| runes | `FN_SPECIAL_FX_A_UNIT` | macro | 4134 |
| runes | `stasis_look` | funcion | 4135 |
| runes | `chains_place` | funcion | 4160 |
| runes | `stop_update` | funcion | 4190 |
| runes | `ice_update` | funcion | 4249 |
| enemies | `g_boss` | variable/tipo | 4291 |
| enemies | `BOSS_TRACK` | macro | 4292 |
| enemies | `g_btrack` | variable/tipo | 4293 |
| enemies | `g_bseen` | variable/tipo | 4294 |
| enemies | `btrack` | funcion | 4295 |
| enemies | `boss_sample` | funcion | 4304 |
| enemies | `boss_update` | funcion | 4374 |
| runes | `fx_on` | funcion | 4413 |
| runes | `fx_end` | funcion | 4418 |
| runes | `rune_keep` | funcion | 4427 |
| runes | `rune_pieces` | funcion | 4436 |
| runes | `grab_update` | funcion | 4458 |
| loot | `loot_actor` | funcion | 4628 |
| world | `OBJ_NEAR` | macro | 4642 |
| world | `OBJ_FAR` | macro | 4643 |
| world | `OBJ_SEEN` | macro | 4644 |
| world | `g_obj_seen` | variable/tipo | 4645 |
| world | `OBJ_HOLD` | macro | 4652 |
| world | `OBJ_QUIET_NS` | macro | 4653 |
| world | `OBJ_TIE_NS` | macro | 4654 |
| world | `TRUNK_QUIET_NS` | macro | 4655 |
| world | `trunks_near` | declaracion | 4658 |
| world | `trunk_made_t` | declaracion | 4659 |
| world | `obj_kind` | funcion | 4661 |
| world | `obj_find` | funcion | 4675 |
| world | `objs_update` | funcion | 4711 |
| world | `MOVER_NEAR` | macro | 4922 |
| world | `MOVER_TRACK` | macro | 4923 |
| world | `MOVER_QUIET` | macro | 4924 |
| world | `MOVER_THEIRS_NS` | macro | 4925 |
| world | `g_mv` | variable/tipo | 4926 |
| world | `mover_kind` | funcion | 4933 |
| world | `mtx_moved` | funcion | 4947 |
| world | `movers_update` | funcion | 4955 |
| world | `movers_forget` | funcion | 5106 |
| world | `BREAKS` | macro | 5116 |
| world | `g_brk` | variable/tipo | 5117 |
| world | `g_our_cut` | variable/tipo | 5118 |
| world | `g_spawn` | variable/tipo | 5119 |
| world | `g_spawn_ours` | variable/tipo | 5120 |
| world | `g_their_trunk` | variable/tipo | 5121 |
| world | `g_breaking` | variable/tipo | 5122 |
| world | `trunk_made_t` | funcion | 5124 |
| world | `trunks_near` | funcion | 5130 |
| world | `map_item` | funcion | 5147 |
| world | `breakable` | funcion | 5151 |
| world | `FN_TAKE_OUT` | macro | 5167 |
| world | `FN_MAPOBJ_GONE` | macro | 5168 |
| world | `take_out` | funcion | 5173 |
| world | `CUT_WAKE_NS` | macro | 5185 |
| world | `g_cutting` | variable/tipo | 5186 |
| world | `cut_later` | funcion | 5187 |
| world | `cutting_update` | funcion | 5196 |
| world | `spawn_at` | funcion | 5204 |
| world | `WORLD_MAX` | macro | 5220 |
| world | `g_world` | variable/tipo | 5221 |
| world | `g_nworld` | variable/tipo | 5222 |
| world | `g_widx` | variable/tipo | 5223 |
| world | `g_broken_new_t` | variable/tipo | 5224 |
| world | `world_find` | funcion | 5226 |
| world | `world_has` | funcion | 5234 |
| world | `world_add` | funcion | 5235 |
| world | `world_seen` | funcion | 5246 |
| world | `world_clear` | funcion | 5250 |
| world | `break_here` | funcion | 5258 |
| world | `WORLD_TRUNK_NS` | macro | 5326 |
| world | `world_enforce` | funcion | 5327 |
| world | `WORLD_ROTATE_NS` | macro | 5349 |
| world | `world_rotate` | funcion | 5350 |
| world | `breaks_update` | funcion | 5360 |
| loot | `ground_update` | funcion | 5584 |
| rules | `g_online_block_a` | variable/tipo | 5688 |
| rules | `g_game_buttons` | variable/tipo | 5689 |
| control | `FIND_MAX` | macro | 5693 |
| control | `g_find` | variable/tipo | 5694 |
| control | `g_flagq` | variable/tipo | 5705 |
| control | `flag_query` | funcion | 5707 |
| control | `flag_cmd` | funcion | 5721 |
| control | `cmd_flag` | funcion | 5742 |
| control | `cmd_setflag` | funcion | 5743 |
| control | `cmd_leave` | funcion | 5744 |
| control | `cmd_bloodmoon` | funcion | 5749 |
| control | `g_eventq` | variable/tipo | 5757 |
| control | `event_query` | funcion | 5759 |
| control | `cmd_event` | funcion | 5768 |
| control | `find_actors` | funcion | 5787 |
| control | `cmd_actors` | funcion | 5810 |
| control | `online_actors_control_init` | funcion | 5840 |
| frame | `rt_online_frame` | funcion | 5854 |

- `core`: 115 definiciones
- `gamedata`: 19 definiciones
- `sample`: 53 definiciones
- `puppet`: 18 definiciones
- `runes`: 22 definiciones
- `enemies`: 65 definiciones
- `loot`: 31 definiciones
- `world`: 58 definiciones
- `flags`: 16 definiciones
- `rules`: 17 definiciones
- `control`: 15 definiciones
- `frame`: 4 definiciones
