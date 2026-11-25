# F4 — Funciones cooperativas: tareas

Cada tarea está escrita para ejecutarse **sola**, por un modelo que no ha leído nada más. Si una tarea te obliga a
adivinar, para y pregunta: no inventes.

## Convención

```
- [ ] T4xx [P] [área] descripción            ([P] = se puede hacer en paralelo; sin [P] = en serie)
  files:           ficheros que la tarea puede crear o modificar (y solo esos)
  verify:          orden que demuestra que funciona (sale con 0)
  depende de:      tareas o fases que deben estar fusionadas antes
  quién:           dueño (la persona) / agente
  contexto:        qué hace el código hoy, con fichero:línea, y por qué hace falta el cambio
  pasos:           lista numerada y concreta
  hecho cuando:    resultado observable
  no hagas:        trampas de esta tarea
  modelo sugerido: alto / medio / bajo
```

`alto` = exige razonar sobre concurrencia, memoria del juego, seguridad o ingeniería inversa.

## Reglas que valen para todas las tareas (léelas una vez)

- Repositorio: el fork, rama creada desde `coop/main`. Rutas siempre **relativas a la raíz del repo**. Nunca rutas
  absolutas ni nombres de usuario en código, pruebas o documentos.
- Las líneas `fichero:línea` son del PR #294 original (HEAD `350c5f8`). `runtime/online_actors.c` se partió en F2 en
  módulos `runtime/oa_*.c`: busca cada función por su nombre con `grep -rn "<nombre>(" runtime/`.
- No edites `generated/`, `extracted/`, `online_mod/` ni `third_party/`. No subas ficheros del juego ni guardados.
- No toques `tools/patches.py` ni `tools/game_sites.py` salvo que la tarea lo diga (cambian `GAMELIB_ID`: todos
  recompilan el juego).
- No cambies estructuras de cable, números de mensaje ni `PROTOCOL` salvo en T409 (dueño del protocolo).
- **Nunca inventes direcciones, desplazamientos de estructura ni firmas de funciones del juego.** Si no están ya en
  el código o en la respuesta escrita de una tarea de investigación, para.
- No copies nada de proyectos de descompilación. No pegues desensamblado ni pseudocódigo en el repo.
- Al juego solo se le llama desde el hilo del juego (`rt_online_frame`, `rt_item_tick`). El hilo de red y el de
  presentación solo intercambian datos por colas con mutex.
- No bloquees el hilo del juego: nada de sockets, ficheros grandes ni esperas ahí.
- Todo lo que llega por red es hostil: longitud, rango, terminador, `isfinite`, y nombres de actor/objeto contra una
  lista permitida.
- Estado que dependa de un arranque del juego: límpialo con `RT_ON_RESET` (`runtime/hle.h`); no guardes punteros del
  juego entre reinicios.
- Texto visible: en inglés en un `.mm` de `runtime/ui/`, y añadido a
  `specs/fases/F4-funciones-coop/textos-nuevos.txt`. No edites `runtime/ui/i18n/*.inc` si no eres el carril i18n.
- Pruebas con guardados: **siempre copias**. Nunca la carpeta de guardado en uso.
- Órdenes en Windows: shell MSYS2 UCRT64; Python del proyecto `tools/venv/Scripts/python.exe`.
- Comprobación mínima de cualquier cambio en `runtime/`: `make runtime-objs` sin avisos nuevos.
- Hecho = `verify` pasa **y** hay entrada en `specs/bitacora/`.

---

## Bloque 0 — Decisiones e investigación

- [ ] T401 [decisión] ADR: política de flags y de recompensas
  files: specs/decisiones/ADR-recompensas-y-flags.md
  verify: el fichero existe, contiene la tabla de política rellena, respuestas a P4-1, P4-2 y P4-8 de `spec.md` y la línea `Decidido por: <nombre>, <fecha>`
  depende de: ninguna
  quién: dueño (un agente redacta el borrador)
  contexto: Hoy, al recibir ciertos flags de otro jugador, el juego regala objetos a todos: piedra Sheikah, runas, paravela, semilla Kolog, orbe de santuario, habilidad de campeón y contenedor de corazón (`runtime/online_actors.c:3987-3997`). El dueño decidió "cada jugador conserva su inventario y corazones", pero también "recompensas de santuario y cofre para los dos". Hay que fijar qué se da, a quién y cuándo.
  pasos:
    1. Copia la tabla propuesta de `plan.md` §D (clases: capacidad del mundo, recompensa de hito, coleccionable, contenido de cofre; columnas: durante la sesión / al unirse a un mundo avanzado).
    2. Para cada casilla escribe: dar o no dar, y una frase de por qué.
    3. Añade las decisiones: ¿puede el invitado hacer avanzar el mundo estando lejos del anfitrión?; frecuencia de la copia de seguridad del anfitrión.
    4. El dueño revisa, cambia lo que quiera y firma.
  hecho cuando: el ADR está firmado; T415 puede escribir la tabla en código sin preguntar.
  no hagas: no decidas por el dueño; no dejes casillas con "depende".
  modelo sugerido: medio

- [ ] T402 [P] [investigación] Spike: cambiar al perfil de guardado cooperativo sin cerrar el programa
  files: specs/bitacora/F4-T402-cambio-de-perfil.md, specs/decisiones/ADR-perfil-coop.md
  verify: la bitácora responde a las 4 preguntas de `pasos` con la orden usada y lo que se vio; el ADR elige camino (a) o (b)
  depende de: F1
  quién: agente + dueño (prueba en el juego)
  contexto: La carpeta de guardado se decide una vez por arranque del juego (`sp_session_begin`, `runtime/main.c:589`; se usa en `runtime/services/fs.c:2096`). La pestaña Online solo aparece dentro del juego (`runtime/ui/menu.mm:2245`, `:2357`), así que hoy uno se une con la partida ya cargada. Queremos que el invitado juegue en un perfil aparte por cada mundo de anfitrión.
  pasos:
    1. Pregunta 1: ¿el reinicio en caliente ("Quit to start screen"; `teardown` en `runtime/main.c:148-168`) vuelve a llamar a `sp_session_begin` y por tanto admite cambiar de perfil con `sp_set_active` (`runtime/services/save_profiles.c:389`) justo antes? Compruébalo leyendo `main.c` y probando con `SAVAGEWIND_REBOOT_AT`.
    2. Pregunta 2: ¿la conexión de red sobrevive a ese reinicio? (El mapa 06 dice que los hilos de red sobreviven; confírmalo con dos juegos.)
    3. Pregunta 3: ¿se puede abrir la pestaña Online desde la pantalla de inicio (la pestaña 13 se filtra a "solo en juego")? ¿Qué habría que cambiar?
    4. Pregunta 4: ¿qué ocurre con el cerrojo de la carpeta (`sc_lock_take`, `runtime/main.c:590`) al cambiar?
    5. Forma de la respuesta: tabla (pregunta, sí/no, evidencia) y un ADR con el camino elegido: (a) reinicio en el proceso al unirse, o (b) unirse desde la pantalla de inicio.
  hecho cuando: T414 puede implementarse sin suposiciones.
  no hagas: no pruebes con tu guardado real; no cambies código de producción en esta tarea (solo código de prueba que no se fusiona).
  modelo sugerido: alto

- [ ] T403 [P] [investigación] Spike: fijar y borrar flags compartidos al cargar el guardado
  files: specs/bitacora/F4-T403-flags-al-cargar.md
  verify: la bitácora contiene el resultado de fijar 3 flags y borrar 3 flags de prueba al cargar, leídos después con `swctl flag <nombre>`
  depende de: F1
  quién: agente + dueño
  contexto: El runtime ya reescribe valores del `game_data.sav` en memoria cuando el juego lo lee, buscando por CRC32 del nombre (`runtime/services/test_warp.c:57-79`, enganchado en `runtime/services/fs.c:2058-2068`); hoy solo con variables de prueba como `SAVAGEWIND_TEST_FLAGS`. Si sirve para la lista de flags compartidos, el invitado podría cargar un mundo idéntico al del anfitrión.
  pasos:
    1. Lee `test_warp.c` entero y anota qué tipos de flag sabe cambiar y cómo se le indican.
    2. Pregunta 1: ¿puede poner a 0 un flag booleano que el guardado tiene a 1?
    3. Pregunta 2: ¿cuánto tarda en aplicar 25 000 cambios (medir)?
    4. Pregunta 3: tras cargar con un flag de cofre borrado, ¿el cofre aparece cerrado en el mundo?
    5. Usa `SAVAGEWIND_TEST_FLAGS` con flags de `online_mod/quests.txt` sobre una copia de guardado.
    6. Forma de la respuesta: tabla (pregunta, respuesta, evidencia) y recomendación sí/no para el requisito R4-07.
  hecho cuando: hay una recomendación clara con evidencia del juego.
  no hagas: no escribas en el fichero de guardado en disco; no uses flags de historia principal para la prueba (usa cofres o ítems de mapa).
  modelo sugerido: medio

