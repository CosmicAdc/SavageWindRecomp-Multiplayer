# F2 — Cimientos · plan

Referencias `fichero:línea`: commit `350c5f8`, comprobadas leyendo el código. Nada de este plan se ha ejecutado:
duraciones **estimadas**. Convención de rutas y de shell: como en F0 y F1 (`$SW`, clon en `$SW/coop`, MSYS2 UCRT64,
`PY=tools/venv/Scripts/python.exe`).

## 1. Estructura de la fase

```
FASE 0 (en serie dentro de cada cadena; las dos cadenas, en paralelo entre sí)

  Cadena NET (un dueño, un worktree)            Cadena OA (un dueño, un worktree)
  T202 cabecera de cable + bot                  T201 firma de comportamiento (referencia)
  T203 pruebas de trama y codificación          T206 reordenar en un solo fichero
  T204 pruebas de sesión (servidor, cliente)    T207 cortar en módulos oa_*.c
  T213 capa única de validación                 T208 partir g_act[] por dueño
        │                                       T209 orden explícito en rt_online_frame
        │      T205 [P] CI (coop.yml)           T210 Links remotos validados al principio
        │                                       T211 inventario de estáticas
        │                                       T212 RT_ON_RESET
        └──────────────┬────────────────────────┘
                       ▼
                 T214 puerta de la fase 0 (integrar, recorrido, etiqueta f2-fase0)
                       ▼
CARRILES (en paralelo, un worktree cada uno, ficheros disjuntos)
  T215 oa_world.c   T216 oa_enemies.c + oa_loot.c   T217 oa_runes.c
  T218 online.c     T219 bot + Docker               T220 traducciones (i18n/*.inc)
                       ▼
                 T221 integración final y cierre
```

**Por qué la cadena OA es estrictamente en serie:** todas sus tareas reescriben el mismo código (primero un fichero
de 6 468 líneas, luego los mismos módulos). Dos agentes a la vez producirían conflictos en casi cada línea.

**Por qué NET y OA sí pueden ir a la vez:** `online_actors.c` no abre sockets; habla con `online.c` solo a través
de las funciones de `runtime/online.h` (mapa 07 §3.7), que en F2 no cambian. Único fichero común: el `Makefile`
(bloques de prueba al final; integra un dueño).

## 2. Cadena NET

### 2.1 Cabecera de cable compartida (T202)

Hoy el formato de los mensajes está escrito varias veces (mapa 06 §8.1): `OnlinePose` (`runtime/online.h:78-128`),
`WireState` (`runtime/online.c:60-104`), `WireHello` y `WireEquip` (`:105-106`), la lista de tipos de mensaje
(`:50`), `MAGIC` y `PROTOCOL` (`:39-40`), y una copia a mano en el bot (`tools/online_bot.c:31-78`, versión 38
literal en `:102`).

Cambio: fichero nuevo `runtime/online_wire.h` con `MAGIC`, `PROTOCOL`, la enumeración de tipos, los motivos de
rechazo y las tres estructuras `Wire*`, **movidas tal cual** desde `online.c`. Debajo, aserciones de tamaño:

    _Static_assert(sizeof(WireHello) == 54, "WireHello");
    _Static_assert(sizeof(WireState) == 3375, "WireState");
    _Static_assert(sizeof(WireEquip) == 432, "WireEquip");
    _Static_assert(sizeof(OnlineEnemy) == 311, "OnlineEnemy");
    _Static_assert(sizeof(OnlineLoot) == 97, "OnlineLoot");

Los números salen del mapa 06 §2.3, **calculados a mano**. Si una aserción no compila, el número del mapa estaba
mal: se corrige el número (no la estructura) y se anota. `online.c` y `tools/online_bot.c` incluyen la cabecera; el
bot se compila con `-Iruntime`.

### 2.2 Pruebas sin el juego (T203, T204)

Base: el arnés de F1 (`tools/test_online_net.c`, que incluye `runtime/online.c`). Patrón del proyecto: programa con
`assert` que sale con 0, regla y objetivo `test-*` al final del `Makefile` (como `test-control`, `Makefile:740-749`).

