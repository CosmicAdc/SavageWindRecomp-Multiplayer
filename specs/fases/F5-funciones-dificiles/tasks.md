# F5 — Funciones difíciles: tareas

Cada tarea está escrita para ejecutarse **sola**, por un modelo que no ha leído nada más. Si una tarea te obliga a
adivinar, para y pregunta: no inventes. En esta fase **se trabaja un bloque cada vez**; `[P]` solo marca tareas que
no tocan ficheros compartidos y tienen una prueba automática.

## Convención

```
- [ ] T5xx [P] [área] descripción            ([P] = se puede hacer en paralelo; sin [P] = en serie)
  files:           ficheros que la tarea puede crear o modificar (y solo esos)
  verify:          orden que demuestra que funciona (sale con 0), o evidencia exigida
  depende de:      tareas o fases que deben estar fusionadas antes
  quién:           dueño (la persona) / agente
  contexto:        qué hace el código hoy, con fichero:línea, y por qué hace falta el cambio
  pasos:           lista numerada y concreta
  hecho cuando:    resultado observable
  no hagas:        trampas de esta tarea
  modelo sugerido: alto / medio / bajo
```

`alto` = exige razonar sobre concurrencia, memoria del juego, seguridad o ingeniería inversa.

Las tareas `[investigación]` dicen la **pregunta exacta** y la **forma de la respuesta**. Su resultado es un
documento, no código de producción.

## Reglas que valen para todas las tareas (léelas una vez)

- **ADR-006 manda sobre los pasos de cada tarea.** El subagente solo escribe código y documentos, hace commit y marca
  todo como "NO verificado". **No compila, no ejecuta pruebas, no lanza el juego ni Docker, no descarga dependencias,
  no genera patrones ni regenera el juego (`make gen`, `make -j8`) y no hace push.** Todo paso que diga "ejecuta",
  "mide", "compila", "lanza", "regenera" o "genera el patrón" (incluidos los `verify` y las capturas) lo hace **el
  orquestador** o el dueño, de uno en uno, y anota la salida en `specs/bitacora/`. El subagente solo puede usar `grep`
  y `git diff`.
- Repositorio: el fork, rama creada desde `coop/main`. Rutas siempre **relativas a la raíz del repo**. Nunca rutas
  absolutas ni nombres de usuario.
- Las líneas `fichero:línea` son del PR #294 original (HEAD `350c5f8`). `runtime/online_actors.c` se partió en F2 en
  módulos `runtime/oa_*.c`: busca cada función con `grep -rn "<nombre>(" runtime/`.
- **Nunca inventes direcciones, desplazamientos de estructura ni firmas de funciones del juego.** Solo valen las que
  ya están en el código o en la bitácora firmada de una tarea de investigación.
- Ingeniería inversa: solo con el Ghidra del dueño y su propia copia del juego (`tools/ghidra/README.md`,
  `tools/ghidra/gq.py`). No copies de proyectos de descompilación. No pegues desensamblado ni pseudocódigo en el
  repo: describe con tus palabras.
- Una dirección nueva entra como **sitio por patrón** en `tools/game_sites.py` (se genera con
  `tools/game_sites.py pattern <main> <dirección> <clase>`), nunca como literal en un `.c`.
- Tocar `tools/game_sites.py` o `tools/patches.py` cambia `GAMELIB_ID`: los dos jugadores regeneran (`make gen`) y
  recompilan el juego (~15 min). Agrupa cambios; un solo lote abierto a la vez.
- No edites `generated/`, `extracted/`, `online_mod/` ni `third_party/`. No subas ficheros del juego, guardados ni
  capturas que contengan arte del juego fuera de `build/`.
- No cambies estructuras de cable ni `PROTOCOL` salvo en T502 y T530 (dueño del protocolo).
- Al juego solo se le llama desde el hilo del juego. No bloquees ese hilo ni el de traducción del renderizador
  (`nvn-queue`): nada de red, ficheros ni esperas ahí.
- Cambios en `runtime/nvn/vulkan/`: lee antes el modelo de sincronización en `runtime/nvn/vulkan/vk.h`; no llames a
  `vkCmd*` directamente desde el hilo de traducción; no destruyas imágenes en uso (usa `vk_defer_image` /
  `vk_defer_view`).
- Todo lo que llega por red es hostil: longitud, rango, terminador, `isfinite`, nombres contra lista permitida.
- Estado por arranque del juego: `RT_ON_RESET` (`runtime/hle.h`); no guardes punteros del juego entre reinicios.
- Cualquier cambio de comportamiento va tras una opción **apagada por defecto**.
- Texto visible: en inglés en un `.mm` de `runtime/ui/`, envuelto con `tr()`, y añadido a
  `specs/fases/F5-funciones-dificiles/textos-nuevos.txt`.
- Pruebas con guardados: siempre copias.
- Comprobación mínima de cualquier cambio en `runtime/`: `make runtime-objs` sin avisos nuevos.
- Hecho = `verify` pasa **y** hay entrada en `specs/bitacora/`.
- Las respuestas del dueño (`specs/decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`,
  `ADR-008-respuestas-del-dueno-ronda-2.md`, `ADR-009-respuestas-del-dueno-ronda-3.md`,
  `ADR-011-respuestas-del-dueno-ronda-4.md`) mandan sobre cualquier tarea. Lo que sale de ellas va marcado `[DUEÑO]`. El
  límite es de 2 jugadores por ahora: diseña para N, prueba con 2 (ADR-007, decisión 12). De la ronda 4 valen aquí:
  si cae el anfitrión, la sesión termina sin traspaso (decisión 32); el anfitrión es siempre el mismo (29); saltarse una
  escena compartida, cada uno la suya, en una versión futura (42); un cambio que obligue a recompilar sombreadores solo
  con aprobación del dueño tras presentar el coste (43); menús que no pausan y colisión activada, apagable por el
  anfitrión (44, 45).

## Orden de los bloques

Los bloques están en el **orden de trabajo** de `plan.md` §10, no en orden alfabético: 0 → **C (santuarios
juntos)** → B → H → E → A → G → F → D → Cierre. `[DUEÑO]` (ADR-008, decisión 20): pasarse los santuarios juntos es
el objetivo prioritario y va antes que el tinte 3D (A) y que la historia (E). Los identificadores de tarea no han
cambiado: por eso no van correlativos al leer el fichero de arriba abajo (T540 es la primera tarea del bloque C).
Las letras 14a/14b/14c son las de `casos-de-uso/CU-14-…md` (14a = historia con los dos presentes, 14b = santuarios
y Bestias juntos, 14c = teletransporte entre mapas); ante la duda, cita por el nombre de la sección.

---

## Bloque 0 — Decisión y protocolo

- [x] T501 [decisión] Qué es un "momento de historia" y qué exige — RESUELTA (ADR-007, decisiones 3, 4 y 6; ADR-009, decisión 21)
  Ya no hay nada que decidir ni que redactar: el dueño contestó en las rondas de respuestas. No se crea
  `specs/decisiones/ADR-momentos-de-historia.md`; donde otra tarea diga "el ADR de T501", lee
  `specs/decisiones/ADR-007-respuestas-del-dueno-ronda-1.md` y `ADR-009-respuestas-del-dueno-ronda-3.md`.
  Reglas `[DUEÑO]` que usan T518, T520, T521 y T523:
    1. Momentos de historia = lista corta: inicio (piedra y primera torre), final de cada Bestia Divina, recuerdos y jefes finales. Los santuarios y su monje **no** (ADR-007, decisión 3).
    2. "Presente" = mismo mapa y a menos de 30 m. Si falta uno, el juego avisa y ofrece teletransportarlo (ADR-007, decisión 4).
    3. El anfitrión puede continuar solo de inmediato, con un diálogo de confirmación. Sin temporizador (ADR-007, decisión 6).
    4. Versión 1: basta con estar allí; uno ve la escena y el otro recibe el progreso. Verla a la vez queda como mejora posterior, sin tareas activas (ADR-009, decisión 21).

- [ ] T502 [protocolo] Cambio de cable de F5 (primer salto)
  files: runtime/online.h, runtime/online.c (solo `PROTOCOL`, mensajes y estructuras), runtime/online_authority.c, la cabecera compartida con el bot, tools/online_bot.c, tests/test_online_wire.c, tests/test_online_authority.c, specs/contratos/protocolo.md
  verify: `make test-online` sale con 0 y `sh tools/nettest/run.sh smoke` sale con 0
  depende de: F4 completa
  quién: agente designado como **dueño del protocolo**
  contexto: F4 dejó el protocolo con mensajes cooperativos y campos de color, forma y vida en STATE. F5 necesita unos pocos campos más; se definen de una vez con manejadores vacíos.
  pasos:
    1. Sube `PROTOCOL` al siguiente número libre; regístralo en `specs/contratos/protocolo.md`.
    2. STATE: en `coop_flags` añade bit 2 "esperando a su compañero para un evento" y bit 3 "tinte 3D activado".
    3. Dos mensajes distintos, para que la autoridad dependa del **tipo** y no del contenido: STORY (emisor → todos): nombre de evento `char[40]`, estado `u8` (quiere empezar, empezó, terminó); y STORY_ALONE (**solo del anfitrión** → todos): nombre de evento `char[40]`, "el anfitrión continúa solo" (ADR-007, decisión 6). Anótalos en la tabla de autoridad: STORY_ALONE de un no anfitrión se descarta.
    4. Mensaje STATE_MIN (S→C, lo genera el servidor): ranura, posición `float[3]`, mapa `char[40]`, indicadores. Lo usará el filtrado por interés. **Un cliente nunca puede enviarlo**: el servidor lo descarta si llega de un cliente.
    5. Tabla de autoridad y límites de tasa para STORY y STORY_ALONE (2/s). Validación al recibir (regla 9): longitud exacta, nombre terminado y acotado (contra la lista de `coop_story_table.inc` cuando T518 la haya creado), estado dentro de rango; para STATE_MIN en el cliente, posición finita y acotada, mapa en formato estricto y ranura `< ONLINE_MAX`.
    6. `_Static_assert` de tamaños; actualiza el bot; pruebas de la tabla de autoridad (`tests/test_online_authority.c`) para STORY, STORY_ALONE y STATE_MIN.
  hecho cuando: juego y bot se conectan con la versión nueva y `protocolo.md` está al día.
  no hagas: no implementes lógica; no cambies la versión más de una vez; el STATE por bloques no va aquí (es T530).
  modelo sugerido: alto

## Bloque C — Santuarios juntos (CU-14) · prioritario

`[DUEÑO]` (ADR-008, decisión 20): que los dos puedan pasarse los santuarios juntos es el objetivo de fondo. Este
bloque es el primero de la fase. Hacer un santuario juntos es posible, no obligatorio: el monje no exige a los dos
(ADR-007, decisión 3).

