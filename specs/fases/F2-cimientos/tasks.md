# F2 — Cimientos · tasks

Formato de tarea y significado de `[P]` y `modelo sugerido`: ver `../F0-preparacion/tasks.md`, "Cómo leer una
tarea". Cada tarea se puede ejecutar sin más contexto que este fichero.

## Reglas que valen para TODAS las tareas de F2

- `$SW` = carpeta de trabajo; clon en `$SW/coop`; shell MSYS2 UCRT64 desde la raíz del clon con
  `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`; `PY=tools/venv/Scripts/python.exe`.
- Worktree propio: `git worktree add "$SW/wt/<tarea>" -b coop/<tarea> coop/main` (necesita su `game_dir.txt`, venv,
  `generated/`, `build/` y `online_mod/`; F0 `plan.md` §10).
- Las líneas citadas son del commit `350c5f8`; tras F1 se habrán movido. **Localiza por nombre de función.**
- El formato en el cable no cambia: `PROTOCOL` sigue en 38; ninguna estructura `#pragma pack(1)` cambia de campos,
  orden ni tamaño. Mover una definición de fichero sí está permitido donde la tarea lo dice.
- **Tarea de refactor = cero cambio de comportamiento.** Si ves un fallo mientras mueves código, no lo arregles:
  anótalo en la bitácora. Las únicas tareas que cambian comportamiento a propósito lo dicen (T210, T212, T213 y los
  carriles).
- No toques `tools/patches.py`, `tools/game_sites.py` ni `generated/`.
- No añadas variables de entorno `SAVAGEWIND_*` nuevas.
- `runtime/online.h` y `runtime/online_actors_internal.h` tienen un único coordinador (el dueño de la cadena OA en
  la fase 0). En un carril, si necesitas cambiar una de las dos, pídelo; no lo hagas tú.
- En el `Makefile`, solo añadir bloques **al final**; no editar reglas existentes.
- Sin rutas absolutas del equipo, sin logs crudos, sin carpetas `input` / `save` / `keys` / `build` / `generated`,
  sin ficheros llamados `main`. Nada de `git push`.
- Solo caben dos juegos a la vez: pide turno para las pruebas en juego. Guardados: siempre las copias de
  `$SW/pruebas/`.
- Al terminar: bitácora con lo que **no** se verificó.

---

- [~] T222 [net] Validación por entrada en STATE y cotas que faltan (revisión de F2: I-1, I-2, I-3, C-1, C-2).
  reclamada: agente Opus oa-t222 (integrada) — validate 181510/0, net 309/0; compila; FALTA recorrido sin `dropped a STATE` y prueba BM_TEST=5000
  files: `runtime/online_validate.h`, `runtime/online_validate.c`, `runtime/online.c` (`sample_local` y el `case` de STATE), `tools/test_online_validate.c`, `specs/contratos/validacion-red.md`
  verify: `make test-online-validate test-online-wire test-online-net` en 0; casos nuevos: (a) un STATE con una entrada mala en cada lista se acepta con esa entrada anulada y el resto intacto; (b) un STATE con `pos` o `yaw` malos se descarta entero; (c) `bloody_moon = 5000` se acepta; (d) `fx_anim[k]` y `dye[k]` fuera de cota anulan su entrada; `make -j8` compila; recorrido completo sin `dropped a STATE` en los logs; con `SAVAGEWIND_ONLINE_BM_TEST=5000` en A, B sigue viendo moverse a A
  depende de: T213
  quién: agente
  contexto: Revisión independiente de F2 (`specs/bitacora/2026-11-25-F2-revision-independiente.md`). **ADR-025 n.º 1** decide I-2: STATE se descarta solo por los campos núcleo; una entrada mala de una lista se anula y el resto se aplica (como ENEMIES y LOOTS). I-1: `bloody_moon` > 4000 tira hoy el STATE entero (`online_validate.h:119-120`) aunque el consumidor (`oa_flags.c:46`) ya lo ignora. I-3: `sample_local` (`online.c:788-790`) deja `pos` no finita al devolver falso y el STATE con LOADING se descarta en el receptor. C-1/C-2: `fx_anim` y `dye` sin cota superior llegan al juego.
  pasos:
    1. Separar en `online_validate` los campos núcleo (`pos`, `yaw`, `flags`, `anim`, `as`, reglas de sesión) de las listas. Núcleo malo → falso (se descarta el mensaje). Entrada mala de una lista → se anula esa entrada **en la copia local** (la marca de vacío que ya use ese campo: `n` reducido, nombre vacío o `-1`) y se cuenta; nunca se corrige su valor.
    2. `bloody_moon`: solo finito y `|x| ≤ ONLINE_V_LIMIT` (la ventana 0..4000 la aplica el consumidor).
    3. `fx_anim[k]`: cota alta conservadora (escribir en la bitácora de dónde sale); fuera de cota → `-1`. `dye[k]`: la cota que usa el emisor para los colores reales; fuera → `-1`. `fx_props` y `ground_props` de tipo `float`: finitos, o la entrada se anula.
    4. `sample_local`: poner `w->pos` (y `yaw`) a cero antes de cada `return false`, para que el STATE con LOADING llegue.
    5. Actualizar `specs/contratos/validacion-red.md` con la regla nueva (núcleo / por entrada).
  hecho cuando: `verify` cumplido; la bitácora dice que **sí** cambia comportamiento (antes se tiraba el STATE entero).
  no hagas: no cambies el formato de ningún mensaje ni `PROTOCOL`; no cortes la conexión por un dato inválido; no quites las comprobaciones `isfinite` de `oa_*`.
  modelo sugerido: alto

## FASE 0 — Cadena OA, primer paso (antes de tocar nada)

- [~] T201 [oa] Firma de comportamiento de referencia.
  reclamada: agente Sonnet firma-t201 (integrada `7b53d18`, adelantada) — `--autoprueba` 0; FALTA etiqueta, tres pasadas y comparar
  files: `tools/coop/firma_log.py`; referencia en `specs/bitacora/<fecha>-F2-firma-referencia.md`
  verify: `$PY tools/coop/firma_log.py "$SW/pruebas/logs/A.log" "$SW/pruebas/logs/B.log"` imprime una tabla "tipo de línea → recuento" por juego y termina con 0; la bitácora contiene la referencia de tres pasadas (mínimo y máximo por tipo); `$PY tools/coop/firma_log.py --comparar <referencia> A.log B.log` termina con 0 contra una cuarta pasada sin cambios de código
  depende de: F1 cerrada (T121)
  quién: agente
  contexto: F2 va a mover miles de líneas "sin cambiar comportamiento" y hace falta una medida de qué hace hoy una sesión. El modo online escribe en el log líneas `[online] ...` en cada evento (unión, creación de actores, traspaso de enemigos, cargas...). El recorrido de 30 min existe desde F1 (`tools/coop/recorrido.py`).
  pasos:
    1. `git tag f1-cerrada coop/main`.
    2. Escribir `firma_log.py`: toma las líneas que contienen `[online]`, sustituye números, direcciones hexadecimales y los nombres de jugador (`Host`, `Client`) por marcadores, y cuenta cuántas hay de cada texto resultante.
    3. Modo `--comparar`: aplica la tolerancia de `plan.md` §4.4 (ningún tipo nuevo; ningún tipo fijo desaparecido; recuentos exactos solo para eventos únicos) y sale con 0 o 1.
    4. Tres pasadas del recorrido; guardar la referencia (un fichero de texto con la tabla) en la bitácora.
    5. Cuarta pasada: comprobar que `--comparar` da 0. Si da 1 sin haber cambiado código, la tolerancia es demasiado estricta: ajustarla y anotarlo.
  hecho cuando: `verify` cumplido.
  no hagas: no guardes los logs en el repositorio; no cambies código del juego; no uses paquetes fuera de la biblioteca estándar.
  modelo sugerido: medio