| Objetivo | Qué cubre | Funciones (`runtime/online.c`) | Dónde corre |
|---|---|---|---|
| `test-online-wire` | Tamaños de cable; trama byte a byte, longitud 0, longitud máxima, mensaje mayor que el buffer; `skel_encode` / `skel_decode` ida y vuelta | `next_msg` `:172`, `consumed` `:182`, `send_msg` `:150`, `skel_encode` `:531`, `skel_decode` `:556` | Windows y Linux |
| `test-online-net` (ampliado) | Cada tipo de mensaje del cliente con carga corta, contadores fuera de rango, ranura ≥ 8; estados OFF → CONNECTING → CONNECTED → FAILED | `cl_message` `:967`, `stop_all` `:1322`, `online_players` `:1387` | Windows y Linux |
| `test-online-session` | Máquina de estados del servidor: HELLO correcto, versión mala, contraseña mala, nombre vacío, sesión llena, doble HELLO, JOIN y LEAVE a terceros, reutilización de ranura, expulsión por silencio | `srv_message` `:232`, `srv_drop` `:218`, `srv_loop` `:343` | Linux (usa `socketpair`); precedente de prueba solo POSIX: `test_prof_lifecycle` |

La expulsión por silencio necesita un reloj controlable: el arnés aporta su propio `plat_now_ns`.
`srv_loop` tiene una estática de función (`next_time`, `:374`) que impide reiniciar el estado entre casos: si
estorba, se anota y se resuelve en el carril T218.

### 2.3 Capa única de validación (T213)

Hoy la validación está repartida: `remote_snap` termina cadenas y recorta tres contadores
(`runtime/online.c:907-964`), cada `case` de `cl_message` hace lo suyo (`:1042-1113`), F1 añadió comprobaciones de
finitud, y `online_actors.c` comprueba `isfinite` en diez sitios y no en otros (mapa 07 M4).

Cambio: `runtime/online_validate.h` + `runtime/online_validate.c`, **funciones puras** (sin estado, sin sockets,
sin juego), una por tipo de dato:

    bool online_validate_state(WireState *w);        // false = descartar el mensaje
    bool online_validate_enemy(OnlineEnemy *e);      // false = descartar esa entrada
    bool online_validate_loot(OnlineLoot *l);
    bool online_validate_name(const char *s, size_t cap);   // terminada, solo [A-Za-z0-9_]
    ...

Reglas (se copian a `specs/contratos/validacion-red.md`, que escribe otro agente; aquí solo se referencian):
cadenas terminadas; nombres de actor con caracteres `[A-Za-z0-9_]`; floats finitos; posiciones con módulo ≤ 100 000
(el límite del emisor, `runtime/online.c:833`); contadores dentro de sus máximos; enumerados dentro de rango;
ranuras < `ONLINE_MAX`. `cl_message` llama a la validación **antes** de guardar nada. La lista de permitidos por
**tipo** de actor (botín, enemigo) se queda en `oa_*`, porque depende de las tablas del overlay.

Prueba `test-online-validate`: casos dirigidos, más 100 000 estructuras con bytes aleatorios (semilla fija): tras
validar, o se rechaza, o no queda ningún float no finito, cadena sin terminar ni contador fuera de rango.

## 3. Cadena OA: partir `online_actors.c` sin cambiar comportamiento

### 3.1 Módulos (de `specs/mapa/07` §8.1)