- [~] T540 [investigación] Qué pasa hoy cuando los dos entran al mismo santuario
  reclamada: agente Sonnet ag-t540 (integrada, adelantada) — lectura del código y diseño de T465 (T465a-e) en la bitácora; FALTA la tabla "visto en el juego" (pruebas T540-0..5) y 4 preguntas al dueño
  files: specs/bitacora/F5-T540-santuario-a-duo-hoy.md
  verify: la bitácora contiene la tabla pedida en el paso 6, rellena con lo visto en los dos juegos, las órdenes usadas y la sección "lo que no se comprobó"
  depende de: F1 (dos juegos funcionando en un PC)
  quién: dueño (ejecuta los dos juegos) + agente (prepara las órdenes y ordena el resultado)
  contexto: El dueño quiere que los dos puedan pasarse los santuarios juntos (ADR-008, decisión 20). Hoy, según la documentación del PR, quien está en un santuario o en una Bestia Divina **no aparece** en el mundo de los demás (`docs/ONLINE.md`, sección "Players on other maps"), y la tabla de huecos de `specs/mapa/07-pr294-integracion-juego.md` (§7, fila "Santuarios y Bestias Divinas juntos") dice: PARCIAL, "No probado", prioridad alta. La misma documentación dice que los que están en el mismo santuario se ven y que sus bolas y cajas se envían como objetos del mundo (`docs/ONLINE.md:996-997`). Nada de esto se ha ejecutado en este proyecto: antes de planificar la matriz (T514) y los arreglos hay que verlo.
  pasos:
    1. Pregunta exacta: con dos instancias en un PC, ¿qué ocurre hoy cuando los dos jugadores entran al mismo santuario? ¿Se ven dentro? ¿Qué ve cada uno mientras solo uno está dentro? ¿Qué pasa al completarlo y al salir?
    2. Montaje: dos juegos en un PC como en `verificacion.md` §0.2, cada uno con una **copia** de guardado situada junto a un santuario sin completar. Elige el más simple que tengas a mano (una "bendición" o uno de un solo mecanismo). Al santuario se entra andando: `teleport` y `goto` no cambian de mapa.
    3. Fase 1, uno dentro y otro fuera: entra solo A. Anota en B: ¿desaparece el Link de A?, ¿qué muestra la lista de jugadores?, ¿hay algún aviso? Anota en A lo mismo respecto a B.
    4. Fase 2, los dos dentro: entra B. Anota en los dos juegos: ¿se ven?, ¿cuánto tarda en aparecer el otro?, ¿se mueve con normalidad?, ¿se ve su equipo?
    5. Fase 3, jugar: si hay objetos o mecanismos, muévelos uno y luego el otro (activa `SAVAGEWIND_ONLINE_MOVER_LOG=1`). Uno habla con el monje: ¿quién recibe el orbe?, ¿qué ve el otro?, ¿puede el otro hablar con el monje después? Salid los dos: ¿se vuelven a ver en el campo?
    6. Forma de la respuesta: tabla (fase | qué se hizo | visto en A | visto en B | ¿igual en los dos? | cuelgue o error, con la última línea del registro), un párrafo de conclusión ("hoy funciona / funciona con estos fallos / no funciona porque …") y una lista de lo que T514 debe mirar con más cuidado.
    7. Repite las fases 2 y 3 una segunda vez para saber si lo visto es estable.
  hecho cuando: la bitácora dice, con lo visto y no con lo leído, qué funciona hoy y qué no al jugar un santuario a dúo, y T514 puede empezar con esa lista.
  no hagas: no uses el guardado real; no cambies código de producción en esta tarea; no des por visto lo que solo dice la documentación (sepáralo en "hipótesis frente a hechos"); no subas capturas con arte del juego; no intentes arreglar nada aquí.
  modelo sugerido: medio

- [ ] T514 [pruebas] Matriz de santuarios jugados a dúo
  files: specs/bitacora/F5-T514-matriz-santuarios.md
  verify: la bitácora contiene la tabla completa de 8 santuarios con las columnas pedidas
  depende de: T540, F1 (arreglo `has_body` en mecanismos), F4
  quién: dueño (un agente prepara la lista y las órdenes de `swctl`)
  contexto: Jugadores en el mismo santuario se ven y sus bolas y cajas se envían como objetos del mundo (`docs/ONLINE.md:996-997`). Los mecanismos se sincronizan (`movers_update`, `runtime/online_actors.c:4787-4980`) pero solo se probó moviendo a mano el ascensor de una entrada; "a lift ridden for real and a Divine Beast's parts aren't tested yet" (`docs/ONLINE.md:984-986`).
  pasos:
    1. Lee la bitácora de T540 y empieza por lo que allí quedó marcado. Elige 8 santuarios: 2 con ascensor o plataformas móviles, 2 con bolas o cajas e interruptores, 2 con aparato de giroscopio o mecanismo giratorio, 1 de combate, 1 de "bendición".
    2. Para cada uno, los dos jugadores entran andando y lo completan. Activa `SAVAGEWIND_ONLINE_MOVER_LOG=1`.
    3. Anota por santuario: ¿se ven?; ¿cada mecanismo se mueve igual en los dos juegos?; ¿los objetos coinciden?; ¿quién recibe el orbe? (debe llegar a los dos, siempre: ADR-007, decisión 2); fps; cuelgues (con la última línea del registro).
    4. Forma de la respuesta: tabla y una lista priorizada de fallos, cada uno con cómo reproducirlo.
  hecho cuando: hay lista de fallos reproducibles (o "ninguno").
  no hagas: no uses el guardado real; no marques como fallo lo que el diseño no sincroniza a propósito (el estado interno del rompecabezas): anótalo como límite; no marques como fallo que uno pueda completar el santuario sin el otro: un santuario no es un momento de historia (ADR-007, decisión 3).
  modelo sugerido: medio

- [ ] T541 [decisión] ADR: enmienda de la regla 11 para ENEMIES por mapa
  files: specs/decisiones/ADR-enmienda-regla-11-enemigos-por-mapa.md
  verify: el fichero existe, enmienda la regla 11 de `specs/constitution.md` con el texto exacto nuevo, define cómo se acota el mapa declarado (paso 3) y lleva la línea `Decidido por: <nombre>, <fecha>`; `git diff --check` sin errores
  depende de: T514 (debe confirmar el problema)
  quién: dueño (un agente redacta el borrador)
  contexto: La regla 11 de la constitución dice que ENEMIES solo lo origina el anfitrión de la partida. T515 quiere que, en un mapa donde no está el anfitrión, lo origine "el dueño de la lista de ese mapa". Eso cambia la regla, y la constitución exige el ADR **antes** del código.
  pasos:
    1. Redacta el texto nuevo de la regla 11: ENEMIES lo origina el anfitrión si está en ese mapa; si no, el jugador de menor ranura presente en él; los flags, el botín y la hora siguen siendo solo del anfitrión.
    2. Propón cómo se acota el mapa que declara un cliente: el servidor toma el mapa del último STATE **ya validado** (formato estricto, longitud < 40, contra la lista de mapas conocidos); un mapa desconocido o nuevo no da autoridad; la autoridad cubre solo ENEMIES **de ese mapa** y solo se reenvía a los jugadores que el servidor ve en ese mapa.
    3. Lista los riesgos (un invitado que miente sobre su mapa para originar ENEMIES) y cómo los acota el paso 2.
    4. El dueño revisa y firma.
  hecho cuando: el ADR está firmado; T515 puede empezar.
  no hagas: no decidas por el dueño; no cambies la constitución en esta tarea (el ADR la enmienda cuando se firme, con el texto exacto).
  modelo sugerido: alto

- [ ] T515 [mundo] Enemigos compartidos en un mapa donde no está el anfitrión
  files: el módulo de enemigos de `online_actors`, runtime/online.c (encaminamiento de ENEMIES por mapa), runtime/online_authority.c, tests/test_online_authority.c
  verify: `make test-online` sale con 0 y recorrido manual: dos invitados (o invitado + bot) en un santuario de combate sin el anfitrión ven al mismo enemigo con la misma vida
  depende de: T514 (debe confirmar el problema), T502, T541 (ADR firmado que enmienda la regla 11; sin él no se escribe código)
  quién: agente escribe; orquestador y dueño ejecutan el recorrido
  contexto: La lista de enemigos compartidos la construye el juego del anfitrión con los enemigos de **su** mundo (`runtime/online_actors.c:1987-2636`), y un jugador en otro mapa "has none of its enemies" (`docs/ONLINE.md:406-407`; `can_own`, `runtime/online_actors.c:2367-2370`). Por lectura del código, en un mapa sin el anfitrión cada invitado simula sus propios enemigos; T514 debe haberlo confirmado.
  pasos:
    1. Si T514 no confirmó el problema, cierra esta tarea como "no aplica".
    2. Define "dueño de lista de un mapa": el anfitrión si está en ese mapa; si no, el jugador de menor ranura presente en él.
    3. El servidor conoce el mapa de cada jugador por su último STATE **ya validado**; acepta ENEMIES del dueño de lista de cada mapa y lo reenvía solo a los jugadores de ese mapa, acotando el mapa declarado exactamente como dice el ADR de T541 (formato estricto, mapa conocido, autoridad solo sobre ENEMIES de ese mapa).
    4. El módulo de enemigos usa "soy dueño de lista de mi mapa" donde hoy usa "soy anfitrión" para construir y enviar la lista.
    5. Al cambiar el dueño (alguien entra o sale del mapa), la lista se reconstruye; prueba que no deja enemigos duplicados.
  hecho cuando: los `verify` pasan y el campo abierto se comporta como antes.
  no hagas: no cambies la autoridad sobre flags, botín ni hora (siguen siendo del anfitrión); no confíes en el mapa que declara un cliente para darle autoridad sobre otro mapa (solo vale lo que fije el ADR de T541); no escribas código antes de que T541 esté firmada.
  modelo sugerido: alto

## Bloque B — Teletransporte entre mapas (CU-14)

- [ ] T511 [investigación] Ghidra: cambiar de mapa en caliente
  files: specs/bitacora/F5-T511-cambio-de-mapa.md
  verify: la bitácora contiene la respuesta en la forma pedida y una prueba en el juego (desde el campo, entrar por código en un santuario concreto 5 veces sin cuelgue), o "no resuelto" con lo que se miró
  depende de: F0
  quién: dueño
  contexto: El teletransporte del runtime solo escribe la posición de Link dentro del mapa cargado (`runtime/control_tp.c`); "a shrine's or a Divine Beast's needs walking in" (`docs/ONLINE.md:747-748`). El runtime sabe leer el tipo y el nombre del mapa actual con dos funciones del juego (`FN_MAP_TYPE`, `FN_MAP_NAME`, `runtime/online_actors.c:1472-1473`), pero no pedir un cambio.
  pasos:
    1. Pregunta exacta: ¿qué función del juego inicia el cambio a otro mapa (la que se ejecuta al entrar en un santuario o salir de él) y qué recibe (nombre o tipo de mapa, punto de entrada)?
    2. Punto de partida: los llamadores de las dos funciones de lectura de mapa, y el evento de entrada a un santuario.
    3. Forma de la respuesta: sitio(s) con su línea `Site(...)`, descripción en palabras de cada argumento, precondiciones (hilo del juego, no estar en evento), y resultado de la prueba.
  hecho cuando: T513 puede usar el camino 1, o queda claro que hay que usar el camino 2.
  no hagas: no pegues desensamblado; no pruebes con el guardado real.
  modelo sugerido: alto