- [ ] T404 [investigación] Ghidra: cómo quita el juego N unidades de un objeto de la bolsa
  files: specs/bitacora/F4-T404-quitar-de-la-bolsa.md
  verify: la bitácora contiene la respuesta en la forma pedida y una prueba en el juego (quitar 1 unidad de un material y verlo en el menú)
  depende de: F0 (proyecto Ghidra del dueño)
  quién: dueño (con `tools/ghidra/gq.py`; un agente puede preparar las consultas)
  contexto: Para enviar un objeto al compañero hay que quitarlo del inventario del emisor. El runtime solo sabe tirar arma, arco o escudo desde el selector rápido (`pouch_discard` + `pouch_cleanup`, `tools/game_sites.py:413` y `:424`; uso en `runtime/quick_drop_action.c`). Para materiales y comida no hay nada.
  pasos:
    1. Pregunta exacta: ¿qué función del juego resta una cantidad de un objeto apilable de la bolsa (la que usan vender, cocinar o "soltar" desde el menú), y qué recibe en cada argumento?
    2. Pregunta secundaria: ¿qué valor de tipo tienen en la entrada de bolsa los materiales, la comida, la armadura y los objetos clave? (El código solo conoce 0 espada, 1 arco, 2 flechas, 3 escudo.)
    3. Camino sugerido: partir del sitio conocido que resta flechas (`pouch_remove_arrows`) y de `pouch_discard`, y mirar sus vecinos y llamadores con `gq.py xrefs` y `gq.py decompile`.
    4. Genera el patrón con `tools/game_sites.py pattern <main> <dirección> code`.
    5. Forma de la respuesta: nombre propuesto del sitio, clase (`code`/`call`), línea `Site(...)` generada, descripción en palabras de cada argumento y del valor devuelto, precondiciones (hilo del juego, cerrojo de la bolsa `pouch_lock`), y resultado de la prueba.
  hecho cuando: T422 puede escribirse solo con lo que dice la bitácora.
  no hagas: no pegues desensamblado ni pseudocódigo en el repo; no tomes nombres ni estructuras de proyectos de descompilación; no entregues una dirección sin patrón.
  modelo sugerido: alto

- [ ] T405 [investigación] Ghidra: durabilidad y modificador al dar un arma
  files: specs/bitacora/F4-T405-armas-durabilidad.md
  verify: la bitácora contiene la respuesta y una prueba en el juego (dar un arma con la durabilidad a la mitad y verla así en el menú)
  depende de: T404
  quién: dueño
  contexto: `rt_item_tick` llama a la función de recogida del juego con nombre, valor (cantidad, o durabilidad × 100) y un cuarto argumento a 0 "sin modificador" (`runtime/items.c:420-460`; mapa 03 §1.3). De un arma soltada solo viaja el nombre: ni durabilidad ni modificadores (mapa 07 §7).
  pasos:
    1. Pregunta 1: ¿qué estructura espera el cuarto argumento de la función de recogida y qué campos tiene (tipo y valor del modificador)?
    2. Pregunta 2: ¿de qué campos de la entrada de bolsa se leen la durabilidad y el modificador de un arma? (Hay un sitio relacionado: `replace_weapon_modifier`.)
    3. Forma de la respuesta: descripción en palabras de los campos, cómo leerlos del emisor y cómo pasarlos al receptor, y resultado de la prueba.
  hecho cuando: se puede ampliar el mensaje GIVE con durabilidad y modificador sin suposiciones.
  no hagas: no inventes el formato; si no se aclara, escribe "no resuelto" y las armas siguen yendo solo por el suelo.
  modelo sugerido: alto

- [ ] T406 [investigación] Ghidra: mantener a Link caído sin fin de partida
  files: specs/bitacora/F4-T406-estado-caido.md
  verify: la bitácora contiene la respuesta y la prueba en el juego (Link caído 10 s; después revive o sigue a fin de partida; sin crash en 5 repeticiones)
  depende de: F0
  quién: dueño
  contexto: Hoy la reanimación es instantánea: el parche `has_fairy` (sitio en `tools/game_sites.py`, grupo opcional; función `rt_online_has_fairy`, `runtime/online_actors.c:2825-2831`) hace que la pregunta del juego "¿tiene un hada?" responda sí en el momento de morir (DOC:775-781). Queremos un estado "caído" que espere al compañero.
  pasos:
    1. Pregunta exacta: entre "la vida llega a 0" y "pantalla de fin de partida", ¿dónde decide el juego morir o revivir, y se puede retrasar esa decisión N segundos manteniendo a Link en el suelo?
    2. Preguntas secundarias: ¿qué hacen la cámara, la música y los enemigos mientras tanto? ¿La pregunta del hada se hace una sola vez o se puede volver a hacer?
    3. Camino sugerido: partir del sitio `has_fairy` y de sus llamadores.
    4. Forma de la respuesta: sitio(s) propuestos con su línea `Site(...)`, qué instrucción se envolvería, comportamiento observado en cada repetición, riesgos.
  hecho cuando: T432 tiene un diseño que se sostiene, o queda escrito "no viable" con el motivo.
  no hagas: no fuerces la escritura de la vida cada fotograma como solución (no es un estado caído); no pegues desensamblado en el repo.
  modelo sugerido: alto

- [ ] T407 [P] [investigación] Spike: teletransporte fuera del modo desarrollador y punto de llegada seguro
  files: specs/bitacora/F4-T407-teleport.md
  verify: la bitácora responde a las 4 preguntas con medidas de 10 teletransportes de prueba
  depende de: F1
  quién: agente + dueño
  contexto: `control_tp_request(x, y, z)` (`runtime/control_tp.c:298`) deja una petición que atiende `control_tp_frame`, pero esa función solo se llama con el modo desarrollador activo (`runtime/control_game.c:36-40`). Los saltos largos dependen del sitio opcional `tp_gate` (`tools/game_sites.py:565`).
  pasos:
    1. Pregunta 1: ¿qué más hace `control_tp_frame` que dependa del modo desarrollador? (Lee `runtime/control_tp.c` entero.)
    2. Pregunta 2: con dos juegos y modo desarrollador, teletransporta al invitado a `posición del anfitrión + (1,5 m detrás, 0,5 m arriba)` 10 veces con `swctl teleport`: ¿cuántas llegan a menos de 3 m y sin daño?
    3. Pregunta 3: ¿qué pasa si el anfitrión está nadando, escalando, planeando o a caballo?
    4. Pregunta 4: ¿cuánto tarda un salto de más de 300 unidades y qué ve el otro jugador mientras?
    5. Forma de la respuesta: tabla de los 10 intentos y lista de estados en los que hay que bloquear.
  hecho cuando: T418 y T419 pueden fijar distancias y bloqueos con datos.
  no hagas: no cambies código de producción; no pruebes con el guardado real.
  modelo sugerido: medio

- [ ] T408 [P] [investigación] Spike: saber qué objeto sale de un cofre
  files: specs/bitacora/F4-T408-contenido-de-cofre.md
  verify: la bitácora muestra, para 5 cofres abiertos en el juego, el nombre del objeto registrado y el flag del cofre, emparejados correctamente
  depende de: F1
  quién: agente + dueño
  contexto: El flag de un cofre abierto se comparte (`<mapa>_TBox_<tipo>_<HashId>`, DOC:704-706) pero el contenido es solo de quien lo abre. El runtime ya tiene un parche a la entrada de la función de recogida del juego (multiplicador de recogida: `rt_item_get_scale(safe_str, value)`, `runtime/items.c:404`) que ve nombre y valor de toda recogida.
  pasos:
    1. Pregunta exacta: ¿cómo distinguir "este objeto salió de un cofre" de "lo recogí del suelo", sin ingeniería inversa nueva?
    2. Añade un registro temporal (no se fusiona) en ese parche: nombre, valor y nombre del evento activo (`framepace_event_name`).
    3. Abre 5 cofres y recoge 5 objetos del suelo; compara los registros.
    4. Comprueba si el flag del cofre cambia antes o después de la recogida (comando `flag` del control).
    5. Forma de la respuesta: regla propuesta (por ejemplo "recogida dentro del evento X" o "recogida a menos de N ms del cambio de un flag `_TBox_`"), con los 10 casos como evidencia y los falsos positivos vistos.
  hecho cuando: T417 puede implementarse con una regla comprobada.
  no hagas: no fusiones el registro temporal; no pruebes con el guardado real.
  modelo sugerido: medio

## Bloque 1 — Protocolo (dueño único)

