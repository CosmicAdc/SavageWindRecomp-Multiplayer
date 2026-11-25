# F1 — Probar el sistema y arreglos rápidos esenciales · tasks

Formato de tarea y significado de `[P]` y `modelo sugerido`: ver `../F0-preparacion/tasks.md`, "Cómo leer una
tarea". Cada tarea se puede ejecutar sin más contexto que este fichero.

## Reglas que valen para TODAS las tareas de F1

- `$SW` = carpeta de trabajo; clon en `$SW/coop`; shell MSYS2 UCRT64 desde la raíz del clon con
  `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`; `PY=tools/venv/Scripts/python.exe`.
- Trabaja en tu propio worktree: `git worktree add "$SW/wt/<tarea>" -b coop/<tarea> coop/main`. Un worktree nuevo
  no tiene `build/`, `generated/`, `tools/venv/`, `online_mod/` ni `game_dir.txt`: cópialos o créalos (F0 `plan.md`
  §10).
- Las líneas citadas son del commit `350c5f8`. **Localiza siempre por nombre de función** (`grep -n`), porque las
  líneas se mueven con cada arreglo anterior.
- No cambies `PROTOCOL` (`runtime/online.c:40`) ni ninguna estructura de `runtime/online.h` ni `WireState`.
- No toques `tools/patches.py`, `tools/game_sites.py` ni nada dentro de `generated/`.
- No reordenes llamadas dentro de `rt_online_frame` ni muevas funciones de sitio (eso es F2).
- No añadas variables de entorno `SAVAGEWIND_*` nuevas (desfasan `runtime/launch_flags_table.c`).
- No uses la carpeta de guardado real del dueño; siempre copias en `$SW/pruebas/`.
- Sin rutas absolutas del equipo ni logs crudos en el repositorio. Nada de `git push`.
- Solo caben dos juegos a la vez en este PC: pide turno antes de una prueba en juego.
- Un arreglo está hecho cuando se ha **visto** funcionar (prueba o juego), no cuando compila. Lo que no hayas
  podido verificar, dilo en la bitácora.

---

## A. Línea base (en serie)

- [ ] T101 [pruebas] Preparar carpetas de prueba y marcar el punto de partida.
  files: `$SW/pruebas/` (fuera del repositorio); etiqueta git local `f0-cerrada`
  verify: `ls "$SW/pruebas"` muestra `guardado-A guardado-B logs ajustes-A.ini ajustes-B.ini`; `git tag -l f0-cerrada` la lista
  depende de: F0 cerrada (T024)
  quién: agente + dueño (aporta la carpeta de guardado a copiar)
  contexto: `docs/ONLINE.md` ("Testing with two games on one PC") exige una carpeta de guardado por juego, "a copy of a save, never one in use". El modo online escribe flags compartidos en el guardado. La etiqueta sirve para comparar después qué cambió F1.
  pasos:
    1. `mkdir -p "$SW/pruebas"/{guardado-A,guardado-B,logs}`
    2. Copiar la carpeta de guardado que indique el dueño a `guardado-A` y a `guardado-B` (dos copias independientes).
    3. `: > "$SW/pruebas/ajustes-A.ini"; : > "$SW/pruebas/ajustes-B.ini"`
    4. `git tag f0-cerrada coop/main`
  hecho cuando: `verify` cumplido.
  no hagas: no enlaces (symlink) la carpeta real; no subas la etiqueta.
  modelo sugerido: bajo

- [ ] T102 [pruebas] Guion que arranca los dos juegos como dice `docs/ONLINE.md`.
  files: `tools/coop/dos_juegos.sh`
  verify: `bash tools/coop/dos_juegos.sh arrancar` y después `$PY tools/swctl.py --port 7777 --wait 180 state` y `--port 7778 --wait 180 state` responden; `bash tools/coop/dos_juegos.sh parar` deja `tasklist | grep -c savagewind` en 0
  depende de: T101
  quién: agente
  contexto: Las variables están en `docs/ONLINE.md` (`SAVAGEWIND_SAVE_DIR`, `SAVAGEWIND_HIDDEN=1`, `SAVAGEWIND_AUDIO_GAIN=0`, `SAVAGEWIND_ONLINE_PORT`, `SAVAGEWIND_ONLINE=host` / `=127.0.0.1`, `SAVAGEWIND_ONLINE_NAME`, `SAVAGEWIND_CONTROL=7777` / `7778`) y en `AGENTS.md` (`SAVAGEWIND_NO_LAUNCHER=1`, `SAVAGEWIND_SETTINGS`, `SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"`). El juego puede relanzarse una vez al arrancar: no vigiles por PID.
  pasos:
    1. Escribir el guion con tres órdenes: `arrancar` (lanza A y B en segundo plano con las variables de `plan.md` §1.3, cada uno con su log en `$SW/pruebas/logs/`), `parar` (`swctl.py exit` a los dos puertos), `ventana` (igual que `arrancar` sin `SAVAGEWIND_HIDDEN`).
    2. La raíz `$SW` se lee de la variable de entorno `SW`; si no existe, el guion termina con un mensaje.
    3. Puerto de sesión 52530 (no el 52520 real).
    4. Probar con `verify`.
  hecho cuando: los dos juegos responden a `state` y se cierran con `parar`.
  no hagas: no escribas rutas absolutas en el guion; no uses `timeout` de MSYS; no uses el puerto 52520.
  modelo sugerido: medio