- [ ] T512 [investigación] Spike: cambiar de mapa recargando el guardado con la posición reescrita
  files: specs/bitacora/F5-T512-recarga-dirigida.md
  verify: `grep -c '^| [1-4] |' specs/bitacora/F5-T512-recarga-dirigida.md` da 4 (una fila por pregunta, con tiempos medidos en 5 repeticiones)
  depende de: F1
  quién: agente redacta; orquestador y dueño ejecutan las pruebas y las medidas
  contexto: El runtime ya reescribe en memoria la posición y el mapa guardados cuando el juego lee el guardado (`runtime/services/test_warp.c`, activado con `SAVAGEWIND_TEST_WARP`); hoy solo al arrancar. Existe además el reinicio del juego dentro del proceso (`teardown`, `runtime/main.c:150-170`; `SAVAGEWIND_REBOOT_AT`).
  pasos:
    1. Pregunta 1: ¿se puede activar el mecanismo de `test_warp` con un destino dado en tiempo de ejecución (no por variable de entorno) y que se aplique en la siguiente lectura del guardado?
    2. Pregunta 2: ¿qué formas hay de provocar esa lectura sin ingeniería inversa? (reinicio en caliente + "Continuar" con entrada guionizada; otras). ¿Cuánto tarda cada una?
    3. Pregunta 3: ¿se pierde progreso no guardado? ¿Qué pasa con la sesión de red durante la recarga? **Pregunta 4 (ADR-011, decisión 32):** si quien recarga es el **anfitrión**, la recarga corta su servidor y la sesión termina sin traspaso: ¿qué ve el invitado y cuánto tarda en poder reintentar? El sitio reservado de 10 minutos (ADR-008, decisión 18) vale solo para el invitado.
    4. Forma de la respuesta: tabla (método | funciona | segundos | qué se pierde) y recomendación.
  hecho cuando: T513 puede usar el camino 2 con datos.
  no hagas: no escribas en el fichero de guardado en disco; no cambies código de producción en esta tarea.
  modelo sugerido: alto

- [ ] T513 [reglas] Teletransporte al compañero en otro mapa
  files: runtime/coop_tp.c, tests/test_coop_tp.c, el módulo de reglas de `online_actors`, runtime/services/test_warp.c (solo si se usa el camino 2), tools/game_sites.py y runtime/game_sites.h (solo si se usa el camino 1; el sitio entra en el lote abierto del bloque H, no en un lote propio), specs/fases/F5-funciones-dificiles/textos-nuevos.txt
  verify: `build/test_coop_tp` sale con 0 y recorrido manual §CU-14c de `verificacion.md` (5 viajes campo → santuario y 5 santuario → campo sin cuelgue)
  depende de: T511 o T512 (el que resultó viable), F4 (teletransporte en el mismo mapa)
  quién: agente escribe; orquestador y dueño ejecutan el recorrido
  contexto: F4 dejó "ir con mi compañero" para el mismo mapa: `coop_tp_can` rechaza con el motivo "mapa distinto". El mapa de cada jugador llega en su STATE (`pose.map`). `[DUEÑO]` (ADR-008, decisión 20): el viaje a otro mapa está aceptado con pantalla de carga y guardado; sirve al objetivo de pasarse los santuarios juntos. Las reglas de F4 siguen valiendo aquí: ir con el compañero está siempre disponible, también en combate, con 30 s de espera entre usos, e invocar al otro requiere que acepte (ADR-008, decisión 16).
  pasos:
    1. En `coop_tp.c`: nuevo resultado "requiere cambio de mapa" en lugar de rechazo, y estado `LOADING_MAP`.
    2. Destino: la **entrada** del mapa del compañero, no su posición exacta.
    3. Camino 1 (si T511): llama a la función de cambio de mapa por `g_site` desde el hilo del juego, con las precondiciones de la bitácora.
    4. Camino 2 (si T512): fija el destino para `test_warp` y provoca la recarga por el método recomendado; avisa antes al jugador ("se cargará tu último guardado"; el aviso nace por identificador, con su literal en `coop_hud.mm` dentro de `tr()`, como en F4). El guardado del invitado es el de su perfil cooperativo (F4), nunca el de su partida individual (ADR-007, decisiones 1 y 5).
    5. **Si quien viaja por recarga es el anfitrión** (ADR-011, decisión 32: si cae el anfitrión, la sesión termina; sin traspaso): `[PROPUESTA]` (pendiente del dueño) no se carga sin avisar: se pide confirmar "Travelling will end the session for your partner" y la sesión termina limpiamente; el invitado recibe el aviso de fin de sesión y puede reintentar. El sitio reservado de 10 minutos solo vale para el invitado que viaja.
    6. Valida el nombre de mapa recibido contra un formato estricto (letras, dígitos, `/` y `_`, longitud < 40) antes de usarlo.
    7. Si ninguno está disponible: marca hacia la entrada del santuario (camino 3) y texto explicativo.
  hecho cuando: el recorrido pasa.
  no hagas: no uses un nombre de mapa de la red sin validar; no cambies de mapa durante un evento; no escribas direcciones en un `.c`.
  modelo sugerido: alto

## Bloque H — Menos direcciones fijas de 1.9.0

- [ ] T533 [P] [herramienta] Inventario de direcciones fijas
  files: tools/coop_addr_inventory.py, specs/bitacora/F5-T533-inventario-direcciones.md
  verify: `tools/venv/Scripts/python.exe tools/coop_addr_inventory.py --check 62` sale con 0 (encuentra las direcciones y las clasifica) y escribe `build/coop_addrs.csv`
  depende de: F2
  quién: agente
  contexto: Los módulos de `online_actors` usan 62 direcciones absolutas de la versión 1.9.0 escritas como literales (`#define FN_... 0x70...ull`, `#define P_... 0x70...ull` y algunas en línea): 47 funciones y 15 punteros de datos (mapa 07 §3.3 y §3.4). La regla del proyecto es localizarlas por patrón (`tools/game_sites.py`) y usarlas por `g_site`.
  pasos:
    1. El guion recorre `runtime/oa_*.c` (y `runtime/online_actors.c` si aún existe) buscando literales hexadecimales que empiecen por `0x70` y tengan 10 dígitos.
    2. Para cada uno: fichero, línea, nombre del `#define` (o "en línea"), clase supuesta (función si el nombre empieza por `FN_` o se usa como primer argumento de `call`; dato si empieza por `P_`), y número de usos.
    3. `--check N` falla si el número de direcciones distintas no es N (avisa de cambios).
    4. Escribe el CSV en `build/` y un resumen por módulo en la bitácora, **sin** copiar las direcciones al documento (ya están en el código).
  hecho cuando: el recuento coincide con 62 o la diferencia está explicada.
  no hagas: no modifiques los ficheros de runtime; no subas el CSV.
  modelo sugerido: bajo

- [ ] T534 [sitios] Lote 1: funciones del núcleo de actores a sitios por patrón
  files: tools/game_sites.py, runtime/game_sites.h, runtime/oa_core.c (o el módulo que tenga crear/pedir/borrar actores, matrices, huesos y handles)
  verify: `cd tools && ../tools/venv/Scripts/python.exe game_sites.py find ../extracted/exefs/main` lista cada sitio nuevo como `ok` y sale con 0; `tools/venv/Scripts/python.exe tools/test_game_sites.py` sale con 0; tras `make gen && make -j8`, el recorrido de 10 minutos de dos juegos funciona igual
  depende de: T533, F1 (puerta de versión)
  quién: agente escribe; orquestador y dueño generan los patrones, regeneran y recompilan (necesita `extracted/exefs/main` del dueño)
  contexto: Las funciones de crear, pedir, recoger y borrar actores, poner matrices, despertar, y leer o escribir huesos están como `#define FN_... 0x70...` al principio del fichero (`runtime/online_actors.c:30-57` y `:666-670` en el PR). Un patrón se genera con `tools/game_sites.py pattern <main> <dirección> code` (imprime una línea `Site(...)` lista para pegar) y debe encontrar exactamente un lugar.
  pasos:
    1. Para cada función del módulo (lista del CSV de T533): el orquestador o el dueño genera su patrón con la orden anterior y se lo pasa al agente.
    2. Pega la línea en `SITES` con un comentario de qué hace, añade la entrada a `KNOWN['1.9.0']`, y mete todas en un grupo nuevo de `OPTIONAL_GROUPS` llamado como el subsistema.
    3. Añade `X(<nombre>)` a `GAME_SITES_OPTIONAL` en `runtime/game_sites.h`.
    4. En el `.c`: sustituye cada literal por `g_site.<nombre>`, y haz que `online_actors_on()` (la función que decide si el modo en línea tiene actores) devuelva falso si alguno de este grupo es 0.
    5. El orquestador regenera y recompila (`make gen`, `make -j8`) y comprueba la línea `[gamelib] ... sites` del registro. El agente no ejecuta nada de esto.
  hecho cuando: los tres `verify` pasan y no queda ningún literal `0x70…` de función en ese módulo.
  no hagas: no escribas patrones a mano; no pongas los sitios como obligatorios (romperías el arranque sin modo en línea); no cambies el comportamiento; no abras otro lote hasta fusionar este.
  modelo sugerido: medio

- [ ] T535 [sitios] Lote 2: resto de funciones a sitios por patrón
  files: tools/game_sites.py, runtime/game_sites.h, los demás módulos `runtime/oa_*.c` con direcciones de función
  verify: los mismos tres de T534, más `tools/venv/Scripts/python.exe tools/coop_addr_inventory.py` mostrando 0 direcciones de función
  depende de: T534
  quién: agente escribe; orquestador y dueño generan, regeneran y recompilan
  contexto: Quedan las funciones de efectos y sonido, datos de juego y flags, eventos, bolsa, runas, física de cuerpos, mapa, retirar objetos y las cuatro o cinco que están en línea en el bloque de "menús sin pausa" (mapa 07 §3.3).
  pasos:
    1. Mismo procedimiento que T534, con un grupo opcional por subsistema (efectos, flags, eventos, runas, mundo, pausa).
    2. Cada subsistema comprueba su grupo al empezar su trabajo por fotograma y, si falta, no hace nada (y lo registra una vez).
    3. Las direcciones que solo aparecen en comentarios como referencia ("as 0x… does") no se convierten: son documentación.
  hecho cuando: los `verify` pasan.
  no hagas: no conviertas punteros de datos aquí (es T536); no elimines los comentarios que explican qué hace cada función.
  modelo sugerido: medio

