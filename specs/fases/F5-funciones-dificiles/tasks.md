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
- Texto visible: en inglés en un `.mm` de `runtime/ui/` y añadido a
  `specs/fases/F5-funciones-dificiles/textos-nuevos.txt`.
- Pruebas con guardados: siempre copias.
- Comprobación mínima de cualquier cambio en `runtime/`: `make runtime-objs` sin avisos nuevos.
- Hecho = `verify` pasa **y** hay entrada en `specs/bitacora/`.

---

## Bloque 0 — Decisión y protocolo

- [ ] T501 [decisión] Qué es un "momento de historia" y qué exige
  files: specs/decisiones/ADR-momentos-de-historia.md
  verify: el fichero existe con la lista de eventos o categorías, las respuestas a P5-1, P5-2 y P5-3 de `spec.md` y la línea `Decidido por: <nombre>, <fecha>`
  depende de: ninguna
  quién: dueño (un agente redacta el borrador con opciones)
  contexto: El dueño decidió que "los momentos de historia necesitan a los dos jugadores presentes", pero no está definido cuáles. Hoy solo existe un bloqueo del botón A a 30 m de un jugador que está en el pedestal de la piedra o de una torre (`runtime/online_actors.c:5981-6001`), que evita empezar lo mismo a la vez pero no exige presencia.
  pasos:
    1. Borrador con tres niveles para elegir: (a) solo historia principal (recuerdos, entrada y jefe de cada Bestia, Espada Maestra, castillo y Ganon); (b) lo anterior más torres, santuarios y Grandes Hadas; (c) todo evento con escena.
    2. Para cada nivel, lo que implica: cuántos eventos, cuánto trabajo de catálogo, qué molesta al jugar.
    3. Preguntas: ¿se puede avanzar solo si el compañero no está conectado?; ¿qué distancia cuenta como presente?
    4. El dueño elige y firma.
  hecho cuando: el ADR está firmado; T518 y T520 pueden empezar.
  no hagas: no decidas por el dueño; no listes nombres internos de eventos que no hayas visto en el registro del juego o en el código del PR.
  modelo sugerido: medio

- [ ] T502 [protocolo] Cambio de cable de F5 (primer salto)
  files: runtime/online.h, runtime/online.c (solo `PROTOCOL`, mensajes y estructuras), runtime/online_authority.c, la cabecera compartida con el bot, tools/online_bot.c, tests/test_online_wire.c, specs/contratos/protocolo.md
  verify: `make test-online` sale con 0 y `sh tools/nettest/run.sh smoke` sale con 0
  depende de: F4 completa
  quién: agente designado como **dueño del protocolo**
  contexto: F4 dejó el protocolo con mensajes cooperativos y campos de color, forma y vida en STATE. F5 necesita unos pocos campos más; se definen de una vez con manejadores vacíos.
  pasos:
    1. Sube `PROTOCOL` al siguiente número libre; regístralo en `specs/contratos/protocolo.md`.
    2. STATE: en `coop_flags` añade bit 2 "esperando a su compañero para un evento" y bit 3 "tinte 3D activado".
    3. Mensaje STORY (emisor → todos): nombre de evento `char[40]`, estado `u8` (quiere empezar, empezó, terminó).
    4. Mensaje STATE_MIN (S→C, lo genera el servidor): ranura, posición `float[3]`, mapa `char[40]`, indicadores. Lo usará el filtrado por interés.
    5. Tabla de autoridad y límites de tasa para STORY (2/s).
    6. `_Static_assert` de tamaños; actualiza el bot.
  hecho cuando: juego y bot se conectan con la versión nueva y `protocolo.md` está al día.
  no hagas: no implementes lógica; no cambies la versión más de una vez; el STATE por bloques no va aquí (es T530).
  modelo sugerido: alto

## Bloque A — Tinte 3D del segundo Link (CU-13)