| Módulo | Contenido | Líneas de origen (aprox., commit `350c5f8`) |
|---|---|---|
| `oa_core.c` | llamadas al juego (`call`, `call_f`, `call_sret`, `:232-259`), `VALID`, `has_body`, `Watch` / `alive`, handles, `Piece`, `piece_update` (`:599`), pool, huérfanos, `create` / `request_p` (`:543-564`), `all_actors` (`:2002`), `bone_mtx` (`:762`), carga de los `.txt`, **todas** las direcciones y offsets | 30-126, 231-660, parte de 662-860, 1993-2002 |
| `oa_gamedata.c` | `crc32_str` (`:3656`), flags, `give_item` (`:3751`), `give_heart`, `start_event` (`:3699`), `set_bool_flag` (`:3794`) | 3656-3769, 3794-3804 |
| `oa_sample.c` | lado local: `sample_pose` (`:1493`), `sample_skeleton` (`:1871`), `sample_shots`, `sample_held`, `fx_update`, `boss_sample`, ganchos `rt_online_arrow` / `_bomb` / `_xlink_emit` (`:814-898`) | 798-945 en parte, 1026-1841, 1871-1893, 4181-4249 |
| `oa_puppet.c` | Link remoto, `pose_link` (`:1901`), `pose_armor`, `animate`, equipo, caballo, disparos, explosiones (`booms_update`, `:1400`), `rt_blur_*` | 1377-1447, 1895-1985, 6011-6468 |
| `oa_runes.c` | `grab_update` (`:4333`), `stop_update` (`:4067`), `ice_update` (`:4126`), `rune_keep`, `rune_pieces` | 4008-4162, 4290-4498 |
| `oa_enemies.c` | registro, propietarios, marionetas, golpes, `strike`, jefes (`boss_update`, `:4251`) | 1987-2778, 2944-3235, 4251-4288 |
| `oa_loot.c` | `loot_update` (`:3366`), `ground_update` (`:5458`) | 3237-3649, 5458-5560 |
| `oa_world.c` | `objs_update` (`:4585`), `movers_update` (`:4829`), `breaks_update` (`:5234`), `break_here`, troncos | 4510-5456 |
| `oa_flags.c` | `quests_update` (`:3851`), Kolog, pizarra, Luna Carmesí, clima, hora | 947-1024, 3718-3738, 3771-4000 |
| `oa_rules.c` | colisión (`players_collide`, `:2833`), PvP, hada, menús sin pausa, bloqueo de A | 2780-2942, bloques de 5767-5845 y 5981-6001 |
| `oa_frame.c` (+ `oa_control.c`) | `rt_online_frame` (`:5721`) como orquestador, `forget_all` (`:316`), detección de carga y muerte; órdenes de control | 5565-5764, 5865-5945 |

Cabecera interna común: `runtime/online_actors_internal.h` (tipos `Watch`, `Piece`, `Handle`; prototipos de lo que
cruza de módulo; las constantes `FN_*` y `P_*`). Los `.c` nuevos entran solos en la compilación
(`RT_SRC := runtime/*.c`, `Makefile:62`); la cabecera, en `RT_HDRS`.

Los rangos de líneas son orientativos y se solapan (el mapa lo dice: "en parte"). El reparto real lo fija T206,
función a función, en una tabla que se guarda en `specs/decisiones/`.

### 3.2 Paso A — reordenar dentro del mismo fichero (T206)

Se agrupan las definiciones por módulo **sin salir de `online_actors.c`** y sin cambiar ni un carácter de su
cuerpo. Solo se permite: mover definiciones enteras (con su comentario de encima), añadir prototipos `static` al
principio, y añadir comentarios separadores `// ==== oa_<módulo> ====`.

Comprobación automática (`tools/coop/oa_check.py`, se escribe en esta tarea):

1. Parte el fichero "antes" (`git show <commit>:runtime/online_actors.c`) y el "después" en **definiciones de
   nivel superior** (texto entre líneas que empiezan en la columna 0 y no son continuación).
2. Quita de "después" los prototipos y separadores añadidos.
3. Compara los dos **multiconjuntos**: deben ser idénticos. Cualquier diferencia se imprime y el guion sale con 1.

Antes de mover nada se comprueba que no hay dependencias de orden ocultas: no hay `#undef` en el fichero ni
`#define` repetidos (comprobado hoy con `grep`: ninguno), y el fichero no usa `__LINE__` (comprobado: ninguno).

### 3.3 Paso B — cortar en ficheros (T207)

Cada grupo va a su `oa_<módulo>.c`. Cambios permitidos, y solo estos:

- Quitar `static` a lo que se usa desde otro módulo, y declararlo en la cabecera interna. Lo que cruce de módulo y
  no tenga ya un prefijo propio se renombra con `oa_` **en un commit aparte**, con sustitución mecánica.
- `#include` necesarios.
- Estado compartido suelto (`g_actors`, `g_map`, `g_local_dead`, `g_dist`, `g_piece_params`, `g_blast`,
  `g_carried_now`, `g_theirs`; mapa 07 §8.2 punto 7): en esta tarea solo se declara `extern` en la cabecera y se
  define en el módulo dueño. No se rediseña.

Comprobación: `oa_check.py --modulos` compara el multiconjunto de definiciones de todos los `oa_*.c` con el del
fichero reordenado, ignorando la palabra `static` y (tras el commit de renombrado) aplicando la tabla de nombres.

### 3.4 Qué demuestra cada comprobación (y qué no)