- [ ] T536 [sitios] Lote 3: punteros de datos
  files: tools/game_sites.py, runtime/game_sites.h, los módulos `runtime/oa_*.c` con `P_...`, specs/bitacora/F5-T536-punteros-de-datos.md
  verify: los tres de T534; `coop_addr_inventory.py` muestra 0 direcciones, o las que queden están listadas en la bitácora con el motivo
  depende de: T535
  quién: dueño (referencias cruzadas en Ghidra) + agente
  contexto: Hay 15 punteros a datos o a la GOT con dirección fija (creador de actores, gestores de eventos y de datos de juego, objeto de la runa Magnesis, lista de cámaras, etc.; mapa 07 §3.4). Un dato no tiene patrón propio: el sitio se define sobre una **instrucción que lo referencia** (clases `ref` o `got` de `tools/game_sites.py`).
  pasos:
    1. Para cada puntero, el dueño busca con `tools/ghidra/gq.py xrefs <dirección>` una instrucción que lo cargue dentro de una función estable.
    2. Genera el patrón: `tools/game_sites.py pattern <main> <dirección de la instrucción> ref` (o `got`).
    3. Comprueba que el valor que imprime la herramienta (`# -> <valor>`) es el puntero esperado.
    4. Añade sitio, `KNOWN`, grupo opcional del subsistema y `GAME_SITES_OPTIONAL`; sustituye el literal por `g_site.<nombre>`.
    5. Si para alguno no hay referencia utilizable, déjalo como literal dentro de la puerta de versión y anótalo en la bitácora.
  hecho cuando: los `verify` pasan.
  no hagas: no inventes la instrucción de referencia; no pegues desensamblado en el repo; no mezcles con cambios de comportamiento.
  modelo sugerido: alto

- [ ] T537 [limpieza] Código sin uso y desplazamientos en una cabecera
  files: los módulos `runtime/oa_*.c`, runtime/oa_offsets.h (nuevo)
  verify: `make runtime-objs` sin avisos; `grep -rn "FN_ARMOR_INIT\|FN_SPECIAL_FX_A\b" runtime/` no devuelve nada; recorrido de 10 minutos de dos juegos igual que antes
  depende de: T536
  quién: agente
  contexto: Dos funciones del juego están definidas y no se usan (`FN_ARMOR_INIT`, `FN_SPECIAL_FX_A`; mapa 07 §3.3) y queda código de la época en que se vestía la armadura real del juego. Además hay unos 67 desplazamientos de estructura (`+ 0x...`) repartidos como literales, todos de 1.9.0 (mapa 07 §3.5).
  pasos:
    1. Quita los dos `#define` sin uso y las ramas muertas señaladas en el mapa 07 §1.7 (comprueba con `grep` que no tienen llamadores).
    2. Crea `runtime/oa_offsets.h` con un nombre por desplazamiento (`OA_ACTOR_HASHID`, `OA_ACTOR_LIFE`, …) y el comentario que hoy tiene cada uno; sustituye los literales por los nombres. **Mismos valores**, sin cambiar ninguno.
    3. Una sola aserción de versión documentada en esa cabecera: estos desplazamientos solo valen para 1.9.0.
  hecho cuando: los `verify` pasan y el diff no cambia ningún valor numérico (revísalo con `git diff --stat` y una lectura).
  no hagas: no "corrijas" desplazamientos; no cambies tipos; no toques `tools/game_sites.py` (esta tarea no cambia `GAMELIB_ID`).
  modelo sugerido: medio

## Bloque E — Momentos de historia con los dos presentes (CU-14)

- [ ] T518 [investigación] Catálogo de eventos de la lista del dueño
  files: specs/bitacora/F5-T518-catalogo-de-eventos.md, runtime/coop_story_table.inc (tabla de datos, generada a mano desde el catálogo)
  verify: la bitácora contiene la tabla; `runtime/coop_story_table.inc` compila incluido en un `.c` de prueba (`clang -fsyntax-only`)
  depende de: T501 (resuelta)
  quién: dueño (juega y recoge el registro) + agente (ordena)
  contexto: El runtime registra el nombre de cada evento que empieza en una línea `[event] <nombre>` (`runtime/hle/framepace.c`, función `framepace_event_name`). No hay ninguna tabla de qué eventos son "de historia"; el código solo conoce los cinco del pedestal y las torres (`runtime/online_actors.c:1625-1633`).
  pasos:
    1. Para cada momento de la lista corta `[DUEÑO]` (ADR-007, decisión 3: piedra y primera torre; final de cada Bestia Divina; recuerdos; jefes finales), el dueño lo juega con una copia de guardado y guarda las líneas `[event]`. Los santuarios y su monje no entran en el catálogo.
    2. Por evento anota: nombre, cómo arranca (botón A sobre un objeto / entrar en una zona / tras otro evento), mapa, y posición de Link al arrancar (`swctl pos`).
    3. Escribe la tabla `{nombre, tipo de arranque, mapa, x, y, z, radio}` en `coop_story_table.inc`.
  hecho cuando: cada momento de la lista tiene al menos una fila.
  no hagas: no copies nombres de eventos de internet ni de proyectos de descompilación: solo los que salgan de tu registro; no incluyas diálogos ni texto del juego.
  modelo sugerido: bajo

- [ ] T519 [investigación] Ghidra: punto por el que pasan los inicios de evento
  files: specs/bitacora/F5-T519-arranque-de-eventos.md
  verify: la bitácora contiene la respuesta y la prueba (aplazar 10 s un evento de zona, 10 repeticiones, sin cuelgue), o "no viable"
  depende de: T518
  quién: dueño
  contexto: Para exigir presencia en eventos que arrancan al entrar en una zona hay que poder **aplazar** el arranque. El PR ya sabe pedir un evento al gestor (`start_event`, `runtime/online_actors.c:3699-3714`, con funciones del juego de dirección fija) y leer el evento activo, pero no interceptar las peticiones del propio juego. Lanzar eventos fuera de contexto puede colgar (`docs/ONLINE.md:751-752`).
  pasos:
    1. Pregunta exacta: ¿existe una función por la que pasan todas las peticiones de inicio de evento (por botón y por zona), que reciba el nombre del evento, y cuyo rechazo o aplazamiento deje al juego en un estado sano (la zona vuelve a pedirlo más tarde)?
    2. Punto de partida: los llamadores de la función de petición que ya usa `start_event`.
    3. Forma de la respuesta: sitio con su línea `Site(...)`, descripción en palabras, qué devuelve al rechazar, y resultado de las 10 repeticiones con dos eventos de zona distintos.
  hecho cuando: T522 tiene diseño, o queda "no viable" y la puerta se limita a eventos de botón.
  no hagas: no pegues desensamblado; no pruebes con eventos de la historia principal en tu guardado real.
  modelo sugerido: alto

- [ ] T520 [P] [lógica] Tabla de historia y puerta de presencia
  files: runtime/coop_story.c, runtime/coop_story.h, tests/test_coop_story.c
  verify: `clang -Wall -Iruntime tests/test_coop_story.c runtime/coop_story.c -lm -o build/test_coop_story && build/test_coop_story` sale con 0
  depende de: T501 (resuelta), T518
  quién: agente
  contexto: No hay lógica de presencia. El bloqueo actual solo mira si *otro* jugador está en un pedestal a menos de 30 m (`runtime/online_actors.c:5981-6001`). Reglas `[DUEÑO]`: presente = mismo mapa y a menos de 30 m (ADR-007, decisión 4); el anfitrión puede continuar solo de inmediato tras confirmarlo, sin temporizador (ADR-007, decisión 6); solo se exige presencia, no que los dos vean la escena (ADR-009, decisión 21).
  pasos:
    1. `const CoopStoryEvent *coop_story_find(const char *nombre);` busca en la tabla de `coop_story_table.inc`.
    2. `const CoopStoryEvent *coop_story_near(const char *mapa, const float pos[3]);` devuelve el evento de arranque por botón cuyo punto está a menos de su radio.
    3. `int coop_story_gate(const CoopStoryEvent *e, const CoopStoryWho *yo, const CoopStoryWho *otros, int n, const CoopStoryRules *r);` → `ALLOW` o `WAIT`. Reglas: presentes = mismo mapa y a menos de `r->dist` (30 m por defecto); `r->host_confirmed` (el anfitrión confirmó "continuar solo" para este evento) → `ALLOW` aunque falte el compañero, esté lejos, en otro mapa o desconectado. Sin esa confirmación, un compañero ausente o desconectado es `WAIT`. La función no lleva ningún reloj: no hay temporizador.
    4. Escríbela para N compañeros (todos los conectados deben estar presentes) y pruébala con 2 jugadores (ADR-007, decisión 12).
    5. Pruebas: evento fuera de tabla → `ALLOW`; compañero lejos → `WAIT`; otro mapa → `WAIT`; compañero desconectado sin confirmación → `WAIT`; cualquiera de los anteriores con `host_confirmed` → `ALLOW`; `host_confirmed` puesto por quien no es anfitrión → se ignora; posiciones no finitas → tratadas como lejos.
  hecho cuando: la prueba pasa; sin cabeceras del juego.
  no hagas: no leas memoria del juego; no uses variables globales mutables.
  modelo sugerido: bajo

