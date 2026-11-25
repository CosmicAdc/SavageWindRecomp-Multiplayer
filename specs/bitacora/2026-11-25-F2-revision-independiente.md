# 2026-11-25 · F2 · Revisión independiente de T202, T207, T208 y T213

Revisor de solo lectura (encargo `ENCARGO-REVISION1.md`). No se ha arreglado nada. Revisado:

- `coop/main` en `5b42fff` (para `runtime/online*.c`, `online_validate.*`, `online_wire.h` y `oa_*.c` es el mismo
  código que `2749d9f`, más `runtime/online_auth.*`, de T321, que solo entra en la comprobación de símbolos).
- `coop/T208` en `23051ec`, **sin integrar**. Ojo: su base es `f807b89`, así que `git diff coop/main coop/T208`
  (dos puntos) muestra como "borrados" los `coop_*.c/h` que `coop/main` ganó después. El cambio propio de T208 es
  `git diff coop/main...coop/T208` (tres puntos): 8 ficheros, +375 −278. Hay que integrarlo con merge, no copiando
  ficheros.

Niveles de evidencia (constitución, regla 16): **hipótesis** = leído o razonado; **sintética** = programa o guion
sin el juego; **compilación** = `clang -c` de objetos sueltos en una carpeta temporal fuera del repositorio. Nada de
esto se ha visto en el juego. No se ejecutó `make` ni el juego.

---

## Hallazgos, por gravedad

### Cuelgue (posible, con un par hostil)

**C-1 · `fx_anim` no tiene cota superior y llega al código del juego como id de animación.** [hipótesis]
- `runtime/online_validate.h:99-100` solo exige `fx_anim[k] >= -1`. El valor pasa sin más a
  `oa_g_blur[i].anim[k]` (`runtime/oa_frame.c:567`), y `rt_blur_anim` (`runtime/oa_puppet.c:40`) lo devuelve como
  `anim << 16` a SwordBlur, que lo usa en lugar del id que lee del slot de animación del muñeco (parche
  `_blur_anim` / `_blur_anim_draw`, `tools/patches.py:583-593`).
- Escenario: un par manda un STATE con `fx = "Blur_Default"`, un arma en la mano y `fx_anim[0] = 32767`. Su muñeco
  dibuja la estela y SwordBlur busca la animación 32767 en la tabla del modelo del muñeco. Lo que haga el juego con
  un índice fuera de tabla está sin verificar; lo más probable es que lea fuera de ella.
- La propia validación lo reconoce ("its upper bound is the model's, not known here"), pero la regla 9 pide
  "índices dentro de rango".
- Propuesta: acotar en el receptor, en `oa_*` (segunda barrera), con el número de animaciones del modelo del muñeco
  (`anim_slots` ya lee el controlador, `+0xd0`, `+0x40`). En la capa pura, una cota alta conservadora (medir el
  máximo real en el recorrido).

**C-2 · Otros valores sin validar que se escriben tal cual en memoria del juego.** [hipótesis]
- `dye[k]` admite de `-1` a `127` (`online_validate.h:107-108`) y llega como `ArmorDyeColor` a la creación de la
  armadura (`oa_frame.c:382`). Los colores reales son unos 16, y está sin verificar qué hace el juego con 100.
- `fx_props[24]` y `ground_props[16]` ("any") se copian con `memcpy` sobre los valores de propiedad del ELink del
  muñeco y del ReactionField local (`oa_frame.c:464`, `:501`). Algunos son `float` ("su velocidad") y no se comprueba
  que sean finitos.
- Escenario: un par envía `dye = 120` o propiedades con NaN o enumerados enormes. Sin un par hostil no ocurre.
- Propuesta: cota de `dye` igual a la que usa el emisor para los colores reales, y una nota en
  `contratos/validacion-red.md` diciendo que `fx_props` y `ground_props` se escriben en el juego (o acotar los que
  sean `float`).

### Incorrecto

**I-1 · Un `bloody_moon` legítimo por encima de 4000 descarta el STATE entero del jugador.** [hipótesis; riesgo ya
anotado en la bitácora de T213, sin mitigar]
- `runtime/online_validate.h:119-120` rechaza el mensaje si `bloody_moon` no es `-1` y no está entre 0 y 4000. El
  emisor manda el temporizador del juego en bruto (`oa_sample.c:749-750`).
- Según `oa_flags.c:17-20`, la Luna Carmesí llega en la primera medianoche pasado 2520. Si se aplaza (un sitio o un
  evento donde no puede salir, que el juego no adelanta) el contador sigue subiendo. También la prueba
  `SAVAGEWIND_ONLINE_BM_TEST=5000` (`oa_flags.c:37-42`) lo pone por encima en el anfitrión.