| Comprobación | Demuestra | No demuestra |
|---|---|---|
| `oa_check.py` | Que el texto de cada función y de cada variable es el mismo; que nada se perdió ni se duplicó | Que el binario sea idéntico |
| Compila con `-Wall` en Windows y Linux, y enlaza | Que no hay símbolos duplicados ni usos antes de declarar | — |
| Tamaño de cada función a `-O0 -fno-inline` antes y después (`nm --size-sort`) | Indicio de que el código de cada función es el mismo [esperado; **sin verificar** que coincida al 100 %] | Igualdad con optimización |
| Pruebas `test-online-*` | Que la capa de red no cambió | Nada sobre los actores |
| Firma de comportamiento (§4.4) | Que una sesión real hace lo mismo, a grandes rasgos | Caminos que el recorrido no pisa |
| Recorrido de F1 con ventana | Lo que ve una persona | — |

No existe una demostración bit a bit una vez que el código vive en varias unidades de compilación (cambia la
decisión de qué se integra en línea). Se dice así en la bitácora.

### 3.5 Partir `g_act[]` (T208)

`g_act[ONLINE_MAX]` (`runtime/online_actors.c:131-180`) mezcla campos de cinco áreas (mapa 07 §8.2 punto 1).
Reparto:

| Nueva tabla | Dueño | Campos |
|---|---|---|
| `g_pup[]` | `oa_puppet.c` | `link`, `link_w`, `link_fails`, `armor`, `gear`, `horse`, `tack`, `horse_as`, `shot`, `shot_id`, `held`, `nock`, `kago`, `kago_n`, `gale`, `trail`, `trail_name`, `dye`, `fxev_seen`, `fxev`, `boom_seen`, `name`, `anim`, `as`, `covered`, `frames`, `skeleton` |
| `g_rune[]` | `oa_runes.c` | `grab`, `grab_w`, `grab_name`, `grab_from`, `grab_wait`, `released`, `released_t`, `magne`, `beam`, `magnet`, `holder`, `slate`, `slate_shown`, `rune_fx`, `stop_fx`, `magnet_fx`, `stop`, `stop_w`, `stop_name`, `stop_wait`, `chains`, `chained` |
| `g_ice[]` | `oa_runes.c` | `ice[3]` |

Los demás módulos ya no leen campos: preguntan con funciones (`oa_puppet_link(i)`, `oa_rune_holds(actor)`,
`oa_rune_released(i)`; mapa 07 §8.1). Cuidado con `forget_all` (`:316-336`): hace `memset(g_act, ...)` y luego
pone `anim = -1` y `covered = -1`; cada módulo debe reproducir **exactamente** su parte en una función
`oa_<módulo>_forget`, llamada en el mismo orden. Lo mismo con `drop(c, i, left)` (`:423`).

### 3.6 Orden explícito del fotograma (T209) y Links validados (T210)

Orden actual de `rt_online_frame` (leído en `:5721-6058`):

```
 1. control: find_actors, flag_query, event_query                      (5723-5725)
 2. puerta: online_actors_on                                           (5726)
 3. fuera de sesión en el mundo: borrar actores y salir                (5732-5746)
 4. detección de carga (sin Link o salto > 100 m): forget_all, g_settle = 60   (5748-5761)
 5. muestreo local: sample_pose, sample_skeleton                       (5766)
 6. menús sin pausa                                                    (5767-5845)
 7. hora del anfitrión                                                 (5851-5864)
 8. pool_update, booms_update, fx_update, orphans_update               (5865-5869)
 9. salida temprana: g_settle; Link local muerto (180 fotogramas al volver)    (5870-5880)
10. ganchos de prueba por variable de entorno                          (5881-5945)
11. owns_left, loot_update, quests_update                              (5948-5954)
12. weather_follow, bloody_moon_follow, pvp_update, players_collide,
    own_strikes_in_menu, own_guard                                     (5955-5964)
13. ground_update                                                      (5965)
14. objs_update, movers_update, breaks_update                          (5966-5972)
15. grab_update, stop_update, ice_update, boss_update                  (5973-5980)
16. bloqueo del botón A                                                (5981-6001)
17. enemigos: enemies_host / enemies_show, o enemies_client            (6002-6009)
18. bucle de jugadores: crear o quitar el Link, comprobar alive (6036),
    colocar, pose, armadura, equipo...                                 (6011-6468)
```