- [ ] T103 [pruebas] CU-01: hospedar y unirse.
  files: `specs/bitacora/<fecha>-F1-base-CU01.md`
  verify: `grep -c "joined (slot" "$SW/pruebas/logs/A.log"` da 2; en A aparece `[online] hosting on port 52530`; `grep -c "Disconnected\|Lost the connection" "$SW/pruebas/logs/B.log"` da 0 tras 2 min
  depende de: T102
  quién: agente
  contexto: El anfitrión arranca un hilo servidor y se conecta a sí mismo por `127.0.0.1` (`online_host`, `runtime/online.c:1347-1361`); cada entrada escribe `[online] server: <nombre> joined (slot N)` (`:259`). Rechazos posibles: versión, contraseña, nombre, sesión llena (`:57`).
  pasos:
    1. `bash tools/coop/dos_juegos.sh arrancar`; esperar a que ambos respondan a `state`.
    2. Comprobar las líneas de `verify`.
    3. Repetir con contraseña: poner `password = prueba` en `online.ini` de la raíz antes de arrancar (el cliente lee el mismo fichero); después quitarla.
    4. Anotar tiempos de conexión y cualquier aviso.
  hecho cuando: unión correcta con y sin contraseña, anotada en bitácora.
  no hagas: no arregles nada; no dejes la contraseña de prueba en `online.ini`.
  modelo sugerido: bajo

- [ ] T104 [pruebas] CU-02: ver al compañero.
  files: `specs/bitacora/<fecha>-F1-base-CU02.md`
  verify: tras `goto tower 7` en ambos, `$PY tools/swctl.py --port 7778 actors OnlinePlayer` devuelve un actor a menos de 20 m; tras mover a A con `stick l 0 1 150`, la posición de ese actor en B cambia
  depende de: T103
  quién: agente
  contexto: El Link remoto es un actor `OnlinePlayer` creado cuando el otro jugador está en el mismo mapa y a menos de 400 m (`runtime/online_actors.c:6013-6018`); se coloca cada fotograma con la posición recibida (`:6054-6056`). La orden `actors <texto>` lista actores vivos (`docs/CONTROL.md`).
  pasos:
    1. En ambos: `set god_mode true`, `goto tower 7`.
    2. `actors OnlinePlayer` en cada puerto; anotar nombre, posición y distancia.
    3. Mover A; volver a consultar en B; comparar con `pos` de A.
    4. `actors OnlineArmor` y `actors OnlineGear` en B; anotar cuántos hay.
    5. **Enemigos atacan a los dos** (ADR-012): con A a más de 60 m, llevar a B a un campamento de enemigos de la
       Meseta. Anotar con `actors Enemy_` en los dos puertos qué enemigos hay, y en los logs las líneas de cambio de
       dueño de cada enemigo (`docs/ONLINE.md`, "Owners"). Comprobar: (a) los enemigos pasan a ser de B y se acercan a
       B (su distancia a `pos` de B baja); (b) con A junto a B y `god_mode` apagado en A unos segundos, A recibe al
       menos un golpe (su vida baja) de un enemigo que es de B; (c) si B se aleja más de 40 m y A queda 10 m más
       cerca, el enemigo pasa a A.
  hecho cuando: evidencia de presencia y de seguimiento anotada; diferencias de posición anotadas; el paso 5 anotado
    con (a), (b) y (c) como PASA/FALLA (un FALLA va a la lista priorizada de T108).
  no hagas: no interpretes "el actor existe" como "se ve bien": eso lo confirma el dueño en T107.
  modelo sugerido: bajo