- [ ] T503 [investigación] Qué dibujos pintan al compañero
  files: specs/bitacora/F5-T503-dibujos-del-companero.md
  verify: la bitácora contiene la tabla pedida, rellena a partir de registros reales, y las órdenes usadas
  depende de: F1 (dos juegos funcionando)
  quién: agente + dueño (ejecutar el juego)
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
  files: specs/bitacora/F5-T504-objetivos-gbuffer.md
  verify: la bitácora responde con una tabla por objetivo de color y la prueba descrita en el paso 3
  depende de: T503
  quién: agente + dueño
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
  quién: agente + dueño
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
  files: specs/bitacora/F5-T507-tinte-por-el-juego.md
  verify: la bitácora responde a las dos preguntas con capturas descritas
  depende de: F1
  quién: agente + dueño
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
  contexto: Hay cuatro caminos descritos en `plan.md` §1. Cambiar el traductor de sombreadores invalida las cachés de todos los jugadores (pregunta P5-4).
  pasos:
    1. Resume cada investigación en dos líneas.
    2. Para cada camino: viable sí/no, coste, riesgo.
    3. Recomendación y decisión firmada.
  hecho cuando: T509 puede empezar, o el bloque A se cierra como "no viable".
  no hagas: no recomiendes cambiar el traductor sin decirlo claramente en la primera línea.
  modelo sugerido: alto

- [ ] T509 [render] Tinte por dibujo para el compañero
  files: runtime/nvn/vulkan/exec.c (una función nueva y una llamada), runtime/coop_tint.c, runtime/coop_tint.h, tests/test_coop_tint.c
  verify: `clang -Wall -Iruntime tests/test_coop_tint.c runtime/coop_tint.c -o build/test_coop_tint && build/test_coop_tint` sale con 0; y en el juego: captura con la opción apagada idéntica a la de antes (`cmp` de los dos ficheros) y captura con la opción encendida donde solo cambia el compañero
  depende de: T508 (camino "por datos" o "por ámbito"), T502
  quién: agente + dueño
  contexto: En `draw_` de `runtime/nvn/vulkan/exec.c` hay `float tint[8] = {0};` (línea 9161 en el PR) que se pasa al enlace 19 de cada dibujo; con peso 0 el sombreador no cambia nada. `exec.c` corre en el hilo de traducción, que limita los fps.
  pasos:
    1. `coop_tint.c`: estructura propia con mutex que el hilo del juego rellena cada fotograma con la "señal" de cada compañero (la que definió el ADR) y su color; función `bool coop_tint_lookup(const CoopDrawKey *k, float out[8])` sin reservas de memoria y O(nº de jugadores).
    2. Prueba unitaria de `coop_tint_lookup` con claves sintéticas.
    3. En `exec.c`: sustituye la inicialización a cero por una llamada a una función nueva `static void coop_draw_tint(float tint[8])` que construye la clave del dibujo en curso y llama a `coop_tint_lookup`; si la opción está apagada, devuelve ceros sin tomar el mutex.
    4. El hilo del juego publica las señales desde el módulo de marionetas; se limpian al salir de sesión y en `RT_ON_RESET`.
    5. Mide fps y tiempo de fotograma antes y después en la misma escena (60 s).
  hecho cuando: los `verify` pasan y la medida está en bitácora.
  no hagas: no bloquees el hilo de traducción (usa `trylock` o una copia atómica; si no hay dato, no tiñas); no toques `translate.c`; no cambies nada más de `exec.c`; no leas memoria del juego desde `exec.c` para esto.
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
    3. El tinte usa el color de jugador elegido en F4.
  hecho cuando: la opción se guarda y se aplica sin reiniciar.
  no hagas: no añadas el ajuste a `runtime/settings.c`; no edites `runtime/ui/menu.mm`.
  modelo sugerido: bajo

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
  verify: la bitácora responde a las 3 preguntas con tiempos medidos en 5 repeticiones
  depende de: F1
  quién: agente + dueño
  contexto: El runtime ya reescribe en memoria la posición y el mapa guardados cuando el juego lee el guardado (`runtime/services/test_warp.c`, activado con `SAVAGEWIND_TEST_WARP`); hoy solo al arrancar. Existe además el reinicio del juego dentro del proceso (`teardown`, `runtime/main.c:148-168`; `SAVAGEWIND_REBOOT_AT`).
  pasos:
    1. Pregunta 1: ¿se puede activar el mecanismo de `test_warp` con un destino dado en tiempo de ejecución (no por variable de entorno) y que se aplique en la siguiente lectura del guardado?
    2. Pregunta 2: ¿qué formas hay de provocar esa lectura sin ingeniería inversa? (reinicio en caliente + "Continuar" con entrada guionizada; otras). ¿Cuánto tarda cada una?
    3. Pregunta 3: ¿se pierde progreso no guardado? ¿Qué pasa con la sesión de red durante la recarga?
    4. Forma de la respuesta: tabla (método | funciona | segundos | qué se pierde) y recomendación.
  hecho cuando: T513 puede usar el camino 2 con datos.
  no hagas: no escribas en el fichero de guardado en disco; no cambies código de producción en esta tarea.
  modelo sugerido: alto