T209 convierte esto en una lista de llamadas `oa_*_frame(...)` con **este mismo orden** y las mismas salidas
tempranas, y escribe el orden en un comentario encima y en `specs/contratos/hilos-y-frame.md`. No cambia cuántas
veces se llama a `online_players` (hoy 8 por fotograma, cada una una copia tomada en un instante distinto): eso es
un cambio de comportamiento y queda fuera de la fase 0.

T210 sí cambia comportamiento, a propósito y en un commit propio: un paso nuevo entre el 9 y el 11 que comprueba
`alive` de cada Link remoto y, si el juego lo borró, hace lo que hoy hace el paso 18 (`forget_all`,
`g_settle = 180`, salir). Así ningún paso posterior ve un Link liberado (mapa 07 A5). El ayudante `link_live` de
F1 se mantiene como segunda barrera.

### 3.7 `RT_ON_RESET` (T211, T212)

Hechos: el juego puede reiniciarse dentro del mismo proceso ("Quit to start screen"); en ese momento se ejecutan
todos los `RT_ON_RESET`, con los hilos del juego ya parados y antes de borrar su memoria (`runtime/hle.h:28-40`).
Ni `online.c` ni `online_actors.c` tienen ninguno (comprobado con `grep`). `forget_all` solo limpia una parte. El
fichero tiene del orden de 85 variables estáticas de fichero y unas 200 dentro de funciones (recuento aproximado con
`grep`); no todas guardan punteros del juego.

Patrón a copiar: `RT_ON_RESET(items_reset)` en `runtime/items.c:337-341`.

1. **Inventario (T211)**: tabla de todas las estáticas de `oa_*.c` y `online.c` con: nombre, módulo, tipo, si guarda
   un puntero del juego / un handle / un `Watch` / estado lógico de una partida, y decisión (limpiar / permitir con
   motivo). Guion `tools/coop/oa_statics.py` que las lista y comprueba contra `tools/coop/oa_statics_permitidas.txt`.
2. **Aplicar (T212)**: por módulo, una función `RT_ON_RESET(oa_<módulo>_reset) { ... }` que pone a cero sus
   variables. Las estáticas de función que haya que limpiar se suben a nivel de fichero **sin cambiar su nombre ni
   su uso**. Lo que no se limpia (tablas leídas de los `.txt`, indicadores de variable de entorno, offsets
   descubiertos una vez) va a la lista de permitidas con su motivo.

Regla dura: dentro de un reset **no se llama al juego** (`call(c, ...)`, `FN_DELETE`, `orphan` con borrado): los
hilos del juego están parados y su memoria va a desaparecer. Solo se olvidan punteros.

El hilo de red sobrevive al reinicio y la conexión se mantiene (mapa 06 §1.4): en `online.c` no se corta la sesión
en el reset; solo se documenta.

## 4. Verificación transversal

### 4.1 En cada commit de la fase 0

    make -j8 && make test-online-gate test-online-wire test-online-net test-online-validate
    $PY tools/coop/oa_check.py            # desde T206
    $PY tools/coop/oa_statics.py          # desde T211

### 4.2 En Docker (Linux)

    docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas sh -c \
      'make B=/tmp/b GEN=/nonexistent test-online-wire test-online-net test-online-session test-online-validate'
    ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13

(La forma `B=/tmp/... GEN=/nonexistent` es la que usa `tools/linux/distro_build.sh` para compilar con el árbol en
solo lectura. **Sin verificar** que las reglas de prueba nuevas respeten `B`; deben escribirse con `$(B)`.)

### 4.3 CI (T205)

Flujo nuevo `.github/workflows/coop.yml` (fichero propio, no toca los existentes): en `push` y `pull_request` a
`coop/main`, en `ubuntu-latest`, instala `clang make` y ejecuta los objetivos `test-online-*`. Sin secretos.

### 4.4 Firma de comportamiento (T201)

`tools/coop/firma_log.py` lee los logs A y B de una pasada del recorrido y produce, por juego, el número de líneas
`[online]` de cada **tipo** (texto con números, nombres y direcciones sustituidos por marcadores). Se guarda la
firma de referencia (media de tres pasadas, con mínimo y máximo por tipo) antes de T206.