- [ ] T105 [pruebas] Guion del recorrido de 30 minutos.
  files: `tools/coop/recorrido.py`
  verify: `$PY tools/coop/recorrido.py --ayuda` lista los pasos; `$PY tools/coop/recorrido.py --pasos 1-3` termina con código 0 con los dos juegos arrancados e imprime una línea `PASO n: PASA|FALLA` por paso
  depende de: T104
  quién: agente
  contexto: El recorrido está definido paso a paso en `plan.md` §1.5 (10 pasos, 30 min). `tools/swctl.py` es un cliente de una sola orden con salida `clave: valor` y `--json`; código de salida 0 bien, 1 error del juego, 2 fallo de conexión.
  pasos:
    1. Escribir el guion: una función por paso que lanza `tools/swctl.py --json --port <p> ...` con `subprocess` y decide PASA/FALLA.
    2. Opciones: `--pasos a-b`, `--puerto-a 7777`, `--puerto-b 7778`, `--ayuda`.
    3. El paso "matar B" y "relanzar B" llaman a `tools/coop/dos_juegos.sh`.
    4. Cada paso comprueba al final que **los dos** juegos responden a `state`; si uno no responde, FALLA y el guion se detiene sin cerrar el otro.
  hecho cuando: `verify` cumplido.
  no hagas: no dependas de paquetes fuera de la biblioteca estándar de Python; no leas memoria del juego con `mem`; no pongas rutas absolutas.
  modelo sugerido: medio

- [ ] T106 [pruebas] CU-03: pasada desatendida de 30 minutos y medida de RAM.
  files: `specs/bitacora/<fecha>-F1-base-CU03.md`
  verify: salida completa de `$PY tools/coop/recorrido.py` guardada; tabla de pasos de `verificacion.md` §2 rellena; RAM libre antes y uso por proceso anotados
  depende de: T105
  quién: agente
  contexto: Dos juegos usan unos 9,5 GB (`docs/ONLINE.md`) en una máquina de 15,7 GB. Si el sistema pagina o mata un proceso, parecerá un cuelgue del modo online sin serlo.
  pasos:
    1. Cerrar Docker Desktop y el navegador; anotar memoria libre.
    2. Arrancar y lanzar el recorrido completo.
    3. Cada 5 min anotar la memoria de cada `savagewind.exe` (`tasklist`).
    4. Si un juego cae: seguir `plan.md` §1.6 y repetir ese paso tres veces.
  hecho cuando: tabla rellena con PASA/FALLA por paso y cada fallo con su reproducibilidad.
  no hagas: no compiles durante la pasada; no arregles nada todavía.
  modelo sugerido: bajo

- [ ] T107 [pruebas] Sesión con ventana, vista por el dueño.
  files: `specs/bitacora/<fecha>-F1-base-ventana.md`
  verify: entrada con FPS, ajustes y la lista de `verificacion.md` §3 marcada por el dueño
  depende de: T106
  quién: dueño (juega) + agente (anota)
  contexto: Un guion no ve si la armadura, el arma o la animación del compañero son correctas. El menú Online está en F1 → Online, solo dentro del juego.
  pasos:
    1. `bash tools/coop/dos_juegos.sh ventana`
    2. El dueño juega 30 min alternando ventanas y marca la lista.
  hecho cuando: lista marcada y defectos descritos.
  no hagas: no uses el guardado real.
  modelo sugerido: bajo (tarea humana)

- [ ] T108 [bitácora] Cerrar la línea base: lista priorizada.
  files: `specs/bitacora/<fecha>-F1-linea-base.md`
  verify: el fichero lista cada fallo con gravedad (cuelgue / incorrecto / cosmético), repro, frecuencia y fase destino; el dueño lo aprueba antes de empezar la parte B
  depende de: T106, T107
  quién: agente (redacta) + dueño (aprueba)
  contexto: Criterio S1 de `spec.md`: la línea base se anota antes de tocar código, para poder comparar.
  pasos:
    1. Reunir T103-T107.
    2. Ordenar por gravedad y frecuencia; asignar fase (F1 si coincide con uno de los nueve arreglos; si no, F2 o posterior).
  hecho cuando: aprobado por el dueño.
  no hagas: no añadas arreglos nuevos a F1 sin que el dueño lo decida.
  modelo sugerido: medio

## B. Herramientas de reproducción (en paralelo entre sí)

- [ ] T109 [P] [bot] Modos hostiles del bot para reproducir fallos.
  files: `tools/online_bot.c`
  verify: `cc -O2 -Wall -o build/online_bot.exe tools/online_bot.c -lws2_32` compila sin avisos nuevos; con el juego A arrancado, `BOT_LOOT=Enemy_Bokoblin_Junior build/online_bot.exe 127.0.0.1 52530 Malo` produce en `A.log` una línea `loot` con ese nombre; sin variables `BOT_*` el bot se comporta como antes
  depende de: T108
  quién: agente
  contexto: El bot es un jugador sin juego: envía HELLO con versión 38 (`tools/online_bot.c:102`) y STATE a 20 Hz, con modos de prueba por `getenv("BOT_...")` (por ejemplo `BOT_FREEZE`, `:143`). Lleva su propia copia de `WireState` (`:31-76`). No sabe enviar mensajes mal formados, que es lo que hace falta para probar los arreglos 4, 5 y 8.
  pasos:
    1. Leer en `runtime/online.c:50` la lista de tipos (`M_LOOT_ADD`, `M_ESTATE`...) y en `runtime/online.h:216-240` las estructuras `OnlineEnemy` y `OnlineLoot`; copiarlas al bot como ya hace con `WireState`.
    2. `BOT_NAN=1`: tras 5 s, el STATE lleva `pos[0] = NAN`.
    3. `BOT_LOOT=<nombre>`: tras 5 s envía un `M_LOOT_ADD` con ese nombre y la posición del bot.
    4. `BOT_SHIELD=1`: tras 5 s envía un `M_ESTATE` con un enemigo cuyo `shield` son 40 bytes `'A'` sin terminador.
    5. Documentar las tres variables en la cabecera del fichero.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies `runtime/online.c` ni `runtime/online.h`; no cambies el comportamiento por defecto del bot; no uses un nombre de actor que cuelgue el juego para la prueba (`GameROMPlayer` es un cuelgue documentado en `docs/ONLINE.md`).
  modelo sugerido: medio