## FASE 0 — Cadena NET (en serie, un dueño)

- [~] T202 [net] Una sola definición de los mensajes, compartida con el bot.
  reclamada: agente Sonnet cable-t202 (integrada `2f38cc9`, adelantada) — make -j8 0, bot sin avisos, test-online-net 162/0; FALTA bot contra el juego y Docker
  files: `runtime/online_wire.h` (nuevo), `runtime/online.c`, `tools/online_bot.c`
  verify: `make -j8` compila; `cc -O2 -Wall -Iruntime -o build/online_bot.exe tools/online_bot.c -lws2_32` compila; `grep -n "typedef struct" tools/online_bot.c` ya no muestra `WireState` ni `WireHello`; `grep -n "version = 38" tools/online_bot.c` no devuelve nada; `make test-online-net` sigue en 0; el bot se une al juego A y camina como antes
  depende de: T201 (para tener F1 cerrada y etiquetada)
  quién: agente
  contexto: El formato de los mensajes vive en `runtime/online.c`: `MAGIC` y `PROTOCOL` (`:39-40`), la enumeración de tipos (`:50`), los motivos de rechazo (`:57`) y las estructuras empaquetadas `WireState`, `WireHello`, `WireEquip` (`:59-108`). `tools/online_bot.c` lleva su propia copia escrita a mano (`:31-78`) y la versión 38 como literal (`:102`). Cada cambio de protocolo hay que repetirlo en los dos sitios; un olvido rompe el bot en silencio.
  pasos:
    1. Crear `runtime/online_wire.h` con `#pragma once`, `#include "online.h"`, y **mover** (cortar y pegar, sin retocar) desde `online.c`: `MAGIC`, `PROTOCOL` con su comentario, la enumeración `M_*`, la enumeración `R_*`, `SKEL_BONE_BYTES`, `SKEL_BYTES` y el bloque `#pragma pack(push, 1)` ... `#pragma pack(pop)` con las tres estructuras.
    2. Al final de la cabecera, las cinco aserciones de tamaño de `plan.md` §2.1. Si alguna no compila, corregir el **número** con el tamaño real que diga el compilador y anotarlo en la bitácora (los números se calcularon a mano).
    3. `online.c`: `#include "online_wire.h"` en lugar de las definiciones.
    4. `online_bot.c`: borrar su copia de las estructuras y de las constantes de tipo; incluir `online_wire.h`; sustituir el literal 38 por `PROTOCOL` y el de la magia por `MAGIC`. Si el bot usa nombres de campo distintos de los de `WireState`, cambiar el bot, no la cabecera.
    5. Actualizar la línea de compilación de la cabecera del bot (`:7`) con `-Iruntime`.
  hecho cuando: `verify` cumplido y `sizeof` idénticos (lo garantizan las aserciones).
  no hagas: no cambies ningún campo, orden ni tipo de las estructuras; no subas `PROTOCOL`; no muevas a esta cabecera nada que no sea formato de cable; no toques `runtime/online.h` salvo que una aserción demuestre un error del mapa (entonces avisa).
  modelo sugerido: medio

- [~] T203 [net] Pruebas de trama y codificación sin el juego.
  reclamada: agente Sonnet trama-t203 (integrada `baf62ff`, adelantada) — Windows: test-online-wire 16647/0 (error SKEL máx. 9,93e-5 rad, tolerancia 2e-4), test-online-net 309/0; el orquestador agrandó el búfer de la prueba (255 enemigos no cabían); FALTA Docker
  files: `tools/test_online_wire.c` (nuevo), `tools/test_online_net.c`, `Makefile` (bloque al final)
  verify: `make test-online-wire test-online-net` terminan con 0 en Windows; en Docker: `docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas sh -c 'make B=/tmp/b GEN=/nonexistent test-online-wire test-online-net'` termina con 0
  depende de: T202
  quién: agente
  contexto: La trama es `u16 longitud, u8 tipo, carga` (`runtime/online.c:3-7`). `next_msg` (`:172`) extrae el siguiente mensaje completo de un `RecvBuf` de 32 768 bytes (`:161`); `consumed` (`:182`) desplaza el resto; `send_msg` (`:150`) descarta en silencio un mensaje de más de 16 384 bytes. `skel_encode` / `skel_decode` (`:531`, `:556`) comprimen cada hueso a 12 bytes. `cl_message` (`:967`) aplica cada tipo. Nada de esto tiene pruebas, salvo los casos de F1.
  pasos:
    1. `tools/test_online_wire.c` (incluye `../runtime/online.c` con los mismos dobles que el arnés de F1): (a) un mensaje entregado byte a byte solo aparece al llegar el último byte; (b) dos mensajes pegados salen los dos; (c) longitud 0 nunca se completa; (d) longitud 65 535 nunca se completa y no escribe fuera del buffer; (e) `consumed` conserva el resto.
    2. `skel_encode` → `skel_decode` con 1 000 cuaterniones unitarios aleatorios (semilla fija): error de ángulo por debajo de una tolerancia que fijas midiendo (anótala); cuaternión nulo; traslaciones fuera de rango saturan sin desbordar.
    3. En `test_online_net.c`: para **cada** tipo `M_*` que maneja `cl_message`, un caso con carga un byte más corta de lo mínimo (no debe cambiar nada) y, donde haya contador, un caso con el contador por encima del máximo (ENEMIES > 32, LOOTS > 64, SKEL con `n + nbow` > 160).
    4. Reglas `$(B)/test_online_wire`, objetivo `test-online-wire`, `.PHONY`, usando `$(B)` y `$(CC)` como `test-control`.
  hecho cuando: `verify` cumplido en los dos sistemas.
  no hagas: no cambies `runtime/online.c` para que una prueba pase; si una prueba descubre un fallo real, déjala escrita, desactívala con un comentario que diga por qué, y anótalo; no abras sockets.
  modelo sugerido: medio

