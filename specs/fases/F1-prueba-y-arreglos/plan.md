# F1 — Probar el sistema y arreglos rápidos esenciales · plan

Referencias `fichero:línea`: commit `350c5f8` de la rama del PR, comprobadas leyendo el código. Nada de lo que
sigue se ha ejecutado todavía: duraciones y consumos son **estimados** hasta que la bitácora los mida.

Convención: `$SW` = carpeta de trabajo; clon en `$SW/coop`; shell MSYS2 UCRT64 con
`export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`; `PY=tools/venv/Scripts/python.exe`.

## 1. Línea base: dos juegos en un PC

Fuente: `docs/ONLINE.md`, sección "Testing with two games on one PC", más las variables de ejecución desatendida de
`AGENTS.md` ("Build and verify"). La sección de `docs/ONLINE.md` no menciona `SAVAGEWIND_NO_LAUNCHER`,
`SAVAGEWIND_SETTINGS` ni `SAVAGEWIND_INPUT`; se añaden porque sin ellas el juego se queda en la pantalla de inicio o
de título.

### 1.1 Presupuesto de RAM (15,7 GB)

| Consumidor | RAM | Fuente |
|---|---|---|
| Dos juegos | ~9,5 GB | `docs/ONLINE.md` ("Two games hold about 9.5 GB of RAM") |
| Windows, shell, antivirus | 3-4 GB | estimado |
| Margen | 2-3 GB | estimado |
| Docker Desktop (hasta 6,2 GB) | **cerrado** | no cabe |
| Navegador | **cerrado** | no cabe |

Antes de cada pasada: cerrar Docker Desktop (y `wsl --shutdown` si WSL sigue ocupando memoria), cerrar el
navegador, no compilar a la vez, y anotar la memoria libre. VRAM con dos juegos en 6 GB: **sin medir**; anotarla.

### 1.2 Carpetas (fuera del repositorio)

    mkdir -p "$SW/pruebas"/{guardado-A,guardado-B,logs}
    # guardado-A y guardado-B: COPIAS de una carpeta de guardado; nunca la que está en uso
    : > "$SW/pruebas/ajustes-A.ini" ; : > "$SW/pruebas/ajustes-B.ini"

### 1.3 Arranque (dos consolas MSYS2 UCRT64, desde la raíz del clon)

Juego A, anfitrión:

    export SAVAGEWIND_SAVE_DIR="$SW/pruebas/guardado-A" SAVAGEWIND_SETTINGS="$SW/pruebas/ajustes-A.ini"
    export SAVAGEWIND_NO_LAUNCHER=1 SAVAGEWIND_HIDDEN=1 SAVAGEWIND_AUDIO_GAIN=0
    export SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"
    export SAVAGEWIND_ONLINE_PORT=52530 SAVAGEWIND_ONLINE=host SAVAGEWIND_ONLINE_NAME=Host SAVAGEWIND_CONTROL=7777
    ./build/savagewind.exe > "$SW/pruebas/logs/A.log" 2>&1

Juego B, cliente: lo mismo con `guardado-B`, `ajustes-B.ini`, `SAVAGEWIND_ONLINE=127.0.0.1`,
`SAVAGEWIND_ONLINE_NAME=Client`, `SAVAGEWIND_CONTROL=7778` y `B.log`.

Notas comprobadas en el código:

- El puerto 52530 es de prueba; el de una sesión real es 52520 (`ONLINE_PORT`, `runtime/online.h:18`).
- Con `SAVAGEWIND_ONLINE_NAME` puesto, `online.ini` no se reescribe (`runtime/online.c:1545`), así que los dos
  juegos pueden compartir carpeta de trabajo.
- `SAVAGEWIND_ONLINE` se lee al arrancar (`runtime/main.c:526-529`): `host` crea la sesión, cualquier otro valor es
  la dirección a la que unirse.
- El juego puede relanzarse una vez al arrancar (un proceso hijo): un guion que vigile por PID debe buscar al hijo
  (`docs/ONLINE.md`).