- [ ] T409 [protocolo] Definir el cambio de cable de F4 en un solo salto de versión
  files: runtime/online.h, runtime/online.c (solo `PROTOCOL`, enumeración de mensajes, estructuras de cable y reenvío dirigido), runtime/online_authority.c, la cabecera compartida con el bot, tools/online_bot.c, tests/test_online_wire.c, tests/test_online_authority.c, specs/contratos/protocolo.md
  verify: `make test-online` sale con 0 **y** `sh tools/nettest/run.sh smoke` sale con 0
  depende de: F3 completa, T401
  quién: agente designado como **dueño del protocolo**
  contexto: F3 dejó el protocolo con saludo ampliado, autoridad en el servidor (`runtime/online_authority.c`) y `seq`/`t_ms` en STATE. F4 necesita doce mensajes y cuatro campos nuevos; se definen todos de una vez para que los carriles no toquen el cable.
  pasos:
    1. Sube `PROTOCOL` al siguiente número libre y regístralo en `specs/contratos/protocolo.md`.
    2. Mensajes nuevos (estructuras empaquetadas, tamaño fijo): WORLD (identificador de mundo 16 bytes), REWARD (hash de flag u32, nombre de objeto char[40], valor s32, clase u8), TP_REQ (destino u8), TP_REPLY (destino u8, resultado u8, posición float[3]), GIVE (destino u8, operación u8, identificador u32, nombre char[40], cantidad s32), MARK (posición float[3], tipo u8), QCHAT (frase u8), PVP_CTL (operación u8), PVP_HIT (objetivo u8, secuencia u16), PVP_DMG (objetivo u8, atacante u8, daño s16), DOWN (segundos u8), REVIVE (objetivo u8).
    3. STATE: añade `int16_t life` (cuartos de corazón), `uint8_t colour`, `uint8_t shape`, `uint8_t coop_flags` (bit 0 caído, bit 1 PvP activo).
    4. Reenvío dirigido en el servidor: los mensajes con campo destino se envían solo a esa ranura, con la ranura de origen delante.
    5. Tabla de autoridad: WORLD, REWARD y PVP_DMG solo del anfitrión; PVP_HIT solo hacia el anfitrión; TP_REQ, GIVE pasan por la validación de longitud y se reenvían al destino; MARK, QCHAT, PVP_CTL, DOWN, REVIVE a todos.
    6. Límites de tasa por tipo (añádelos a la tabla de F3): MARK 2/s, QCHAT 2/s, TP_REQ 1 cada 2 s, GIVE 5/s, PVP_HIT 4/s.
    7. `_Static_assert` de tamaños; pruebas de la tabla de autoridad para cada mensaje nuevo; actualiza el bot.
    8. Deja todos los manejadores vacíos.
  hecho cuando: juego y bot compilan y se conectan con la versión nueva; `protocolo.md` lista cada mensaje con tamaño, sentido y quién puede enviarlo.
  no hagas: no implementes la lógica de ninguna función; no dejes cadenas sin terminador garantizado en el receptor; no cambies la versión más de una vez.
  modelo sugerido: alto

## Bloque 2 — Mundo del anfitrión, guardados y recompensas

- [ ] T410 [P] [lógica] Línea base por invitado y filtro de cambios de flags
  files: runtime/coop_flags.c, runtime/coop_flags.h, tests/test_coop_flags.c
  verify: `clang -Wall -Iruntime tests/test_coop_flags.c runtime/coop_flags.c -o build/test_coop_flags && build/test_coop_flags` sale con 0
  depende de: T401
  quién: agente
  contexto: Hoy el anfitrión hace el OR de los 4096 bytes de flags de cada jugador (`online_take_quests`, `runtime/online.c:664-670`) y reparte la unión: un invitado con la partida avanzada vuelca todo su progreso en el mundo del anfitrión. Los flags son bits; el bit k corresponde a la línea k de `online_mod/quests.txt`.
  pasos:
    1. `typedef struct { uint8_t base[4096], accepted[4096]; bool have_base; } CoopGuestFlags;`
    2. `void coop_flags_reset(CoopGuestFlags *g);`
    3. `int coop_flags_delta(CoopGuestFlags *g, const uint8_t now[4096], const uint8_t allowed[4096], int max, uint16_t *out, int cap);` — la primera llamada guarda `now` como línea base y devuelve 0. Las siguientes devuelven los índices de bits que están en `now`, no en `base`, no en `accepted` y sí en `allowed`. Si son más de `max`, devuelve -1 y no acepta ninguno. Los devueltos se marcan en `accepted`.
    4. `void coop_flags_allowed_from_names(const char *const *names, int n, uint8_t allowed[4096]);` — marca permitido cada flag salvo los que empiecen por un prefijo de una lista de exclusión que recibirá de la tabla del ADR T401.
    5. Pruebas: primera llamada; un bit nuevo; bit de la línea base que se apaga y se enciende (no cuenta); 17 bits nuevos con `max` 16 → -1; todos a 1 en la primera llamada → 0 cambios; bit no permitido.
  hecho cuando: la prueba pasa y el fichero no incluye cabeceras del juego ni de red.
  no hagas: no leas `quests.txt` aquí (recibe los nombres ya cargados); no uses memoria dinámica; no toques `online.c`.
  modelo sugerido: medio

- [ ] T411 [mundo] El anfitrión deja de aplicar la unión de flags
  files: el módulo de flags de `online_actors` (función `quests_update`), runtime/online.c (solo `online_take_quests`, `quest_of`, y la limpieza por ranura)
  verify: `sh tools/nettest/run.sh flags_all_ones` sale con 0 (bot invitado con todos los bits a 1: el arnés comprueba que QUESTS del anfitrión no cambia) **y** recorrido manual §CU-06 de `verificacion.md`
  depende de: T409, T410, T413
  quién: agente + dueño (prueba en el juego)
  contexto: `quests_update` (`runtime/online_actors.c:3851`) en modo anfitrión llama a `online_take_quests(all)` (`:3929`), hace OR con los flags propios y publica el resultado con `online_set_quests` (`:3931`). En `online.c`, el QUEST de cada ranura se guarda en `g.quest_of[ranura]` (`:1065-1070`) y `online_take_quests` los une (`:664-670`). Además `quest_of` no se borra cuando otro jugador ocupa la ranura.
  pasos:
    1. En `online.c`: sustituye la unión por una función que entregue al hilo del juego, por ranura, el último QUEST recibido y un indicador "nuevo".
    2. En `quests_update` (anfitrión): mantén un `CoopGuestFlags` por ranura; para cada QUEST nuevo llama a `coop_flags_delta` con `max = 16`.
    3. Cada índice devuelto se activa en el juego del anfitrión con el mismo camino que ya usa la función para flags ajenos (`set_bool_flag`), y pasa por la política de recompensas (T416) cuando exista.
    4. Si `coop_flags_delta` devuelve -1: registra una línea y muestra un aviso; no apliques nada.
    5. `QUESTS` pasa a ser exactamente los flags del anfitrión.
    6. Reinicia el `CoopGuestFlags` de una ranura cuando llega JOIN de un jugador nuevo; consérvalo si es una reanudación (F3).
    7. No toques la parte de Luna Carmesí (`held_off`, `revival`).
    8. Añade al bot el modo `BOT_FLAGS_ALL=1` y el escenario `flags_all_ones`.
  hecho cuando: el bot con todos los bits a 1 no cambia ningún flag del anfitrión, y un invitado real que activa un santuario sí lo cambia.
  no hagas: no cambies cómo el invitado aplica QUESTS; no llames al juego desde `online.c`; no quites el filtro `g.hosting`; no pruebes con un guardado real.
  modelo sugerido: alto

- [ ] T412 [P] [lógica] Identidad de mundo y nombre del perfil cooperativo
  files: runtime/coop_world.c, runtime/coop_world.h, tests/test_coop_world.c
  verify: `clang -Wall -Iruntime tests/test_coop_world.c runtime/coop_world.c -o build/test_coop_world && build/test_coop_world` sale con 0
  depende de: ninguna
  quién: agente
  contexto: No existe ningún identificador del mundo de un anfitrión. Hace falta para que el invitado guarde su personaje "ligado al mundo de ese anfitrión" y para no mezclar perfiles de anfitriones distintos.
  pasos:
    1. `bool coop_world_load(const char *dir, uint8_t id[16]);` lee `<dir>/coop-world.txt` (32 dígitos hexadecimales).
    2. `bool coop_world_create(const char *dir, const uint8_t rnd[16], uint8_t id[16]);` lo escribe (escritura a fichero temporal y renombrado). Los bytes aleatorios se reciben como argumento.
    3. `void coop_world_profile_name(const uint8_t id[16], char out[32]);` → `coop-` + los 8 primeros bytes en hexadecimal.
    4. `bool coop_world_valid_name(const char *s);` solo `[a-z0-9-]`, longitud ≤ 31.
    5. Pruebas: crear y leer en un directorio temporal; fichero corrupto → false; nombre estable.
  hecho cuando: la prueba pasa en Windows y Linux.
  no hagas: no generes números aleatorios dentro del módulo; no construyas rutas con datos de la red sin pasar por `coop_world_valid_name`; no uses rutas absolutas en la prueba.
  modelo sugerido: bajo

- [ ] T413 [mundo] Copia de seguridad automática antes de hospedar o unirse
  files: runtime/save_backup.c, runtime/online.c (solo la llamada en `online_host` y `online_join`), tests/test_coop_backup.c
  verify: `clang ... tests/test_coop_backup.c` (orden exacta en la cabecera de la prueba) sale con 0 **y**, con el juego: tras "Create session" existe una carpeta nueva con fecha en la carpeta de copias
  depende de: F1
  quién: agente
  contexto: `save_backup_at_start` (`runtime/save_backup.c:55`) copia el guardado al arrancar, una vez por ejecución, pero **no** lo hace si está definida `SAVAGEWIND_SAVE_DIR` (`:57`) ni antes de entrar en una sesión. Los flags y recompensas del modo en línea se escriben en el guardado y no se pueden deshacer.
  pasos:
    1. Lee `runtime/save_backup.c` entero y reutiliza su función de copia.
    2. Añade `bool save_backup_for_coop(void);` que copia la carpeta de guardado de la sesión actual (`sp_session_save_dir()`) a la carpeta de copias con el prefijo `coop-<fecha>`; como mucho una vez por día natural; conserva las 5 últimas con ese prefijo.
    3. Funciona también con `SAVAGEWIND_SAVE_DIR`.
    4. Se ejecuta en un hilo de trabajo (no en el hilo de UI ni en el del juego); `online_host`/`online_join` no continúan hasta que termine o falle, mostrando el estado "Backing up your save…".
    5. Si falla, la sesión no empieza y se dice por qué.
    6. Prueba con carpetas falsas en un directorio temporal: crea, limita a 5, una por día.
  hecho cuando: las dos comprobaciones pasan.
  no hagas: no copies mientras el juego está escribiendo sin tratar el error (reintenta como hace `fs.c` con `EACCES`); no borres copias que no tengan el prefijo `coop-`; no bloquees la UI.
  modelo sugerido: medio