- [x] T204 [net] Pruebas de la máquina de estados de sesión.
  reclamada: agente Sonnet sesion-t204 (integrada `3297186`) — Docker/Linux: 9 casos 0 fallos, 2 pendientes (T218); Windows: aviso y 0
  files: `tools/test_online_session.c` (nuevo), `Makefile` (bloque al final)
  verify: en Docker: `docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas sh -c 'make B=/tmp/b GEN=/nonexistent test-online-session'` termina con 0 y lista al menos los nueve casos de `pasos`; en Windows `make test-online-session` imprime que la prueba es solo POSIX y termina con 0
  depende de: T203
  quién: agente
  contexto: El servidor es un único hilo: `srv_loop` (`runtime/online.c:343`) acepta hasta 8 conexiones y llama a `srv_message` (`:232`) por cada mensaje. La primera vez exige HELLO: magia y versión iguales, contraseña igual si la hay, nombre no vacío; si falla envía REJECT con el motivo (`R_FULL=1, R_VERSION, R_PASSWORD, R_NAME`, `:57`) y corta; si va bien envía WELCOME con la ranura, un JOIN por cada jugador presente y un JOIN del nuevo a todos (`:235-261`). `srv_drop` (`:218`) cierra y envía LEAVE. Un cliente callado 10 s se expulsa (`TIMEOUT_NS`, `:46`). `srv_message` escribe en sockets reales, por eso la prueba usa `socketpair` (POSIX).
  pasos:
    1. Arnés que incluye `../runtime/online.c`, crea un `Server` en memoria y, por cada cliente simulado, un `socketpair`: un extremo en `sv->c[i].s` (con `used = true`), el otro lo lee la prueba.
    2. Casos: (1) HELLO correcto → WELCOME con la ranura; (2) versión distinta → REJECT `R_VERSION`; (3) contraseña mala → REJECT `R_PASSWORD`; (4) nombre vacío → REJECT `R_NAME`; (5) mensaje que no es HELLO como primero → conexión rechazada; (6) segundo cliente recibe JOIN del primero y el primero recibe JOIN del segundo; (7) `srv_drop` → los demás reciben LEAVE con esa ranura; (8) la ranura liberada se reutiliza; (9) STATE de un cliente llega a los demás con su ranura delante y **no** vuelve al emisor.
    3. Sesión llena (`R_FULL`) y expulsión por silencio viven en `srv_loop`, que usa `accept` y una estática de función (`:374`): si no se pueden probar sin cambiar `online.c`, escribir los casos como pendientes con el motivo (los desbloquea T218).
    4. Regla del `Makefile` con condición de plataforma (en Windows, el objetivo solo imprime un aviso).
  hecho cuando: `verify` cumplido; los casos no probables quedan listados con su motivo.
  no hagas: no modifiques `runtime/online.c`; no uses puertos reales; no dependas de esperas reales (`sleep`).
  modelo sugerido: alto

- [x] T205 [P] [ci] Flujo de CI para las pruebas del cooperativo.
  reclamada: orquestador (`487a5af`) — pasos del flujo en el contenedor Linux en 0; FALTA verlo verde en Actions tras el push
  files: `.github/workflows/coop.yml` (nuevo)
  verify: `$PY -c "import yaml,sys; yaml.safe_load(open('.github/workflows/coop.yml'))"` no da error (o revisión manual si no hay `yaml`); tras el siguiente push que pida el dueño, el flujo aparece en Actions y termina en verde
  depende de: T203
  quién: agente
  contexto: Los flujos existentes (`tests.yml`, `windows.yml`) no ejecutan ninguna prueba del modo online. El plan aprobado pide pruebas de protocolo en cada cambio. Un flujo nuevo es un fichero aislado: no entra en conflicto con los del proyecto original.
  pasos:
    1. Flujo `Coop tests`: `on: push` y `pull_request` a `coop/main`, más `workflow_dispatch`; `permissions: contents: read`.
    2. Un job en `ubuntu-latest`: `actions/checkout`, `sudo apt-get install -y clang make`, y `make B=/tmp/b GEN=/nonexistent test-online-gate test-online-wire test-online-net test-online-session`.
    3. Copiar la forma (concurrencia, `timeout-minutes`) de `.github/workflows/tests.yml`.
  hecho cuando: `verify` cumplido (la segunda parte, cuando haya push).
  no hagas: no añadas secretos; no edites otros flujos; no hagas el push tú.
  modelo sugerido: bajo

- [x] T213 [net] Capa única de validación de lo que llega por red.
  reclamada: agente Opus valida-t213 (integrada `4bed182`) — validate 181320/0, net 309/0, wire 16647/0; Linux en 0; FALTA bot contra el juego y firma
  files: `runtime/online_validate.h`, `runtime/online_validate.c` (nuevos), `runtime/online.c`, `tools/test_online_validate.c` (nuevo), `Makefile` (bloque al final)
  verify: `make test-online-validate` termina con 0 (casos dirigidos + 100 000 estructuras aleatorias con semilla fija); `make test-online-wire test-online-net` siguen en 0; `grep -n "online_validate_" runtime/online.c` muestra una llamada en cada `case` de `cl_message` que guarda datos; recorrido de F1 completo con firma igual a la referencia; bot con `BOT_NAN=1`, `BOT_LOOT=...` y `BOT_SHIELD=1`: el juego sigue y registra los descartes
  depende de: T204
  quién: agente
  contexto: La validación está repartida y es incompleta: `remote_snap` (`runtime/online.c:907-964`) termina cadenas y recorta tres contadores; cada `case` de `cl_message` (`:1042-1113`) comprueba longitudes a su manera; F1 añadió comprobaciones de finitud para algunos campos; `online_actors.c` comprueba `isfinite` en unos diez sitios y no en otros (posición del jugador, matrices de equipo sobre hueso, disparos, enemigos, hielo, Magnesis, Stasis; `specs/mapa/07` M4). Un olvido de este tipo ya ocurrió (`shield`, F1).
  pasos:
    1. `online_validate.h/.c`: funciones puras, sin estado ni dependencias del juego. Una por estructura: `WireState` (todas sus cadenas terminadas; **todos** sus campos `float` finitos; posiciones y traslaciones con módulo ≤ 100 000; contadores y enumerados en rango), `WireEquip`, `OnlineEnemy`, `OnlineLoot`, PICK, HIT, TIME, SKEL. Recorre los campos **uno a uno, por nombre**, leyendo la definición en `online_wire.h` y `online.h`.
    2. `online_validate_name(s, cap)`: terminada dentro de `cap` y solo caracteres `[A-Za-z0-9_]`; vacía es válida.
    3. En `cl_message`: llamar a la validación sobre una copia local antes de guardar; si devuelve falso, descartar (el mensaje, o la entrada dentro de un mensaje con lista) y contar el descarte. Un `LOG` la primera vez por tipo.
    4. Quitar de `online.c` las comprobaciones sueltas que la capa ya cubre (las de F1 y las de `remote_snap`), **en el mismo commit**, para que no queden dos sitios.
    5. `test_online_validate.c`: casos dirigidos (un valor malo por campo) y el bucle aleatorio: rellenar la estructura con bytes aleatorios, validar, y si devuelve verdadero comprobar con código independiente que no queda ningún `float` no finito, cadena sin terminar ni contador fuera de rango.
    6. Escribir las reglas aplicadas, campo por campo, en la entrada de bitácora, para que el agente de `specs/contratos/validacion-red.md` las copie.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies el formato de ningún mensaje; no cortes la conexión por un dato inválido; no valides tipos de actor aquí (botín / enemigo: eso depende de las tablas del overlay y se queda en `oa_*`); no borres las comprobaciones `isfinite` de `oa_*` (segunda barrera).
  modelo sugerido: alto