- No lanzar bajo `timeout` de MSYS cuando haya ventana (`docs/BUILD-windows.md`, "Tests").

Todo esto se empaqueta en `tools/coop/dos_juegos.sh` (T102) para que cualquier agente lo lance igual.

### 1.4 Conducir los juegos (`tools/swctl.py`, `docs/CONTROL.md`)

    $PY tools/swctl.py --port 7777 --wait 180 state      # espera a que A responda
    $PY tools/swctl.py --port 7778 --wait 180 state
    $PY tools/swctl.py --port 7777 set god_mode true     # que la prueba no acabe en game over
    $PY tools/swctl.py --port 7777 goto tower 7          # torre de la Meseta (docs/CONTROL.md, "Named places")
    $PY tools/swctl.py --port 7778 actors OnlinePlayer   # ¿está el Link del otro en este mundo?

Órdenes útiles: `pos`, `goto` / `teleport` (un solo mapa: a un santuario o Bestia hay que entrar andando), `look`,
`press` / `hold` / `release` / `stick`, `time`, `give`, `actors <texto>`, `flag` / `setflag`, `bloodmoon`, `exit`.

### 1.5 Recorrido de prueba (30 min) — cubre CU-01, CU-02, CU-03

| Min | Paso | Caso | Qué debe verse (log o `swctl`) |
|---|---|---|---|
| 0-2 | Arrancar A y B | CU-01 | A: `[online] hosting on port 52530` y `[online] server: Host joined (slot 0)`, luego `... Client joined (slot 1)` (`runtime/online.c:1359`, `:259`) |
| 2-4 | `goto tower 7` en ambos | CU-02 | En cada juego `actors OnlinePlayer` devuelve uno; log `[online] <nombre>: OnlinePlayer (actor ...)` (`runtime/online_actors.c:6051`) |
| 4-7 | B camina (`stick l 0 1 150`, varias veces) | CU-02 | En A, la posición de `actors OnlinePlayer` cambia siguiendo el `pos` de B con unos metros de margen |
| 7-10 | Ambos al campamento del acantilado: `teleport -1203 198 1641` (receta de `docs/ONLINE.md`) | CU-03 | Líneas de traspaso de enemigos en el log; ningún cuelgue |
| 10-14 | B se aleja más de 400 m (`goto stable 2`) y vuelve (`goto tower 7`) | CU-02, CU-03 | El Link remoto desaparece más allá de 400 m (`runtime/online_actors.c:6018`) y se vuelve a crear al volver; `[online] a load: the players' actors are gone` |
| 14-17 | A cambia la hora (`time set 18`); B la sigue | CU-03 | En B: `[online] the host's time: ...` (`runtime/online_actors.c:5861`) |
| 17-20 | A abre y cierra pausa (`press plus`) varias veces | CU-03 | Sin cuelgue; `[online] our pose: ...` cambia |
| 20-24 | Ambos saltan lejos a la vez (`goto hateno`) | CU-03 | Carga en los dos; actores olvidados y rehechos; sin cuelgue |
| 24-27 | Matar el proceso B sin cerrar; A sigue 1 min | CU-03 | A no se cuelga; a los 10 s el servidor expulsa a B (`TIMEOUT_NS`, `runtime/online.c:46`); `[online] out of the session` no aplica a A |
| 27-30 | Relanzar B; vuelve a unirse | CU-01 | `Client joined (slot 1)` otra vez; actor recreado |

El recorrido automatizable va en `tools/coop/recorrido.py` (T105), que solo usa `swctl.py` y escribe una línea por
paso con PASA / FALLA. La pasada con ventana (el dueño) añade lo que un guion no ve: armadura, arma, animación.

### 1.6 Cómo registrar un cuelgue