- [ ] T521 [reglas] Puerta de presencia para eventos que arrancan con un botón
  files: el módulo de reglas de `online_actors` (bloque del bloqueo de A), runtime/online.c (STORY, STORY_ALONE y bits de STATE), specs/fases/F5-funciones-dificiles/textos-nuevos.txt
  verify: `build/test_coop_story` sale con 0 y, con dos juegos (o juego + bot) y 3 eventos de botón de la tabla, el registro muestra por cada uno `[story] <evento> WAIT` con el compañero lejos, `[story] <evento> ALLOW` con el compañero cerca y `[story] <evento> HOST_CONTINUE` tras confirmar el anfitrión, y el otro juego registra el bit "esperando" de STATE (la parte visible —aviso, oferta de traer, diálogo— la verifica T523)
  depende de: T502, T520
  quién: agente escribe; orquestador y dueño ejecutan el recorrido
  contexto: El bloque de `runtime/online_actors.c:5981-6001` pone `g_online_block_a = 1` cuando otro jugador con el bit `ONLINE_E_DEMO` está a menos de 30 m y este juego no está en un evento; `runtime/hle/input.c:1207-1208` quita entonces el botón A de lo que ve el juego.
  pasos:
    1. Revisa el bloqueo actual contra la lista corta `[DUEÑO]` (ADR-007, decisión 3) y anota el resultado en la bitácora: hoy cubre el pedestal de la piedra y las torres. Consérvalo tal cual como protección contra empezar lo mismo a la vez (no exige presencia). La exigencia de presencia nueva sale **solo** de la tabla de T518: piedra, primera torre, final de cada Bestia Divina, recuerdos y jefes finales. Ninguna fila de santuario.
    2. Añade: cada fotograma, `coop_story_near(mapa, posición del Link local)`; si devuelve un evento y `coop_story_gate` dice `WAIT`, activa el bloqueo de A, pon el bit "esperando" en STATE, escribe una línea de registro `[story] <evento> WAIT` (una vez por cambio de estado) y envía STORY al compañero (una vez cada 5 s). Las líneas `ALLOW` y `HOST_CONTINUE` se registran igual.
    3. No bloquees si este juego ya está en un evento (`framepace_in_event`).
    4. Continuar solo `[DUEÑO]` (ADR-007, decisión 6): si este juego es el anfitrión y confirma el diálogo de T523, marca `host_confirmed` para ese evento, quita el bloqueo en ese mismo fotograma y envía STORY_ALONE (T502). Sin cuenta atrás ni espera mínima. La confirmación vale para ese evento; se borra al terminar o al alejarse del punto.
    5. Cuando el evento empieza y termina, envía STORY para que el otro lo sepa. El progreso llega al compañero por los flags, como hoy; **no** se lanza la escena en su juego (ADR-009, decisión 21).
    6. Opción de sesión del anfitrión "Story needs both players" (apagada por defecto).
  hecho cuando: con el compañero lejos, A no inicia el evento y el registro lo muestra (`WAIT`); con el compañero cerca, sí (`ALLOW`); el anfitrión, tras confirmar, lo inicia solo de inmediato (`HOST_CONTINUE`). El aviso en pantalla y el diálogo los entrega T523.
  no hagas: no bloquees A lejos de los puntos de la tabla (rompería recoger objetos y hablar); no añadas puntos de santuario a la tabla; no pongas temporizadores; no aceptes "continuar solo" de quien no es anfitrión; no lances eventos en el otro juego; no dejes el bloqueo puesto al salir de sesión.
  modelo sugerido: alto

- [ ] T522 [reglas] Puerta de presencia para eventos que arrancan por zona
  files: tools/game_sites.py, tools/patches.py, runtime/game_sites.h, el módulo de reglas de `online_actors`
  verify: `tools/venv/Scripts/python.exe tools/test_game_sites.py` sale con 0; recorrido manual §CU-14a para 2 eventos de zona (10 repeticiones cada uno, sin cuelgue)
  depende de: T519 (respuesta viable), T521
  quién: agente + dueño
  contexto: La bitácora de T519 describe la función por la que pasan los inicios de evento y cómo aplazar uno de forma segura.
  pasos:
    1. Lee la bitácora de T519. Si dice "no viable", **no hagas esta tarea**.
    2. Añade el sitio y el parche en un grupo opcional nuevo **dentro del lote abierto del bloque H** (no abras un lote propio; regla 14). El parche **afirma con `assert` la instrucción original** que sustituye y reproduce su efecto (regla 13); llama a `rt_coop_story_event(nombre)` y, si devuelve "esperar", hace lo que T519 describe para aplazar.
    3. `rt_coop_story_event` copia el nombre con longitud acotada, consulta una decisión **ya calculada** por el hilo del juego (variable atómica por evento cercano) y devuelve.
    4. Agrupa el cambio de `game_sites.py`/`patches.py` con cualquier otro lote pendiente.
  hecho cuando: los `verify` pasan.
  no hagas: no hagas trabajo pesado ni tomes cerrojos dentro de la función llamada desde el parche (puede ejecutarse en cualquier hilo del juego); no escribas direcciones en un `.c`; no escribas un parche sin el `assert` de la instrucción original.
  modelo sugerido: alto

- [ ] T523 [ui] "Esperando a tu compañero", oferta de teletransporte y confirmación para continuar solo
  files: runtime/ui/coop_hud.mm, specs/fases/F5-funciones-dificiles/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos; `tools/venv/Scripts/python.exe tools/i18n_check.py --missing es` lista los textos de `textos-nuevos.txt`; captura de las dos pantallas durante una espera, con las filas E.5, E.5b y E.10b de `verificacion.md`
  depende de: T521
  quién: agente
  contexto: F4 dejó en `runtime/ui/coop_hud.mm` avisos en pantalla y marcas en el mundo, y el teletransporte al compañero con invocación aceptada. La puerta de historia necesita decir por qué A no responde y dónde está el punto. `[DUEÑO]`: si falta uno, el juego avisa y ofrece teletransportarlo (ADR-007, decisión 4); invocar al otro requiere que acepte (ADR-008, decisión 16); el anfitrión puede continuar solo de inmediato con una confirmación (ADR-007, decisión 6).
  pasos:
    1. En quien espera: aviso fijo "Waiting for your partner" mientras dure el bit, con la acción "Bring your partner here", que lanza la invocación de F4 (el compañero debe aceptarla).
    2. En el compañero: aviso "<nombre> is waiting for you" y una marca de tipo "aquí" en la posición del que espera, con flecha de borde.
    3. En el compañero: botón rápido "Go to" si el teletransporte está disponible (si está en otro mapa, lo que haya entregado T513).
    4. Solo en el anfitrión: acción "Continue without your partner", que abre un diálogo de confirmación (sí / no, con "no" preseleccionado). Al confirmar, avisa a T521; el compañero ve "<nombre> continued the story without you".
  hecho cuando: la captura lo muestra, el aviso desaparece al empezar el evento y el invitado no ve la acción de continuar solo (E.5, E.5b y E.10b de `verificacion.md` quedan verificadas aquí, no en T521).
  no hagas: no dependas solo del color; no llames al juego desde el dibujo; no pongas cuenta atrás en el diálogo; no teletransportes al compañero sin su aceptación.
  modelo sugerido: bajo

- [ ] T542 [reglas] Regla de avance: el anfitrión no acepta flags de un momento de historia sin presencia o sin su confirmación
  files: runtime/coop_story.c, runtime/coop_story.h, tests/test_coop_story.c, el módulo de flags de `online_actors`, specs/bitacora/F5-T542-regla-de-avance.md
  verify: `clang -Wall -Iruntime tests/test_coop_story.c runtime/coop_story.c -lm -o build/test_coop_story && build/test_coop_story` sale con 0 (casos nuevos de `coop_story_flag_ok`) y el orquestador ejecuta el recorrido de la fila E.13 de `verificacion.md`
  depende de: T520, T521
  quién: agente escribe; orquestador y dueño ejecutan el recorrido
  contexto: `casos-de-uso/CU-14-…md` (punto 6 de las reglas y CA-5) pide una **regla de avance**: los flags de un momento de la tabla solo se aceptan en el anfitrión si en ese instante los dos estaban presentes, o si el anfitrión confirmó continuar solo. Sin ella, un invitado que avanza la historia sin presencia lo vuelca en el mundo del anfitrión. T521 solo bloquea el botón A; no filtra los flags que llegan.
  pasos:
    1. En `coop_story.c`: `bool coop_story_flag_ok(const CoopStoryEvent *e, const CoopStoryWho *yo, const CoopStoryWho *otros, int n, const CoopStoryRules *r);` (misma lógica de presencia que `coop_story_gate`, para decidir si se acepta un flag de ese momento). Pruebas: presentes → acepta; falta uno sin confirmación → rechaza; con `host_confirmed` → acepta; flag que no es de la tabla → acepta (no es asunto de esta regla).
    2. En el módulo de flags del anfitrión, donde se aplican los flags aceptados de un invitado (T411 de F4), consulta `coop_story_flag_ok`; si rechaza, no aplica el flag y registra una línea.
    3. Solo en el anfitrión; la tabla de la que sale el nombre del evento es la de T518.
  hecho cuando: la prueba pasa y, con el compañero lejos y sin confirmación, un flag de un momento de la tabla enviado por un bot no cambia nada en el anfitrión.
  no hagas: no cambies las reglas de T411; no filtres flags que no sean de un momento de la tabla; no pongas temporizadores.
  modelo sugerido: medio

## Bloque A — Tinte 3D del segundo Link (CU-13)

- [ ] T503 [investigación] Qué dibujos pintan al compañero
  files: specs/bitacora/F5-T503-dibujos-del-companero.md
  verify: la bitácora contiene la tabla pedida, rellena a partir de registros reales, y las órdenes usadas
  depende de: F1 (dos juegos funcionando)
  quién: agente prepara la tabla; orquestador y dueño ejecutan el juego y recogen los registros
  contexto: El renderizador no sabe a qué actor pertenece un dibujo. Tiene herramientas de depuración: `SAVAGEWIND_DEBUG_PICK=<x>,<y>` lista qué dibujos cambiaron ese píxel con su hash de sombreador, y `SAVAGEWIND_DEBUG_FRAME=<f>` vuelca cada pase a `build/pass_NNN.png` y registra cada dibujo en líneas `[draw]` (código en `runtime/nvn/vulkan/exec.c`, función `debug_draw` y bloque de `DEBUG_PICK`).
  pasos:
    1. Pregunta exacta: ¿qué dibujos pintan los píxeles del Link del compañero, en qué pases y con qué hash de sombreador de fragmento? ¿Son los mismos hashes que usa el Link local?
    2. Monta dos juegos con el compañero quieto delante de la cámara (o un juego y el bot). Usa una ventana de tamaño fijo.
    3. Lanza con `SAVAGEWIND_DEBUG_FRAME` en un fotograma dentro del mundo y `SAVAGEWIND_DEBUG_PICK` en un píxel del cuerpo del compañero, otro de su ropa y otro del Link local.
    4. Forma de la respuesta: tabla (píxel | pase | hash de sombreador | objetivos de color que escribe | ¿también lo usa el Link local?) y un párrafo de conclusión.
  hecho cuando: T504 y T505 saben qué pases y sombreadores mirar.
  no hagas: no subas las imágenes de `build/` al repo; no cambies código de producción.
  modelo sugerido: medio

- [ ] T504 [investigación] Dónde está el color base en el G-buffer
  files: specs/bitacora/F5-T504-objetivos-gbuffer.md, runtime/nvn/vulkan/exec.c (solo el cambio temporal del paso 3, que no se fusiona)
  verify: la bitácora responde con una tabla por objetivo de color y la prueba descrita en el paso 3
  depende de: T503
  quién: agente prepara el cambio temporal; orquestador y dueño lo compilan y capturan la pantalla
  contexto: El tinte que ya emite el traductor solo modifica el **objetivo de color 0** (`runtime/nvn/maxwell/shader.h:89`; mezcla en `runtime/nvn/maxwell/translate.c:2424-2425`). El juego usa sombreado diferido y la documentación del proyecto dice que el color base está en el objetivo 1; lo que contiene el objetivo 0 no está comprobado.
  pasos:
    1. Pregunta exacta: en el pase donde se dibuja el cuerpo de un personaje, ¿qué guarda cada objetivo de color (0, 1, 2…)? ¿Teñir el objetivo 0 cambia el color final del personaje?
    2. Con `SAVAGEWIND_DEBUG_FRAME` mira las imágenes de cada objetivo de ese pase; `SAVAGEWIND_DEBUG_PICK_RT=<i>` elige el objetivo.
    3. Prueba directa (cambio temporal, no se fusiona): en `exec.c`, donde está `float tint[8] = {0};` (línea 9161 en el PR), pon un tinte rojo con peso 1 **solo** para los hashes de sombreador hallados en T503, y captura la pantalla.
    4. Forma de la respuesta: tabla (objetivo | contenido aparente | efecto de teñirlo) y veredicto: "el tinte existente sirve" / "hace falta teñir el objetivo N" / "no sirve".
  hecho cuando: hay veredicto con una captura descrita.
  no hagas: no fusiones el cambio temporal; no modifiques `translate.c` en esta tarea.
  modelo sugerido: alto