## FASE 0 — Cadena OA (estrictamente en serie, un dueño, un worktree)

- [x] T206 [oa] Reordenar `online_actors.c` por módulos, dentro del mismo fichero.
  reclamada: agente Opus oa-t206 (integrada `3370d18`) — oa_check IGUALES; mismo único aviso; FALTA firma de referencia
  files: `runtime/online_actors.c`, `tools/coop/oa_check.py` (nuevo), `specs/decisiones/<n>-reparto-oa.md`
  verify: `$PY tools/coop/oa_check.py --antes f1-cerrada:runtime/online_actors.c --despues runtime/online_actors.c` termina con 0; `make -j8` compila sin avisos nuevos; `make test-online-gate` en 0; recorrido corto (pasos 1-3) pasa
  depende de: T201
  quién: agente
  contexto: `runtime/online_actors.c` tiene 6 468 líneas y unas 180 funciones, con las áreas mezcladas (por ejemplo `all_actors` se declara en `:860` y se define en `:2002`; `crc32_str` se declara en `:1044` y `:2340` y se define en `:3656`). `specs/mapa/07` §8.1 propone 11 módulos; `plan.md` §3.1 da los rangos aproximados. Antes de cortar en ficheros se agrupa dentro del mismo fichero, porque así el compilador sigue viendo una sola unidad y es fácil demostrar que nada cambió.
  pasos:
    1. Escribir `oa_check.py` (ver `plan.md` §3.2): parte cada fichero en definiciones de nivel superior con su comentario de encima; compara multiconjuntos ignorando prototipos `static ...;` y líneas separadoras `// ==== ... ====`; imprime lo que sobra y lo que falta; sale con 0 o 1. Probarlo primero contra el fichero sin cambios (debe dar 0) y contra una copia con una línea alterada (debe dar 1).
    2. Hacer la tabla función → módulo (todas las definiciones de nivel superior, también variables y macros) y guardarla en `specs/decisiones/`.
    3. Comprobar que no hay `#undef` ni `#define` repetidos: `grep -n "#undef" runtime/online_actors.c` y `grep -n "^#define" runtime/online_actors.c | awk '{print $2}' | sort | uniq -d` (hoy: ninguno).
    4. Mover las definiciones por bloques, en este orden de módulos: core, gamedata, sample, puppet, runes, enemies, loot, world, flags, rules, control, frame. Añadir al principio los prototipos `static` que el compilador pida.
    5. Compilar y pasar `oa_check.py` tras cada módulo movido. Un commit al final.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies ni un carácter dentro de una función, variable o macro (ni espacios, ni comentarios, ni nombres); no quites `static`; no crees ficheros `.c` nuevos todavía; no arregles nada que veas mal.
  modelo sugerido: alto

- [x] T207 [oa] Cortar en módulos `oa_*.c` con una cabecera interna.
  reclamada: agente Opus oa-t207 (integrada `929661f`; ADR renumerado a ADR-023) — make -j8, 6 pruebas, Linux distro_build 0; dos juegos a 60 fps; FALTA firma
  files: `runtime/oa_core.c`, `oa_gamedata.c`, `oa_sample.c`, `oa_puppet.c`, `oa_runes.c`, `oa_enemies.c`, `oa_loot.c`, `oa_world.c`, `oa_flags.c`, `oa_rules.c`, `oa_control.c`, `oa_frame.c`, `runtime/online_actors_internal.h` (nuevos); `runtime/online_actors.c` (se elimina)
  verify: `$PY tools/coop/oa_check.py --antes <commit de T206>:runtime/online_actors.c --modulos "runtime/oa_*.c"` termina con 0; `make -j8` enlaza en Windows; `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` termina con 0; `wc -l runtime/oa_*.c` anotado; recorrido completo de F1 con `firma_log.py --comparar` en 0
  depende de: T206
  quién: agente
  contexto: Tras T206 el fichero está agrupado por módulos con separadores. Los `.c` nuevos en `runtime/` entran solos en la compilación (`RT_SRC := runtime/*.c`, `Makefile:62`) y toda cabecera de `runtime/` es dependencia de todos los objetos. El ejecutable de Windows exporta todos sus símbolos, así que un nombre que deja de ser `static` puede chocar con otro del runtime.
  pasos:
    1. Crear `online_actors_internal.h`: los `#include` comunes, las macros (`VALID`, `IN_IMAGE`, `ARMOR`...), todas las constantes `FN_*` y `P_*`, y los tipos (`Watch`, `Handle`, `Piece`, `InstParams`, `MapLine`...).
    2. Cortar cada bloque a su fichero. Añadir a `oa_check.py` el modo `--modulos`, que une los ficheros y compara ignorando la palabra `static`.
    3. Compilar: cada "no declarado" es un símbolo que cruza de módulo. Para cada uno: quitar `static` en su definición y declararlo en la cabecera interna. Las variables de estado compartido se definen en su módulo dueño y se declaran `extern` en la cabecera.
    4. Commit 1 ("cortar"). Verificar.
    5. Commit 2 ("prefijos"): renombrar con `oa_` todo símbolo que quedó sin `static` y no empieza por `rt_`, `online_` u `oa_`, con una sustitución mecánica por palabra completa; guardar la tabla de nombres en `specs/decisiones/`; `oa_check.py` acepta la tabla con `--nombres <fichero>`.
    6. Compilar en Linux (Docker). Recorrido y firma.
  hecho cuando: `verify` cumplido tras los dos commits.
  no hagas: no cambies cuerpos de función más allá del renombrado mecánico del commit 2; no rediseñes el estado compartido; no partas `g_act` (es T208); no añadas `RT_ON_RESET` (es T212); no dejes un símbolo exportado sin prefijo.
  modelo sugerido: alto