- [ ] T414 [mundo] Perfil de guardado cooperativo del invitado
  files: runtime/services/save_profiles.c, runtime/services/save_profiles.h, runtime/online.c (solo el manejador de WORLD y el arranque de la unión), runtime/main.c (solo si el ADR de T402 lo exige)
  verify: recorrido manual §CU-07 de `verificacion.md` (suma de comprobación del perfil normal igual antes y después; objetos recogidos persisten entre sesiones)
  depende de: T402, T409, T412, T413
  quién: agente + dueño
  contexto: El invitado juega hoy con su partida normal, que recibe flags y objetos de la sesión. La carpeta de guardado se fija una vez por arranque (`runtime/main.c:589`). Existen `sp_copy` (`runtime/services/save_profiles.c:428`) y `sp_set_active` (`:389`). El ADR de T402 dice cómo cambiar de perfil.
  pasos:
    1. El anfitrión, al crear sesión, carga o crea su identificador de mundo (`coop_world_*`) y lo envía en WORLD a cada invitado que entra.
    2. El invitado, al recibir WORLD: calcula el nombre del perfil; si no existe, lo crea con `sp_copy` desde su perfil actual.
    3. Cambia al perfil cooperativo por el camino que eligió el ADR de T402.
    4. Al salir de la sesión, vuelve al perfil anterior por el mismo camino.
    5. Si el invitado ya está en el perfil correcto, no hagas nada.
    6. Muestra en la pestaña en qué perfil se está jugando.
  hecho cuando: el recorrido §CU-07 pasa.
  no hagas: no uses el identificador recibido para formar una ruta sin validarlo; no cambies de carpeta con el juego en marcha sin pasar por el reinicio que definió el ADR; no borres perfiles; no toques el perfil del anfitrión.
  modelo sugerido: alto

- [ ] T415 [P] [lógica] Política de recompensas y libro de entregas
  files: runtime/coop_rewards.c, runtime/coop_rewards.h, tests/test_coop_rewards.c
  verify: `clang -Wall -Iruntime tests/test_coop_rewards.c runtime/coop_rewards.c -o build/test_coop_rewards && build/test_coop_rewards` sale con 0
  depende de: T401
  quién: agente
  contexto: Las entregas están escritas a mano en siete líneas de `quests_update` (`runtime/online_actors.c:3987-3997`) y ocurren siempre que llega el flag, también cuando un invitado entra en un mundo avanzado. No hay registro de lo ya entregado.
  pasos:
    1. `typedef enum { RW_NONE, RW_WORLD_ABILITY, RW_MILESTONE, RW_COLLECTIBLE, RW_CHEST } RewardClass;`
    2. `RewardClass coop_reward_class(const char *flag);` por nombre: `IsGet_PortableUnit`, runas y mejoras de la piedra, `FindDungeon_Finish` → capacidad; `Clear_Dungeon*`, `IsGet_Obj_HeroSoul_*`, `Get_HeartUtuwa_*` → hito; Kolog → coleccionable; `*_TBox_*` → cofre.
    3. `bool coop_reward_decide(RewardClass c, bool at_join);` según la tabla firmada en el ADR T401.
    4. Libro: `coop_ledger_load(path)`, `coop_ledger_has(hash)`, `coop_ledger_add(hash)` (añade una línea y hace `fflush`), con tope de 65 536 entradas.
    5. Pruebas: una por casilla de la tabla; libro: añadir, recargar, duplicado, fichero corrupto.
  hecho cuando: la prueba pasa y la tabla del código coincide casilla a casilla con el ADR.
  no hagas: no llames al juego; no decidas una casilla que el ADR no fija (para y pregunta); no uses rutas absolutas.
  modelo sugerido: bajo

- [ ] T416 [mundo] Entregar recompensas solo según la política
  files: el módulo de flags de `online_actors` (función `quests_update`)
  verify: recorrido manual §CU-07 (tabla de política caso a caso con dos juegos) con entrada en bitácora
  depende de: T411, T414, T415
  quién: agente + dueño
  contexto: En `quests_update`, justo después de activar un flag recibido (`set_bool_flag`), hay siete entregas incondicionales con `give_item` y `give_heart` (`runtime/online_actors.c:3987-3997`).
  pasos:
    1. Carga el libro del perfil actual al entrar en sesión (ruta junto al perfil: `coop-ledger.txt`).
    2. Marca como "al unirse" los flags que llegan en el primer QUESTS tras entrar; los posteriores son "durante la sesión".
    3. Sustituye las siete líneas por: `clase = coop_reward_class(nombre)`; si `coop_reward_decide(clase, al_unirse)` y no está en el libro → entrega como antes y añade al libro.
    4. Los flags que no se entregan se añaden igualmente al libro como "vistos" para que una reconexión no los trate como nuevos.
    5. Lo mismo en el anfitrión para los flags aceptados de un invitado (T411).
  hecho cuando: cada casilla de la tabla se comporta como dice el ADR y reconectar no duplica nada.
  no hagas: no cambies las funciones `give_item`/`give_heart`; no entregues nada fuera del hilo del juego; no pruebes con el guardado real.
  modelo sugerido: alto

- [ ] T417 [mundo] Contenido de cofre para los dos
  files: runtime/items.c (solo el punto de observación de recogidas), el módulo de flags de `online_actors`, runtime/online.c (colas de REWARD)
  verify: recorrido manual §Cofres de `verificacion.md`: 5 cofres abiertos por un jugador, el otro recibe los 5 contenidos, 0 duplicados
  depende de: T408, T416, T421
  quién: agente + dueño
  contexto: El flag de un cofre se comparte pero el contenido es solo de quien lo abre (DOC:704-706). T408 dejó una regla comprobada para reconocer que una recogida viene de un cofre.
  pasos:
    1. En el punto de observación de recogidas de `items.c`, publica (cola con mutex) nombre y valor de cada recogida local.
    2. En el hilo del juego, aplica la regla de T408 para emparejar recogida y flag de cofre.
    3. Envía REWARD (hash del flag, nombre, valor, clase cofre). Si lo envía un invitado, va al anfitrión, que lo valida (el flag está en la lista y acaba de cambiar) y lo reenvía.
    4. El receptor comprueba política y libro (T415), valida el nombre contra la lista de `items.c` y entrega por `items_request`.
  hecho cuando: el recorrido pasa.
  no hagas: no entregues objetos recibidos por REWARD que no estén en la lista de objetos del juego local; no reenvíes un REWARD de un cofre que el anfitrión no ve abierto; no retransmitas las recogidas del suelo.
  modelo sugerido: alto

## Bloque 3 — Teletransporte

- [ ] T418 [P] [lógica] Máquina de estados del teletransporte al compañero
  files: runtime/coop_tp.c, runtime/coop_tp.h, tests/test_coop_tp.c
  verify: `clang -Wall -Iruntime tests/test_coop_tp.c runtime/coop_tp.c -lm -o build/test_coop_tp && build/test_coop_tp` sale con 0
  depende de: T407
  quién: agente
  contexto: No existe teletransporte al compañero. Las piezas del juego ya están (posición y mapa del compañero en `OnlinePlayer`, `control_tp_request`); falta la lógica de petición, consentimiento y enfriamiento.
  pasos:
    1. Estados: `IDLE, ASKED, ACCEPTED, TRAVELLING, DONE, REFUSED, EXPIRED`.
    2. `coop_tp_ask(tp, ahora_ns)`, `coop_tp_on_reply(tp, aceptado, pos)`, `coop_tp_tick(tp, ahora_ns)` (caduca a los 15 s; enfriamiento configurable, por defecto 30 s).
    3. `int coop_tp_can(const CoopTpWho *yo, const CoopTpWho *el)` devuelve 0 o un motivo: mapa distinto, evento, muerto, cargando, en pausa, montado, PvP activo, en enfriamiento. `CoopTpWho` son solo datos (booleanos, nombre de mapa, posición).
    4. `bool coop_tp_landing(const float pos_suelo[3], float yaw, float out[3])`: 1,5 m detrás y 0,5 m arriba; rechaza no finitos y coordenadas fuera de ±20 000.
    5. `coop_tp_ground_track(track, pos, en_suelo, ahora_ns)` y `coop_tp_ground_get(track, ahora_ns, out)`: última posición en el suelo de los últimos 10 s.
    6. Pruebas: cada transición, cada motivo, caducidad, enfriamiento, posiciones no finitas.
  hecho cuando: la prueba pasa; sin cabeceras del juego.
  no hagas: no llames a `control_tp_request` aquí; no leas el reloj dentro del módulo.
  modelo sugerido: medio