- [ ] T110 [P] [pruebas] Arnés mínimo de `runtime/online.c` sin el juego.
  files: `tools/test_online_net.c`, `Makefile` (un bloque nuevo **al final**)
  verify: `make test-online-net` compila y termina con 0; el arnés ejecuta al menos: un STATE válido llega a `online_players`; un mensaje con ranura 8 se ignora
  depende de: T108
  quién: agente
  contexto: No hay ninguna prueba automática del modo online. Las pruebas del proyecto incluyen el `.c` de producción (`#include "../runtime/control.c"` en `tools/test_control.c`; regla en `Makefile:740-749`). En `online.c` casi todo es `static`: `cl_message` (`:967`) aplica un mensaje recibido y `stop_all` (`:1322`) corta la sesión. Depende de `g_site`, `plat_now_ns`, `LOG` y pthreads.
  pasos:
    1. Crear `tools/test_online_net.c` que defina los dobles mínimos (un `g_site` a cero, `plat_now_ns` con un reloj falso, lo que pida el enlazador) y haga `#include "../runtime/online.c"`.
    2. Para simular "conectado": bajo `g.lock`, poner `g.state = ONLINE_CONNECTED`, `g.id = 0`.
    3. Escribir los dos casos de `verify` llamando a `cl_message` con bytes fabricados.
    4. Añadir al final del `Makefile` la regla `$(B)/test_online_net`, el objetivo `test-online-net` y su `.PHONY`, copiando la forma de `test-control`.
    5. Si `online.c` no se deja incluir tras dos horas de intento: parar, anotar qué símbolo lo impide y avisar; los arreglos 5, 6 y 8 se verificarán entonces con el bot.
  hecho cuando: `verify` cumplido, o el bloqueo documentado.
  no hagas: no cambies `runtime/online.c` para facilitar la prueba (ni quitar `static`); no abras sockets reales; no edites reglas existentes del `Makefile`.
  modelo sugerido: alto

## C. Carril OA — `runtime/online_actors.c` (en serie, un solo dueño)

- [ ] T111 [oa] Puerta de versión: nada de direcciones fijas si el juego no es 1.9.0.
  files: `runtime/online_gate.h` (nuevo), `runtime/online_actors.c`, `tools/test_online_gate.c` (nuevo), `Makefile` (bloque al final)
  verify: `make test-online-gate` termina con 0; `make -j8` compila; con el juego 1.9.0, el log muestra una vez `[online] game version 1.9.0: actors on` y los pasos 1-3 del recorrido pasan
  depende de: T108
  quién: agente
  contexto: `online_actors_on` (`runtime/online_actors.c:225-229`) solo comprueba que existe el overlay. A partir de ahí `rt_online_frame` (`:5721`) llama a 62 direcciones fijas de 1.9.0 (`FN_*` y `P_*`, definidas desde `:30`). Con otra versión del juego saltaría a código equivocado. `g_game_version` (`runtime/game_sites.h:113`) ya se compara con `"1.9.0"` en otros módulos (`runtime/quick_drop_action.c:70`). Además `rt_online_frame` ejecuta `find_actors`, `flag_query` y `event_query` (`:5723-5725`) **antes** de la puerta.
  pasos:
    1. `runtime/online_gate.h`: `static inline bool online_version_ok(const char *v) { return v && !strcmp(v, "1.9.0"); }`.
    2. `tools/test_online_gate.c`: `assert` de `"1.9.0"` → verdadero; `"1.8.2"`, `""`, `NULL` → falso. Regla y objetivo `test-online-gate` al final del `Makefile`.
    3. En `online_actors_on`: devolver falso si `!online_version_ok(g_game_version)`; escribir en el log una sola vez qué versión hay y si los actores están activos.
    4. En `rt_online_frame`: leer `find_actors`, `flag_query` y `event_query`. La que use `call(` con una constante `FN_*` o lea un `P_*` debe quedar protegida por la puerta **respondiendo vacío** (deja la petición como contestada, sin resultado), para que el hilo de control no se quede esperando.
    5. En los ganchos `rt_online_bomb`, `rt_online_arrow`, `rt_online_xlink_emit`, `rt_blur_anim`, `rt_blur_frame`, `rt_menu_draw_pause`, `rt_online_has_fairy`, `rt_online_menu_live`: si la versión no es válida, volver con el valor que deja el juego como estaba (lee cada función para saber cuál es: por ejemplo `rt_online_has_fairy` devuelve su argumento `had`, `rt_menu_draw_pause` devuelve 1).
  hecho cuando: `verify` cumplido y el comportamiento con 1.9.0 es idéntico.
  no hagas: no conviertas direcciones fijas en sitios por patrón (es otro trabajo); no toques `tools/game_sites.py`; no pongas la comprobación dentro de bucles por actor (una vez por fotograma basta).
  modelo sugerido: alto