- [~] T208 [oa] Partir `g_act[]` por dueño.
  reclamada: agente Opus oa-t208 (integrada) — diagnóstico: sin regresión (el bucle de enemigos es HE-23, anterior a T208, ahora T453); pasada con T209: pasos 4 y 5 PASAN; FALTA firma
  files: `runtime/oa_puppet.c`, `runtime/oa_runes.c`, `runtime/oa_world.c`, `runtime/oa_rules.c`, `runtime/oa_frame.c`, `runtime/oa_core.c`, `runtime/online_actors_internal.h`
  verify: `grep -rn "g_act\[" runtime/` no devuelve nada; `make -j8` compila; `$PY tools/coop/firma_log.py --comparar ...` en 0 tras el recorrido completo; en juego: Magnesis, Stasis y Cryonis de B se ven en A
  depende de: T207
  quién: agente
  contexto: `g_act[ONLINE_MAX]` (definida en `runtime/online_actors.c:131-180` en el commit original) guarda por jugador remoto campos de varias áreas: su Link y piezas, sus runas (`grab`, `stop`, `released`, piezas de runa, `holder`) y sus pilares de hielo. `objs_update` lee `grab`, `stop` y `released` (`:4698`, `:4756`), y `players_collide` lee el Link (`:2852`). Mientras sea una sola estructura, cualquier cambio en un área recompila y arriesga las demás. `forget_all` (`:316-336`) la borra entera con `memset` y luego pone `anim = -1` y `covered = -1`; `drop` (`:423`) limpia la de un jugador.
  pasos:
    1. Crear `g_pup[]` en `oa_puppet.c`, `g_rune[]` y `g_ice[]` en `oa_runes.c`, con los campos de la tabla de `plan.md` §3.5, todas `static`.
    2. Sustituir cada `g_act[i].campo` por la tabla nueva. Donde otro módulo lee un campo ajeno, añadir una función de consulta en el módulo dueño (`oa_puppet_link(i)`, `oa_rune_holds(actor)`, `oa_rune_released(i)`...) y declararla en la cabecera interna.
    3. `forget_all` y `drop`: mover la parte de cada tabla a `oa_puppet_forget` / `oa_runes_forget` (y `_drop`), llamadas **en el mismo punto y orden** que hoy. Reproducir `anim = -1` y `covered = -1`.
    4. Eliminar `g_act`.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies el orden en que se olvidan o borran las piezas; no cambies qué se limpia; no conviertas campos en punteros ni cambies sus tipos.
  modelo sugerido: alto

- [~] T209 [oa] Orden explícito en `rt_online_frame`.
  reclamada: agente Opus oa-t209 (integrada en `69fa86c`) — compila (1 aviso previo), online_players 8/8, pruebas de red en 0; FALTA pasada de dos juegos y firma
  files: `runtime/oa_frame.c`, y una función `oa_<módulo>_frame` en cada `runtime/oa_*.c` que tenga un paso; `runtime/online_actors_internal.h`
  verify: `rt_online_frame` cabe en una pantalla (menos de 80 líneas) y consiste en llamadas en el orden de `plan.md` §3.6, con ese orden en un comentario encima; `make -j8` compila; `firma_log.py --comparar` en 0 tras el recorrido completo; el número de llamadas a `online_players` por fotograma no cambia (`grep -c "online_players(" runtime/oa_*.c` igual que antes de la tarea)
  depende de: T208
  quién: agente
  contexto: `rt_online_frame` (unas 750 líneas, `runtime/online_actors.c:5721-6468` en el original) hace todo el trabajo por fotograma en un orden que **es** parte del comportamiento: muestreo → reglas → botín → flags → mundo → runas → enemigos → Links remotos, con salidas tempranas en medio (`g_settle`, Link local muerto, fuera de sesión). Un fallo de F1 nació de ese orden implícito. Hoy el orden solo se conoce leyendo la función entera.
  pasos:
    1. Extraer cada bloque numerado de `plan.md` §3.6 a una función con nombre en su módulo, **sin cambiar su contenido**: las variables locales que el bloque usa pasan como parámetros (`c`, `pinfo`, `player`, `link`, `me`).
    2. Las salidas tempranas se conservan: si un bloque hacía `return`, su función devuelve un `bool` "terminar el fotograma" y `rt_online_frame` hace `if (...) return;` en ese punto.
    3. Las estáticas de función de `rt_online_frame` (`was_dead`, `tick`, `why`...) pasan a estáticas de fichero del módulo que se queda con el bloque, con el mismo nombre.
    4. Comentario de cabecera con la lista numerada. Pasar el texto de la lista al dueño para `specs/contratos/hilos-y-frame.md` (ese fichero lo escribe otro agente).
  hecho cuando: `verify` cumplido.
  no hagas: no reordenes pasos; no fusiones las ocho llamadas a `online_players` en una; no quites los ganchos de prueba por variable de entorno; no muevas la comprobación `alive` del bucle de jugadores (es T210).
  modelo sugerido: alto

- [~] T210 [oa] Validar los Links remotos al principio del fotograma (cambio de comportamiento).
  reclamada: agente Opus oa-t210 (integrada) — paso 9b `oa_puppets_validate`, el paso 4 ya no lee un Link borrado; compila, pruebas de red en 0; FALTA recorrido, firma y prueba en juego (A muere 5 veces con Magnesis)
  files: `runtime/oa_frame.c`, `runtime/oa_puppet.c`, `runtime/online_actors_internal.h`
  verify: `make -j8` compila; recorrido completo pasa; en juego, A muere con B a la vista y usando Magnesis, cinco veces, sin cuelgue; `firma_log.py --comparar`: la única diferencia admitida es el orden o el recuento de la línea `the game removed ...'s Link`; revisión: entre el paso nuevo y el final del fotograma ningún módulo usa un Link remoto sin pasar por `oa_puppet_link(i)`
  depende de: T209
  quién: agente
  contexto: La comprobación de que el Link remoto sigue vivo (`alive(link, watch)`) ocurre en el último paso del fotograma (bucle de jugadores; `:6036` en el original): si el juego lo borró, se llama a `forget_all`, se esperan 180 fotogramas y se sale. Pero los pasos anteriores (colisión, PvP, runas, hielo) ya han usado ese puntero. F1 puso un parche local (`link_live`). Aquí se arregla el orden: validar primero, usar después.
  pasos:
    1. En `oa_puppet.c`: `bool oa_puppets_validate(void)` que recorre los jugadores y devuelve verdadero si algún Link guardado ya no está `alive`.
    2. En `rt_online_frame`, justo después de las salidas tempranas del paso 9 y antes del paso 11: si `oa_puppets_validate()` es verdadero, hacer exactamente lo que hacía el bucle de jugadores (el mismo `LOG`, `forget_all`, `g_settle = 180`, `return`).
    3. Dejar la comprobación antigua del bucle de jugadores como está (segunda barrera).
    4. `oa_puppet_link(i)` devuelve 0 si el Link no está vivo.
  hecho cuando: `verify` cumplido; la entrada de bitácora dice explícitamente que esta tarea **sí** cambia comportamiento y cuál.
  no hagas: no borres actores desde la validación (solo olvidar, como hoy); no cambies los 180 fotogramas; no quites `link_live`.
  modelo sugerido: alto