- [ ] T505 [investigación] Distinguir los dibujos del compañero de los del Link local
  files: specs/bitacora/F5-T505-senal-del-companero.md
  verify: la bitácora contiene la respuesta y, si hay señal, 3 fotogramas de prueba donde identifica bien todos los dibujos del compañero y ninguno del Link local
  depende de: T503
  quién: agente prepara la comparación; orquestador y dueño ejecutan el juego
  contexto: El Link del compañero es el actor `OnlinePlayer` (modelos de `PauseMenuPlayer`, `tools/online_mod.py:277-286`) y su ropa son actores `OnlineArmor_*`. Puede que usen los mismos recursos que el Link local o copias distintas. El renderizador ya calcula por dibujo una identidad para los vectores de movimiento (`runtime/nvn/vulkan/mvec.c` y `mvec.h`: sombreadores, flujos de vértices, dirección de índices) y su comentario dice que los sombreadores de escena tienen la matriz de mundo del objeto en un bloque de constantes.
  pasos:
    1. Pregunta exacta: ¿existe algún dato disponible en `draw_` (direcciones de texturas, de buffers de vértices o de constantes; contenido de un bloque de constantes) que sea distinto para el compañero y estable entre fotogramas?
    2. Con los registros `[draw]` de T503, compara lado a lado un dibujo del compañero y su equivalente del Link local.
    3. Si las direcciones de recursos coinciden, prueba con `SAVAGEWIND_LOG_FS_CB=<hash>:<k>:<primero>:<n>` si algún bloque de constantes lleva una posición que coincida con la del compañero (la conoces por `swctl pos` del otro juego).
    4. Forma de la respuesta: "señal encontrada: <descripción y cómo comprobarla por dibujo>" o "sin señal", con evidencia.
  hecho cuando: el ADR T508 puede elegir camino.
  no hagas: no asumas que un modelo con huesos tiene una única matriz de mundo; no cambies código de producción.
  modelo sugerido: alto

- [ ] T506 [investigación] Ghidra: función que dibuja un modelo y origen de su buffer de huesos
  files: specs/bitacora/F5-T506-dibujo-por-modelo.md
  verify: la bitácora contiene la respuesta en la forma pedida, o "no encontrado" con lo que se miró
  depende de: T505 (solo se hace si T505 respondió "sin señal")
  quién: dueño (un agente prepara las consultas de `gq.py`)
  contexto: El proyecto ya envuelve funciones del juego con marcadores que el renderizador ve: `nvn_ui_scope` inserta un registro solo-host alrededor del dibujo de un layout de interfaz (`runtime/nvn/cmdbuf.c`, `runtime/ui_layout.c`). Para modelos 3D no se sabe si existe una función "dibuja este modelo" que envuelva todos sus dibujos en todos los pases.
  pasos:
    1. Pregunta exacta: ¿hay una función del juego, llamada una vez por modelo y por pase, dentro de la cual se emiten todos los comandos de dibujo de ese modelo? ¿Recibe el modelo o el actor como argumento?
    2. Pregunta alternativa: ¿de qué campo del modelo sale la dirección del buffer de matrices de huesos que acaba enlazado en el dibujo?
    3. Punto de partida: las funciones de modelo que ya usa el PR (leer y escribir huesos, ocultar materiales) y sus vecinas.
    4. Forma de la respuesta: nombre de sitio propuesto, línea `Site(...)`, descripción en palabras de los argumentos, y una prueba (un marcador temporal que solo se activa para el compañero).
  hecho cuando: hay un sitio utilizable o un "no encontrado" razonado.
  no hagas: no pegues desensamblado; no uses nombres de proyectos de descompilación.
  modelo sugerido: alto

- [ ] T507 [investigación] Alternativa por el juego: tinte de armadura y brillo de material
  files: specs/bitacora/F5-T507-tinte-por-el-juego.md, el módulo de marionetas de `online_actors` (solo la prueba temporal del paso 2, que no se fusiona)
  verify: la bitácora responde a las dos preguntas con capturas descritas
  depende de: F1
  quién: agente prepara la prueba temporal; orquestador y dueño la ejecutan
  contexto: El tinte de armadura de cada jugador ya viaja y se aplica al crear cada pieza con el parámetro `ArmorDyeColor` (`runtime/online_actors.c:6069-6077`), pero `docs/ONLINE.md:1003-1008` dice que no se ha visto en pantalla y que quizá haya que poner el color en los materiales. Existen además escritores de parámetros de material para el brillo de Magnesis y de Stasis (`FN_SPECIAL_FX_B`, `runtime/online_actors.c:1458`).
  pasos:
    1. Pregunta 1: con el anfitrión llevando una pieza teñida en la tienda de tintes, ¿la pieza `OnlineArmor_*` del otro juego se ve teñida?
    2. Pregunta 2: aplicar el brillo de material al modelo del compañero de forma continua, ¿da una señal visual aceptable y sin coste? (Prueba temporal: llamar a la función existente una vez por segundo sobre el Link del compañero.)
    3. Forma de la respuesta: sí/no para cada una, con descripción de lo visto y fps.
  hecho cuando: el ADR T508 sabe si existe una vía sin renderizador.
  no hagas: no fusiones la prueba temporal; no llames a esas funciones sobre actores que no sean la marioneta ya validada como viva en ese fotograma.
  modelo sugerido: medio

- [ ] T508 [decisión] ADR: cómo se tiñe (o no) el segundo Link
  files: specs/decisiones/ADR-tinte-3d.md
  verify: el fichero existe con el camino elegido (por datos / por ámbito / por el juego / no viable), la evidencia de T503-T507 y la firma del dueño
  depende de: T504, T505, T507 (y T506 si se hizo)
  quién: dueño (un agente redacta)
  contexto: Hay cuatro caminos descritos en `plan.md` §1. Cambiar el traductor de sombreadores invalida las cachés de todos los jugadores: `[DUEÑO]` (ADR-011, decisión 43) solo con aprobación del dueño, **presentando antes el coste** (caché de sombreadores invalidada y tiempo de recompilación que pagaría cada jugador). `[DUEÑO]` Lo que se busca es un tinte suave sobre todo el Link del compañero, con el Link propio normal (ADR-007, decisión 8). Si no es viable, el resultado final es nombre en color y marcador con forma (F4), y eso no bloquea el resto de F5 (ADR-009, decisión 22).
  pasos:
    1. Resume cada investigación en dos líneas.
    2. Para cada camino: viable sí/no, coste, riesgo.
    3. Si algún camino obliga a cambiar el traductor: presenta al dueño el coste de recompilar los sombreadores (paso 2) **antes** de recomendarlo y espera su aprobación expresa (ADR-011, decisión 43).
    4. Recomendación y decisión firmada.
  hecho cuando: T509 puede empezar, o el bloque A se cierra como "no viable" con nombre en color y marcador con forma como solución final (no hace falta otra consulta al dueño para eso).
  no hagas: no recomiendes cambiar el traductor sin decirlo claramente en la primera línea.
  modelo sugerido: alto

- [ ] T509 [render] Tinte por dibujo para el compañero
  files: runtime/nvn/vulkan/exec.c (una función nueva y una llamada), runtime/coop_tint.c, runtime/coop_tint.h, tests/test_coop_tint.c, el módulo de marionetas de `online_actors` (solo publicar las señales), specs/bitacora/F5-T509-tinte-medidas.md
  verify: `clang -Wall -Iruntime tests/test_coop_tint.c runtime/coop_tint.c -o build/test_coop_tint && build/test_coop_tint` sale con 0; y en el juego: captura con la opción apagada idéntica a la de antes (`cmp` de los dos ficheros) y captura con la opción encendida donde solo cambia el compañero
  depende de: T508 (camino "por datos" o "por ámbito"), T502
  quién: agente escribe; orquestador y dueño compilan, miden y capturan
  contexto: En `draw_` de `runtime/nvn/vulkan/exec.c` hay `float tint[8] = {0};` (línea 9161 en el PR) que se pasa al enlace 19 de cada dibujo; con peso 0 el sombreador no cambia nada. `exec.c` corre en el hilo de traducción, que limita los fps.
  pasos:
    1. `coop_tint.c`: estructura propia con mutex que el hilo del juego rellena cada fotograma con la "señal" de cada compañero (la que definió el ADR) y su color; función `bool coop_tint_lookup(const CoopDrawKey *k, float out[8])` sin reservas de memoria y O(nº de jugadores).
    2. Prueba unitaria de `coop_tint_lookup` con claves sintéticas.
    3. En `exec.c`: sustituye la inicialización a cero por una llamada a una función nueva `static void coop_draw_tint(float tint[8])` que construye la clave del dibujo en curso y llama a `coop_tint_lookup`; si la opción está apagada, devuelve ceros sin tomar el mutex.
    4. El hilo del juego publica las señales desde el módulo de marionetas; se limpian al salir de sesión y en `RT_ON_RESET`.
    5. El orquestador o el dueño miden fps y tiempo de fotograma antes y después en la misma escena (60 s) y lo anotan en la bitácora; el agente deja preparado el procedimiento.
  hecho cuando: los `verify` pasan y la medida está en bitácora.
  no hagas: no tiñas al Link local (ADR-007, decisión 8); no bloquees el hilo de traducción (usa `trylock` o una copia atómica; si no hay dato, no tiñas); no toques `translate.c`; no cambies nada más de `exec.c`; no leas memoria del juego desde `exec.c` para esto.
  modelo sugerido: alto