- Escenario: el temporizador del anfitrión pasa de 4000. Todos los clientes descartan **todos** sus STATE: su Link
  se congela en la última posición, y las reglas de sesión que viajan en ese STATE (`collide`, `revive`, `nopause`,
  el tiempo de la Luna Carmesí) dejan de llegar. Solo sale una línea de log, la primera vez.
- El consumidor ya ignoraba ese valor (`oa_flags.c:46`). Antes de T213 el resto del STATE se aplicaba igual.
- Propuesta: en la capa, exigir solo que sea finito (`|x| ≤ ONLINE_V_LIMIT`) y dejar la ventana 0..4000 al
  consumidor, que ya la tiene. Si se quiere una cota, que sea la del juego (medirla) y no 4000.

**I-2 · "Todo o nada" en STATE: un subcampo malo congela al jugador entero.** [hipótesis]
- `online_validate_state` (`online_validate.h:74-135`) descarta el STATE si falla cualquier entrada de cualquier
  lista: 8 disparos y 4 explosiones (también las que pasan de `n`), 8 objetos, 6 mecanismos, 5 materiales, 8 eventos
  de efecto, 2 jefes, 3 pilares, caballo y aparejo. Antes de T213 solo `pos` y `yaw` descartaban el STATE. Es un
  cambio de comportamiento documentado (bitácora de T213, punto 1), pero tiene un efecto que no se dice: muchas de
  esas entradas son memoria del juego copiada en bruto por el emisor (`oa_sample.c`: matrices de actores, de
  aparejos, de pilares, `fx_frame`).
- Ejemplo concreto: `anim_slots` (`oa_sample.c:128`) hace `(int16_t)(id >> 16)`. Si el juego tiene en el slot un id
  con el bit alto activo que no sea `-1`, el valor queda por debajo de `-1` y el receptor tira el STATE completo
  mientras dure la estela.
- Escenario: un NaN o un valor fuera de rango en un solo objeto cercano. Mientras dure, los demás ven a ese jugador
  quieto (posición, animación, todo).
- Propuesta (la decide el dueño, porque toca el contrato "nunca reparar"): en STATE, descartar el mensaje solo por
  los campos núcleo (`pos`, `yaw`, `flags`, `anim`, `as`, las reglas) y, en las listas, **anular la entrada** (como
  ENEMIES y LOOTS, que ya descartan por entrada). Mientras tanto, revisar en el recorrido de F1 que
  `dropped a STATE` no aparece.

**I-3 · El STATE de "cargando" sale con la posición no finita y el receptor lo tira, así que LOADING no llega.**
[hipótesis; viene de F1/T115, pero es terreno de T213]
- `sample_local` copia la posición antes de comprobarla (`runtime/online.c:788-790`) y devuelve `false` con
  `w->pos` todavía NaN o mayor de 100 000. El llamador manda ese `w` con `ONLINE_F_LOADING` (`online.c:1313`).
- Escenario: durante una carga, `PlayerInfo+0x88` tiene basura no finita. Cada receptor descarta el STATE por
  `pos`, nunca ve el flag LOADING y sigue tratando al jugador como presente en el mundo (`oa_frame.c:323`: `want`
  sigue verdadero). Las líneas `[online]` de carga del otro no salen como deberían.
- Propuesta: en `sample_local`, poner `w->pos` a cero antes de cada `return false` (cambio del emisor; no toca el
  formato).

**I-4 · Los pilares de Cryonis de un jugador se pierden sin borrar en `oa_drop`.** [hipótesis; anterior a F2,
T208 lo conserva tal cual, como manda su tarea]
- `oa_drop` (T208: `runtime/oa_core.c:254-261`; antes, `online_actors.c` `drop`) entrega al pool las piezas del
  muñeco y de las runas, pero no `ice[q].p`. Luego `oa_runes_drop` (`oa_runes.c:73`) hace `memset` de `g_ice[i]`.
  `forget_all` tampoco huerfana los pilares pendientes.
- `oa_ice_update` corre antes del bucle de Links (`oa_frame.c:287` frente a `:323`), así que en el fotograma en que
  el jugador deja de interesar (pasa de 400 m o deja de estar en el mismo mapa sin el flag LOADING) sus pilares ya
  existen y se quedan en el mundo sin dueño.
- Escenario: B tiene un pilar y cruza los 400 m respecto a A. El pilar queda en el mundo de A. Como `oa_rune_ice`
  ya no lo reconoce, si A pasa a menos de 100 m su `oa_sample_pose` lo manda como pilar **propio**, y B ve un pilar
  fantasma (eco).
- Propuesta: en una tarea de cambio de comportamiento (no en T208), añadir los tres `ice[q].p` a `oa_runes_pieces`
  o borrarlos en `oa_runes_drop`. Pasa algo parecido con el brillo de Magnesis (`FN_SPECIAL_FX_B` a 1), que no se
  apaga al soltar por `drop`.