- [ ] T211 [oa] Inventario de variables estáticas que guardan estado del juego.
  files: `tools/coop/oa_statics.py` (nuevo), `tools/coop/oa_statics_permitidas.txt` (nuevo), `specs/decisiones/<n>-inventario-estaticas.md`
  verify: `$PY tools/coop/oa_statics.py --listar` imprime todas las variables `static` (de fichero y de función) de `runtime/oa_*.c` y `runtime/online.c` con fichero, línea, función y tipo; el documento de decisiones clasifica el 100 % de ellas; `$PY tools/coop/oa_statics.py` (modo comprobación) termina con **1** y lista las pendientes de limpiar (es lo esperado antes de T212)
  depende de: T210
  quién: agente
  contexto: El juego puede reiniciarse dentro del mismo proceso ("Quit to start screen"): su memoria se borra y se vuelve a crear, pero las variables `static` del runtime sobreviven. La regla del proyecto es limpiar ese estado con `RT_ON_RESET` y no conservar punteros del juego entre arranques (`AGENTS.md`; `runtime/hle.h:28-40`). El código online no tiene ningún `RT_ON_RESET`. Hay del orden de 85 estáticas de fichero y unas 200 de función; ejemplos con punteros del juego: `g_handles`, `g_orphan`, `g_boom`, `g_own`, `g_reg`, `g_ground`, `g_boss`, `g_brk`, `g_spawn`, `g_world`, y estáticas de función como `tr[]` en `sample_shots`.
  pasos:
    1. `oa_statics.py`: localizar con una expresión regular las declaraciones `static` que no son funciones, a nivel de fichero y dentro de funciones (sangradas); para cada una, nombre, tipo, fichero, línea y función contenedora.
    2. Clasificar cada una en el documento: (a) puntero del juego, handle, `Watch` o `Piece` → limpiar; (b) estado lógico de una partida (por ejemplo `slate_started`, `korok_played`, `had`, `held_off`, `was_dead`, contadores `tick`) → limpiar; (c) tabla cargada de un `.txt`, indicador de variable de entorno, offset descubierto una vez, mutex → permitir, con motivo.
    3. Escribir en `oa_statics_permitidas.txt` las de la clase (c): `fichero:nombre  motivo`.
    4. Modo comprobación: sale con 0 solo si cada estática está en la lista de permitidas o aparece asignada dentro de una función `RT_ON_RESET` de su fichero.
  hecho cuando: `verify` cumplido; el dueño de la cadena revisa las dudosas.
  no hagas: no cambies código de `runtime/` en esta tarea; no clasifiques como "permitir" algo que guarde una dirección del juego (`uint64_t` que se usa como actor o unidad).
  modelo sugerido: alto

- [ ] T212 [oa] `RT_ON_RESET` en todos los módulos (cambio de comportamiento en el reinicio).
  files: `runtime/oa_*.c`, `runtime/online.c` (solo si el inventario lo pide), `tools/coop/oa_statics_permitidas.txt`
  verify: `$PY tools/coop/oa_statics.py` termina con 0; `make -j8` compila; en juego, con sesión activa: "Quit to start screen" → Continuar, tres veces en cada juego, sin cuelgue; el compañero ve LEAVE y la sesión queda cerrada en los dos (ADR-025); el log muestra una línea `[online] reset` por reinicio; recorrido completo pasa
  depende de: T211
  quién: agente
  contexto: `RT_ON_RESET(fn) { ... }` (`runtime/hle.h:40`) registra una función que se ejecuta en el reinicio en caliente, con los hilos del juego ya parados y antes de borrar su memoria. Ejemplo a copiar: `RT_ON_RESET(items_reset)` en `runtime/items.c:337-341`. La macro usa `__LINE__` para nombrar su entrada: un `RT_ON_RESET` por línea. `forget_all` hoy limpia solo parte del estado y **encola huérfanos** para borrarlos después; tras un reinicio esos punteros apuntan a memoria de otro arranque.
  pasos:
    1. Por cada módulo, una función `RT_ON_RESET(oa_<módulo>_reset) { ... }` al final del fichero que pone a cero (`memset` o asignación) cada variable marcada "limpiar" en el inventario.
    2. Las estáticas de **función** marcadas "limpiar" se suben a nivel de fichero justo encima de su función, con el mismo nombre y el mismo valor inicial; el cuerpo de la función no cambia.
    3. `oa_core`: en su reset, vaciar también el anillo de handles, el pool y los huérfanos **sin llamar al juego**.
    4. Valores iniciales que no son cero (por ejemplo `anim = -1`, `covered = -1`, cachés que empiezan en -1): restaurar ese valor, no cero.
    5. Un `LOG("[online] reset\n")` en el reset de `oa_frame`.
    6. Ir módulo a módulo: compilar, `oa_statics.py`, recorrido corto. Un commit por módulo.
    7. **ADR-025 n.º 2 (Q-EST-1): el reinicio cierra la sesión.** `online.c` registra su `RT_ON_RESET` que manda LEAVE y cierra la sesión limpiamente (como "Leave" del menú), y el juego empieza como recién arrancado. El compañero ve "left", no un cierre de TCP.
  hecho cuando: `verify` cumplido.
  no hagas: no llames a `call(c, ...)`, `FN_DELETE`, `delete_piece` ni a nada que entre en el juego desde un reset; no cierres el socket desde el reset de `oa_*` (eso lo hace `online.c`, ver paso 7); no limpies las tablas de los `.txt` ni los mutex.
  modelo sugerido: alto

## Puerta de la fase 0