- [ ] T419 [reglas] Integrar el teletransporte
  files: runtime/control_game.c, runtime/control_tp.c, runtime/control.h, el módulo de reglas de `online_actors`, runtime/online.c (colas de TP_REQ / TP_REPLY)
  verify: recorrido manual §CU-08 de `verificacion.md` (10 teletransportes: 10 llegadas a menos de 3 m, 0 con daño)
  depende de: T409, T418
  quién: agente + dueño
  contexto: `control_game_frame` solo avanza `control_tp_frame` si `control_enabled()` (modo desarrollador), `runtime/control_game.c:36-40`. `control_tp_request` (`runtime/control_tp.c:298`) es seguro desde otro hilo (petición atómica). `sample_pose` ya lee el AS del cuerpo del Link local.
  pasos:
    1. Añade en `control_tp.c` un indicador atómico `coop_tp_active` y `void control_tp_allow_coop(bool);`. En `control_game_frame`: llama a `control_tp_frame` si `control_enabled() || coop_tp_active`.
    2. En el hilo del juego, cada fotograma: actualiza `coop_tp_ground_track` con la posición del Link local y si su AS de cuerpo no contiene `Fall`, `Glide`, `Para`, `Swim`, `Climb` (ajusta la lista con el resultado de T407).
    3. Quien pide: comprueba `coop_tp_can`, envía TP_REQ.
    4. Anfitrión: valida mapa y enfriamiento del que pide antes de reenviar.
    5. Quien recibe: muestra la petición (aviso con aceptar/rechazar, o aceptación automática) y responde TP_REPLY con `coop_tp_ground_get` + `coop_tp_landing`.
    6. Quien pidió: valida el punto (finito, a menos de 400 m en horizontal de la posición conocida del compañero), activa `control_tp_allow_coop(true)` y llama a `control_tp_request`; al terminar (`control_tp_status`), lo desactiva.
    7. Si `g_site.tp_gate` es 0 y la distancia horizontal supera 300, rechaza con motivo.
  hecho cuando: el recorrido §CU-08 pasa y fuera de un viaje `control_tp_frame` sigue sin ejecutarse en modo normal.
  no hagas: no actives el resto del modo desarrollador; no confíes en el punto recibido sin validarlo; no teletransportes durante un evento ni con el Link local muerto.
  modelo sugerido: alto

## Bloque 4 — Enviar objetos

- [ ] T420 [P] [lógica] Transacción de envío de objetos
  files: runtime/coop_give.c, runtime/coop_give.h, tests/test_coop_give.c
  verify: `clang -Wall -Iruntime tests/test_coop_give.c runtime/coop_give.c -o build/test_coop_give && build/test_coop_give` sale con 0 (incluye una prueba que corta la comunicación en cada paso y comprueba que la suma de objetos se conserva)
  depende de: T409
  quién: agente
  contexto: No existe envío directo. Sin una transacción, una caída de red a mitad duplicaría o perdería el objeto.
  pasos:
    1. Operaciones del mensaje GIVE: `OFFER, ACCEPT, REFUSE, TAKEN, DELIVERED, ABORT`.
    2. Emisor: `IDLE → OFFERED → (ACCEPT) → REMOVING → TAKEN_SENT → (DELIVERED) → DONE`. Receptor: `IDLE → (OFFER) → ACCEPTED → (TAKEN) → ADDING → DELIVERED_SENT`.
    3. La máquina no quita ni añade nada: devuelve acciones (`ACT_SEND`, `ACT_REMOVE_LOCAL`, `ACT_ADD_LOCAL`, `ACT_REFUND_LOCAL`, `ACT_NOTIFY`) que ejecuta la integración, y recibe sus resultados (`on_removed(ok)`, `on_added(ok)`).
    4. Plazos: sin ACCEPT en 20 s → ABORT; tras TAKEN sin DELIVERED en 60 s → `ACT_REFUND_LOCAL`.
    5. Identificadores: el receptor recuerda los 64 últimos y repite la respuesta sin volver a añadir.
    6. Una transacción a la vez por pareja.
    7. Pruebas: camino feliz; rechazo; fallo al quitar; fallo al añadir (→ ABORT y devolución); mensaje duplicado; pérdida de cada mensaje.
  hecho cuando: la prueba pasa; en todos los casos emisor + receptor suman lo mismo que al principio.
  no hagas: no añadas en el receptor antes de recibir TAKEN; no uses memoria dinámica; no incluyas cabeceras del juego.
  modelo sugerido: alto

- [ ] T421 [reglas] Recibir un objeto enviado
  files: runtime/items.c, runtime/items.h (si existe; si no, la cabecera donde se declara `items_request`), el módulo de reglas de `online_actors`, runtime/online.c (colas de GIVE)
  verify: `sh tools/nettest/run.sh give_recv` no aplica (necesita el juego): recorrido manual §CU-09 paso "recibir" con el bot en modo `BOT_GIVE=<objeto>,<n>` y captura del inventario
  depende de: T420
  quién: agente + dueño
  contexto: `items_request(i, count)` (`runtime/items.c:367`) encola una entrega por índice en la lista de objetos del juego; `rt_item_tick` (`:420`) la ejecuta en el hilo del juego llamando a la función de recogida del propio juego. Solo corre "en juego" (no en menús ni eventos) y admite 32 peticiones. El resultado se deduce contando entradas de la bolsa (`:352-365`).
  pasos:
    1. Añade una búsqueda por nombre en la lista de objetos de `items.c` (`int items_find(const char *name)`), que devuelve -1 si el nombre no está.
    2. Al recibir OFFER: nombre no encontrado, cantidad fuera de 1..999, o juego no "en juego" → REFUSE con motivo.
    3. Al recibir TAKEN: `items_request`; espera el resultado y llama a `on_added(ok)`.
    4. Añade al bot `BOT_GIVE=<objeto>,<n>` (hace de emisor que siempre "consigue quitar").
  hecho cuando: el bot envía 3 manzanas y aparecen en el inventario del juego; un nombre inventado se rechaza sin efectos.
  no hagas: no uses `give_item` de `online_actors` (direcciones fijas) si la vía de `items.c` está disponible; no llames a `guest_call` fuera de `rt_item_tick`; no aceptes objetos con la etiqueta `Important`.
  modelo sugerido: medio

- [ ] T422 [reglas] Quitar del inventario del emisor
  files: tools/game_sites.py, tools/patches.py (solo si T404 lo pide), runtime/game_sites.h, runtime/items.c, el módulo de reglas de `online_actors`
  verify: `tools/venv/Scripts/python.exe tools/test_game_sites.py` sale con 0, `tools/game_sites.py find <main>` lista el sitio nuevo como `ok`, y recorrido manual §CU-09 completo (20 envíos, suma constante)
  depende de: T404 (respuesta escrita), T421
  quién: agente + dueño (recompilar el juego y probar)
  contexto: No hay función para quitar materiales o comida de la bolsa. La bitácora de T404 describe la función del juego que lo hace, su patrón y sus precondiciones. Tocar `tools/game_sites.py` cambia `GAMELIB_ID`: los dos jugadores deben regenerar y recompilar el juego.
  pasos:
    1. Lee la bitácora de T404. Si dice "no resuelto", **no hagas esta tarea**.
    2. Añade el sitio a `SITES` y a `KNOWN['1.9.0']` de `tools/game_sites.py`, en un grupo opcional nuevo (`OPTIONAL_GROUPS`), y a `GAME_SITES_OPTIONAL` en `runtime/game_sites.h`.
    3. En `items.c`, junto a `rt_item_tick`: una cola de retiradas con el mismo patrón (mutex + atómico + `RT_ON_RESET`), ejecutada en el hilo del juego con las precondiciones que indica T404.
    4. Si `g_site.<sitio>` es 0, la retirada responde "no disponible" y la UI ofrece "soltar para el compañero" (T423).
    5. Conecta `ACT_REMOVE_LOCAL` y `ACT_REFUND_LOCAL` de T420.
    6. Agrupa este cambio con cualquier otro de `game_sites.py`/`patches.py` pendiente en F4 (una sola recompilación).
  hecho cuando: los tres `verify` pasan.
  no hagas: no escribas la dirección en un `.c` (solo `g_site`); no llames a la función sin las precondiciones de T404; no permitas quitar objetos equipados ni con etiqueta `Important`.
  modelo sugerido: alto

- [ ] T423 [reglas] "Soltar para el compañero" (sin ingeniería inversa)
  files: el módulo de botín de `online_actors`, runtime/online.c (cola de avisos), runtime/ui/coop_hud.mm
  verify: recorrido manual §CU-09 paso "soltar": el compañero ve una marca sobre el objeto soltado y un aviso
  depende de: T409, T425
  quién: agente + dueño
  contexto: Lo que un jugador saca de la bolsa y deja caer se añade a la lista de botín compartida tras 0,4 s quieto, y el otro puede recogerlo (DOC:528-547; `loot_update` en `runtime/online_actors.c:3237-3649`). Funciona hoy, pero el compañero no sabe que hay algo para él.
  pasos:
    1. Cuando el juego local añade a la lista un objeto que soltó su propio jugador, envía un MARK de tipo "objeto" en esa posición.
    2. El overlay del compañero dibuja la marca 20 s y un aviso "<nombre> dropped something for you".
    3. No cambies nada de la lógica de botín.
  hecho cuando: soltar 3 setas produce una marca y un aviso en la otra pantalla y el otro las recoge.
  no hagas: no marques el botín de enemigos; no envíes más de 2 MARK por segundo.
  modelo sugerido: medio