Tolerancia: una firma posterior es "igual" si (a) no aparece ningún tipo de línea nuevo, (b) no desaparece ningún
tipo que salió en las tres pasadas de referencia, y (c) los recuentos de los tipos de evento único (`joined`,
`hosting`, `actors on`, `a load`) coinciden exactamente. Los recuentos de tipos periódicos pueden variar: dependen
del tiempo real. Es un indicio fuerte, no una demostración.

## 5. Carriles tras la fase 0

| Carril | Tarea | Ficheros (exclusivos) | Qué hace | `verify` resumido |
|---|---|---|---|---|
| Mundo | T215 | `runtime/oa_world.c` | `has_body` en los troncos de `objs_update`; guarda de clase en `break_here` antes de escribir la vida | lectura + talar un árbol y romper una caja en los dos juegos |
| Enemigos y botín | T216 | `runtime/oa_enemies.c`, `runtime/oa_loot.c` | pool propio para marionetas de enemigo y botín (hoy usan el del jugador 0); aviso cuando se pierde un huérfano | campamento del recorrido; `drop` del jugador 0 no borra enemigos |
| Runas | T217 | `runtime/oa_runes.c` | `has_body` tras encontrar el objeto en el botín en `grab_update` | Magnesis sobre un arma del suelo en los dos juegos |
| Red | T218 | `runtime/online.c`, `tools/test_online_net.c` | subir a la estructura de sesión las estáticas de función; atómicos para `state`, `hosting`, `id` | prueba: dos sesiones seguidas en un proceso |
| Bot y Docker | T219 | `tools/online_bot.c`, `tools/docker/*` | buffer del bot a 32 768; modo `BOT_NOREAD`; guion de medidas con `netem` | con más de 26 enemigos el bot no se cae; tabla de medidas |
| Traducciones | T220 | `runtime/ui/i18n/*.inc` | los textos de Online que faltan en 12 idiomas y los 3 de `es` | `tools/i18n_check.py` sale con 0 |

Regla de carriles: cada uno toca **solo** sus ficheros. Si necesita un prototipo nuevo en
`online_actors_internal.h` o un campo en `online.h`, no lo añade: lo pide al dueño de la fase (una sola persona
coordina esas dos cabeceras, mapa 07 §8.3).

## 6. Patrones existentes que se reutilizan

| Necesidad | Patrón | Dónde |
|---|---|---|
| Estado por arranque | `RT_ON_RESET(fn) { ... }` | `runtime/hle.h:40`; ejemplo `runtime/items.c:337-341` |
| Hilo externo pide, hilo del juego ejecuta | cola con mutex + contador atómico | `runtime/items.c:337-447` (`items_request` `:367`, `rt_item_tick` `:420`) |
| Órdenes de control que responden en el fotograma | petición `volatile` + respuesta | `runtime/online_actors.c:5565-5705` |
| Sockets Winsock / BSD | `#ifdef _WIN32`, `WSAStartup` una vez | `runtime/control.c`, `runtime/dsu.c` |
| Prueba que incluye el `.c` | regla + objetivo + `.PHONY` al final del `Makefile` | `Makefile:740-749` |
| Prueba solo POSIX | condición de plataforma en la regla | `test-prof` / `test_prof_lifecycle` |

## 7. Integración y rollback

- Un commit por paso verificable (reordenar; cortar; renombrar; `g_act`; orden; Links; reset por módulo).
  `git revert` de uno no arrastra a los demás, salvo los que dependen de él: se revierten en orden inverso.
- Etiquetas locales: `f1-cerrada` al empezar, `f2-fase0` al pasar T214.
- Si la firma o el recorrido empeoran tras un paso: revertir **ese** commit, no parchear encima.
- Ningún paso toca `tools/patches.py` ni `tools/game_sites.py`: no hace falta `make gen`. Tocar
  `online_actors_internal.h` u otra cabecera de `runtime/` recompila el runtime entero (`RT_HDRS`).
- Durante la fase 0 no se sincroniza con `upstream/test`.

## 8. Duraciones (estimadas)

| Bloque | Estimado |
|---|---|
| Cadena NET | 2-3 días de agente |
| T206 + T207 (partición) | 2-3 días, la mayor parte en comprobar |
| T208-T210 | 1-2 días |
| T211-T212 | 2 días |
| Cada carril | medio día a un día, más su turno de juego |
| Verificación en juego por paso | 30-40 min (recorrido) |