1. No cerrar el otro juego: anotar primero la hora y el paso del recorrido.
2. Guardar `A.log` y `B.log` enteros fuera del repositorio (`$SW/pruebas/logs/<fecha>-<paso>/`).
3. Del log del juego caído: las últimas 100 líneas y todas las `[online]` del último minuto.
4. Del otro juego: `swctl.py pos`, `swctl.py state` y sus últimas líneas `[online]`.
5. Entrada en `specs/bitacora/` con la plantilla de `verificacion.md`: sin rutas personales ni logs crudos, solo
   las líneas relevantes.
6. Repetir el paso tres veces para saber si es reproducible (siempre / a veces / una vez).

Registros opcionales del PR (variables de entorno; `docs/ONLINE.md`): `SAVAGEWIND_ONLINE_ENEMY_AS_LOG`,
`_SHOT_LOG`, `_GEAR_LOG`, `_POSE_LOG`, `_LIFE_LOG`. Activarlos solo para investigar un fallo concreto.

## 2. Herramientas de reproducción (antes de los arreglos)

**2.1 Modos hostiles del bot** (`tools/online_bot.c`, T109). Hoy el bot es un cliente honesto con variables
`BOT_*` (cabecera del fichero). Se añaden, siguiendo el mismo patrón `getenv`:

- `BOT_NAN=1`: su STATE lleva la posición no finita.
- `BOT_LOOT=<nombre>`: envía un LOOT_ADD con ese nombre de actor junto a su posición.
- `BOT_SHIELD=1`: envía un ESTATE cuyo campo `shield` son 40 bytes sin terminador.

El bot se compila con `cc -O2 -o build/online_bot.exe tools/online_bot.c -lws2_32` (`tools/online_bot.c:7`) y se
une al juego A en `127.0.0.1 52530`.

**2.2 Arnés de `online.c` sin juego** (`tools/test_online_net.c`, T110). Patrón del proyecto: la prueba incluye el
`.c` de producción (`#include "../runtime/online.c"`) y aporta lo poco que le falta (mapa 05 §5). Permite llamar a
`cl_message` (`runtime/online.c:967`), `stop_all` (`:1322`) y a las funciones `online_*` con mensajes fabricados.
Es la semilla de las pruebas de protocolo de F2. **Sin verificar** que `online.c` se deje incluir sin más dobles
que `g_site`, `plat_now_ns` y `LOG`; si no se puede en un tiempo razonable, los arreglos 5, 6 y 8 se verifican con
el bot y por lectura, y se anota.

## 3. Los nueve arreglos

Regla común: **cambio mínimo y local**; no se reordena `rt_online_frame`, no se mueven funciones de sitio, no se
toca el protocolo. Cada arreglo añade una línea de log la primera vez que actúa, para poder verlo en una prueba.