- [ ] T513 [reglas] Teletransporte al compañero en otro mapa
  files: runtime/coop_tp.c, tests/test_coop_tp.c, el módulo de reglas de `online_actors`, runtime/services/test_warp.c (solo si se usa el camino 2), tools/game_sites.py y runtime/game_sites.h (solo si se usa el camino 1)
  verify: `build/test_coop_tp` sale con 0 y recorrido manual §CU-14a de `verificacion.md` (5 viajes campo → santuario y 5 santuario → campo sin cuelgue)
  depende de: T511 o T512 (el que resultó viable), F4 (teletransporte en el mismo mapa)
  quién: agente + dueño
  contexto: F4 dejó "ir con mi compañero" para el mismo mapa: `coop_tp_can` rechaza con el motivo "mapa distinto". El mapa de cada jugador llega en su STATE (`pose.map`).
  pasos:
    1. En `coop_tp.c`: nuevo resultado "requiere cambio de mapa" en lugar de rechazo, y estado `LOADING_MAP`.
    2. Destino: la **entrada** del mapa del compañero, no su posición exacta.
    3. Camino 1 (si T511): llama a la función de cambio de mapa por `g_site` desde el hilo del juego, con las precondiciones de la bitácora.
    4. Camino 2 (si T512): fija el destino para `test_warp` y provoca la recarga por el método recomendado; avisa antes al jugador ("se cargará tu último guardado").
    5. Valida el nombre de mapa recibido contra un formato estricto (letras, dígitos, `/` y `_`, longitud < 40) antes de usarlo.
    6. Si ninguno está disponible: marca hacia la entrada del santuario (camino 3) y texto explicativo.
  hecho cuando: el recorrido pasa.
  no hagas: no uses un nombre de mapa de la red sin validar; no cambies de mapa durante un evento; no escribas direcciones en un `.c`.
  modelo sugerido: alto

## Bloque C y D — Santuarios y Bestias Divinas juntos