## Bloque 5 — Marcadores, pings, chat rápido y accesibilidad

- [ ] T424 [P] [lógica] Paleta segura y formas
  files: runtime/coop_palette.c, runtime/coop_palette.h, tests/test_coop_palette.c
  verify: `clang -Wall -Iruntime tests/test_coop_palette.c runtime/coop_palette.c runtime/colour_vision.c -lm -o build/test_coop_palette && build/test_coop_palette` sale con 0
  depende de: ninguna
  quién: agente
  contexto: El overlay usa 8 colores fijos por número de ranura (`runtime/ui/online_ui.mm:20-23`) y el color es lo único que distingue a un jugador de otro además del nombre. El proyecto tiene matrices de simulación de daltonismo en `runtime/colour_vision.h` (`colour_vision_simulation(tipo, severidad, m)`).
  pasos:
    1. Tabla de 8 colores (RGB) elegidos para distinguirse con protanopia, deuteranopia y tritanopia.
    2. Enumeración de 8 formas: círculo, cuadrado, triángulo, rombo, estrella, cruz, hexágono, anillo.
    3. `coop_palette_assign(preferidos[], n, out_color[], out_forma[])`: respeta la elección; si dos coinciden en color y forma, el de índice mayor recibe la siguiente forma libre.
    4. Prueba: para cada par de colores de la tabla, tras aplicar cada una de las 3 simulaciones de `colour_vision_simulation` a severidad 1, la diferencia de luminancia o de tono supera un umbral que fijes y documentes.
  hecho cuando: la prueba pasa; si un par no supera el umbral, cambia el color, no el umbral.
  no hagas: no incluyas ImGui ni nada de UI; no dependas de `g_settings`.
  modelo sugerido: bajo

- [ ] T425 [ui] Color y forma por jugador en el overlay
  files: runtime/ui/online_ui.mm, runtime/ui/coop_hud.mm (nuevo), runtime/ui/coop_hud.h (nuevo), runtime/online.c (solo publicar color y forma del jugador local en STATE)
  verify: `make runtime-objs` sin avisos; captura de ventana (`SAVAGEWIND_WINDOW_SHOT`) con dos jugadores mostrando nombre con color y forma elegidos
  depende de: T409, T424
  quién: agente
  contexto: `online_overlay_draw` (`runtime/ui/online_ui.mm:181-304`) dibuja para cada jugador un círculo de color en la lista (`:229`), la figura y el nombre sobre él (`:283-287`) y una flecha en el borde (`:289-301`), siempre con `k_colors[ranura]`.
  pasos:
    1. Crea `coop_hud.mm` con `coop_shape_draw(ImDrawList*, centro, radio, forma, color)` para las 8 formas, cada una con borde oscuro.
    2. Sustituye `k_colors[i]` por el color y la forma que llegan en el STATE de cada jugador (validados: índice < 8, si no → por ranura).
    3. Dibuja la forma junto al nombre en la lista, sobre el jugador y en la flecha del borde.
    4. En la pestaña: selectores "Your colour" y "Your shape" (guardados en `online.ini`).
  hecho cuando: cambiar color o forma se ve en la otra pantalla en menos de 1 s.
  no hagas: no edites `runtime/ui/menu.mm`; no llames al juego ni a la red desde el dibujo; no uses fuentes ni iconos del juego.
  modelo sugerido: medio

- [ ] T426 [ui] Pings (marcas en el mundo)
  files: runtime/ui/coop_hud.mm, runtime/online.c (colas de MARK), runtime/online.h (solo las funciones `online_send_mark`/`online_marks`; avisa al dueño del protocolo)
  verify: `make runtime-objs` sin avisos; `sh tools/nettest/run.sh mark_rate` sale con 0 (bot que envía 50 MARK por segundo: el otro bot recibe ≤ 2 por segundo); captura con un ping visible en las dos pantallas
  depende de: T425
  quién: agente
  contexto: No hay pings: PING/PONG del protocolo solo mide latencia. El overlay ya sabe proyectar un punto del mundo a la pantalla (`project`, `runtime/ui/online_ui.mm:135`) con la cámara que publica el hilo de render (`online_camera_get`, usada en `:256`).
  pasos:
    1. Lista de hasta 16 marcas activas (posición, tipo, emisor, hora), con mutex; 8 s de vida; máximo 4 por jugador (la más antigua se sustituye).
    2. Al recibir MARK: valida posición finita y dentro de ±20 000, tipo < 4.
    3. Dibujo: forma según el tipo (aquí, peligro, objeto, ayuda), color del emisor, distancia en metros; si está fuera de pantalla, flecha en el borde como la de los jugadores.
    4. Colocación local: (a) en la posición del Link local; (b) si hay un jugador o enemigo conocido proyectado a menos de un 10 % del ancho de pantalla del centro, en su posición.
    5. Escenario `mark_rate` con el modo de bot `BOT_FLOOD=mark,50`.
  hecho cuando: los dos `verify` pasan.
  no hagas: no dibujes marcas con una cámara de más de 0,5 s (se descartan, como hace el código actual); no intentes trazar rayos contra el terreno.
  modelo sugerido: medio

- [ ] T427 [ui] Chat rápido
  files: runtime/ui/coop_hud.mm, runtime/online.c (colas de QCHAT), specs/fases/F4-funciones-coop/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos; captura: un juego en español y otro en inglés muestran la misma frase cada uno en su idioma
  depende de: T426
  quién: agente
  contexto: El chat solo se escribe desde el menú (`runtime/ui/online_ui.mm`, campo "Chat") y viaja como texto libre (`M_CHAT`). Con mando es lento y no se traduce.
  pasos:
    1. Tabla fija de 8 frases en inglés (por ejemplo: "Come here", "Wait", "Help!", "Thanks", "Yes", "No", "Follow me", "Look at this").
    2. Enviar: QCHAT con el índice. Recibir: valida índice < 8 y añade a las líneas de chat `tr(frase)` con el nombre del emisor.
    3. Selector en pantalla: rueda o lista de 8 que se abre con la acción "quick chat" (T436) y se elige con el stick o los números.
    4. Las frases deben aparecer como literales dentro de `tr("...")` en este `.mm` para que el comprobador de idiomas las vea.
  hecho cuando: la captura muestra cada juego en su idioma.
  no hagas: no envíes el texto de la frase por la red; no abras el selector durante un evento o con el menú abierto.
  modelo sugerido: bajo

- [ ] T428 [ui] Avisos en pantalla y tamaño de texto
  files: runtime/ui/coop_hud.mm, runtime/online.c (cola de avisos: `online_notice_push`), el módulo de reglas de `online_actors` (solo sustituir registros por avisos)
  verify: `make runtime-objs` sin avisos; captura del aviso "A is held back" con el bot en modo `BOT_DEMO=1` a menos de 30 m
  depende de: T427
  quién: agente
  contexto: Avisos importantes para el jugador solo van al registro: por ejemplo "another player is at a Sheikah pedestal near: A held back" (`runtime/online_actors.c:5999`). El jugador pulsa A, no pasa nada y no sabe por qué. El overlay ya tiene avisos de "se unió / se fue" durante 5 s.
  pasos:
    1. `online_notice_push(const char *texto_en_ingles, int tipo)`: cola con mutex de 8 avisos, llamable desde el hilo del juego; el texto es un literal fijo.
    2. El overlay los pinta 5 s con `tr()`, con icono por tipo (información, aviso, error).
    3. Sustituye por avisos: botón A retenido; "<nombre> wants to come to you"; flags rechazados por exceso (T411); recompensa recibida.
    4. Ajuste "Overlay text size" (80-160 %) en la pestaña, aplicado a todo el overlay cooperativo.
  hecho cuando: el aviso aparece en la captura y el tamaño cambia al mover el ajuste.
  no hagas: no pases texto de la red como aviso; no reserves memoria por fotograma; no bloquees el hilo del juego esperando el mutex más que una copia corta.
  modelo sugerido: medio

- [ ] T429 [ui] Vida del compañero en pantalla
  files: el módulo de muestreo de `online_actors` (función `sample_pose`), runtime/online.c (copiar el campo), runtime/ui/coop_hud.mm
  verify: `make runtime-objs` sin avisos; captura con dos juegos: tras dañar al invitado (`swctl` o un enemigo), sus corazones bajan en la pantalla del anfitrión
  depende de: T409, T425, decisión P4-7
  quién: agente
  contexto: Ningún mensaje lleva la vida del jugador. El juego local ya lee la suya como un entero de cuartos de corazón en `player + 0x848` (constante `ENEMY_LIFE` usada en `pvp_update`, `runtime/online_actors.c:2906`; DOC:644).
  pasos:
    1. En `sample_pose`, copia ese valor al campo `life` del STATE (definido en T409), acotado a 0..160.
    2. En el overlay, bajo el nombre del compañero: corazones (uno por cada 4 cuartos, con cuartos parciales) dibujados con formas propias, y el número.
    3. Si `life` es 0 y el indicador "caído" está activo, muestra el estado caído.
  hecho cuando: la captura lo muestra.
  no hagas: no leas la memoria del juego desde el overlay; no uses los iconos de corazón del juego; no inventes el máximo de corazones (no se conoce: muestra solo los actuales).
  modelo sugerido: medio