- [ ] T112 [oa] `has_body()` antes de `FN_BODY_TO` en los mecanismos.
  files: `runtime/online_actors.c`
  verify: `grep -n "FN_BODY_TO" runtime/online_actors.c` — en `movers_update` las dos llamadas quedan dentro de un `if (has_body(a))`; `make -j8` compila; prueba en juego: un mecanismo de santuario movido en A se mueve en B (anotar cuál)
  depende de: T111
  quién: agente
  contexto: `has_body(a)` (`runtime/online_actors.c:73-77`) dice si `actor+0x190` es de verdad un cuerpo físico; existe porque en otras clases de actor ese campo es otra cosa y eso ya causó un cuelgue (comentario en `:67-72`). `movers_update` (`:4829`) mueve mecanismos elegidos por HashId y por nombre, con una matriz que llega de la red, y solo comprueba `VALID(body)` antes de `FN_BODY_TO` (`:4901-4902` en el gancho de prueba y `:4928-4929` en el bucle de los demás jugadores).
  pasos:
    1. `grep -n "static void movers_update" runtime/online_actors.c` y localizar las dos apariciones de `uint64_t body = *(uint64_t *)(uintptr_t)(a + 0x190);` seguidas de `if (VALID(body)) call(c, FN_BODY_TO, ...)`.
    2. En ambas, cambiar la condición a `if (has_body(a))` (el cuerpo se sigue leyendo de `a + 0x190`).
    3. Dejar la llamada a `FN_SET_MTX` como está.
    4. Compilar y probar.
  hecho cuando: `verify` cumplido. Si no se pudo probar un mecanismo real, decirlo: "verificado por lectura".
  no hagas: no añadas `has_body` a `objs_update` ni a `grab_update` en esta tarea (F2); no cambies qué mecanismos se eligen.
  modelo sugerido: medio

- [ ] T113 [oa] Runas: usar el Link remoto solo si sigue vivo.
  files: `runtime/online_actors.c`
  verify: `make -j8` compila; en juego, B usa Magnesis y Stasis y A lo ve como antes; A muere (`god_mode` apagado) mientras B sostiene un objeto con Magnesis, tres veces, sin cuelgue; revisión: ninguna ruta de `grab_update`, `stop_update`, `rune_pieces`, `rune_keep` usa `g_act[i].link` sin pasar por el ayudante
  depende de: T112
  quién: agente
  contexto: En `rt_online_frame`, `grab_update` y `stop_update` se llaman en `:5976-5977`, pero la comprobación `alive(g_act[i].link, g_act[i].link_w)` está más abajo, en `:6036`. Antes de ella, `rune_keep` (`:4304`) y `rune_pieces` (`:4312`) leen `g_act[i].link + 0x398` y llaman a `bone_mtx(c, g_act[i].link, "Weapon_R", ...)`. Si el juego borró ese actor desde el fotograma anterior (game over, carga), se llama al juego con un actor liberado. `alive` está en `:91`.
  pasos:
    1. Añadir cerca de `g_act` un ayudante: `static uint64_t link_live(int i) { return g_act[i].link && alive(g_act[i].link, g_act[i].link_w) ? g_act[i].link : 0; }` (colócalo después de la definición de `alive` y de `g_act`).
    2. En `rune_keep`, `rune_pieces`, `grab_update`, `stop_update` (y lo que ellas llamen: `chains_place`, `stasis_look` si usan el Link), sustituir cada uso de `g_act[i].link` como actor por una variable local `uint64_t lk = link_live(i);` y saltar el trabajo si es 0.
    3. No tocar el bloque de `:6036` (sigue siendo quien detecta el borrado y llama a `forget_all`).
  hecho cuando: `verify` cumplido. La ventana del fallo es de un fotograma: si no se reprodujo "antes", escribirlo.
  no hagas: no muevas las llamadas de sitio dentro de `rt_online_frame`; no llames a `forget_all` desde el ayudante; no cambies `alive`.
  modelo sugerido: alto

