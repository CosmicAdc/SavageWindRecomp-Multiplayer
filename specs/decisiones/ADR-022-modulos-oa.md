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
