# ADR-026 · Inventario de las variables que guardan estado del juego en el código en línea (T211)

- Estado: aceptada con las resoluciones del orquestador sobre las dudosas (2026-11-25, abajo: "Resoluciones"); T212
  la aplica
- Tarea: T211 de `fases/F2-cimientos/tasks.md`; la usa T212 (`RT_ON_RESET` en todos los módulos)
- Código inventariado: `coop/T211` sobre `e6be965` (T210 integrada)
- Relacionadas: constitución, regla 8; [ADR-023](ADR-023-modulos-oa.md) (los módulos `oa_*`);
  [ADR-025](ADR-025-respuestas-del-dueno-ronda-6.md) n.º 2 (el reinicio cierra la sesión); `mapa/08`, HE-01

## Contexto

"Salir al inicio" (y el reinicio por idioma o mods) borra la memoria del juego y la vuelve a crear dentro del mismo
proceso; las variables del runtime sobreviven. `RT_ON_RESET(fn)` (`runtime/hle.h`) registra una función que corre en ese
reinicio con los hilos del juego ya parados y antes de borrar su memoria. El código en línea no tiene ninguna
(`mapa/08` §2.1, fila "Salir al inicio"). Esta tarea hace el inventario y la comprobación; T212 añade las limpiezas.

## Decisión

### Qué cuenta como variable

`tools/coop/oa_statics.py` (sin compilar: expresiones regulares sobre el texto sin comentarios, cadenas ni
preprocesador) encuentra:

- **De fichero**: toda definición de variable de nivel superior, **con o sin `static`**. Las 40 sin `static` son las
  que T207 exportó de un módulo a otro (`oa_g_pool`, `oa_g_own`, `oa_g_theirs`...; ADR-023) más tres que ya eran
  globales (`g_online`, `g_online_block_a`, `g_game_buttons`): guardan el mismo tipo de estado que las `static` y la
  regla 8 habla de "cada variable global o `static`". `tasks.md` dice "variables `static`"; se lee como "las que
  sobreviven al reinicio", que es su propósito. No cuentan `extern`, `typedef`, prototipos ni funciones (tampoco las
  que devuelven un puntero a función). Una variable declarada dos veces (`g_pose_lock` en `online.c`: definición
  tentativa y la de verdad) cuenta una vez.
- **De función**: toda declaración `static` dentro de una función. Un `static const` cuenta (y se permite).

Ficheros por defecto: `runtime/oa_*.c`, `runtime/online.c` y `runtime/online_actors.c` (sus tres listas de nombres
de red). Los demás `online_*.c` (`online_addr`, `online_auth`, `online_invite`, `online_netstat`,
`online_ratelimit`, `online_validate`) no tienen ninguna variable salvo una constante (`online_invite.c:ALPHABET`):
quedan fuera. De `coop_*.c`, solo `coop_rewards.c` tiene estado de partida (el registro de recompensas: `g_hash`,
`g_count`, `g_open`, `g_caught_up`, `g_need_newline`, `g_path`), pero **nadie lo llama todavía** (ninguna llamada a
`coop_ledger_*` en `runtime/`); su cabecera ya dice que la integración llamará a `coop_ledger_close` desde un
`RT_ON_RESET`. Queda fuera de la comprobación (T212 no puede tocarlo: no está en sus `files`); se anota para quien lo
integre. El resto de `coop_*.c` solo tiene constantes.

### Claves

`fichero:nombre` (de fichero) y `fichero:función:nombre` (de función). Si una función tiene dos estáticas con el mismo
nombre en bloques distintos, la segunda es `nombre#2`, la tercera `nombre#3` (por orden en el fichero).

### Clases

- **puntero** (limpiar): una dirección del juego (`uint64_t` usado como actor o unidad), un `Handle`, un `Watch`, un
  `Piece`, una dirección de una cadena del juego, o un puntero del anfitrión de máquina que puede quedar colgando.
- **partida** (limpiar): estado lógico de una partida o una sesión: listas, colas, índices, contadores (también los de
  avisos en el log y los de las pruebas por variable de entorno: un proceso recién arrancado empieza en 0), horas.
- **permitir** (en `tools/coop/oa_statics_permitidas.txt`, con su motivo): tabla cargada de un `.txt` del overlay (y su
  número de líneas y su caché, si solo sale del `.txt`), variable de entorno leída una vez, desplazamiento dentro de
  un objeto descubierto una vez (el mismo en cada arranque de 1.9.0), mutex, constante, ajustes del jugador
  (`g_online`: `hle.h` dice que los ajustes se quedan), inicio de la pila de red, y **búfer de trabajo** que se escribe
  entero antes de cada lectura **y no guarda direcciones del juego** (dudosa 2).
- Una `?` tras la clase marca una dudosa (abajo).

Nada que guarde una dirección del juego está en "permitir" (`no hagas` de T211): los búferes de trabajo que sí las
guardan iban como "puntero?" (dudosa 1); tras la resolución 1 son "puntero" (se limpian).

### Comprobación

    PY=tools/venv/Scripts/python.exe
    $PY tools/coop/oa_statics.py --autoprueba
    $PY tools/coop/oa_statics.py --listar
    $PY tools/coop/oa_statics.py --clasificacion specs/decisiones/ADR-026-inventario-estaticas.md
    $PY tools/coop/oa_statics.py            # modo comprobación: 1 hasta que T212 limpie las 295

El modo comprobación sale con 0 solo si cada variable está en la lista de permitidas o, siendo de fichero, aparece
**asignada** (`=`, `op=`, o `memset`/`memcpy`/`memmove` de ella) **dentro del cuerpo** de una función
`RT_ON_RESET(...)` **de su propio fichero**. Una llamada a otra función que la limpie (`stop_all`, `forget_all`) no
cuenta: la limpieza tiene que verse en el reset. Una estática de función no se puede limpiar desde fuera: o se permite
o se sube a nivel de fichero (T212 paso 2). También sale con 1 si la lista nombra una variable que ya no existe.
`--clasificacion` comprueba que la tabla de abajo clasifica el 100 % de las variables, sin sobrar ninguna, y que las
"permitir" son exactamente las de la lista.

`--autoprueba` usa un fichero sintético: estática de fichero, de función (también sangrada en un bloque interior y en
dos líneas), dos con el mismo nombre en una función, puntero a función que **sí** es variable, función que devuelve un
puntero a función (no es variable), función que devuelve un puntero (no), prototipo (no), `static const`, `struct`
anónimo en varias líneas con dos declaradores, `__attribute__` delante y detrás del nombre, definición tentativa,
`extern`, `typedef`, macro; y la comprobación (limpiada en el reset, permitida, pendiente, permitida que no existe,
limpiar en el reset de otro fichero no cuenta, `==` no es asignación) y la clasificación (completa, una sin
clasificar, "permitir" fuera de la lista).