| # | Qué hace el código hoy | Cambio | Repro "antes" | Verificación "después" |
|---|---|---|---|---|
| 1 Puerta de versión | `online_actors_on` solo mira si existe el overlay (`online_actors.c:225-229`); `rt_online_frame` llama a direcciones fijas de 1.9.0. `g_game_version` existe (`runtime/game_sites.h:113`) y otros módulos lo comparan con `"1.9.0"` (`runtime/quick_drop_action.c:70`) | Función pura `online_version_ok(v)` en un fichero nuevo; `online_actors_on` y los ganchos `rt_*` del fichero la consultan | No se puede con el juego (solo hay 1.9.0) | Prueba unitaria `make test-online-gate`; en el juego, log `[online] game version 1.9.0: actors on` y recorrido sin cambios |
| 2 `has_body` en mecanismos | `movers_update` llama a `FN_BODY_TO` si `+0x190` pasa `VALID()` (`:4901-4902`, `:4928-4929`); `has_body` (`:73-77`) existe porque ese campo es otra cosa en clases sin cuerpo físico | Sustituir la condición por `has_body(a)` en los dos sitios | Sin repro fiable (depende de qué actor case con un HashId) | Lectura; mecanismo real (ascensor o puerta de un santuario) sigue moviéndose en los dos juegos |
| 3 Runas y Link vivo | `grab_update` y `stop_update` (`:5976-5977`) usan `g_act[i].link` (`bone_mtx`, lectura de `+0x398`, `:4306-4326`) antes de que `:6036` compruebe `alive` | Ayudante `link_live(i)` (devuelve el Link solo si `alive`), usado en esas rutas | Sin repro fiable (ventana de un fotograma tras game over o carga) | Lectura; Magnesis y Stasis siguen viéndose; morir y recargar con Magnesis activo no cuelga |
| 4 Nombres permitidos | `request(c, list[j].name, ...)` (`:3604`) y `request_p(c, e->name, ...)` (`:2226`) crean actores con el nombre recibido; `is_loot` (`:3246`) e `is_enemy` (`:2029`) no se aplican a lo que llega | Botín: exigir `is_loot(nombre)` al recibir en el anfitrión (`loot_host_add`, `:3360`) y antes de crear (`:3604`). Enemigos: ayudante `enemy_name_ok(nombre)` antes de `:2226`; el registro del anfitrión conserva su propio nombre al copiar un ESTATE (`:2398`) | Bot con `BOT_LOOT=<actor que no es botín>`: antes aparece `loot N (<nombre>) made here` | Después: no se crea; log de rechazo; el botín normal sigue compartiéndose |
| 5 Números no finitos | El emisor honesto filtra (`online.c:832-834`); el receptor no (`remote_snap`, `:907`; `M_TIME`, `:1108`; `M_HIT`, `:1100`) | Al recibir: descartar STATE con posición o rumbo no finitos o con módulo mayor que 100 000; TIME fuera de 0..24; HIT con daño negativo; matrices no finitas en ENEMIES, ESTATE, LOOT y PICK | Bot con `BOT_NAN=1`; arnés | Arnés en verde; con el bot, log de descarte y el juego sigue |
| 6 Estado de sesión | `stop_all` (`online.c:1322-1335`) solo limpia `g.r`, `id`, chat, `hosting`, `ping`. Sobreviven `g.quests` / `quests_have`, `g.loots`, `g.en[]`, `quest_of[]` y las colas de entrada | Limpiar todos los campos de sesión bajo `g.lock`; limpiar `quest_of[slot]` al entrar un jugador en una ranura | Arnés: entregar QUESTS, llamar a `stop_all`, comprobar que `online_quests` devuelve falso | Arnés en verde; salir y volver a entrar en una sesión no trae flags de la anterior |
| 7 `SIGPIPE` | `send(s, c, n, 0)` (`online.c:143`); el patrón correcto está en `runtime/control.c:652-669` | `MSG_NOSIGNAL` donde exista y `SO_NOSIGPIPE` en los sockets donde exista | Docker: servidor dedicado y un bot que se mata con `docker kill` [sin verificar que se reproduzca siempre] | Mismo escenario diez veces: el servidor sigue vivo |
| 8 `shield` sin terminar | ESTATE termina `name`, `as`, `weapon` pero no `shield` (`online.c:1095`); ENEMIES sí (`:1052`) | Añadir `shield[39] = 0` | Arnés: ESTATE con 40 bytes sin NUL | Arnés en verde |
| 9 Contraseña visible | `opt_text("Password", ...)` (`online_ui.mm:64`); `opt_text` pinta el valor en la fila (`option_rows.mm:308`) y lo edita sin máscara (`:313`) | Variante `opt_text_secret` que pinta asteriscos y usa `ImGuiInputTextFlags_Password` | Captura de F1 → Online con una contraseña puesta | Captura: asteriscos; unirse con contraseña sigue funcionando |

Alcance honesto del arreglo 5: en F1 se cubren los campos que llegan directamente a llamadas del juego con más
riesgo. Las decenas de matrices internas de `WireState` (equipo, disparos, runas, objetos) se cubren en F2 con la
capa única de validación; algunas ya se filtran en `online_actors.c` (`:4683`, `:4913`, `:6289`, `:6323`, `:6356`).