- [ ] T510 [ui] Opción de tinte 3D
  files: runtime/ui/online_ui.mm, runtime/online.c (clave de `online.ini` y bit en STATE), specs/fases/F5-funciones-dificiles/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos; captura de la pestaña con la opción; al activarla en un juego, el otro ve el bit en el STATE (registro)
  depende de: T509
  quién: agente
  contexto: Los ajustes de Online viven en `online.ini` (`online_settings_load`/`save` en `runtime/online.c`) y se muestran en `online_tab_draw` (`runtime/ui/online_ui.mm`). El tinte debe estar apagado por defecto.
  pasos:
    1. Clave `tint3d` (0/1, por defecto 0) en `online.ini`.
    2. Interruptor "Tint other players' models" en la sección "On screen", con descripción.
    3. El tinte usa el color de jugador de F4 y es **suave**: se mezcla con el color del modelo, no lo tapa. Solo se tiñe a los demás; el Link propio nunca `[DUEÑO]` (ADR-007, decisión 8).
  hecho cuando: la opción se guarda y se aplica sin reiniciar.
  no hagas: no añadas el ajuste a `runtime/settings.c`; no edites `runtime/ui/menu.mm`.
  modelo sugerido: bajo

## Bloque G — Más de 2 jugadores

- [ ] T526 [medidas] Ancho de banda con 2, 4 y 8 jugadores
  files: tools/nettest/measure_n.sh, tools/online_bot.c (solo el modo `BOT_SKEL`), specs/bitacora/F5-T526-ancho-de-banda-n.md
  verify: `sh tools/nettest/measure_n.sh` sale con 0 y escribe `build/nettest/n/resumen.csv` con filas para N = 2, 4, 8
  depende de: F3 (arnés de Docker)
  quién: agente escribe; orquestador ejecuta las medidas (Docker)
  contexto: Por cálculo, cada jugador sube unos 100 kB/s (STATE de 3375 bytes y SKEL de ~1,6 kB, ambos 20 veces por segundo) y el servidor reenvía cada flujo a los N−1 restantes: el total crece con N² (mapa 06 §2.5). No está medido. Además el bot no envía SKEL.
  pasos:
    1. Añade al bot (`tools/online_bot.c`, si falta) un modo `BOT_SKEL=1` que envíe un SKEL de tamaño real con datos sintéticos.
    2. (Orquestador, al ejecutar `measure_n.sh`.) Lanza el servidor y N bots (2, 4, 8) durante 120 s en dos disposiciones: todos juntos, y repartidos (cada uno a más de 1 km y la mitad en otro mapa).
    3. Mide bytes/s de entrada y salida en la interfaz del servidor y uso de CPU.
    4. Forma de la respuesta: CSV y tabla en bitácora (N | disposición | subida del servidor | bajada | CPU).
  hecho cuando: hay línea base para comparar T528, T530 y T531.
  no hagas: no extrapoles; mide los tres valores de N.
  modelo sugerido: medio

- [ ] T527 [P] [lógica] Filtro de interés y tasa por distancia
  files: runtime/coop_interest.c, runtime/coop_interest.h, tests/test_coop_interest.c
  verify: `clang -Wall -Iruntime tests/test_coop_interest.c runtime/coop_interest.c -lm -o build/test_coop_interest && build/test_coop_interest` sale con 0
  depende de: ninguna
  quién: agente
  contexto: El juego ya no dibuja a un jugador en otro mapa o a más de 400 m (`runtime/online_actors.c:6013-6018`), pero el servidor le reenvía igualmente su STATE y SKEL 20 veces por segundo.
  pasos:
    1. `int coop_interest_rate(const char *mapa_a, const float pos_a[3], const char *mapa_b, const float pos_b[3]);` devuelve envíos por segundo de A hacia B: 20 hasta 60 m, 10 hasta 200 m, 5 hasta 450 m, 0 más allá o en mapas distintos. Mapa vacío = desconocido = tratar como mismo mapa.
    2. Histéresis del 10 % en cada umbral (recibe la tasa anterior como argumento).
    3. `bool coop_interest_due(int rate, uint64_t ultimo_ns, uint64_t ahora_ns);`
    4. Pruebas: cada tramo, histéresis, posiciones no finitas (→ 20, no filtrar), mapas distintos.
  hecho cuando: la prueba pasa; sin sockets ni cabeceras del juego.
  no hagas: no uses variables globales; no leas el reloj dentro.
  modelo sugerido: bajo

- [ ] T528 [red] Aplicar el filtro de interés en el servidor
  files: runtime/online.c (reenvío de STATE y SKEL), tools/nettest/scenarios/interest.sh
  verify: `sh tools/nettest/run.sh interest` sale con 0 y `sh tools/nettest/measure_n.sh` muestra, con 4 bots repartidos, una subida del servidor ≤ 50 % de la línea base de T526
  depende de: T502, T526, T527
  quién: agente
  contexto: El servidor reenvía cada STATE y SKEL a todos los demás (`srv_message`, casos `M_STATE` y `M_SKEL` en `runtime/online.c`) y guarda el último STATE de cada cliente (`SClient.last`). Desde F3 cada cliente tiene una cola de salida con casillas reemplazables.
  pasos:
    1. Al recibir STATE de A, lee su posición y mapa (ya validados) y guárdalos por cliente.
    2. Para cada destinatario B: calcula `coop_interest_rate` y reenvía STATE y SKEL solo si `coop_interest_due`.
    3. Si la tasa es 0: envía un STATE_MIN de A a B una vez por segundo (para la lista de jugadores, las flechas y saber en qué mapa está).
    4. El cliente trata STATE_MIN como "jugador presente pero sin pose": actualiza posición y mapa, no crea su Link.
    5. Escenario `interest`: 2 bots a 1 km reciben ~1 mensaje por segundo el uno del otro; al acercarse a 30 m, ~20.
  hecho cuando: los `verify` pasan y con 2 jugadores juntos todo se ve igual que antes.
  no hagas: no filtres eventos fiables (CHAT, JOIN, LEAVE, MARK…); no confíes en una posición no finita para filtrar (envía a tasa completa); no cambies estructuras de cable.
  modelo sugerido: alto

- [ ] T529 [P] [lógica] Codificador de STATE por bloques
  files: runtime/coop_statecodec.c, runtime/coop_statecodec.h, tests/test_coop_statecodec.c
  verify: `clang -Wall -Iruntime tests/test_coop_statecodec.c runtime/coop_statecodec.c -o build/test_coop_statecodec && build/test_coop_statecodec` sale con 0 (incluye ida y vuelta de 10 000 estados aleatorios y decodificación de entradas corruptas sin fallo de memoria, compilado con `-fsanitize=address,undefined`)
  depende de: T502
  quién: agente
  contexto: El STATE es una estructura única de 3375 bytes que se envía entera 20 veces por segundo aunque nada cambie (`WireState`, `runtime/online.c`). La mayor parte (equipo, efectos, objetos, mecanismos, propiedades) cambia pocas veces por segundo.
  pasos:
    1. Divide la estructura de cable en un **núcleo** (posición, rumbo, indicadores, secuencia, hora: siempre) y hasta 16 **bloques** contiguos definidos por una tabla (desplazamiento, tamaño), sin reinterpretar campos.
    2. `int statecodec_encode(const uint8_t *nuevo, const uint8_t *ultimo_enviado, uint64_t ahora, uint8_t *out, int cap);` emite núcleo + máscara de 16 bits + solo los bloques que cambiaron; cada bloque se reenvía como mucho cada 2 s aunque no cambie.
    3. `int statecodec_decode(const uint8_t *in, int n, uint8_t *estado);` aplica sobre el último estado; valida máscara y longitudes; devuelve -1 ante cualquier incoherencia sin tocar `estado`.
    4. Mensaje completo (todos los bloques) cuando el emisor lo marque (al entrar un jugador).
    5. Pruebas: ida y vuelta, sin cambios (tamaño ≈ núcleo), todo cambia, entradas truncadas y aleatorias.
  hecho cuando: la prueba pasa con los sanitizadores y un estado sin cambios ocupa menos de 100 bytes.
  no hagas: no cambies el significado de ningún campo; no leas fuera de `n`; no uses memoria dinámica.
  modelo sugerido: alto

- [ ] T530 [protocolo] Enviar STATE por bloques (segundo salto de versión)
  files: runtime/online.h, runtime/online.c, la cabecera compartida con el bot, tools/online_bot.c, tests/test_online_wire.c, specs/contratos/protocolo.md
  verify: `make test-online` y `sh tools/nettest/run.sh all` salen con 0; `measure_n.sh` muestra con 4 bots juntos una subida del servidor ≤ 50 % de la de T528, y con `BOT_SKEL=1` la reducción de SKEL
  depende de: T528, T529, T531
  quién: agente designado como **dueño del protocolo**
  contexto: T529 dejó un codificador probado. El servidor guarda el último STATE de cada cliente para dárselo a quien entra y para el filtro de interés; con bloques debe reconstruir el estado completo por cliente.
  pasos:
    1. Sube `PROTOCOL` (segundo y último salto de F5) y regístralo.
    2. Cliente emisor: `statecodec_encode` contra lo último enviado; mensaje completo al conectar, al reanudar y cuando llega un JOIN.
    3. Servidor: decodifica sobre su copia por cliente (valida), y reenvía a cada destinatario codificando contra lo último que **ese destinatario** recibió de ese origen (necesario porque el filtro de interés hace que cada uno reciba ritmos distintos y porque la cola reemplazable descarta mensajes).
    4. Cliente receptor: `statecodec_decode` sobre su copia por jugador; si devuelve -1, descarta y pide completo.
    5. **SKEL en el mismo salto** (regla 15): el formato de SKEL por huesos movidos (máscara de huesos presentes + huesos) se define aquí, con el codificador que dejó T531, y no cambia después. El servidor y el cliente lo usan con las mismas reglas que STATE (reenvío codificado contra lo último que recibió cada destinatario).
    6. Actualiza el bot y las pruebas.
  hecho cuando: los `verify` pasan y dos juegos reales se ven igual que antes durante 10 minutos.
  no hagas: no reenvíes en el servidor los bloques tal cual llegan (un destinatario que se perdió un mensaje quedaría desincronizado); no cambies la versión más veces; no mezcles con otros cambios de cable.
  modelo sugerido: alto

- [ ] T531 [P] [lógica] SKEL: codificador de solo los huesos que cambian (sin tocar el cable)
  files: runtime/coop_skelcodec.c, runtime/coop_skelcodec.h, tests/test_coop_skelcodec.c
  verify: `clang -Wall -Iruntime tests/test_coop_skelcodec.c runtime/coop_skelcodec.c -o build/test_coop_skelcodec && build/test_coop_skelcodec` sale con 0 (la reducción medida con `measure_n.sh` y `BOT_SKEL=1` se comprueba en T530)
  depende de: T502
  quién: agente
  contexto: SKEL lleva 121 huesos (más los del arco) a 12 bytes por hueso, 20 veces por segundo: unos 30 kB/s por jugador (`SKEL_BONE_BYTES`, `runtime/online.c:53-56`; `docs/ONLINE.md:55-57`). `docs/ONLINE.md:1001` lo lista como pendiente: "only the bones that moved".
  pasos:
    1. Codificador **puro**, sin sockets ni cabeceras del juego y **sin cambiar el cable ni `PROTOCOL`** (el formato en el cable lo cablea T530, el único salto de F5): máscara de bits de huesos presentes + los huesos presentes, a partir de `skel_encode`/`skel_decode` de `runtime/online.c` (léelos, no los edites).
    2. Un hueso se envía si su rotación o traslación cuantizadas difieren de lo último enviado; cada hueso se reenvía al menos una vez por segundo; esqueleto completo al conectar y ante un JOIN.
    3. El receptor mantiene el último valor por hueso; valida la máscara contra el número de huesos y la longitud.
    4. Pruebas: ida y vuelta, nada cambia, todo cambia, mensaje truncado, máscara con bits de más.
  hecho cuando: la prueba pasa; T530 lo integra y la animación del compañero se ve igual (anotado en bitácora de T530).
  no hagas: no cambies la cuantización de un hueso; no aceptes índices de hueso fuera de rango; no edites `online.c`, `online.h`, el bot ni `protocolo.md` (son de T530, dueño del protocolo).
  modelo sugerido: alto