## Recuento

376 variables: 135 de fichero (40 sin `static`) y 241 de función. 295 a limpiar (67 puntero, 228 partida) y 81 a
permitir. 5 dudosas.

| Fichero | de fichero | (sin `static`) | de función | puntero | partida | permitir | limpiar |
|---|---|---|---|---|---|---|---|
| `oa_control.c` | 3 | 0 | 12 | 3 | 9 | 3 | 12 |
| `oa_core.c` | 21 | 16 | 13 | 8 | 14 | 12 | 22 |
| `oa_enemies.c` | 16 | 6 | 49 | 8 | 40 | 17 | 48 |
| `oa_flags.c` | 1 | 1 | 29 | 1 | 21 | 8 | 22 |
| `oa_frame.c` | 3 | 0 | 1 | 0 | 4 | 0 | 4 |
| `oa_gamedata.c` | 0 | 0 | 3 | 2 | 0 | 1 | 2 |
| `oa_loot.c` | 9 | 1 | 8 | 3 | 12 | 2 | 15 |
| `oa_puppet.c` | 8 | 2 | 6 | 3 | 6 | 5 | 9 |
| `oa_rules.c` | 12 | 5 | 12 | 0 | 20 | 4 | 20 |
| `oa_runes.c` | 2 | 0 | 4 | 2 | 3 | 1 | 5 |
| `oa_sample.c` | 26 | 8 | 57 | 28 | 44 | 11 | 72 |
| `oa_world.c` | 14 | 0 | 32 | 9 | 30 | 7 | 39 |
| `online.c` | 17 | 1 | 15 | 0 | 25 | 7 | 25 |
| `online_actors.c` | 3 | 0 | 0 | 0 | 0 | 3 | 0 |
| **total** | 135 | 40 | 241 | 67 | 228 | 81 | 295 |

(`tasks.md` estimaba "del orden de 85 de fichero y unas 200 de función" contando solo `static`: hay 95 `static` de
fichero y 241 de función.)

## `online.c`: qué hilo usa cada una (HE-01 b)

Hilos: **juego** (`rt_online_frame` y los `oa_*`, que llaman a `online_set_local_*`, `online_take_*`,
`online_players`...), **red-c** (`cl_thread`: `sample_local`, `sample_equip`, `cl_message`, `remote_snap`,
`drop_note`, `motion_anim`), **red-s** (`srv_thread`: `srv_loop`), **menú** (`ui/online_ui.mm` y `main.c`:
`online_host`, `online_join`, `online_leave`, `stop_all`), **control** (`oa_control.c`: `online_leave`), **render**
(`nvn/vulkan/exec.c`: la cámara de `g`). Sacado de leer quién llama a quién [L]; la tabla de abajo lo repite en el
motivo de cada una.

| Variable | Hilos | Nota |
|---|---|---|
| `g_local_hour` | juego escribe, red-s lee | |
| `g_online` | menú, red-c, juego | ajustes: permitir |
| `net_init:done` | menú (y `main.c`) | permitir |
| `srv_loop:next_time` | red-s | |
| `g_srv`, `g_srv_thread` | menú, red-s | |
| `g_srv_running` | menú, juego (`online_is_host`) | |
| `g` | juego, red-c, menú, control, render | tiene `.lock` (mutex): no se borra entera |
| `motion_anim:cur`, `cand` | red-c | |
| `g_skel`, `g_skel_len`, `g_skel_t` | juego escribe, red-c lee (bajo `g.lock`) | |
| `g_pose_lock` | juego, red-c | mutex: permitir |
| `g_local_en`, `g_pose`, `g_pose_t`, `g_equip`, `g_equip_t` | juego escribe, red-c lee (bajo `g_pose_lock`) | |
| `sample_local:last`, `yaw`, `have`, `anim` | red-c | `sample_local` **lee la memoria del invitado** (`g_site.player_info`, `+0x88`) desde red-c: HE-01 (b), regla 6 |
| `g_drop_name` | red-c | constante: permitir |
| `g_drop_n` | red-c | |
| `cl_thread:lb`, `sb`, `eb` | red-c | búferes de envío: permitir |
| `cl_thread:next_loots`, `next_enemies`, `last`, `next_equip` | red-c | |

Ninguna estática de `online.c` guarda una dirección del juego (por eso ninguna es "puntero"); el problema de HE-01 (b)
es la **lectura** de `sample_local` en red-c, no lo que guarda. Con T218 (o quien arregle HE-01 b), `sample_local`
pasaría al hilo del juego y sus cuatro estáticas cambiarían de hilo.

## Para T212

1. **Orden en `online.c`** (ADR-025 n.º 2): su `RT_ON_RESET` manda LEAVE y para la sesión **antes** de limpiar: las
   estáticas de red-c y red-s las usan hilos del anfitrión de máquina que siguen vivos durante el reinicio (los del
   juego ya están parados; los de red no). `stop_all` ya une los dos hilos; después se puede limpiar sin carrera.
2. **`g` no se borra entera**: tiene `.lock` (`PTHREAD_MUTEX_INITIALIZER`) y `id = -1`. `stop_all` limpia casi todo;
   no limpia `state`, `status_fmt`/`status_arg`, la cámara (`vp`, `ysign`, `cam_t`), `ping`... (lo dice su comentario).
   La comprobación acepta cualquier asignación de un campo de `g` dentro del reset (`g.id = -1`), así que **no
   comprueba que estén todos**: revisarlo a mano.
3. **Valores iniciales que no son cero** (paso 4): `oa_control_tests_frame:last = -12345`, `oa_enemies_host:who = -1`,
   `oa_enemies_show:at = -1`, `g_next_gid = 1`, `g_revive_rule = -1`, `oa_g_nopause_rule = -1`,
   `sample_shots:next_id = 1`, `oa_sample_pose:was = -1`, `why = -1`, `oa_objs_update:held = -1`,
   `oa_movers_update:held = -1`, `g_local_hour = -1`, `g.id = -1`.