- [ ] T514 [pruebas] Matriz de santuarios jugados a dúo
  files: specs/bitacora/F5-T514-matriz-santuarios.md
  verify: la bitácora contiene la tabla completa de 8 santuarios con las columnas pedidas
  depende de: F1 (arreglo `has_body` en mecanismos), F4
  quién: dueño (un agente prepara la lista y las órdenes de `swctl`)
  contexto: Jugadores en el mismo santuario se ven y sus bolas y cajas se envían como objetos del mundo (`docs/ONLINE.md:996-997`). Los mecanismos se sincronizan (`movers_update`, `runtime/online_actors.c:4787-4980`) pero solo se probó moviendo a mano el ascensor de una entrada; "a lift ridden for real and a Divine Beast's parts aren't tested yet" (`docs/ONLINE.md:984-986`).
  pasos:
    1. Elige 8 santuarios: 2 con ascensor o plataformas móviles, 2 con bolas o cajas e interruptores, 2 con aparato de giroscopio o mecanismo giratorio, 1 de combate, 1 de "bendición".
    2. Para cada uno, los dos jugadores entran andando y lo completan. Activa `SAVAGEWIND_ONLINE_MOVER_LOG=1`.
    3. Anota por santuario: ¿se ven?; ¿cada mecanismo se mueve igual en los dos juegos?; ¿los objetos coinciden?; ¿quién recibe el orbe?; fps; cuelgues (con la última línea del registro).
    4. Forma de la respuesta: tabla y una lista priorizada de fallos, cada uno con cómo reproducirlo.
  hecho cuando: hay lista de fallos reproducibles (o "ninguno").
  no hagas: no uses el guardado real; no marques como fallo lo que el diseño no sincroniza a propósito (el estado interno del rompecabezas): anótalo como límite.
  modelo sugerido: medio

- [ ] T515 [mundo] Enemigos compartidos en un mapa donde no está el anfitrión
  files: el módulo de enemigos de `online_actors`, runtime/online.c (encaminamiento de ENEMIES por mapa), runtime/online_authority.c, tests/test_online_authority.c
  verify: `make test-online` sale con 0 y recorrido manual: dos invitados (o invitado + bot) en un santuario de combate sin el anfitrión ven al mismo enemigo con la misma vida
  depende de: T514 (debe confirmar el problema), T502
  quién: agente + dueño
  contexto: La lista de enemigos compartidos la construye el juego del anfitrión con los enemigos de **su** mundo (`runtime/online_actors.c:1987-2636`), y un jugador en otro mapa "has none of its enemies" (`docs/ONLINE.md:406-407`; `can_own`, `runtime/online_actors.c:2367-2370`). Por lectura del código, en un mapa sin el anfitrión cada invitado simula sus propios enemigos; T514 debe haberlo confirmado.
  pasos:
    1. Si T514 no confirmó el problema, cierra esta tarea como "no aplica".
    2. Define "dueño de lista de un mapa": el anfitrión si está en ese mapa; si no, el jugador de menor ranura presente en él.
    3. El servidor conoce el mapa de cada jugador por su último STATE; acepta ENEMIES del dueño de lista de cada mapa y lo reenvía solo a los jugadores de ese mapa.
    4. El módulo de enemigos usa "soy dueño de lista de mi mapa" donde hoy usa "soy anfitrión" para construir y enviar la lista.
    5. Al cambiar el dueño (alguien entra o sale del mapa), la lista se reconstruye; prueba que no deja enemigos duplicados.
  hecho cuando: los `verify` pasan y el campo abierto se comporta como antes.
  no hagas: no cambies la autoridad sobre flags, botín ni hora (siguen siendo del anfitrión); no confíes en el mapa que declara un cliente para darle autoridad sobre otro mapa.
  modelo sugerido: alto

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

## Bloque E — Momentos de historia con los dos presentes (CU-14)