### Cosmético

- **K-1** · `g_drop_n` (`runtime/online.c:871`) no se reinicia al cortar ni con `RT_ON_RESET`: en la segunda sesión
  no se registra el primer descarte de cada tipo (regla 8; encaja en T212).
- **K-2** · Los descartes por longitud o por contador (`ENEMIES n > 32`, `online.c:1015`; `LOOTS n > 64`, `:1082`;
  STATE, EQUIP, PICK y demás cortos) hacen `break` sin `drop_note`, así que no se cuentan. T213, paso 3, pide contar
  el descarte.
- **K-3** · `oa_pup(i)`, `oa_puppet_link(i)`, `oa_puppet_name(i)`, `oa_rune_grab(i)`, `oa_rune_released(_t)(i)` y
  `oa_link_live(i)` no comprueban el rango de `i` (T208 `oa_puppet.c:11-17`, `oa_runes.c:38-43`). Hoy todas las
  llamadas reciben un índice de bucle `0..ONLINE_MAX-1` (comprobado con `grep`); un futuro llamador con `me = -1` u
  `origin` leería fuera. Propuesta: `if ((unsigned)i >= ONLINE_MAX)` devolviendo 0, o un `assert`.
- **K-4** · T208 convierte 112 accesos `oa_g_act[i].x` de `oa_frame.c` en llamadas `oa_pup(i)->x` a otra unidad de
  compilación (sin LTO no se expanden en línea): del orden de cientos de llamadas por fotograma con varios jugadores
  (no todos los sitios se ejecutan en cada fotograma). Seguramente es
  despreciable, pero la regla 17 pide medirlo; se puede hacer en T209 con `OaPup *pp = oa_pup(i);` al principio del
  bucle.
- **K-5** · `online_validate.h:11-13` habla de `runtime/online_actors.c` (`is_loot`, `enemy_actor`...), que tras
  T207 viven en `oa_*.c` con prefijo `oa_`.
- **K-6** · `online_wire.h` exporta `MAGIC`, `PROTOCOL`, `M_*` y `R_*` sin prefijo a todo lo que incluya
  `online_validate.h`. Puede chocar con otras cabeceras. Propuesta para la próxima subida de protocolo: prefijo
  `ONLINE_`.
- **K-7** · El único aviso de compilación de los módulos, `-Wempty-body` en `oa_frame.c:221`, ya estaba antes de
  F2 (sale igual en `coop/main` y en `coop/T208`).

---

## Revisado sin hallazgos

**T213 (`online_validate.*`, `cl_message`, `remote_snap`)**
- Lecturas fuera de búfer: `next_msg` solo entrega mensajes completos. Cada `case` comprueba la longitud mínima
  antes de leer: STATE `1+3375`, EQUIP `1+432`, ENEMIES `13 + n·311` con `n ≤ 32`, LOOTS `1 + n·97` con `n ≤ 64`,
  ESTATE con la comprobación por entrada dentro del bucle, PICK 52, HIT 9, TIME 4, PONG 8, JOIN `1+24`. SKEL:
  `online_validate_skel(p+1, len-1)` garantiza `len ≥ 3 + (n+nbow)·12`, justo lo que lee el bucle de
  `skel_decode`. [hipótesis]
- Campos: todas las cadenas de WireState (`as`, `grab.name`, `stop.name`, `map`, `boss.name`, `fx`, `fxev.name`,
  `horse`, `horse_as`, `tack`, `movers.as`, `held.name`), de WireEquip, de OnlineEnemy y de OnlineLoot se terminan
  antes de usarse. Todos los `float` se comprueban: finitos y con `|x| ≤ 100 000`. Los que quedan como "any" son
  `ping_ms`, `pvp_seq`, `aggro_*`, `broken`, `trunks_gone`, los hash, `objs_trunk`, los ids y seq, `gid`/`lid`,
  `fx_props` y `ground_props` (estos dos, ver C-2). `broken`, `trunks_gone`, QUEST y QUESTS sin validar son
  cuestión de autoridad (regla 11), no de T213. [hipótesis]
- Copias parciales: STATE, EQUIP, PICK, LOOT_ADD, HIT y TIME se validan sobre una copia local antes de guardar.
  ENEMIES, LOOTS y ESTATE escriben entrada a entrada solo lo que pasa. ENEMIES desplaza `en[0] = en[1]` solo después
  de validar `centre` (así está diseñado). [hipótesis]
- Contadores en su máximo exacto: `shots.n = 8`, `ice_n = 3`, ENEMIES 32 (9 965 B), LOOTS 64 (6 209 B), SKEL
  `n+nbow = 160` con `nbow = 32` (1 922 B). Se aceptan todos y caben en los 16 384 B de `send_msg`. [hipótesis]