4. **Choques de nombre al subir estáticas de función** (paso 2 dice "con el mismo nombre": aquí no se puede). Mismo
   fichero, mismo nombre, todas a limpiar (o una de fichero que ya existe):
   `oa_control.c` `t` (2), `tick` (2); `oa_core.c` `saved` (2, si se limpian: dudosa 1); `oa_enemies.c` `t0` (4),
   `tick` (2 más la de fichero), `turn` (2); `oa_flags.c` `last` (2), `logged` (2), `wait` (2); `oa_loot.c` `tick`
   (2); `oa_runes.c` `told` (3); `oa_sample.c` `from` (2), `held` (2), `hw` (2), `last` (3), `seen` (2), `tick` (4),
   `tr` (2), `tw` (2); `oa_world.c` `held` (2), `t0` (3), `tick` (3), `told` (4); `online.c` `last` (2). Ver dudosa 3.
5. Las limpiezas que ya existen (`forget_all`, `stop_all`, `oa_loot_forget`...) no cuentan para la comprobación:
   el reset tiene que asignar cada variable él mismo (o T212 cambia la regla, con el dueño).

## Dudosas para el dueño

1. **Búferes de trabajo que guardan direcciones del juego** (5, marcadas "puntero?"): `oa_core.c:oa_call_f:saved` y
   `oa_call_sret:saved` (copia de los registros del hilo del juego), `oa_gamedata.c:oa_start_event:meta` y `req`, y
   `oa_flags.c:korok_event:meta` (memoria donde el juego monta un evento). Cada uno se escribe entero (o se borra) al
   entrar, antes de leerlo, y nunca se lee de una llamada a otra: limpiarlos no cambia nada. La regla de T211 ("no
   permitir algo que guarde una dirección del juego") obliga a "limpiar"; la propuesta es **permitirlos** como búfer de
   trabajo. Si se limpian, los dos `saved` chocan al subirlos (punto 4).
2. **La categoría "búfer de trabajo" sin direcciones del juego** (permitida; no está en la lista (c) de `tasks.md`):
   `oa_map_load:line`, `oa_enemies_host:st`, `oa_enemies_client:e` y `st`, `oa_loot_update:list`,
   `oa_ground_update:at`, `oa_puppet_frame:dp` y `name`, `oa_sample_skeleton:q` y `t`, `oa_objs_update:theirs` y
   `theirs_trunk`, `online.c` `cl_thread:lb`, `sb`, `eb`. ¿Se aprueba la categoría?
3. **Cómo subir las estáticas de función que chocan** (punto 4: 24 nombres, 23 sin los `saved`). Propuesta: subirlas como
   `<función>_<nombre>` (y `_2`, `_3` para las de bloques distintos), cambiando solo ese nombre dentro de su función
   (cambio mecánico, como el renombrado de T207). Alternativa: agrupar las de cada función en un `struct` de fichero.
   Las dos tocan el cuerpo, que T212 paso 2 dice no tocar.

## Resoluciones (orquestador, 2026-11-25; las aplica T212)

1. **Dudosa 1: limpiar.** Los cinco búferes de trabajo que guardan direcciones del juego (`oa_call_f:saved`,
   `oa_call_sret:saved`, `oa_start_event:meta` y `req`, `korok_event:meta`) no se permiten: se suben a nivel de
   fichero y se limpian en el reset. En la tabla pasan de "puntero?" a "puntero".
2. **Dudosa 2: permitir.** Se aprueba la categoría "búfer de trabajo" **sin** direcciones del juego, con su motivo en
   `oa_statics_permitidas.txt` (ya estaban allí).
3. **Dudosa 3: `<función>_<nombre>`.** Una estática de función que choca al subirla (con otra subida del mismo fichero
   o con una de fichero que ya existe) se sube como `<función>_<nombre>` (`_2`, `_3` para la segunda y la tercera de la
   misma función, por orden en el fichero), y dentro de su función solo cambia ese nombre; nada más del cuerpo. Las
   que no chocan conservan su nombre (T212 paso 2).
4. **Variables globales sin `static`** (`oa_g_pool`, `oa_g_own`, ...): cuentan, y se limpian igual que las `static`,
   en el reset del módulo que las define.

Al subirlas, la clave de cada una cambia de `fichero:función:nombre` a `fichero:nombre` (o `fichero:<función>_<nombre>`):
T212 cambia esas claves en la tabla de abajo (misma clase y motivo) para que `--clasificacion` siga cuadrando. Una
variable nueva desde T211 (`online.c:g_state_cleared_n`, de T222) se añade a la tabla como "partida".

## Riesgos y lo que no se ha verificado

- [sin verificar] El escáner es una heurística, no un compilador. Se contrastó con `grep` (toda línea con `static` que
  no aparece en el listado es una función o un prototipo, y los 39 nombres `extern` de
  `runtime/online_actors_internal.h` que son variables aparecen todos), pero no con `clang` ni `nm`.
- [H] Las tablas del overlay se permiten porque su ruta sale de una variable de entorno que no cambia en el proceso.
  Si un reinicio "por mods" pudiera regenerar `online_mod/` dentro del mismo proceso, habría que limpiarlas; no se ha
  encontrado código que lo haga.
- [L] La clasificación sale de leer cada declaración y su uso; nada se ha visto en el juego.

## Tabla

Una línea por variable: `clave  clase  motivo`. La lee `oa_statics.py --clasificacion`.

```
oa-estaticas:
# oa_control.c
oa_control.c:g_find                          puntero   petición de `actors` del hilo de control; guarda direcciones de actores (hit[].actor)
oa_control.c:g_flagq                         partida   petición de `flag`/`setflag` a medias: no debe contestarse en el arranque siguiente
oa_control.c:g_eventq                        partida   petición de `event` a medias: no debe arrancar un evento en el arranque siguiente
oa_control.c:online_actors_control_init:cmds permitir  tabla constante de órdenes de control
oa_control.c:oa_control_tests_frame:m        permitir  tabla cargada de enemies.txt (prueba ENEMY_TEST)
oa_control.c:oa_control_tests_frame:n        permitir  número de líneas de enemies.txt (va con m; -1 sin cargar)
oa_control.c:next                            partida   progreso de la prueba ENEMY_TEST
oa_control.c:oa_control_tests_frame_tick     partida   contador de la prueba ENEMY_TEST
oa_control.c:p                               puntero   Piece (actor del juego) de la prueba ENEMY_TEST
oa_control.c:seen_a                          puntero   direcciones de actores vistos (prueba LOOK_LOG)
oa_control.c:seen_v                          partida   valores leídos de esos actores (va con seen_a)
oa_control.c:oa_control_tests_frame_tick_2   partida   contador de la prueba LOOK_LOG
oa_control.c:oa_control_tests_frame_t        partida   contador de la prueba DIE_TEST
oa_control.c:last                            partida   última vida registrada (LIFE_LOG; -12345 al empezar)
oa_control.c:oa_control_tests_frame_t_2      partida   contador de la prueba KNOCK_TEST
# oa_core.c
oa_core.c:g_handles                          puntero   anillo de handles de petición de actores del juego
oa_core.c:g_next_handle                      partida   índice del anillo de handles (va con g_handles)
oa_core.c:oa_g_local_dead                    partida   el Link local está muerto
oa_core.c:oa_g_aggro_id                      partida   último enemigo del anfitrión golpeado
oa_core.c:oa_g_aggro_seq                     partida   número de ese golpe
oa_core.c:oa_g_objs_out                      partida   objetos del mundo que se mueven cerca (lista de salida)
oa_core.c:oa_g_objs_out_trunk                partida   bits de tronco de esa lista
oa_core.c:oa_g_movers_out                    partida   mecanismos que se mueven cerca (lista de salida)
oa_core.c:oa_g_broken                        partida   objetos del mapa que rompimos
oa_core.c:oa_g_trunks_gone                   partida   troncos que cortamos
oa_core.c:oa_g_online_actor                  partida   qué ranuras tienen figura (online_has_actor; lo lee el overlay)
oa_core.c:oa_actor_name:mod                  permitir  si el overlay tiene online_mod.txt: fichero del overlay mirado una vez (la ruta sale de una variable de entorno)
oa_core.c:online_actors_on:on                permitir  versión 1.9.0 y overlay, mirados una vez: no cambian en el proceso
oa_core.c:oa_call_f_saved                    puntero   copia de los registros del hilo del juego (direcciones del juego); búfer de trabajo, pero se limpia (resolución 1)
oa_core.c:oa_call_sret_saved                 puntero   como oa_call_f:saved
oa_core.c:oa_g_pool                          puntero   piezas (actores) guardadas de cada jugador
oa_core.c:g_orphan                           puntero   peticiones huérfanas (handles con unit del juego)
oa_core.c:oa_g_pool_at                       partida   posición bajo el Link donde se esconden las piezas
oa_core.c:g_gear_map                         permitir  tabla cargada de gear.txt
oa_core.c:g_gear_n                           permitir  número de líneas de gear.txt (-1 sin cargar)
oa_core.c:oa_map_load:line                   permitir  búfer de lectura de línea: se escribe antes de cada lectura
oa_core.c:oa_armor_actor:m                   permitir  tabla cargada de armor.txt
oa_core.c:oa_armor_actor:n                   permitir  número de líneas de armor.txt (-1 sin cargar)
oa_core.c:oa_armor_bones:m                   permitir  tabla cargada de bones.txt
oa_core.c:oa_armor_bones:n                   permitir  número de líneas de bones.txt (-1 sin cargar)
oa_core.c:oa_armor_bones:cache               permitir  caché de mapas de huesos sacados solo de bones.txt (nada del juego)
oa_core.c:oa_armor_bones:next                permitir  siguiente hueco de esa caché
oa_core.c:oa_g_dist                          partida   distancia del jugador que se está procesando
oa_core.c:oa_g_pvp                           partida   último golpe PvP
oa_core.c:oa_g_piece_params                  puntero   puntero a los parámetros de la pieza siguiente (una local de oa_puppet; 0 fuera de ella)
oa_core.c:glider_of:off                      permitir  desplazamiento del parapente dentro del jugador, descubierto una vez (el mismo en cada arranque de 1.9.0)
oa_core.c:miss                               partida   contador de reintentos de esa búsqueda
oa_core.c:oa_g_actors                        puntero   lista de actores del juego (oa_all_actors)
oa_core.c:oa_g_theirs                        puntero   botín de los otros hecho aquí (Piece)
# oa_gamedata.c
oa_gamedata.c:meta                           puntero   memoria donde el juego monta los datos del evento (direcciones del juego); búfer de trabajo, pero se limpia (resolución 1)
oa_gamedata.c:req                            puntero   petición de evento con el actor (req.actor); búfer de trabajo, pero se limpia (resolución 1)
oa_gamedata.c:oa_give_heart:name             permitir  cadena constante
# oa_enemies.c
oa_enemies.c:enemy_actor:m                   permitir  tabla cargada de enemies.txt
oa_enemies.c:enemy_actor:n                   permitir  número de líneas de enemies.txt (-1 sin cargar)
oa_enemies.c:oa_g_refused_enemy              partida   nombres de enemigo rechazados: cuenta y hora del último aviso
oa_enemies.c:oa_g_refused_weapon             partida   igual, armas
oa_enemies.c:oa_g_refused_loot               partida   igual, botín
oa_enemies.c:enemy_weapon:off                permitir  desplazamiento del arma dentro de un enemigo, descubierto una vez (el mismo en cada arranque de 1.9.0)
oa_enemies.c:miss                            partida   contador de reintentos de esa búsqueda
oa_enemies.c:k_hands                         permitir  tabla constante
oa_enemies.c:oa_g_own                        puntero   enemigos nuestros (actor, Watch)
oa_enemies.c:g_own_left                      puntero   enemigos que dejamos (actor, Watch)
oa_enemies.c:g_esrc                          puntero   fuentes de elemento hechas aquí (Piece)
oa_enemies.c:k_elem_source                   permitir  tabla constante
oa_enemies.c:g_reg                           partida   registro de enemigos del anfitrión (ids, dueños, estado)
oa_enemies.c:oa_g_reg_out                    partida   lista de enemigos enviada la última vez
oa_enemies.c:oa_g_reg_n                      partida   su longitud
oa_enemies.c:mark_dead:maps                  permitir  tabla constante
oa_enemies.c:oa_enemies_host:st              permitir  búfer de trabajo: online_take_estates lo llena antes de cada lectura
oa_enemies.c:oa_enemies_host_t0              partida   hora de inicio de la prueba KILL_TEST
oa_enemies.c:done                            partida   prueba KILL_TEST hecha
oa_enemies.c:oa_enemies_host_t0_2            partida   hora de inicio de la prueba ELEMENT_TEST
oa_enemies.c:until                           partida   prueba ELEMENT_TEST
oa_enemies.c:next                            partida   prueba ELEMENT_TEST
oa_enemies.c:src                             puntero   Piece (actor) de la prueba ELEMENT_TEST
oa_enemies.c:who                             partida   prueba ELEMENT_TEST (-1 al empezar)
oa_enemies.c:last_life                       partida   prueba ELEMENT_TEST
oa_enemies.c:oa_enemies_host_turn            partida   prueba ELEMENT_TEST
oa_enemies.c:used                            partida   prueba ELEMENT_TEST
oa_enemies.c:name                            partida   prueba ELEMENT_TEST
oa_enemies.c:last_as                         partida   prueba ELEMENT_TEST
oa_enemies.c:could                           partida   si cada jugador podía tener enemigos (para el log)
oa_enemies.c:aggro_seen                      partida   último número de golpe visto de cada jugador
oa_enemies.c:aggro_init                      partida   va con aggro_seen
oa_enemies.c:g_puppet                        puntero   copias de enemigos del anfitrión (Piece y efecto)
oa_enemies.c:weapon_scale:m                  permitir  tabla cargada de wscale.txt
oa_enemies.c:weapon_scale:n                  permitir  número de líneas de wscale.txt (-1 sin cargar)
oa_enemies.c:oa_attack_power:m               permitir  tabla cargada de un .txt del overlay
oa_enemies.c:oa_attack_power:n               permitir  número de líneas (-1 sin cargar)
oa_enemies.c:oa_our_defense:m                permitir  tabla cargada de un .txt del overlay
oa_enemies.c:oa_our_defense:n                permitir  número de líneas (-1 sin cargar)
oa_enemies.c:oa_enemies_client:e             permitir  búfer de trabajo: online_enemies lo llena antes de cada lectura
oa_enemies.c:oa_enemies_client_tick          partida   contador
oa_enemies.c:oa_enemies_client:st            permitir  búfer de trabajo: se escribe entrada a entrada antes de enviarlo
oa_enemies.c:gone_logged                     partida   contador de avisos en el log
oa_enemies.c:marked                          partida   enemigos ya marcados como muertos
oa_enemies.c:nm                              partida   índice de marked
oa_enemies.c:slog2                           partida   contador de log (SCALE_LOG)
oa_enemies.c:plog                            partida   contador de log (PUPPET_LOG)
oa_enemies.c:oa_enemies_show:ht              permitir  variable de entorno ELEM_HIT_TEST leída una vez
oa_enemies.c:oa_enemies_show_t0              partida   prueba ELEM_HIT_TEST
oa_enemies.c:oa_enemies_show_turn            partida   prueba ELEM_HIT_TEST
oa_enemies.c:done_ids                        partida   prueba ELEM_HIT_TEST
oa_enemies.c:logged                          partida   prueba ELEM_HIT_TEST
oa_enemies.c:oa_enemies_show:ft              permitir  variable de entorno FIRE_TEST leída una vez
oa_enemies.c:oa_enemies_show_t0_2            partida   prueba FIRE_TEST
oa_enemies.c:fire                            puntero   Piece (actor) de la prueba FIRE_TEST
oa_enemies.c:placed                          partida   prueba FIRE_TEST
oa_enemies.c:at                              partida   prueba FIRE_TEST (-1 al empezar)
oa_enemies.c:g_boss                          puntero   jefes cerca (actor, Watch)
oa_enemies.c:g_btrack                        puntero   seguimiento de la vida de los jefes (actor)
oa_enemies.c:g_bseen                         partida   daño de cada jugador a cada jefe, visto la última vez
oa_enemies.c:oa_boss_sample_tick             partida   contador
oa_enemies.c:t                               partida   prueba BOSS_HIT/BOSS_KILL
oa_enemies.c:last                            partida   prueba BOSS_HIT/BOSS_KILL
oa_enemies.c:logt                            partida   contador de log (BOSS_LOG)
oa_enemies.c:tick                            partida   contador de oa_enemies_frame
# oa_flags.c
oa_flags.c:tested                            partida   prueba BM_TEST hecha
oa_flags.c:oa_bloody_moon_follow_wait        partida   prueba BM_TEST
oa_flags.c:oa_bloody_moon_follow_logged      partida   contador de avisos en el log
oa_flags.c:oa_weather_follow_logged          partida   contador de avisos en el log
oa_flags.c:tries                             partida   intentos del evento de la tableta
oa_flags.c:oa_slate_event_wait               partida   espera entre intentos
oa_flags.c:slate_rune:runes                  permitir  tabla constante
oa_flags.c:korok_event:off                   permitir  desplazamiento del HashId dentro de un actor, descubierto una vez (el mismo en cada arranque de 1.9.0)
oa_flags.c:meta                              puntero   memoria donde el juego monta los datos del evento (direcciones del juego); búfer de trabajo, pero se limpia (resolución 1)
oa_flags.c:oa_g_fake_blood_moon              partida   orden `bloodmoon` pendiente (hilo de control → fotograma)
oa_flags.c:oa_quests_update:m                permitir  tabla de oa_map_load que no se usa (siempre 0)
oa_flags.c:oa_quests_update:names            permitir  tabla cargada de quests.txt
oa_flags.c:oa_quests_update:hash             permitir  tabla cargada de quests.txt (CRC de cada nombre)
oa_flags.c:oa_quests_update:revival          permitir  tabla cargada de quests.txt (qué flags renacen con la Luna de Sangre: sale solo del nombre)
oa_flags.c:held_off                          partida   flags retenidos tras una Luna de Sangre
oa_flags.c:had                               partida   flags que tenía este juego en la última vuelta
oa_flags.c:oa_quests_update:n                permitir  número de líneas de quests.txt (-1 sin cargar)
oa_flags.c:tick                              partida   contador
oa_flags.c:oa_quests_update:kinds            permitir  tabla constante
oa_flags.c:counted                           partida   diagnóstico de flags hecho una vez por partida (mira los datos del juego)
oa_flags.c:had_once                          partida   va con had
oa_flags.c:oa_quests_update_last             partida   últimos flags enviados (anfitrión)
oa_flags.c:sent                              partida   enviados al menos una vez (anfitrión)
oa_flags.c:oa_quests_update_last_2           partida   últimos flags enviados (invitado)
oa_flags.c:every                             partida   contador de reenvío (invitado)
oa_flags.c:slate_started                     partida   evento de la tableta arrancado
oa_flags.c:korok_played                      partida   escenas de Korok ya lanzadas
oa_flags.c:korok_np                          partida   índice de korok_played
oa_flags.c:wait_h                            partida   Koroks esperando escena
oa_flags.c:wait_n                            partida   intentos de cada uno
# oa_frame.c
oa_frame.c:g_last_link                       partida   última posición del Link local
oa_frame.c:g_settle                          partida   fotogramas de espera tras una carga
oa_frame.c:was_dead                          partida   el Link local estaba muerto el fotograma anterior
oa_frame.c:st                                partida   contador de la prueba SLATE_TEST
# oa_loot.c
oa_loot.c:g_death                            partida   muertes vigiladas (posición, fotogramas, ids de antes)
oa_loot.c:g_mine                             puntero   botín nuestro (actor, Watch)
oa_loot.c:oa_g_carried_now                   puntero   lo que lleva este Link (direcciones de actores)
oa_loot.c:g_loot_list                        partida   lista de botín del anfitrión
oa_loot.c:g_nloot_list                       partida   su longitud
oa_loot.c:g_next_gid                         partida   siguiente id de botín (1 al empezar)
oa_loot.c:g_drop_seen                        partida   lo que se vio caer (ids, hora, posición)
oa_loot.c:g_drop_hush                        partida   zonas silenciadas un rato
oa_loot.c:oa_loot_update_tick                partida   contador
oa_loot.c:dt                                 partida   contador
oa_loot.c:was                                partida   posición anterior del Link
oa_loot.c:base                               partida   primera mirada hecha
oa_loot.c:oa_loot_update:list                permitir  búfer de trabajo: se copia o lo llena online_loots antes de cada lectura
oa_loot.c:g_ground                           puntero   lo que hay en el suelo (actor, Watch)
oa_loot.c:oa_ground_update_tick              partida   contador
oa_loot.c:wlog                               partida   contador de log (WEAPONS_LOG)
oa_loot.c:oa_ground_update:at                permitir  búfer de trabajo: posiciones de esta llamada, escritas antes de leerlas
# oa_puppet.c
oa_puppet.c:g_pup                            puntero   Links de los otros (Link, piezas, efectos)
oa_puppet.c:k_anims                          permitir  tabla constante
oa_puppet.c:oa_g_blast                       partida   explosiones recientes de los otros
oa_puppet.c:oa_g_blur                        puntero   estela de cada títere (actor)
oa_puppet.c:g_boom                           puntero   explosiones hechas aquí (Handle, actor, Watch)
oa_puppet.c:g_pose_q                         partida   esqueleto con el que se posó el Link
oa_puppet.c:g_pose_t                         partida   va con g_pose_q
oa_puppet.c:g_pose_n                         partida   va con g_pose_q
oa_puppet.c:last                             partida   último esqueleto recibido de cada jugador
oa_puppet.c:oa_puppet_frame:dp               permitir  búfer de trabajo: se borra antes de cada uso (parámetros de tinte)
oa_puppet.c:oa_puppet_frame:name             permitir  búfer de trabajo: se escribe antes de cada uso
oa_puppet.c:oa_puppet_frame:kinds            permitir  tabla constante
oa_puppet.c:n                                partida   contador de log (GEAR_LOG)
oa_puppet.c:oa_puppet_frame:heap             permitir  tabla constante
# oa_rules.c
oa_rules.c:oa_g_nopause_now                  partida   hay un menú sin pausa abierto (lo leen rt_scene_state y el backend)
oa_rules.c:oa_g_nopause_frames               partida   fotogramas con ese menú
oa_rules.c:last                              partida   último AS de cada enemigo nuestro
oa_rules.c:wait                              partida   espera de golpe
oa_rules.c:arrow                             partida   flecha
oa_rules.c:arrow_d                           partida   flecha
oa_rules.c:shot                              partida   flecha
oa_rules.c:oa_own_strikes_in_menu:log        permitir  variable de entorno MENU_LOG leída una vez
oa_rules.c:g_revive_rule                     partida   regla de hada de la sesión (-1 fuera de sesión)
oa_rules.c:oa_g_nopause_rule                 partida   regla de menús sin pausa de la sesión (-1 fuera de sesión)
oa_rules.c:rt_online_menu_live:still         permitir  variable de entorno MENU_STILL leída una vez
oa_rules.c:rt_menu_draw_pause:still          permitir  variable de entorno MENU_STILL leída una vez
oa_rules.c:oa_players_collide:off            permitir  variable de entorno NO_COLLIDE leída una vez
oa_rules.c:swing_seen                        partida   hora de la última estela vista
oa_rules.c:seen                              partida   último golpe PvP visto de cada jugador
oa_rules.c:init                              partida   va con seen
oa_rules.c:g_online_block_a                  partida   A retenido (lo lee runtime/hle/input.c): sin limpiar, A seguiría bloqueado tras el reinicio
oa_rules.c:g_game_buttons                    partida   botones que se dan al juego (lo escribe runtime/hle/input.c)
oa_rules.c:logged                            partida   contador de log de oa_rules_nopause_frame
oa_rules.c:held_stop                         partida   estado de oa_rules_nopause_frame
oa_rules.c:x_was                             partida   estado de oa_rules_nopause_frame
oa_rules.c:seen_ps                           partida   estados de pausa vistos
oa_rules.c:told                              partida   contador de log
oa_rules.c:dlog                              partida   contador de log de oa_rules_block_a_frame
# oa_runes.c
oa_runes.c:g_rune                            puntero   runas de los otros (actores, Watch, Piece, efectos)
oa_runes.c:g_ice                             puntero   pilares de hielo de los otros (Piece)
oa_runes.c:chains_place_told                 partida   contador de log
oa_runes.c:oa_stop_update_told               partida   contador de log
oa_runes.c:oa_grab_update:kn                 permitir  cadena constante
oa_runes.c:oa_grab_update_told               partida   contador de log
# oa_sample.c
oa_sample.c:oa_k_channel                     permitir  tabla constante
oa_sample.c:oa_k_bones                       permitir  tabla constante
oa_sample.c:oa_k_shot_names                  permitir  tabla constante
oa_sample.c:g_arrow_ring                     puntero   flechas en vuelo (direcciones de actores; las escribe un gancho del juego)
oa_sample.c:g_arrow_next                     partida   índice de ese anillo
oa_sample.c:g_bomb_made                      puntero   bombas de la runa (direcciones de actores)
oa_sample.c:g_bomb_next                      partida   índice
oa_sample.c:g_fx_ui                          puntero   instancias de ELink vigiladas (direcciones del juego)
oa_sample.c:g_fx_ring                        puntero   efectos emitidos (name: dirección de una cadena del juego)
oa_sample.c:g_fx_next                        partida   índice
oa_sample.c:g_fx_taken                       partida   índice leído
oa_sample.c:g_fxev                           partida   eventos de efecto para enviar
oa_sample.c:g_kago_wind                      partida   último efecto de Campeón
oa_sample.c:g_fxev_seq                       partida   número de evento
oa_sample.c:g_fx_trail                       partida   estela del arma ahora
oa_sample.c:oa_g_fx_trail_t                  partida   cuándo empezó
oa_sample.c:oa_g_rf                          puntero   actor ReactionField
oa_sample.c:oa_g_rf_ui                       puntero   su ELink
oa_sample.c:oa_g_rf_watch                    puntero   su Watch
oa_sample.c:g_rf_ring                        puntero   reacciones del suelo (name: dirección del juego)
oa_sample.c:g_rf_props                       partida   últimas propiedades
oa_sample.c:g_rf_next                        partida   índice
oa_sample.c:g_rf_taken                       partida   índice leído
oa_sample.c:rt_online_xlink_emit:all         permitir  variable de entorno FX_ALL leída una vez
oa_sample.c:rt_game_fmt:on                   permitir  variable de entorno EFF_ERR_LOG leída una vez
oa_sample.c:rt_game_fmt:n                    partida   contador de log
oa_sample.c:continuous_sound:known           permitir  tabla constante
oa_sample.c:continuous_sound:seen            partida   sonidos vistos (hash, hora)
oa_sample.c:continuous_sound:next            partida   índice
oa_sample.c:oa_fx_update:rf_tick             partida   contador
oa_sample.c:oa_fx_update:rf_was              partida   posición anterior
oa_sample.c:oa_fx_update:rf_log              permitir  variable de entorno RF_LOG leída una vez
oa_sample.c:oa_fx_update:rf_last             partida   última reacción
oa_sample.c:oa_fx_update:rf_last_t           partida   su hora
oa_sample.c:oa_fx_update:who                 permitir  tabla constante
oa_sample.c:g_elem_seen                      partida   elementos vistos por disparo
oa_sample.c:sample_shots:tr                  puntero   disparos seguidos (actor, Watch)
oa_sample.c:sample_shots:next_id             partida   siguiente id de disparo (1 al empezar)
oa_sample.c:sample_shots:boom_seq            partida   número de explosión
oa_sample.c:sample_shots:booms               partida   explosiones recientes
oa_sample.c:sample_shots:bomb_seen           puntero   bombas ya vistas (direcciones de actores)
oa_sample.c:sample_shots:seen                puntero   flechas ya vistas (direcciones de actores)
oa_sample.c:sample_shots:not_his             puntero   flechas que no son suyas (actor)
oa_sample.c:sample_shots:not_his_next        partida   índice
oa_sample.c:sample_shots:bow_t               partida   hora del arco
oa_sample.c:sample_shots:shoot_t             partida   hora del disparo
oa_sample.c:sample_held:tr                   puntero   lo que lleva (actor, Watch)
oa_sample.c:sample_held:tick                 partida   contador
oa_sample.c:oa_g_map                         partida   mapa actual
oa_sample.c:oa_sample_pose:held              puntero   arma en la mano (actor)
oa_sample.c:oa_sample_pose:thrown            puntero   arma lanzada (actor)
oa_sample.c:oa_sample_pose:listed_at         partida   hora
oa_sample.c:oa_sample_pose:hw                puntero   Watch de held
oa_sample.c:oa_sample_pose:tw                puntero   Watch de thrown
oa_sample.c:oa_sample_pose:last              partida   última posición del arma lanzada
oa_sample.c:oa_sample_pose:still_since       partida   hora
oa_sample.c:oa_sample_pose:had               puntero   escudo y arco que tenía (actores)
oa_sample.c:oa_sample_pose:let_go            puntero   los que soltó (actores)
oa_sample.c:oa_sample_pose:since             partida   hora
oa_sample.c:oa_sample_pose:hw2               puntero   Watch de had
oa_sample.c:oa_sample_pose:lw                puntero   Watch de let_go
oa_sample.c:oa_sample_pose:last_bone         partida   último hueso de cada pieza
oa_sample.c:oa_sample_pose:logged            partida   log hecho
oa_sample.c:oa_sample_pose:was               partida   pose válida el fotograma anterior (-1 al empezar)
oa_sample.c:oa_sample_pose:pedestals         permitir  tabla constante
oa_sample.c:oa_sample_pose:held#2            puntero   lo que sostiene con Magnesis (actor)
oa_sample.c:oa_sample_pose:from              partida   de dónde lo cogió
oa_sample.c:oa_sample_pose:stopped           puntero   lo que paró con Stasis (actor)
oa_sample.c:oa_sample_pose:sw                puntero   su Watch
oa_sample.c:oa_sample_pose:from#2            partida   de dónde
oa_sample.c:oa_sample_pose:tick              partida   contador
oa_sample.c:oa_sample_pose:last#2            partida   matrices de sus pilares de hielo
oa_sample.c:oa_sample_pose:n_last            partida   cuántos
oa_sample.c:oa_sample_pose:tick#2            partida   contador
oa_sample.c:oa_sample_pose:horse             puntero   caballo que monta (actor)
oa_sample.c:oa_sample_pose:tack              puntero   sus arreos (actores)
oa_sample.c:oa_sample_pose:hw#2              puntero   Watch del caballo
oa_sample.c:oa_sample_pose:tw#2              puntero   Watch de los arreos
oa_sample.c:oa_sample_pose:tick#3            partida   contador
oa_sample.c:oa_sample_pose:last#3            partida   última pose registrada (POSE_LOG)
oa_sample.c:oa_sample_skeleton:q             permitir  búfer de trabajo: se escribe hueso a hueso antes de enviarlo
oa_sample.c:oa_sample_skeleton:t             permitir  búfer de trabajo: como q
oa_sample.c:why                              partida   por qué no se muestrea (-1 al empezar)
# oa_world.c
oa_world.c:g_break_test_t                    partida   cuándo cortó su árbol la prueba BREAK_TEST
oa_world.c:g_obj_seen                        puntero   objetos del mundo seguidos (actor, Watch)
oa_world.c:obj_kind:fixed                    permitir  tabla constante
oa_world.c:obj_find:last                     partida   hora de la última búsqueda
oa_world.c:oa_objs_update:tick               partida   contador
oa_world.c:oa_objs_update:ot                 permitir  variable de entorno OBJ_TEST leída una vez
oa_world.c:oa_objs_update:t0                 partida   prueba OBJ_TEST
oa_world.c:oa_objs_update:held               partida   prueba OBJ_TEST (-1 al empezar)
oa_world.c:oa_objs_update:told               partida   log hecho
oa_world.c:oa_objs_update:to                 partida   prueba OBJ_TEST: destino
oa_world.c:oa_objs_update:theirs             permitir  búfer de trabajo: se llena en esta llamada antes de leerlo (nt)
oa_world.c:oa_objs_update:theirs_trunk       permitir  búfer de trabajo: como theirs
oa_world.c:oa_objs_update:told#2             partida   contador de log
oa_world.c:oa_objs_update:logged             partida   contador de log
oa_world.c:g_mv                              puntero   mecanismos seguidos (actor, Watch)
oa_world.c:mover_kind:kinds                  permitir  tabla constante
oa_world.c:oa_movers_update:tick             partida   contador
oa_world.c:oa_movers_update:log_on           permitir  variable de entorno MOVER_LOG leída una vez
oa_world.c:oa_movers_update:rescan           partida   volver a buscar
oa_world.c:oa_movers_update:mt               permitir  variable de entorno MOVER_TEST leída una vez
oa_world.c:oa_movers_update:t0               partida   prueba MOVER_TEST
oa_world.c:oa_movers_update:held             partida   prueba MOVER_TEST (-1 al empezar)
oa_world.c:oa_movers_update:from             partida   prueba MOVER_TEST
oa_world.c:oa_movers_update:held_h           partida   prueba MOVER_TEST
oa_world.c:g_brk                             puntero   lo que se puede romper cerca (actor, Watch)
oa_world.c:g_our_cut                         puntero   nuestros cortes (tronco: actor, Watch)
oa_world.c:g_spawn                           puntero   peticiones en curso (Handle)
oa_world.c:g_spawn_ours                      partida   el próximo tronco es nuestro
oa_world.c:g_their_trunk                     puntero   troncos de los otros hechos aquí (actor, Watch)
oa_world.c:g_breaking                        puntero   lo roto aquí para otro (actor, Watch)
oa_world.c:g_cutting                         puntero   lo que se está cortando (actor, Watch)
oa_world.c:g_world                           puntero   lo roto en el mundo (HashId, actor, Watch)
oa_world.c:g_nworld                          partida   su longitud
oa_world.c:g_widx                            partida   su índice por HashId
oa_world.c:g_broken_new_t                    partida   hora de la última rotura
oa_world.c:world_rotate:next                 partida   siguiente a enviar
oa_world.c:world_rotate:at                   partida   hora
oa_world.c:oa_breaks_update:tick             partida   contador
oa_world.c:oa_breaks_update:was              partida   posición anterior
oa_world.c:oa_breaks_update:told             partida   HashIds ya avisados
oa_world.c:oa_breaks_update:ntold            partida   índice
oa_world.c:oa_breaks_update:d0               partida   prueba DROP_TEST
oa_world.c:oa_breaks_update:dropped          partida   prueba DROP_TEST
oa_world.c:oa_breaks_update:t0               partida   prueba BREAK_TEST
oa_world.c:oa_breaks_update:tested           partida   prueba BREAK_TEST
oa_world.c:oa_breaks_update:told#2           partida   log hecho
# online.c (hilos: juego = rt_online_frame y oa_*; red-c = cl_thread; red-s = srv_thread; menú = online_ui.mm y main.c; control = oa_control; render = vulkan/exec.c)
online.c:g_local_hour                        partida   hora local (-1 al empezar). Hilos: juego escribe (online_set_local_hour), red-s lee (srv_loop)
online.c:g_online                            permitir  ajustes del jugador (fichero de ajustes; hle.h: los ajustes se quedan). Hilos: menú, red-c, juego (oa_rules)
online.c:net_init:done                       permitir  inicio de la pila de red, una vez por proceso. Hilos: menú (y main.c por línea de órdenes)
online.c:srv_loop:next_time                  partida   próxima difusión de la hora. Hilo: red-s
online.c:g_srv                               partida   estado del servidor (sockets, clientes). Hilos: red-s, menú (online_host, stop_all)
online.c:g_srv_thread                        partida   hilo del servidor (se une en stop_all). Hilo: menú
online.c:g_srv_running                       partida   servidor en marcha. Hilos: menú, juego (online_is_host)
online.c:g                                   partida   sesión del cliente (jugadores, colas, chat, cámara); NO su .lock (mutex). Hilos: juego, red-c, menú, control, render
online.c:motion_anim:cur                     partida   animación de movimiento. Hilo: red-c (sample_local)
online.c:motion_anim:cand                    partida   candidata. Hilo: red-c
online.c:g_skel                              partida   esqueleto local codificado. Hilos: juego escribe, red-c lee
online.c:g_skel_len                          partida   va con g_skel. Hilos: juego, red-c
online.c:g_skel_t                            partida   va con g_skel. Hilos: juego, red-c
online.c:g_pose_lock                         permitir  mutex. Hilos: juego, red-c
online.c:g_local_en                          partida   enemigos locales para enviar. Hilos: juego escribe, red-c lee
online.c:g_pose                              partida   pose local para enviar. Hilos: juego escribe, red-c lee (sample_local)
online.c:g_pose_t                            partida   su hora. Hilos: juego, red-c
online.c:sample_local:last                   partida   última posición. Hilo: red-c (sample_local lee la memoria del invitado: HE-01 b)
online.c:sample_local:yaw                    partida   rumbo. Hilo: red-c
online.c:sample_local:have                   partida   hay posición anterior. Hilo: red-c
online.c:sample_local:anim                   partida   animación. Hilo: red-c
online.c:g_equip                             partida   equipo local para enviar. Hilos: juego escribe, red-c lee
online.c:g_equip_t                           partida   su hora. Hilos: juego, red-c
online.c:g_drop_name                         permitir  tabla constante. Hilo: red-c
online.c:g_drop_n                            partida   mensajes descartados por tipo. Hilo: red-c (cl_message)
online.c:g_state_cleared_n                   partida   entradas de STATE anuladas por la validación (T222, después de T211). Hilo: red-c (cl_message)
online.c:cl_thread:lb                        permitir  búfer de envío: se escribe antes de cada envío. Hilo: red-c
online.c:cl_thread:next_loots                partida   próximo envío de LOOTS. Hilo: red-c
online.c:cl_thread:sb                        permitir  búfer de envío. Hilo: red-c
online.c:cl_thread:next_enemies              partida   próximo envío de ENEMIES. Hilo: red-c
online.c:cl_thread:eb                        permitir  búfer de envío. Hilo: red-c
online.c:cl_thread:last                      partida   último equipo enviado. Hilo: red-c
online.c:cl_thread:next_equip                partida   próximo envío de EQUIP. Hilo: red-c
# online_actors.c
online_actors.c:g_weapon_names               permitir  tabla cargada de weapons.txt (lista de nombres de red permitidos)
online_actors.c:g_item_names                 permitir  tabla cargada de items.txt
online_actors.c:g_rupee_names                permitir  tabla cargada de rupees.txt
fin
```