## Bloque 6 — Revivir

- [ ] T430 [P] [lógica] Política de revivir
  files: runtime/coop_revive.c, runtime/coop_revive.h, tests/test_coop_revive.c
  verify: `clang -Wall -Iruntime tests/test_coop_revive.c runtime/coop_revive.c -lm -o build/test_coop_revive && build/test_coop_revive` sale con 0
  depende de: T401 (P4-4)
  quién: agente
  contexto: Hoy `rt_online_has_fairy` responde "sí" siempre que hay sesión y la regla `revive` está activa (`runtime/online_actors.c:2825-2831`): reanimación automática sin que el compañero intervenga.
  pasos:
    1. `bool coop_revive_allowed(const CoopReviveIn *in)`: regla activa, hay al menos un compañero vivo, en el mismo mapa, no cargando, a menos de `in->max_dist` metros (por defecto 50).
    2. Máquina de estado caído (para T432): `UP → DOWN(t) → REVIVING(progreso) → UP | DEAD`, con cuenta atrás (por defecto 30 s) y progreso mientras un compañero mantiene el botón a menos de 2 m durante 3 s.
    3. Los dos caídos a la vez → `DEAD` para ambos.
    4. Pruebas: cada rama, límites de distancia, tiempo agotado, compañero que se aleja a mitad.
  hecho cuando: la prueba pasa; sin cabeceras del juego.
  no hagas: no leas el reloj ni posiciones globales dentro del módulo.
  modelo sugerido: bajo

- [ ] T431 [reglas] Revivir solo si el compañero puede ayudarte
  files: el módulo de reglas de `online_actors` (`rt_online_has_fairy`, `players_collide`)
  verify: recorrido manual §CU-11 de `verificacion.md` (compañero cerca → revive; lejos → fin de partida; 10 repeticiones sin crash)
  depende de: T430
  quién: agente + dueño
  contexto: `rt_online_has_fairy` (`runtime/online_actors.c:2825-2831`) se llama desde código del juego en el momento de morir y devuelve 1 si hay sesión y la regla está activa. La regla se refresca dentro de `players_collide` **después** de un `return` temprano (`:2837-2839`): con las colisiones apagadas por variable de entorno o durante un evento, no se actualiza.
  pasos:
    1. Mueve el refresco de `g_revive_rule` y `g_nopause_rule` a un punto del fotograma que se ejecute siempre (antes del `return` temprano de `players_collide`, o a una función propia llamada desde el orquestador).
    2. Cada fotograma, en el hilo del juego, calcula y guarda en una variable atómica el resultado de `coop_revive_allowed` con los datos de los jugadores.
    3. `rt_online_has_fairy` devuelve 1 solo si esa variable es verdadera (además de las condiciones actuales).
    4. Aviso en pantalla al revivir ("<nombre> revived you") o al no poder ("No partner near: game over").
  hecho cuando: el recorrido pasa.
  no hagas: no llames a `online_players()` dentro de `rt_online_has_fairy` (copia 8 jugadores en la pila desde un punto delicado del juego: usa el valor ya calculado); no crees ni borres actores mientras el Link local está muerto (cuelgue documentado, DOC:732-734).
  modelo sugerido: alto

- [ ] T432 [reglas] Estado caído real
  files: tools/game_sites.py, tools/patches.py, runtime/game_sites.h, el módulo de reglas de `online_actors`, runtime/ui/coop_hud.mm
  verify: `tools/venv/Scripts/python.exe tools/test_game_sites.py` sale con 0 y recorrido manual §CU-11 ampliado (caído 30 s; levantado por el compañero; ambos caídos)
  depende de: T406 (respuesta escrita y viable), T431
  quién: agente + dueño
  contexto: La bitácora de T406 describe dónde decide el juego morir o revivir y cómo retrasarlo. Hoy la reanimación es instantánea.
  pasos:
    1. Lee la bitácora de T406. Si dice "no viable", **no hagas esta tarea** y deja T431 como comportamiento final.
    2. Añade los sitios y el parche que indique T406 en un grupo opcional nuevo; si el grupo falta, se usa T431.
    3. Conecta la máquina de T430: al caer, indicador "caído" en STATE y mensaje DOWN; el compañero ve marca y cuenta atrás; mantener el botón de la acción "revive" (T436) cerca envía REVIVE; el anfitrión valida distancia; el caído revive por el camino que describa T406.
    4. Agrupa los cambios de `game_sites.py`/`patches.py` con los de T422 (una sola recompilación).
  hecho cuando: los `verify` pasan.
  no hagas: no implementes nada que T406 no describa; no escribas direcciones en un `.c`; no mantengas a Link vivo escribiendo su vida cada fotograma.
  modelo sugerido: alto

## Bloque 7 — PvP

- [ ] T433 [P] [lógica] Acuerdo de PvP y validación de golpes
  files: runtime/coop_pvp.c, runtime/coop_pvp.h, tests/test_coop_pvp.c
  verify: `clang -Wall -Iruntime tests/test_coop_pvp.c runtime/coop_pvp.c -lm -o build/test_coop_pvp && build/test_coop_pvp` sale con 0
  depende de: T401 (P4-6)
  quién: agente
  contexto: Hoy cada jugador activa "Damage between players" por su cuenta y el atacante escribe en su STATE a quién golpea y con cuánto daño; la víctima lo aplica (`runtime/online_actors.c:2874-2928`). Solo se descarta el daño ≤ 0: un valor enorme se acepta.
  pasos:
    1. Acuerdo: `OFF → PROPOSED(por X) → ON`; `coop_pvp_ctl(st, quien, operación)`; aceptar exige que sea otro jugador; apagar vale desde cualquiera y en cualquier estado; con más de 2 jugadores, hacen falta todos los presentes.
    2. `int coop_pvp_validate(const CoopPvpHit *h)` devuelve el daño a aplicar o 0: exige PvP activo, ambos vivos, mismo mapa, ninguno cargando, distancia ≤ `alcance + 2 m` (margen por latencia), secuencia nueva, ritmo ≤ 4 golpes por segundo por atacante, y daño = `min(poder_del_arma, 120)`; nunca negativo.
    3. `CoopPvpHit` son solo datos: posiciones, alcance, poder del arma (lo busca la integración), horas.
    4. Pruebas: cada rechazo, daño máximo, secuencia repetida, ráfaga de 10 golpes en 1 s.
  hecho cuando: la prueba pasa; sin cabeceras del juego.
  no hagas: no confíes en ningún valor de daño que venga del atacante; no uses variables globales.
  modelo sugerido: medio

- [ ] T434 [reglas] PvP por acuerdo con daño validado por el anfitrión
  files: el módulo de reglas de `online_actors` (`pvp_update`), runtime/online.c (colas de PVP_CTL, PVP_HIT, PVP_DMG), runtime/ui/online_ui.mm (sustituir el interruptor por proponer/aceptar/apagar)
  verify: `sh tools/nettest/run.sh pvp_cheat` sale con 0 (bot que envía PVP_DMG sin ser anfitrión y PVP_HIT a 500 m: el otro bot recibe 0 PVP_DMG) **y** recorrido manual §CU-12
  depende de: T409, T433
  quién: agente + dueño
  contexto: `pvp_update` (`runtime/online_actors.c:2874`) hace dos cosas: publica el golpe propio en `g_pvp` (objetivo, daño = poder de ataque del arma según `power.txt`) y aplica los golpes de los demás leídos de su STATE (`pvp_seq`, `pvp_target`, `pvp_damage`), restando la defensa propia y respetando el bloqueo con escudo (`:2907-2928`).
  pasos:
    1. Acuerdo: la pestaña ofrece "Propose PvP" / "Accept" / "Turn off"; envían PVP_CTL. El juego del anfitrión mantiene el estado (`coop_pvp_ctl`) y lo publica en sus reglas de sesión; los demás lo adoptan.
    2. Golpe: donde hoy se rellena `g_pvp`, envía PVP_HIT (objetivo, secuencia) en su lugar.
    3. Anfitrión: por cada PVP_HIT construye `CoopPvpHit` con las posiciones que conoce de ambos y el poder del arma del atacante según su EQUIP (`attack_power`), llama a `coop_pvp_validate` y, si el daño es mayor que 0, envía PVP_DMG.
    4. Víctima: aplica PVP_DMG con el código actual de defensa y bloqueo. Deja de leer `pvp_damage` del STATE.
    5. Bombas: el mismo camino, con el daño fijo actual calculado por el anfitrión.
    6. Añade el modo de bot `BOT_PVP_CHEAT=1` y el escenario.
  hecho cuando: los dos `verify` pasan.
  no hagas: no apliques daño que no venga en un PVP_DMG marcado por el servidor como del anfitrión; no quites el bloqueo con escudo ni la defensa; no permitas PvP con el acuerdo apagado aunque llegue PVP_DMG.
  modelo sugerido: alto

## Bloque 8 — Interfaz, entrada, idiomas y cierre