- Valores que manda un juego honesto: `gear_bone ≤ 8` (`oa_k_bones` tiene 8); `pvp_target` empieza en 0;
  `weather` en 0..8 o 255 (`oa_weather_now`); `effects` dentro de la máscara; vida de jefe ≥ 0 (`done`); `dye` de
  −1 a 127; `owner` como ranura u `OWNER_NONE`; el origen del botín del anfitrión es `me`. Nombres: los que escribe
  `oa_actor_name_of` admiten de 0x21 a 0x7e. Todos los que aparecen en el código (Weapon_*, Armor_*, Enemy_*,
  Item_*, GameRomHorse*, Obj_IceMakerBlock) cumplen `[A-Za-z0-9_]`; queda **sin verificar** contra la lista completa
  de actores del juego. [hipótesis]

**T207 (`oa_*.c`, `online_actors_internal.h`)**
- `oa_check.py --antes 3370d18:runtime/online_actors.c --modulos "runtime/oa_*.c" --nombres ADR-023` da IGUALES
  (453 elementos), y T206 frente a `7b53d18` también da IGUALES. [sintética]
- La cabecera interna no define nada `static` ni datos (cada módulo tendría su copia). [hipótesis]
- `nm` de los 12 `oa_*.o` y los `online*.o` (`coop/main` y `coop/T208`): ningún símbolo definido dos veces,
  ningún `oa_*` indefinido sin definición y ningún `extern` de la cabecera sin definir. Todos los exportados
  empiezan por `oa_`, `rt_` u `online_`, salvo `g_online_block_a` y `g_game_buttons`, que ya eran globales y los
  declara `hle/input.c`. Ninguno de los 173 nombres exportados de T208 aparece en el resto de `runtime/` salvo los
  usos ya previstos (`input.c`, `control_game.c`, `online_ui.mm`, `vulkan/exec.c`). `online_auth.*`, nuevo en
  `coop/main`, no choca. [compilación]
- Los tipos de los `extern` coinciden con sus definiciones, porque cada módulo incluye la cabecera. [compilación]

**T208 (`coop/T208`)**
- Sustitución mecánica comprobada línea a línea en `oa_frame.c`: las 153 apariciones de `oa_g_act[i].` pasan a
  `oa_pup(i)->`, salvo una, que pasa a `oa_rune_grab(i)`. Fuera de `forget_all` no hay más cambios. [sintética]
- `forget_all` y `oa_drop` conservan el contenido y el orden. Piezas: armadura, equipo, caballo, aparejo, disparos,
  materiales y flecha, después pizarra, `rune_fx`, `stop_fx` y `magnet_fx`, y después el pool. En `oa_drop`, el
  borrado del Link, el LOG y el `memset` van en el mismo orden. `anim` y `covered` vuelven a −1. `g_ice` se limpia
  como antes con el `memset` de `g_act`. El tamaño total limpiado no cambia, solo se reparte. [hipótesis]
- No queda `g_act`, `oa_g_act` ni `OaAct` en `runtime/`, `tools/` ni `tests/` de la rama (`git grep`).
  [sintética]
- Compila limpio, con el mismo y único aviso que antes (K-7). [compilación]

**T202 (`online_wire.h`, `tools/online_bot.c`)**
- Los cambios de T202 en `online.c` son solo cortar definiciones y añadir `#include "online_wire.h"`. [hipótesis]
- La estructura del bot de antes (`345d060^`) frente a `WireState` de la cabecera: los 44 campos tienen el mismo
  desplazamiento y los cuatro tamaños coinciden (3375, 54, 311, 97). Lo comprobé con un programa de `offsetof`
  compilado aparte. `MAGIC`/`PROTOCOL` valen lo mismo que los literales `0x4c4f5753`/38, y los `M_*` siguen en el
  mismo orden. En el bot solo cambian tipos y punteros (`ws.shots` → `(uint8_t *)&ws.shots`,
  `ws.fxev[0]` → `&ws.fxev[0]`, `WireLoot`/`WireEnemy` → `OnlineLoot`/`OnlineEnemy`). Los bytes enviados son los
  mismos. [sintética; el bot no se compiló con `-lws2_32` ni se ejecutó]

## No verificado

- Nada de lo anterior se ha visto en el juego. Los escenarios de C-1, C-2, I-1, I-2, I-3 e I-4 son razonados.
- El comportamiento del juego con un id de animación o un color de tinte fuera de rango (C-1, C-2).
- Si el temporizador de la Luna Carmesí pasa de 4000 sin la prueba (I-1).
- La lista completa de nombres de actor del juego frente a `[A-Za-z0-9_]`.