- [ ] T114 [oa] Lista de permitidos para nombres de actor que llegan por red.
  files: `runtime/online_actors.c`
  verify: con el juego A arrancado, `BOT_LOOT=Enemy_Bokoblin_Junior build/online_bot.exe 127.0.0.1 52530 Malo` ya **no** produce `loot N (Enemy_Bokoblin_Junior) made here` y sí una línea `[online] loot name refused`; soltar un arma en B sigue apareciendo en A; `make -j8` compila
  depende de: T113, T109
  quién: agente
  contexto: El botón "crear actor por nombre" recibe nombres de la red sin filtrar: botín en `request(c, list[j].name, pc->handle)` (`runtime/online_actors.c:3604`), con la lista que el anfitrión acepta en `loot_host_add` (`:3360`); enemigos en `request_p(c, e->name, ...)` (`:2226`), con un registro que el anfitrión sobrescribe con lo que envía un cliente (`g_reg[r].e = st[k]`, `:2398`). Ya existen los filtros que hacen falta: `is_loot(nombre)` (`:3246`) y la condición de `is_enemy` (`:2029-2032`: empieza por `Enemy_`, no es jefe, y `enemy_actor(nombre)` lo conoce).
  pasos:
    1. Botín: en `loot_host_add`, volver sin añadir si `!is_loot(l->name)` (registrar el rechazo una vez). Antes del `request` de `:3604`, si `!is_loot(list[j].name)`, marcar la entrada como perdida (`g_theirs[t].gone = true`) y continuar.
    2. Enemigos: añadir `static bool enemy_name_ok(const char *n)` con la misma condición que `is_enemy` pero por nombre; antes del `request_p` de `:2226`, si no la cumple, no pedir el actor.
    3. En la copia `g_reg[r].e = st[k]` (`:2398`): conservar el `name` que ya tenía el registro, igual que ya conserva `owner` y `hash`.
    4. Armas del enemigo (`e->weapon`, `e->shield`, usadas como parámetros en `:2223-2224`): pasarlas solo si empiezan por `Weapon_`.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies `is_loot` ni `is_enemy` (los usa más código); no filtres en `runtime/online.c` (la capa de red no conoce los nombres); no descartes mensajes enteros por un nombre malo, solo esa entrada.
  modelo sugerido: alto

## D. Carril NET — `runtime/online.c` (en serie, un solo dueño)

- [ ] T115 [net] Rechazar números no finitos o fuera de rango al recibir.
  files: `runtime/online.c`, `tools/test_online_net.c`
  verify: `make test-online-net` termina con 0 con casos nuevos (STATE con posición `NAN` → el jugador no cambia; TIME 99 → `online_host_hour` no lo entrega; HIT con daño negativo → no llega a `online_take_hits`); con el bot `BOT_NAN=1`, el juego A sigue y registra el descarte una vez
  depende de: T110, T109
  quién: agente
  contexto: El emisor honesto filtra su propia posición (`sample_local`, `runtime/online.c:832-834`: no finito o mayor que 100 000 → no envía), pero el receptor no comprueba nada: `remote_snap` (`:907`) copia posición y rumbo, que acaban en `FN_SET_MTX` del Link remoto (`runtime/online_actors.c:6054-6056`); `M_TIME` (`:1108`) acepta cualquier float; `M_HIT` (`:1100`) acepta daño negativo, que **sube** la vida del enemigo (`runtime/online_actors.c:2308`).
  pasos:
    1. Añadir `static bool floats_ok(const float *f, int n, float lim)` (todos finitos y `fabsf <= lim`).
    2. STATE: en el `case M_STATE` de `cl_message`, antes de `remote_snap`, descartar el mensaje si posición (límite 100 000) o rumbo no pasan.
    3. TIME: aceptar solo `0 <= hora <= 24`.
    4. HIT: descartar si `damage < 0`.
    5. ENEMIES y ESTATE: descartar la **entrada** cuyas matrices `m`, `wm` o `sm` no pasen (límite 100 000). LOOT_ADD y LOOTS: `pos` y `rot`. PICK: `pos`.
    6. Un `LOG` la primera vez que se descarta algo de cada tipo.
    7. Añadir los casos al arnés.
  hecho cuando: `verify` cumplido. Anotar en bitácora que las matrices internas de `WireState` (equipo, disparos, runas, objetos) quedan para F2.
  no hagas: no cambies tamaños ni campos de los mensajes; no cortes la conexión por un valor malo (solo descartar); no filtres en el lado emisor.
  modelo sugerido: medio