- [ ] T518 [investigación] Catálogo de eventos de la lista del dueño
  files: specs/bitacora/F5-T518-catalogo-de-eventos.md, runtime/coop_story_table.inc (tabla de datos, generada a mano desde el catálogo)
  verify: la bitácora contiene la tabla; `runtime/coop_story_table.inc` compila incluido en un `.c` de prueba (`clang -fsyntax-only`)
  depende de: T501
  quién: dueño (juega y recoge el registro) + agente (ordena)
  contexto: El runtime registra el nombre de cada evento que empieza en una línea `[event] <nombre>` (`runtime/hle/framepace.c`, función `framepace_event_name`). No hay ninguna tabla de qué eventos son "de historia"; el código solo conoce los cinco del pedestal y las torres (`runtime/online_actors.c:1625-1633`).
  pasos:
    1. Para cada momento de la lista del ADR T501, el dueño lo juega con una copia de guardado y guarda las líneas `[event]`.
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
  contexto: Para exigir presencia en eventos que arrancan al entrar en una zona hay que poder **aplazar** el arranque. El PR ya sabe pedir un evento al gestor (`start_event`, `runtime/online_actors.c:3703-3713`, con funciones del juego de dirección fija) y leer el evento activo, pero no interceptar las peticiones del propio juego. Lanzar eventos fuera de contexto puede colgar (`docs/ONLINE.md:751-752`).
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
  depende de: T501, T518
  quién: agente
  contexto: No hay lógica de presencia. El bloqueo actual solo mira si *otro* jugador está en un pedestal a menos de 30 m (`runtime/online_actors.c:5981-6001`).
  pasos:
    1. `const CoopStoryEvent *coop_story_find(const char *nombre);` busca en la tabla de `coop_story_table.inc`.
    2. `const CoopStoryEvent *coop_story_near(const char *mapa, const float pos[3]);` devuelve el evento de arranque por botón cuyo punto está a menos de su radio.
    3. `int coop_story_gate(const CoopStoryEvent *e, const CoopStoryWho *yo, const CoopStoryWho *otros, int n, const CoopStoryRules *r);` → `ALLOW` o `WAIT`. Reglas del ADR: presentes = mismo mapa y a menos de `r->dist`; si no hay nadie más conectado, según `r->solo_ok`.
    4. Pruebas: evento fuera de tabla → `ALLOW`; compañero lejos → `WAIT`; otro mapa → `WAIT`; solo y permitido → `ALLOW`; posiciones no finitas → tratadas como lejos.
  hecho cuando: la prueba pasa; sin cabeceras del juego.
  no hagas: no leas memoria del juego; no uses variables globales mutables.
  modelo sugerido: bajo

- [ ] T521 [reglas] Puerta de presencia para eventos que arrancan con un botón
  files: el módulo de reglas de `online_actors` (bloque del bloqueo de A), runtime/online.c (STORY y bits de STATE)
  verify: recorrido manual §CU-14c de `verificacion.md` para 3 eventos de botón de la tabla
  depende de: T502, T520
  quién: agente + dueño
  contexto: El bloque de `runtime/online_actors.c:5981-6001` pone `g_online_block_a = 1` cuando otro jugador con el bit `ONLINE_E_DEMO` está a menos de 30 m y este juego no está en un evento; `runtime/hle/input.c:1207-1208` quita entonces el botón A de lo que ve el juego.
  pasos:
    1. Mantén el bloqueo actual.
    2. Añade: cada fotograma, `coop_story_near(mapa, posición del Link local)`; si devuelve un evento y `coop_story_gate` dice `WAIT`, activa el bloqueo de A, pon el bit "esperando" en STATE y envía un aviso (una vez cada 5 s) al compañero.
    3. No bloquees si este juego ya está en un evento (`framepace_in_event`).
    4. Cuando el evento empieza y termina, envía STORY para que el otro lo sepa.
    5. Opción de sesión del anfitrión "Story needs both players" (apagada por defecto).
  hecho cuando: con el compañero lejos, A no inicia el evento y se ve el aviso; con el compañero cerca, sí.
  no hagas: no bloquees A lejos de los puntos de la tabla (rompería recoger objetos y hablar); no lances eventos en el otro juego; no dejes el bloqueo puesto al salir de sesión.
  modelo sugerido: alto