- [ ] T214 [integración] Integrar las dos cadenas y pasar la puerta.
  files: `specs/bitacora/<fecha>-F2-fase0.md`; etiqueta local `f2-fase0`
  verify: en `coop/main`: `make -j8`; `make test-online-gate test-online-wire test-online-net test-online-validate`; en Docker `test-online-session` y `distro_build.sh debian:13`; `$PY tools/coop/oa_statics.py`; pruebas de la línea base de F0 — todo en 0 o igual que en F0; recorrido completo desatendido y con ventana; `firma_log.py --comparar` en 0 (con las diferencias admitidas de T210 y T212 anotadas)
  depende de: T205, T212, T213
  quién: agente + dueño (pasada con ventana, aprobación)
  contexto: Los carriles en paralelo solo son seguros si la base es estable: a partir de aquí cada agente toca un módulo y confía en que los demás no cambian debajo.
  pasos:
    1. Fusionar la cadena NET y luego la cadena OA en `coop/main` (el `Makefile` es el único fichero común).
    2. Ejecutar todo `verify`.
    3. `git tag f2-fase0`.
    4. Entrada de bitácora: qué demuestra cada comprobación y qué no (`plan.md` §3.4).
  hecho cuando: el dueño aprueba abrir los carriles.
  no hagas: no abras carriles con una prueba en rojo "ya conocida" sin anotarla; no subas nada.
  modelo sugerido: medio

## CARRILES (en paralelo; cada uno toca solo sus ficheros)

- [ ] T215 [P] [mundo] `has_body` en troncos y guarda en `break_here`.
  files: `runtime/oa_world.c`
  verify: `make -j8` compila; en juego: A tala un árbol y mueve el tronco, B lo ve; A rompe una caja, en B desaparece; revisión: toda llamada a `FN_BODY_TO` de `oa_world.c` está tras `has_body`; `firma_log.py --comparar` sin tipos de línea nuevos salvo los de rechazo
  depende de: T214
  quién: agente
  contexto: `has_body(a)` dice si un actor tiene de verdad un cuerpo físico en `+0x190`. En `objs_update`, los troncos entran sin esa comprobación y acaban en `FN_BODY_TO` con una matriz de la red (`runtime/online_actors.c:4611-4627`, `:4727-4738` en el original). `break_here` (`:5132-5196`) escribe `vida = 0` en `actor+0x848` de cualquier actor "rompible" (lista de prefijos de nombre, `:5025-5033`) cuyo HashId llegue en la lista de otro jugador (`:5178-5180`): si la clase de ese actor no guarda la vida ahí, pisa otro campo.
  pasos:
    1. En `objs_update`: exigir `has_body(a)` antes de cada `FN_BODY_TO` sobre un tronco.
    2. En `break_here`: antes de escribir en `+0x848`, exigir `has_body(a)` y que el valor actual sea una vida plausible (entre 0 y un máximo que fijas leyendo cómo se usa `ENEMY_LIFE` / `+0x848` en el módulo; anótalo). Si no, no escribir y registrar una vez.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies la lista de prefijos "rompible"; no toques otros módulos ni la cabecera interna.
  modelo sugerido: alto

- [ ] T216 [P] [enemigos] Pool propio para marionetas de enemigo y botín.
  files: `runtime/oa_enemies.c`, `runtime/oa_loot.c`
  verify: `make -j8` compila; `grep -n "piece_update(c, 0," runtime/oa_enemies.c runtime/oa_loot.c` no devuelve nada; en juego: en el campamento del recorrido, cuando el jugador de la ranura 0 se aleja más de 400 m y vuelve, las marionetas de enemigo y el botín del otro juego siguen ahí; recorrido completo pasa
  depende de: T214
  quién: agente
  contexto: Las piezas de cada jugador remoto se guardan en un pool por jugador (`g_pool[8][24]`) para reutilizarlas. Las marionetas de enemigo, el botín y las fuentes de elemento llaman a `piece_update(c, 0, ...)`: usan el pool **del jugador 0** (`runtime/online_actors.c:2231`, `:3044`, `:3058`, `:2281` en el original). Consecuencias: `drop(c, 0, true)` les borra piezas, y con el pool lleno (24) se borra la entrada 0 (`:385`). El índice de jugador de `piece_update` va de 0 a `ONLINE_MAX - 1`.
  pasos:
    1. Leer en `oa_core.c` cómo `piece_update`, `pool_update` y `drop` usan el índice de jugador.
    2. Pedir al coordinador una ranura de pool adicional (índice `ONLINE_MAX`) para "mundo": es un cambio en `oa_core.c` y en la cabecera interna que hace el coordinador, no este carril. Mientras llega, preparar el cambio detrás de una constante local.
    3. Sustituir el 0 por la constante de la ranura "mundo" en las llamadas de estos dos módulos.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies `oa_core.c` ni la cabecera interna tú mismo; no cambies el tamaño del pool por jugador.
  modelo sugerido: alto

- [ ] T217 [P] [runas] `has_body` en Magnesis sobre un objeto del botín.
  files: `runtime/oa_runes.c`
  verify: `make -j8` compila; en juego: B coge con Magnesis un arma metálica del suelo y A la ve moverse; B coge una caja metálica y A la ve moverse; revisión: toda llamada a `FN_BODY_TO` de `oa_runes.c` está tras `has_body`
  depende de: T214
  quién: agente
  contexto: `grab_update` busca el objeto que otro jugador sostiene con Magnesis: primero entre el botín de ese jugador, luego entre los actores por nombre y posición. En la búsqueda por nombre ya exige `has_body` (`runtime/online_actors.c:4387` en el original), pero cuando lo encuentra en el botín (`:4369-4381`) pasa directo a `FN_BODY_TO` (`:4405-4406`) sin comprobarlo.
  pasos:
    1. Localizar en `grab_update` las dos rutas que asignan el actor encontrado.
    2. En la ruta del botín, exigir `has_body(actor)`; si no lo tiene, tratarlo como "no encontrado" (la misma rama que ya existe).
  hecho cuando: `verify` cumplido.
  no hagas: no cambies la búsqueda por nombre ni las distancias; no toques `oa_loot.c`.
  modelo sugerido: medio

- [~] T218 [P] [red] Estado de `online.c` sin estáticas de función y con atómicos.
  reclamada: agente Opus ag-t218 (integrada, adelantada) — net 328/0, validate 181510/0, wire 16647/0; Docker debian: session 12 casos/272 comprobaciones/0; compila; HE-01 (b) diseñado en su bitácora; FALTA recorrido y firma
  files: `runtime/online.c`, `tools/test_online_net.c`, `tools/test_online_session.c`
  verify: `grep -nE "^\s+static " runtime/online.c` solo devuelve las que quedan justificadas en un comentario; `make test-online-net` con un caso nuevo "dos sesiones seguidas en el mismo proceso: la segunda no ve nada de la primera" en 0; en Docker, `test-online-session` con los casos pendientes de T204 (sesión llena, expulsión por silencio) en 0; `make -j8`; recorrido corto pasa
  depende de: T214
  quién: agente
  contexto: Varias funciones de `runtime/online.c` guardan estado en variables `static` locales que sobreviven entre sesiones y no tienen cerrojo: `motion_anim` (`:524`), `sample_local` (`:824-825`, `:835`), temporizadores y buffers de `cl_thread` (`:1232-1233`, `:1252`, `:1264`, `:1267`, `:1297-1298`), `next_time` en `srv_loop` (`:374`). Impiden además probar dos sesiones en un proceso. Aparte, `g.state`, `g.hosting`, `g.id` y `g_srv_running` se leen desde otros hilos sin cerrojo ni atómicos (`:1375-1377`): en x86-64 es benigno, pero es una carrera de datos según C11.
  pasos:
    1. Mover cada estática de función a la estructura que le corresponde (`g` para el cliente, `Server` para el servidor), con el mismo nombre; inicializarla donde se inicia la sesión (`start_client`, `srv_open`) y limpiarla en `stop_all`.
    2. `g.state`, `g.hosting`, `g.id`, `g_srv_running`: tipos `_Atomic` (o `atomic_int` / `atomic_bool` de `<stdatomic.h>`, como `runtime/items.c:332`).
    3. Añadir los casos de prueba.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies el envío bloqueante ni añadas colas (F3); no cambies el formato de los mensajes; no cambies los periodos (20 Hz, 1 Hz, 2 s).
  modelo sugerido: alto