- [ ] T116 [net] Limpiar todo el estado de sesión al cortar.
  files: `runtime/online.c`, `tools/test_online_net.c`
  verify: `make test-online-net` termina con 0 con un caso nuevo: entregar `M_QUESTS`, `M_LOOTS`, `M_ENEMIES` y un `M_PICK`; llamar a `stop_all`; después `online_quests` devuelve falso, `online_loots` 0, `online_enemies` 0 y `online_take_picks` 0
  depende de: T115
  quién: agente
  contexto: `stop_all` (`runtime/online.c:1322-1335`) solo borra `g.r`, `g.id`, el chat, `g.hosting` y `g.ping`. Sobreviven a la sesión: `g.quests` y `g.quests_have` (`online_quests`, `:682-688`, los entrega como válidos en la sesión siguiente), `g.loots` / `g.nloots`, `g.en[]`, `g.quest_of[]` (`:475`) y las colas de entrada (`nhit_in`, `nest_in`, `nla_in`, `nlt_in`, `npick_in`, `:465-478`). Un jugador que cambia de sesión puede aplicar en la nueva los flags de la anterior, que se escriben en el guardado.
  pasos:
    1. Leer la definición de la estructura `g` (desde `:443`) y clasificar cada campo: de sesión (se borra) o de proceso (`lock`, `thread`, cámara, ajustes: se queda).
    2. En `stop_all`, dentro del `pthread_mutex_lock(&g.lock)` existente, poner a cero todos los campos de sesión, incluidas las colas de salida y las banderas `*_new`.
    3. En el `case M_JOIN` de `cl_message` (`:984`), además de limpiar `g.r[slot]`, limpiar `g.quest_of[slot]`.
    4. Añadir el caso al arnés.
  hecho cuando: `verify` cumplido.
  no hagas: no hagas `memset` de toda `g` (destruirías el mutex y el hilo); no toques las variables `static` dentro de funciones (F2); no cambies el orden de los `pthread_join`.
  modelo sugerido: alto

- [ ] T117 [net] `SIGPIPE`: que un envío a un par cerrado no mate el proceso.
  files: `runtime/online.c`
  verify: `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` termina con 0; con `tools/docker/compose.yml` (F0 T023), matar un bot con `docker kill` diez veces seguidas: el contenedor `servidor` sigue en ejecución (`docker compose ps`); `make -j8` en Windows compila
  depende de: T116
  quién: agente
  contexto: `send_all` (`runtime/online.c:140-148`) llama a `send(s, c, n, 0)` (`:143`). En Linux y macOS, escribir en un socket cuyo par cerró genera `SIGPIPE`, que por defecto termina el proceso; el runtime no lo ignora. El patrón correcto ya existe en `runtime/control.c:652-669`: `SO_NOSIGPIPE` en el socket donde exista y `MSG_NOSIGNAL` en `send` donde exista. Windows no tiene `SIGPIPE`.
  pasos:
    1. En `send_all`: `int flags = 0;` y, bajo `#ifdef MSG_NOSIGNAL`, `flags = MSG_NOSIGNAL;`; pasar `flags` a `send`.
    2. Añadir `static void sock_nosigpipe(sock_t s)` que, bajo `#ifdef SO_NOSIGPIPE`, haga el `setsockopt`; llamarla donde ya se llama a `sock_nodelay` (sockets aceptados y socket del cliente).
    3. Compilar en Windows y en Linux (Docker).
    4. Probar el escenario de `verify`. Si "antes" no se reprodujo en diez intentos, anotarlo.
  hecho cuando: `verify` cumplido.
  no hagas: no instales un manejador global de señales (`signal(SIGPIPE, ...)`); no cambies los sockets a no bloqueantes (F3).
  modelo sugerido: medio

- [ ] T118 [net] Terminar el campo `shield` en ESTATE.
  files: `runtime/online.c`, `tools/test_online_net.c`
  verify: `make test-online-net` termina con 0 con un caso nuevo: un `M_ESTATE` con `shield` de 40 bytes sin NUL; tras `online_take_estates`, `strlen(e.shield) <= 39`
  depende de: T117
  quién: agente
  contexto: Al recibir ENEMIES se terminan cuatro cadenas, `shield` incluida (`runtime/online.c:1052`). Al recibir ESTATE se terminan `name`, `as` y `weapon`, pero no `shield` (`:1095`). El anfitrión copia ese registro a su tabla (`runtime/online_actors.c:2398`) y usa `shield` como cadena: lectura más allá del campo.
  pasos:
    1. En el `case M_ESTATE` de `cl_message`, añadir `g.est_in[g.nest_in].shield[39] = 0` a la línea que termina las otras tres.
    2. Añadir el caso al arnés.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies el tamaño del campo; no toques `OnlineEnemy` en `runtime/online.h`.
  modelo sugerido: bajo