- [ ] T522 [reglas] Puerta de presencia para eventos que arrancan por zona
  files: tools/game_sites.py, tools/patches.py, runtime/game_sites.h, el módulo de reglas de `online_actors`
  verify: `tools/venv/Scripts/python.exe tools/test_game_sites.py` sale con 0; recorrido manual §CU-14c para 2 eventos de zona (10 repeticiones cada uno, sin cuelgue)
  depende de: T519 (respuesta viable), T521
  quién: agente + dueño
  contexto: La bitácora de T519 describe la función por la que pasan los inicios de evento y cómo aplazar uno de forma segura.
  pasos:
    1. Lee la bitácora de T519. Si dice "no viable", **no hagas esta tarea**.
    2. Añade el sitio y el parche en un grupo opcional nuevo; el parche llama a `rt_coop_story_event(nombre)` y, si devuelve "esperar", hace lo que T519 describe para aplazar.
    3. `rt_coop_story_event` copia el nombre con longitud acotada, consulta una decisión **ya calculada** por el hilo del juego (variable atómica por evento cercano) y devuelve.
    4. Agrupa el cambio de `game_sites.py`/`patches.py` con cualquier otro lote pendiente.
  hecho cuando: los `verify` pasan.
  no hagas: no hagas trabajo pesado ni tomes cerrojos dentro de la función llamada desde el parche (puede ejecutarse en cualquier hilo del juego); no escribas direcciones en un `.c`.
  modelo sugerido: alto

- [ ] T523 [ui] "Esperando a tu compañero"
  files: runtime/ui/coop_hud.mm, specs/fases/F5-funciones-dificiles/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos; captura de las dos pantallas durante una espera
  depende de: T521
  quién: agente
  contexto: F4 dejó en `runtime/ui/coop_hud.mm` avisos en pantalla y marcas en el mundo. La puerta de historia necesita decir por qué A no responde y dónde está el punto.
  pasos:
    1. En quien espera: aviso fijo "Waiting for your partner" mientras dure el bit.
    2. En el compañero: aviso "<nombre> is waiting for you" y una marca de tipo "aquí" en la posición del que espera, con flecha de borde.
    3. Botón rápido "Go to" si el teletransporte está disponible.
  hecho cuando: la captura lo muestra y el aviso desaparece al empezar el evento.
  no hagas: no dependas solo del color; no llames al juego desde el dibujo.
  modelo sugerido: bajo

## Bloque F — Colisión con el compañero

- [ ] T524 [investigación] ¿Puede la marioneta tener cuerpo físico?
  files: specs/bitacora/F5-T524-cuerpo-de-la-marioneta.md
  verify: la bitácora responde a las 5 preguntas con lo visto en el juego
  depende de: F1
  quién: agente + dueño
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
  files: tools/online_mod.py (incluido su número de versión), runtime/main.c (solo el número de versión del overlay que comprueba), el módulo de marionetas y el de reglas de `online_actors`
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

## Bloque G — Más de 2 jugadores