- [ ] T532 [reglas] Quitar los supuestos de "ranura 0" y de "solo dos"
  files: los módulos de reglas, enemigos, botín y marionetas de `online_actors`
  verify: sesión de 20 minutos con 2 juegos y 2 bots (4 jugadores) sin crash, con entrada en bitácora; `grep -n "pl\[0\]\|piece_update(c, 0" runtime/oa_*.c` no devuelve usos sin comentario que los justifique
  depende de: F3 (ranura del anfitrión garantizada y anunciada en WELCOME)
  quién: agente escribe; orquestador y dueño ejecutan los 20 minutos con 2 juegos y 2 bots
  contexto: `[DUEÑO]` (ADR-007, decisión 12): el límite sigue en 2 jugadores por ahora; esta tarea no lo sube, deja el código listo para N. Las reglas de sesión se leen de `pl[0]` (`runtime/online_actors.c:2838-2840`); las marionetas de enemigo, el botín, las fuentes de elemento y las pruebas usan el pool de piezas del jugador 0 (`piece_update(c, 0, ...)`, líneas 2231, 3044, 3058, 2281), de modo que `drop(c, 0, true)` les borra piezas y el pool de 24 se llena antes.
  pasos:
    1. Sustituye `pl[0]` por la ranura del anfitrión que da la capa de red.
    2. Crea un pool propio "del mundo" (índice aparte de los 8 de jugadores) para enemigos, botín y fuentes de elemento.
    3. Revisa cada bucle `for (i < ONLINE_MAX)` que elige "el otro jugador": debe funcionar con 3 o más.
    4. Prueba con 2 bots además de los 2 juegos (la lanza el orquestador).
  hecho cuando: los `verify` pasan.
  no hagas: no cambies el tamaño de `ONLINE_MAX`; no mezcles esta tarea con cambios de comportamiento.
  modelo sugerido: alto

## Bloque F — Colisión con el compañero

- [ ] T524 [investigación] ¿Puede la marioneta tener cuerpo físico?
  files: specs/bitacora/F5-T524-cuerpo-de-la-marioneta.md
  verify: la bitácora responde a las 5 preguntas con lo visto en el juego
  depende de: F1
  quién: agente prepara; orquestador y dueño ejecutan las pruebas en el juego
  contexto: El Link del compañero (`OnlinePlayer`) se genera con `tools/online_mod.py` a partir de los ficheros del usuario; su física se toma de `PauseMenuPlayer` quitándole las partes de contacto y de cuerpos rígidos (`tools/online_mod.py:277-286`). Hoy los jugadores no chocan: cada juego empuja a su Link 0,7 m fuera del otro (`players_collide`, `runtime/online_actors.c:2833-2872`). Las marionetas de enemigo con controlador de personaje "se quedan donde están" (`docs/ONLINE.md:576-577`).
  pasos:
    1. En una copia local de `tools/online_mod.py` (no se fusiona), genera `OnlinePlayer` conservando un cuerpo rígido simple, sin controlador de personaje.
    2. Pregunta 1: ¿el juego arranca y la marioneta se dibuja?
    3. Pregunta 2: ¿el Link local choca con ella? ¿Puede subirse encima?
    4. Pregunta 3: ¿el cuerpo sigue a la marioneta si se mueve con `FN_BODY_TO` cada fotograma (como los objetos del mundo), y `has_body()` lo reconoce?
    5. Pregunta 4: ¿los enemigos o la cámara reaccionan a ese cuerpo?
    6. Pregunta 5: ¿coste en fps?
    7. Forma de la respuesta: tabla (pregunta | sí/no | lo visto) y veredicto viable / no viable.
  hecho cuando: hay veredicto.
  no hagas: no subas `online_mod/` ni nada derivado del juego; no fusiones el generador modificado.
  modelo sugerido: alto

- [ ] T525 [reglas] Colisión con cuerpo físico
  files: tools/online_mod.py (incluido su número de versión), runtime/main.c (solo el número de versión del overlay que comprueba), el módulo de marionetas y el de reglas de `online_actors`, runtime/ui/online_ui.mm (solo el selector de modo de colisión del anfitrión), specs/fases/F5-funciones-dificiles/textos-nuevos.txt
  verify: recorrido manual §Colisión de `verificacion.md` (20 carreras contra el compañero: 0 traspasos; sesión de 20 min sin crash)
  depende de: T524 (veredicto viable)
  quién: agente + dueño
  contexto: T524 demostró que la marioneta puede llevar cuerpo. El generador marca su salida con un número de versión (`VERSION = 42` en `tools/online_mod.py`; el runtime busca `online_mod/v42` en `runtime/main.c:507`): al cambiar el generador hay que subirlo en los dos sitios para que el overlay se regenere en cada PC.
  pasos:
    1. Si T524 dijo "no viable", **no hagas esta tarea**.
    2. Lleva al generador el cambio de física validado; sube el número de versión en `online_mod.py` y en `main.c`.
    3. En el módulo de marionetas: mueve el cuerpo cada fotograma con la función ya usada para objetos, **después** de comprobar `has_body()` y que la marioneta sigue viva en ese fotograma.
    4. Opción del anfitrión: colisión por cuerpo / por empuje (la actual) / apagada. Por defecto, la actual.
    5. Con colisión por cuerpo activa, `players_collide` no empuja.
  hecho cuando: el recorrido pasa.
  no hagas: no llames a `FN_BODY_TO` sin `has_body()` (cuelgue documentado en el propio fichero); no olvides el número de versión; no subas nada de `online_mod/`.
  modelo sugerido: alto

## Bloque D — Bestias Divinas

- [ ] T516 [pruebas] Bestia Divina recorrida a dúo
  files: specs/bitacora/F5-T516-bestia-dentro.md
  verify: la bitácora contiene la tabla de partes móviles de una Bestia con el resultado en los dos juegos
  depende de: T514
  quién: dueño
  contexto: Las partes de una Bestia (`Remains*`) se tratan como mecanismos de nivel (su matriz y su AS), pero no se han probado dentro de una Bestia (`docs/ONLINE.md:1013-1014`).
  pasos:
    1. Los dos jugadores entran en una Bestia (la que el dueño tenga disponible en una copia de guardado).
    2. Mueve cada parte controlable desde el mapa de la Bestia. Activa `SAVAGEWIND_ONLINE_MOVER_LOG=1`.
    3. Anota por parte: ¿se mueve en el otro juego?, ¿con el mismo ángulo?, ¿los jugadores encima se mueven con ella?
    4. Forma de la respuesta: tabla y lista de fallos reproducibles.
  hecho cuando: hay tabla y lista.
  no hagas: no uses el guardado real.
  modelo sugerido: bajo

- [ ] T517 [investigación] Bestia vista en movimiento desde fuera
  files: specs/bitacora/F5-T517-bestia-desde-fuera.md
  verify: la bitácora responde a la pregunta con lo visto en los dos juegos
  depende de: T516
  quién: dueño + agente
  contexto: `docs/ONLINE.md:1013` lista como pendiente "A Divine Beast seen moving by the others while one is inside". Un jugador dentro está en otro mapa (`MainFieldDungeon/Remains…`) que el de quien mira desde el campo.
  pasos:
    1. Pregunta exacta: cuando un jugador mueve una parte desde dentro, ¿qué actor representa a la Bestia en el campo abierto del otro jugador, tiene HashId, y se mueve con el mecanismo actual de `movers` si se le envía?
    2. Usa `swctl actors Remains` en el juego de fuera para listar los actores candidatos.
    3. Forma de la respuesta: "se puede con el mecanismo actual cambiando X" / "necesita investigación con Ghidra: <pregunta concreta>" / "no viable".
  hecho cuando: hay una de las tres respuestas con evidencia.
  no hagas: no implementes nada en esta tarea.
  modelo sugerido: medio

## Cierre

- [ ] T538 [i18n] Traducir los textos nuevos de F5 a los 13 idiomas
  files: runtime/ui/i18n/*.inc
  verify: `tools/venv/Scripts/python.exe tools/i18n_check.py` → `0 missing` en los 13 y sale con 0
  depende de: T510, T513, T521, T523, T525 (cada una, o cerrada como "no aplica"/"no viable" con su ADR: la i18n de lo que sí se entregó no espera a lo que no)
  quién: agente (dueño único de los `.inc`)
  contexto: Cada tarea dejó sus textos en inglés en `specs/fases/F5-funciones-dificiles/textos-nuevos.txt`. La clave de traducción es el texto inglés: `T("English", "traducción")` en cada `runtime/ui/i18n/<idioma>.inc`.
  pasos:
    1. El orquestador ejecuta `tools/i18n_check.py --missing <idioma>` para cada uno y te pasa la lista.
    2. Añade las líneas al bloque `// Online (co-op)` de cada fichero.
    3. Conserva `%s`/`%d` en el mismo orden.
  hecho cuando: `0 missing`.
  no hagas: no cambies la cadena inglesa; no reformatees líneas existentes.
  modelo sugerido: bajo

- [ ] T539 [cierre] Cierre de cada bloque y bitácora de F5
  files: specs/bitacora/F5-cierre-<bloque>.md
  verify: por cada bloque (A-H), la bitácora tiene OK, FALLO o "no viable (ADR …)" para sus criterios de `spec.md` §5, con evidencia
  depende de: las tareas del bloque que se cierra
  quién: dueño (un agente prepara las órdenes)
  contexto: F5 se cierra bloque a bloque. Un bloque puede cerrarse como "no viable" si la investigación lo demuestra y hay un ADR con la alternativa.
  pasos:
    1. Sigue la sección del bloque en `verificacion.md`.
    2. Repite los recorridos de regresión de F1 y F4.
    3. Rellena la plantilla de bitácora.
  hecho cuando: el bloque está cerrado con evidencia.
  no hagas: no uses tu guardado real; no marques OK sin evidencia; no cierres como "no viable" sin ADR.
  modelo sugerido: medio