## E. Carril UI (en paralelo con C y D)

- [ ] T119 [P] [ui] Contraseña oculta en pantalla.
  files: `runtime/ui/option_rows.h`, `runtime/ui/option_rows.mm`, `runtime/ui/online_ui.mm`
  verify: `make -j8` compila; `$PY tools/i18n_check.py` no lista textos **nuevos** como faltantes respecto a la línea base; captura de F1 → Online con una contraseña puesta: se ven asteriscos en la fila y al editar; unirse a una sesión con contraseña sigue funcionando (T103 paso 3)
  depende de: T108
  quién: agente
  contexto: La contraseña se edita con `opt_text("Password", g_online.password, ...)` (`runtime/ui/online_ui.mm:64`). `opt_text` (`runtime/ui/option_rows.mm:302-318`) muestra el valor tal cual en la fila cuando no se edita (`:308`) y lo edita con `ui_input_text(..., ImGuiInputTextFlags_EnterReturnsTrue, ...)` sin máscara (`:313`). Quien retransmite su pantalla enseña la contraseña.
  pasos:
    1. En `option_rows.mm`, convertir el cuerpo de `opt_text` en una función interna con un parámetro `bool secret`; `opt_text` la llama con `false`.
    2. Con `secret`: en la fila, mostrar tantos `*` como caracteres (o la pista si está vacía); al editar, añadir `ImGuiInputTextFlags_Password` a los flags.
    3. Declarar `bool opt_text_secret(const char *label, char *buf, size_t cap, const char *hint, const char *desc);` en `option_rows.h` junto a `opt_text` (`:45`).
    4. En `online_ui.mm:64` usar `opt_text_secret`.
    5. Probar con teclado y con mando; si el teclado en pantalla (`runtime/ui/osk.mm`) muestra el texto, anotarlo como pendiente.
  hecho cuando: `verify` cumplido.
  no hagas: no añadas textos nuevos de interfaz (exigirían 13 traducciones); no cambies cómo se guarda o se envía la contraseña (F3); no cambies el comportamiento de `opt_text` para sus otros usos.
  modelo sugerido: medio

## F. Cierre (en serie)

- [ ] T120 [pruebas] Integrar los carriles y repetir la pasada completa.
  files: `specs/bitacora/<fecha>-F1-final.md`
  verify: en `coop/main` con T109-T119 integrados: `make -j8`, `make test-online-gate test-online-net`, las pruebas de la línea base de F0 y `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` terminan como en F0 o mejor; `$PY tools/coop/recorrido.py` completo: todos los pasos PASA, dos veces (desatendida y con ventana)
  depende de: T109, T114, T118, T119
  quién: agente + dueño (pasada con ventana)
  contexto: Criterios S3 a S6 de `spec.md`. Orden de integración: BOT, NET, OA, UI; un commit por arreglo para poder revertir uno solo.
  pasos:
    1. Fusionar cada carril en `coop/main` en ese orden; resolver el `Makefile` (dos bloques al final).
    2. Compilar y pasar pruebas tras cada carril; recorrido corto (pasos 1-3).
    3. Pasada completa desatendida; después, con ventana.
    4. Comparar con la línea base de T108.
  hecho cuando: `verify` cumplido o cada FALLA explicado y asignado.
  no hagas: no aplastes los commits de los arreglos en uno; no subas nada.
  modelo sugerido: medio

- [ ] T121 [bitácora] Cierre de F1.
  files: `specs/bitacora/<fecha>-F1-cierre.md`, `specs/fases/F1-prueba-y-arreglos/verificacion.md`
  verify: los ocho criterios S1-S8 de `spec.md` con evidencia; lista priorizada de pendientes actualizada; `grep -n "define PROTOCOL" runtime/online.c` da 38; `git diff f0-cerrada -- runtime/online.h` vacío; el dueño aprueba
  depende de: T120
  quién: agente (redacta) + dueño (aprueba)
  contexto: Salida de F1 según el plan aprobado: sesión de 30 min sin cuelgue en el recorrido y lista priorizada de lo que quede.
  pasos:
    1. Rellenar `verificacion.md`.
    2. Para cada arreglo: "antes", "después" y qué no se pudo verificar.
    3. Pendientes con fase destino (incluidos: traducciones de Online, matrices internas de `WireState`, teclado en pantalla y contraseña).
  hecho cuando: el dueño aprueba.
  no hagas: no declares un arreglo como verificado en juego si solo se verificó por lectura.
  modelo sugerido: bajo

## Resumen

21 tareas (T101-T121). En serie: T101-T108 (línea base), y dentro de cada carril. En paralelo: T109, T110 y T119
entre sí y con los carriles; los carriles OA (T111-T114) y NET (T115-T118) entre sí. La prueba en juego es un
recurso único: por turnos.