- [ ] T526 [medidas] Ancho de banda con 2, 4 y 8 jugadores
  files: tools/nettest/measure_n.sh, specs/bitacora/F5-T526-ancho-de-banda-n.md
  verify: `sh tools/nettest/measure_n.sh` sale con 0 y escribe `build/nettest/n/resumen.csv` con filas para N = 2, 4, 8
  depende de: F3 (arnés de Docker)
  quién: agente
  contexto: Por cálculo, cada jugador sube unos 100 kB/s (STATE de 3375 bytes y SKEL de ~1,6 kB, ambos 20 veces por segundo) y el servidor reenvía cada flujo a los N−1 restantes: el total crece con N² (mapa 06 §2.5). No está medido. Además el bot no envía SKEL.
  pasos:
    1. Añade al bot (si falta) un modo `BOT_SKEL=1` que envíe un SKEL de tamaño real con datos sintéticos.
    2. Lanza el servidor y N bots (2, 4, 8) durante 120 s en dos disposiciones: todos juntos, y repartidos (cada uno a más de 1 km y la mitad en otro mapa).
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
  verify: `make test-online` y `sh tools/nettest/run.sh all` salen con 0; `measure_n.sh` muestra con 4 bots juntos una subida del servidor ≤ 50 % de la de T528
  depende de: T528, T529
  quién: agente designado como **dueño del protocolo**
  contexto: T529 dejó un codificador probado. El servidor guarda el último STATE de cada cliente para dárselo a quien entra y para el filtro de interés; con bloques debe reconstruir el estado completo por cliente.
  pasos:
    1. Sube `PROTOCOL` (segundo y último salto de F5) y regístralo.
    2. Cliente emisor: `statecodec_encode` contra lo último enviado; mensaje completo al conectar, al reanudar y cuando llega un JOIN.
    3. Servidor: decodifica sobre su copia por cliente (valida), y reenvía a cada destinatario codificando contra lo último que **ese destinatario** recibió de ese origen (necesario porque el filtro de interés hace que cada uno reciba ritmos distintos y porque la cola reemplazable descarta mensajes).
    4. Cliente receptor: `statecodec_decode` sobre su copia por jugador; si devuelve -1, descarta y pide completo.
    5. Actualiza el bot y las pruebas.
  hecho cuando: los `verify` pasan y dos juegos reales se ven igual que antes durante 10 minutos.
  no hagas: no reenvíes en el servidor los bloques tal cual llegan (un destinatario que se perdió un mensaje quedaría desincronizado); no cambies la versión más veces; no mezcles con otros cambios de cable.
  modelo sugerido: alto

- [ ] T531 [red] SKEL: enviar solo los huesos que cambian
  files: runtime/online.c (`skel_encode`, `skel_decode` y su envío), tests/test_online_skel.c
  verify: `clang ... tests/test_online_skel.c` (orden en la cabecera de la prueba) sale con 0; `measure_n.sh` con `BOT_SKEL=1` muestra la reducción
  depende de: T530
  quién: agente
  contexto: SKEL lleva 121 huesos (más los del arco) a 12 bytes por hueso, 20 veces por segundo: unos 30 kB/s por jugador (`SKEL_BONE_BYTES`, `runtime/online.c:53-56`; `docs/ONLINE.md:55-57`). `docs/ONLINE.md:1001` lo lista como pendiente: "only the bones that moved".
  pasos:
    1. Formato nuevo dentro del mismo mensaje (aprovecha el salto de T530; coordínalo con el dueño del protocolo): máscara de bits de huesos presentes + los huesos presentes.
    2. Un hueso se envía si su rotación o traslación cuantizadas difieren de lo último enviado; cada hueso se reenvía al menos una vez por segundo; esqueleto completo al conectar y ante un JOIN.
    3. El receptor mantiene el último valor por hueso; valida la máscara contra el número de huesos y la longitud.
    4. Pruebas: ida y vuelta, nada cambia, todo cambia, mensaje truncado, máscara con bits de más.
  hecho cuando: los `verify` pasan y la animación del compañero se ve igual (anotado en bitácora).
  no hagas: no cambies la cuantización de un hueso; no aceptes índices de hueso fuera de rango.
  modelo sugerido: alto