Alcance honesto del arreglo 9: solo la pantalla. La contraseña sigue viajando en claro, guardada en claro en
`online.ini` y visible en la línea de órdenes de `--server`: F3. El teclado en pantalla para mando
(`runtime/ui/osk.mm`) puede mostrarla mientras se escribe [sin verificar].

## 4. Patrones existentes que se reutilizan

| Necesidad | Patrón | Dónde |
|---|---|---|
| Comparar la versión del juego | `strcmp(g_game_version, "1.9.0")` | `runtime/quick_drop_action.c:70`, `runtime/quick_inventory_sort.c:37` |
| Envío sin `SIGPIPE` | `#ifdef MSG_NOSIGNAL` / `#ifdef SO_NOSIGPIPE` | `runtime/control.c:652-669` |
| ¿Es un objeto físico? | `has_body(a)` | `runtime/online_actors.c:73-77` |
| ¿Sigue vivo este actor? | `alive(actor, watch)` | `runtime/online_actors.c:91` |
| Prueba que incluye el `.c` de producción | regla + objetivo `test-*` + `.PHONY` | `Makefile:740-749` (`test-control`) |
| Variable de prueba del bot | `getenv("BOT_...")` | `tools/online_bot.c` |

## 5. Qué va en serie y qué en paralelo

Parte A: **en serie** (una sola máquina, un solo par de juegos).

Parte B: tres carriles, cada uno con **un solo dueño** y su worktree; dentro de un carril las tareas van en serie
porque tocan el mismo fichero:

| Carril | Fichero | Tareas | Por qué en serie |
|---|---|---|---|
| OA | `runtime/online_actors.c` (+ `runtime/online_gate.h`, `tools/test_online_gate.c`) | T111 → T112 → T113 → T114 | Cuatro cambios en un fichero de 6 468 líneas; un solo dueño evita conflictos |
| NET | `runtime/online.c` (+ `tools/test_online_net.c`) | T110 → T115 → T116 → T117 → T118 | Mismo fichero y mismo arnés |
| UI | `runtime/ui/option_rows.h`, `option_rows.mm`, `online_ui.mm` | T119 | Independiente |
| BOT | `tools/online_bot.c` | T109 | Independiente; lo usan T114 y T115 como repro |

Los carriles OA, NET, UI y BOT sí corren en paralelo entre sí: no comparten ficheros, salvo el `Makefile`
(T110 y T111 añaden cada uno un bloque de prueba **al final**; se integran en orden y el segundo rebasa).

Probar en el juego no es paralelizable: solo caben dos juegos en este PC. Los carriles compilan y pasan sus pruebas
unitarias por separado; la verificación en juego se hace **una vez por carril**, en turno, y al final la pasada
completa (T120).

## 6. Integración y rollback

- Un commit por arreglo, con el número del hallazgo en el mensaje. Así `git revert <commit>` deshace uno solo.
- Orden de integración en `coop/main`: BOT, NET, OA, UI. Tras cada carril: `make -j8`, pruebas unitarias,
  recorrido corto (pasos 0-7 del §1.5).
- Si tras integrar un carril el recorrido empeora respecto a la línea base: `git revert` del último commit de ese
  carril y repetir; se anota.
- Ningún arreglo cambia `tools/patches.py` ni `tools/game_sites.py`: no hace falta `make gen`.
- Compilación Linux tras integrar todo: `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13`.

## 7. Duraciones (estimadas)

| Bloque | Estimado |
|---|---|
| Parte A completa | medio día (incluye dos pasadas de 30 min y la redacción) |
| T109, T110 | 2-4 h cada una |
| Cada arreglo | 1-3 h más su turno de prueba en juego |
| Recompilar el runtime tras tocar un `.c` | minutos; tras tocar un `.h` de `runtime/`, el runtime entero |
| Pasada final (T120) | 2 × 30 min más análisis |