- [ ] T435 [ui] Sección cooperativa de la pestaña Online
  files: runtime/ui/online_ui.mm, specs/fases/F4-funciones-coop/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos; `tools/venv/Scripts/python.exe tools/i18n_check.py --missing es` lista solo textos de `textos-nuevos.txt`; captura de la pestaña
  depende de: T419, T421, T425, T434
  quién: agente
  contexto: La pestaña Online (`online_tab_draw`, `runtime/ui/online_ui.mm:38-130`) tiene Estado, Nombre, Crear, Unirse, lista de jugadores, Chat, "On screen" y "Combat". Las funciones de F4 necesitan sus controles.
  pasos:
    1. Junto a cada jugador de la lista: botones "Go to" (teletransporte) y "Give item…".
    2. "Give item…": lista de los objetos que se pueden dar, con cantidad (por ahora desde la lista de `items.c`; con T422, desde la bolsa real).
    3. Sección "Co-op": aceptación automática de teletransporte, enfriamiento (solo anfitrión), distancia de revivir (solo anfitrión), perfil de guardado en uso, color, forma, tamaño de texto.
    4. Sección "Combat": los tres botones de PvP en lugar del interruptor.
    5. Los ajustes del anfitrión aparecen deshabilitados para el invitado, como ya hace el código con `collide`/`nopause`/`revive` (`:113-127`).
    6. Usa los widgets `opt_*`; todo alcanzable con mando.
  hecho cuando: cada función de F4 se puede usar sin teclado.
  no hagas: no edites `runtime/ui/menu.mm` ni `runtime/settings.*` (los ajustes van a `online.ini`); no llames al juego ni hagas E/S desde el dibujo.
  modelo sugerido: medio

- [ ] T436 [entrada] Atajos: ping, chat rápido, aceptar/rechazar, revivir
  files: runtime/hle/bindings.c, runtime/input_bind.h, runtime/hle/input.c (solo si es imprescindible), docs/ACTIONS.md, tests (la prueba existente de acciones)
  verify: `make test-actions` sale con 0 y `make runtime-objs` sin avisos
  depende de: T426, T427
  quién: agente
  contexto: El proyecto tiene un sistema de asignación de teclas y botones por acción (`runtime/hle/bindings.c`, tablas en `runtime/input_bind.h`, documentación en `docs/ACTIONS.md`, prueba `make test-actions`). El modo en línea solo añadió dos variables globales en `runtime/hle/input.c:1203-1208` (botones que ve el juego y bloqueo de A).
  pasos:
    1. Lee `docs/ACTIONS.md` y cómo está definida una acción del runtime que no pertenece al juego (por ejemplo la de abrir el menú o la cámara libre).
    2. Añade 4 acciones del runtime: "Ping", "Quick chat", "Accept request", "Decline request" (y "Revive partner" si T432 se hace), con valores por defecto que no choquen con controles del juego (propuesta: teclas G, T, Y, N; en mando, sin asignar por defecto).
    3. Expón `bool coop_action_pressed(int accion)` leído por el overlay.
    4. Se guardan en `savagewind.ini` por el mecanismo existente de `bind_settings_save`.
  hecho cuando: las acciones aparecen en la pantalla de Controles y se pueden reasignar.
  no hagas: no reordenes enumeraciones existentes (rompe ajustes guardados); no consumas botones que el juego necesita; si no encuentras un precedente claro de "acción del runtime", para y pregunta.
  modelo sugerido: medio

- [ ] T437 [i18n] Traducir los textos nuevos de F4 a los 13 idiomas
  files: runtime/ui/i18n/*.inc
  verify: `tools/venv/Scripts/python.exe tools/i18n_check.py` → `0 missing` en los 13 y sale con 0
  depende de: T423, T425-T429, T431, T434, T435, T436
  quién: agente (dueño único de los `.inc`)
  contexto: Cada carril dejó sus textos en inglés en `specs/fases/F4-funciones-coop/textos-nuevos.txt`. La clave de traducción es el texto inglés: `T("English", "traducción")` en cada `runtime/ui/i18n/<idioma>.inc`.
  pasos:
    1. `tools/i18n_check.py --missing <idioma>` para cada uno de los 13.
    2. Añade las líneas al bloque `// Online (co-op)` del final de cada fichero.
    3. Conserva `%s`/`%d` en el mismo orden; las 8 frases de chat rápido deben ser cortas y naturales en cada idioma.
  hecho cuando: `0 missing` y capturas en español, japonés y hebreo.
  no hagas: no cambies la cadena inglesa; no reformatees líneas existentes.
  modelo sugerido: bajo

- [ ] T438 [P] [pruebas] Modos de bot y escenarios de F4
  files: tools/online_bot.c, tools/nettest/scenarios/*.sh
  verify: `sh tools/nettest/run.sh all` sale con 0 con los escenarios nuevos incluidos
  depende de: T409
  quién: agente
  contexto: El bot (ampliado en F3) no conoce los mensajes de F4. Las tareas T411, T421, T426 y T434 necesitan modos concretos.
  pasos:
    1. `BOT_FLAGS_ALL=1` (QUEST con todos los bits a 1 desde el primer envío), `BOT_FLAGS_ONE=<índice>,<s>` (activa un bit a los s segundos).
    2. `BOT_GIVE=<objeto>,<n>` (emisor de la transacción), `BOT_GIVE_DROP=<paso>` (corta la conexión tras ese paso).
    3. `BOT_PVP_CHEAT=1` (envía PVP_DMG y PVP_HIT imposibles).
    4. `BOT_TP=1` (pide teletransporte cada 20 s y acepta los que recibe).
    5. `BOT_FLOOD=mark,<hz>`.
    6. Escenarios: `flags_all_ones`, `flags_one`, `give_states`, `pvp_cheat`, `mark_rate`.
  hecho cuando: `all` pasa.
  no hagas: no copies estructuras de cable a mano; no cambies el comportamiento por defecto del bot.
  modelo sugerido: medio

- [ ] T439 [integración] Registrar los módulos en el fotograma, objetivos de prueba y variables
  files: el orquestador de fotograma de `online_actors` (`rt_online_frame`), Makefile (bloque de pruebas del final), runtime/launch_flags_table.c (regenerado)
  verify: `make test-online test-coop` sale con 0 y `make test-launch-flags` sale con 0
  depende de: T411, T416, T419, T421, T431, T434
  quién: agente integrador
  contexto: Tras F2, `rt_online_frame` es una lista de llamadas con el orden escrito. Cada función de F4 necesita su llamada por fotograma en un sitio concreto del orden, y su limpieza al salir de sesión y en `RT_ON_RESET`.
  pasos:
    1. Añade las llamadas por fotograma de: seguimiento de suelo y teletransporte, transacción de objetos, política de revivir, PvP, avisos. Ninguna antes de comprobar que el Link local existe y no está cargando.
    2. Registra la limpieza de cada módulo en el mecanismo de F2 (`oa_register_forget` o equivalente) y en `RT_ON_RESET`.
    3. Makefile: objetivo `test-coop` con todas las pruebas `tests/test_coop_*.c`.
    4. Regenera `runtime/launch_flags_table.c` con `tools/gen_launch_flags.py`.
  hecho cuando: los `verify` pasan y una sesión de 10 minutos no deja estado tras "Leave" (segunda sesión limpia).
  no hagas: no cambies el orden de las llamadas existentes; no edites a mano la tabla generada; no añadas reglas en mitad del Makefile.
  modelo sugerido: alto

- [ ] T440 [P] [docs] Guía del jugador: cómo se juega en cooperativo
  files: docs/coop-jugar.md
  verify: `git diff --check` sin errores; cada botón citado existe (`grep` en `runtime/ui/online_ui.mm`)
  depende de: T435
  quién: agente
  contexto: `docs/ONLINE.md` es un diario de ingeniería en inglés. No hay ninguna explicación para el jugador de las reglas del cooperativo.
  pasos:
    1. En español llano: de quién es el mundo; qué se guarda y dónde (perfil cooperativo, copias de seguridad y cómo restaurar una); qué recompensas recibe cada uno (tabla del ADR).
    2. Cómo usar: ir con el compañero, dar objetos, pings, chat rápido, revivir, PvP.
    3. Limitaciones conocidas: santuarios y Bestias juntos, momentos de historia y otros mapas llegan en F5.
  hecho cuando: el dueño la lee y no tiene preguntas.
  no hagas: no incluyas rutas absolutas ni nombres de usuario; no prometas lo que es de F5.
  modelo sugerido: bajo

- [ ] T441 [cierre] Recorrido de extremo a extremo y bitácora de F4
  files: specs/bitacora/F4-cierre.md
  verify: la bitácora tiene OK o FALLO con evidencia para cada criterio S1-S15 de `spec.md`
  depende de: todas las anteriores (T422 y T432 solo si su investigación fue viable)
  quién: dueño (un agente prepara las órdenes)
  contexto: Un cambio cuenta como hecho cuando se ha visto funcionar en el juego. F4 escribe en guardados: todas las pruebas se hacen con copias.
  pasos:
    1. Sigue `verificacion.md` de esta carpeta entera.
    2. Repite el recorrido de 30 minutos de F1.
    3. Rellena la plantilla de bitácora.
  hecho cuando: todos los criterios están en OK o cada FALLO tiene tarea abierta.
  no hagas: no uses tu guardado real; no marques OK sin evidencia.
  modelo sugerido: medio