- [ ] T532 [reglas] Quitar los supuestos de "ranura 0" y de "solo dos"
  files: los módulos de reglas, enemigos, botín y marionetas de `online_actors`
  verify: sesión de 20 minutos con 2 juegos y 2 bots (4 jugadores) sin crash, con entrada en bitácora; `grep -n "pl\[0\]\|piece_update(c, 0" runtime/oa_*.c` no devuelve usos sin comentario que los justifique
  depende de: F3 (ranura del anfitrión garantizada y anunciada en WELCOME)
  quién: agente + dueño
  contexto: Las reglas de sesión se leen de `pl[0]` (`runtime/online_actors.c:2838-2840`); las marionetas de enemigo, el botín, las fuentes de elemento y las pruebas usan el pool de piezas del jugador 0 (`piece_update(c, 0, ...)`, líneas 2231, 3044, 3058, 2281), de modo que `drop(c, 0, true)` les borra piezas y el pool de 24 se llena antes.
  pasos:
    1. Sustituye `pl[0]` por la ranura del anfitrión que da la capa de red.
    2. Crea un pool propio "del mundo" (índice aparte de los 8 de jugadores) para enemigos, botín y fuentes de elemento.
    3. Revisa cada bucle `for (i < ONLINE_MAX)` que elige "el otro jugador": debe funcionar con 3 o más.
    4. Prueba con 2 bots además de los 2 juegos.
  hecho cuando: los `verify` pasan.
  no hagas: no cambies el tamaño de `ONLINE_MAX`; no mezcles esta tarea con cambios de comportamiento.
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
  quién: agente + dueño (necesita `extracted/exefs/main` del dueño para generar los patrones y recompilar)
  contexto: Las funciones de crear, pedir, recoger y borrar actores, poner matrices, despertar, y leer o escribir huesos están como `#define FN_... 0x70...` al principio del fichero (`runtime/online_actors.c:30-57` y `:666-670` en el PR). Un patrón se genera con `tools/game_sites.py pattern <main> <dirección> code` (imprime una línea `Site(...)` lista para pegar) y debe encontrar exactamente un lugar.
  pasos:
    1. Para cada función del módulo (lista del CSV de T533): genera su patrón con la orden anterior.
    2. Pega la línea en `SITES` con un comentario de qué hace, añade la entrada a `KNOWN['1.9.0']`, y mete todas en un grupo nuevo de `OPTIONAL_GROUPS` llamado como el subsistema.
    3. Añade `X(<nombre>)` a `GAME_SITES_OPTIONAL` en `runtime/game_sites.h`.
    4. En el `.c`: sustituye cada literal por `g_site.<nombre>`, y haz que `online_actors_on()` (la función que decide si el modo en línea tiene actores) devuelva falso si alguno de este grupo es 0.
    5. Regenera y recompila (`make gen`, `make -j8`) y comprueba la línea `[gamelib] ... sites` del registro.
  hecho cuando: los tres `verify` pasan y no queda ningún literal `0x70…` de función en ese módulo.
  no hagas: no escribas patrones a mano; no pongas los sitios como obligatorios (romperías el arranque sin modo en línea); no cambies el comportamiento; no abras otro lote hasta fusionar este.
  modelo sugerido: medio

- [ ] T535 [sitios] Lote 2: resto de funciones a sitios por patrón
  files: tools/game_sites.py, runtime/game_sites.h, los demás módulos `runtime/oa_*.c` con direcciones de función
  verify: los mismos tres de T534, más `tools/venv/Scripts/python.exe tools/coop_addr_inventory.py` mostrando 0 direcciones de función
  depende de: T534
  quién: agente + dueño
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
  verify: `make runtime-objs` sin avisos; `grep -n "FN_ARMOR_INIT\|FN_SPECIAL_FX_A\b" runtime/` no devuelve nada; recorrido de 10 minutos de dos juegos igual que antes
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

## Cierre

- [ ] T538 [i18n] Traducir los textos nuevos de F5 a los 13 idiomas
  files: runtime/ui/i18n/*.inc
  verify: `tools/venv/Scripts/python.exe tools/i18n_check.py` → `0 missing` en los 13 y sale con 0
  depende de: T510, T513, T521, T523, T525
  quién: agente (dueño único de los `.inc`)
  contexto: Cada tarea dejó sus textos en inglés en `specs/fases/F5-funciones-dificiles/textos-nuevos.txt`. La clave de traducción es el texto inglés: `T("English", "traducción")` en cada `runtime/ui/i18n/<idioma>.inc`.
  pasos:
    1. `tools/i18n_check.py --missing <idioma>` para cada uno.
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