- [ ] T219 [P] [bot] Bot y Docker listos para medir la red.
  files: `tools/online_bot.c`, `tools/docker/compose.yml`, `tools/docker/red-medidas.sh` (nuevo)
  verify: el bot compila con `cc -O2 -Wall -Iruntime ...` en Windows y en la imagen `swcoop-pruebas`; `bash tools/docker/red-medidas.sh` ejecuta cuatro escenarios (sin `netem`; 80 ms; 80 ms + 2 % de pérdida; 200 ms + 5 %) y escribe una tabla con mensajes recibidos por segundo en cada bot; un bot con `BOT_NOREAD=1` deja de leer sin cerrar y el guion anota qué le pasa al otro bot (se espera que deje de recibir: es el fallo H-1, que arreglará F3)
  depende de: T214
  quién: agente
  contexto: El bot recibe en un buffer de 8 192 bytes (`tools/online_bot.c:107`), pero un mensaje ENEMIES puede medir hasta 9 965: con más de 26 enemigos cerca del anfitrión el bot se desconecta (`specs/mapa/06` §5.2). `BOT_FREEZE` deja de enviar pero sigue leyendo, así que no reproduce el fallo más grave de la capa de red: un cliente que no lee bloquea el hilo único del servidor, porque los envíos son bloqueantes (`send_all`, `runtime/online.c:140-148`). F3 necesita esa reproducción y medidas con latencia y pérdida.
  pasos:
    1. Buffer de recepción a 32 768 (el mismo tamaño que `RecvBuf`, `runtime/online.c:161`).
    2. `BOT_NOREAD=<s>`: a los `s` segundos deja de llamar a `recv`, sin cerrar la conexión, y sigue enviando.
    3. El bot imprime cada 5 s cuántos mensajes STATE recibió.
    4. `red-medidas.sh`: lanza el `compose` con cada perfil de `tc netem` y recoge las cifras.
  hecho cuando: `verify` cumplido y la tabla está en la bitácora.
  no hagas: no arregles H-1 aquí; no cambies `runtime/online.c`; no publiques puertos fuera de la red de compose.
  modelo sugerido: medio

- [~] T220 [P] [i18n] Traducir los textos de Online que faltan.
  reclamada: adelantada e integrada en `coop/main` (`coop/I18N` `179fc76` + `caaa473`): `tools/i18n_check.py` = `0 missing` en los 13 idiomas, `make -j8` compila; solo falta comprobación visual en juego de F1 → Online en español y validar junto a `T221`. No volver a tomar esta tarea.
  files: `runtime/ui/i18n/de.inc`, `es.inc`, `fr.inc`, `he.inc`, `it.inc`, `ja.inc`, `ko.inc`, `nl.inc`, `pt.inc`, `ru.inc`, `th.inc`, `zh-Hans.inc`, `zh-Hant.inc`
  verify: `$PY tools/i18n_check.py; echo $?` imprime `0 missing` en los 13 idiomas y termina con 0; `make -j8` compila; en juego, F1 → Online en español muestra todos los textos traducidos
  depende de: T214
  quién: agente (único dueño de los `.inc` mientras dure la tarea)
  contexto: La clave de cada traducción es el texto en inglés; cada texto de interfaz necesita una línea `T("English", "...")` en los 13 ficheros `runtime/ui/i18n/<idioma>.inc`. El PR solo tradujo al español. Comprobado: `tools/i18n_check.py` sale con 1; faltan 51 textos en cada uno de 12 idiomas y 3 en `es` (casi todos de `runtime/ui/online_ui.mm`). `--missing <idioma>` imprime las líneas que faltan, listas para pegar.
  pasos:
    1. Por cada idioma: `$PY tools/i18n_check.py --missing <idioma>` y pegar las líneas en un bloque al final del `.inc`, entre comentarios `// --- Online (coop) ---`.
    2. Traducir. Conservar los `%s` y `%d` en el mismo orden. Nombres propios (`Link`, `Tailscale`, `ZeroTier`, `Radmin VPN`) se dejan con `""` (inglés).
    3. Ejecutar el check hasta `0 missing`.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies los textos en inglés del código (cambiaría la clave); no reordenes los `.inc`; no toques traducciones existentes; no dejes que otro carril edite los `.inc` a la vez.
  modelo sugerido: medio

## Cierre

- [ ] T221 [integración] Integrar los carriles, verificación final y cierre de F2.
  files: `specs/bitacora/<fecha>-F2-cierre.md`, `specs/fases/F2-cimientos/verificacion.md`
  verify: los once criterios S1-S11 de `spec.md` con evidencia; todo `verify` de T214 repetido sobre `coop/main` con los carriles integrados; `$PY tools/i18n_check.py` en 0; recorrido completo desatendido y con ventana; el dueño aprueba
  depende de: T215, T216, T217, T218, T219, T220
  quién: agente + dueño
  contexto: Salida de F2: base modular y probada sobre la que F3 (red robusta) y F4 (funciones cooperativas) trabajan en paralelo.
  pasos:
    1. Fusionar los carriles uno a uno; tras cada uno, compilar, pruebas automáticas y recorrido corto.
    2. Verificación completa.
    3. Bitácora: por tarea, nivel de verificación alcanzado (lectura / prueba / juego) y lo no verificado. Pendientes con fase destino.
  hecho cuando: el dueño aprueba.
  no hagas: no aplastes commits; no subas nada salvo que el dueño lo pida.
  modelo sugerido: medio

## Resumen

21 tareas (T201-T221). Fase 0: cadena NET (T202, T203, T204, T213) y cadena OA (T201, T206-T212), cada una en
serie y las dos en paralelo entre sí; T205 en paralelo. Carriles en paralelo tras T214: T215-T220. Solo las tareas
T210, T212, T213 y los carriles T215-T218 cambian comportamiento, y lo dicen.
