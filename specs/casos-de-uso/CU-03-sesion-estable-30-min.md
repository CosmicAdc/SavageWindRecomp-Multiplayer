# CU-03 · Jugar sin cuelgues una sesión de 30 minutos

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Dos jugadores juegan 30 minutos seguidos un recorrido normal (campo, combate, un santuario, menús, una muerte) sin
que ningún juego se cuelgue ni la sesión se congele. Hoy es parcial: el PR tiene mucho endurecimiento empírico, pero
quedan caminos de cuelgue conocidos.

## Actores

- **Anfitrión** e **invitado**, jugando de forma normal.
- **Desarrollador**: quien ejecuta el recorrido de prueba y lee los registros.

## Precondiciones

- CU-01 y CU-02 funcionan.
- Los dos con juego 1.9.0 y el mismo build.

## Flujo principal

1. Los dos se conectan (CU-01) y se encuentran en el campo.
2. Juegan juntos: caminan, pelean con un campamento de enemigos, recogen botín, usan runas, abren menús y el mapa.
3. Uno de ellos muere y vuelve.
4. Entran juntos en un santuario y salen.
5. Uno sale de la sesión y vuelve a entrar.
6. A los 30 minutos siguen conectados, sin cuelgues, sin congelaciones de más de 2 s y sin actores duplicados.

## Flujos alternativos y de error

| Situación | Qué pasa hoy | Evidencia |
|---|---|---|
| Un jugador deja de leer la red (PC suspendido, red caída sin cierre) | El servidor se bloquea en `send` y **congela a todos**; "End session" cuelga la interfaz del anfitrión | NET:140-148, mapa 06 H-1 |
| Compañero cae sin cerrar la conexión | Expulsado a los 10 s; a los 3 s deja de dibujarse | NET:384, NET:1402-1405 |
| Versión del juego distinta de 1.9.0 con `online_mod` presente | Se llama a 62 direcciones fijas de 1.9.0: salto a código equivocado | OA:225-229, mapa 07 A1 |
| Mecanismo de santuario movido por el compañero | `FN_BODY_TO` sin comprobar `has_body()` | OA:4928-4929, mapa 07 A2 |
| El juego borra el Link remoto entre fotogramas | Las runas usan el puntero antes de comprobar que sigue vivo | OA:5976-5977 frente a OA:6036, mapa 07 A5 |
| Llega un nombre de actor inesperado por la red | Se pide crear ese actor sin lista de permitidos (`GameROMPlayer` cuelga: DOC:122) | OA:3604, OA:2226, mapa 07 A4 |
| Llegan números no finitos | Pasan a `FN_SET_MTX` y `FN_BODY_TO` en varios caminos | mapa 07 M4, mapa 06 H-6 |
| Cambiar de sesión sin cerrar el juego | Sobreviven flags, botín y colas de la sesión anterior | NET:1322-1335, mapa 06 T-1 |
| "Salir a la pantalla de inicio" con sesión abierta | No hay `RT_ON_RESET`: quedan punteros del arranque anterior | mapa 07 M1, mapa 06 §1.4 |
| Muerte del jugador local | Se evita crear actores durante el game over y 180 fotogramas después (rehacerlos colgaba el juego) | OA:5873-5880, DOC:732-734 |
| En Linux, el compañero cierra de golpe | `send` sin protección frente a `SIGPIPE` puede matar el proceso | NET:143, mapa 06 H-8 |

## Estado actual en el PR: **PARCIAL**

- A favor: vigilancia de actores (`Watch`/`alive`, OA:84-105), reutilización de piezas (pool, OA:379-420), esperas
  tras carga y muerte (OA:5748-5761, OA:5873-5880), trama de red sin desbordamientos (mapa 06 §4.1).
- En contra: los puntos de la tabla anterior. Además no existe ninguna prueba automática de Online (mapa 06 §7.1) y
  la rama del PR no ejecuta CI (mapa 06 §8.4).
- Nada de esto se ha ejecutado todavía en este equipo: la línea base real se mide en F1.

## Qué hay que construir

**F1 — arreglos rápidos** (lista del plan; cada uno con su repro antes y después en `bitacora/`):

1. Puerta de versión: no llamar direcciones de 1.9.0 si `g_game_version` no es 1.9.0 (en `online_actors_on()`,
   OA:225-229).
2. `has_body()` en `movers_update` (OA:4928) y en los troncos de `objs_update` (OA:4611-4627).
3. Runas: usar el Link remoto solo tras comprobar `alive()` (OA:5976 frente a OA:6036).
4. Lista de permitidos para nombres de actor que llegan por red: reutilizar `is_loot()` (OA:3246) antes de OA:3604 e
   `is_enemy()` (OA:2029) antes de OA:2226.
5. Rechazar NaN e infinito (y magnitudes absurdas) en los números que llegan por red.
6. Limpiar todo el estado de sesión en `stop_all` (NET:1322-1335).
7. `SIGPIPE` en Linux (NET:143); terminador de `shield` en ESTATE (NET:1095); contraseña oculta (CU-01).

**F2 — cimientos**:

8. `RT_ON_RESET` para las ~85 variables con punteros del juego (mapa 07 M1), siguiendo el patrón de
   `runtime/items.c:337-344`.
9. Partir `online_actors.c` en los 11 módulos de mapa 07 §8 y fijar el orden dentro de `rt_online_frame`.
10. Pruebas automáticas del protocolo sin el juego, compartiendo la definición de mensajes con el bot.

**F3**: envío no bloqueante por cliente (quita la congelación global H-1).

## Reglas y decisiones de diseño

- **[DUEÑO]** Enfoque "cimientos primero": antes de añadir funciones se prueba y se arregla lo rápido y esencial
  (PLAN-APROBADO, Contexto).
- **[DUEÑO]** Un arreglo cuenta como hecho cuando se ha visto funcionar en el juego (PLAN-APROBADO, Reglas de
  trabajo).
- **[DUEÑO] (ADR-011, decisión 46; Q-03-1)** El recorrido de prueba de 30 minutos es el de la **Meseta de los Albores**:
  campo, un campamento, un santuario, menús, una muerte, salir y volver a entrar. Se escribe fijo (con lugares concretos
  de `goto`) para que la medida sea repetible.
- **[DUEÑO] (ADR-011, decisión 47; Q-03-2)** F1 se cierra con **dos juegos en un mismo PC**. La prueba con dos PC por
  Internet se hace **antes de cerrar F3** (el dueño tendrá un segundo PC más adelante; si no existe para entonces,
  queda como pendiente explícito en la bitácora y no se sustituye por Docker).
- **[PROPUESTA]** Criterio de congelación: ningún fotograma de más de 2 s atribuible a la red.

## Criterios de aceptación

- **CA-1** Dado el montaje común, cuando se ejecuta el recorrido de prueba durante 30 min, entonces los dos procesos
  siguen vivos, `swctl.py ping` responde en ambos y no hay ninguna línea de cuelgue en los registros.
- **CA-2** Dado un `g_game_version` distinto de 1.9.0 (simulado en una prueba), cuando hay `online_mod` y sesión,
  entonces `rt_online_frame` no llama a ninguna dirección fija y el registro dice por qué el modo online está apagado.
- **CA-3** Dado un bot que envía un LOOT_ADD con nombre `GameROMPlayer`, entonces el juego no pide ese actor y lo
  registra como rechazado. (Requiere ampliar el bot para enviar LOOT_ADD.)
- **CA-4** Dado un bot que envía un STATE con posición NaN, entonces el juego no mueve ningún actor con ese valor y
  no se cuelga.
- **CA-5** Dado A y B en sesión, cuando B sale, entra en una sesión nueva de otro anfitrión (o A reinicia la suya),
  entonces B no aplica flags de la sesión anterior: `swctl.py --port 7778 flag <nombre>` de un flag que solo tenía
  el anfitrión anterior sigue a 0.
- **CA-6** Dado A y B en sesión, cuando A hace "salir a la pantalla de inicio" y vuelve a cargar, entonces ningún
  juego se cuelga y B vuelve a ver a A.
- **CA-7** Dado `SAVAGEWIND_ONLINE_DIE_TEST=<s>` en B (OA:5922-5924), cuando B muere y revive, entonces A sigue
  viendo a B después y no hay cuelgue.
- **CA-8** (F3) Dado un cliente que deja de leer (bot modificado: `BOT_FREEZE` sigue leyendo, mapa 06 §7.2), entonces
  los demás jugadores siguen recibiendo estado y el cliente mudo es expulsado en menos de 15 s.

## Qué NO hacer

- No mezclar arreglos: un arreglo por cambio, cada uno con su repro "antes" y "después" en `bitacora/`.
- No cambiar comportamiento al partir `online_actors.c` en módulos (F2): mover código, no reescribirlo. El orden de
  llamadas dentro de `rt_online_frame` (OA:5721) es parte del comportamiento.
- No añadir direcciones fijas nuevas ni inventar offsets; la puerta de versión usa `g_game_version`
  (`runtime/game_sites.h:113`).
- No llamar a funciones del juego desde el hilo de red ni desde el de interfaz.
- No dar un arreglo por hecho porque compila: hay que verlo en el juego con dos instancias.
- No "arreglar" un cuelgue silenciando el síntoma (por ejemplo, capturando la excepción): se corrige la causa o se
  documenta como pendiente.
- No tocar `tools/patches.py` ni `tools/game_sites.py` salvo necesidad, y entonces agrupar los cambios.
- No ejecutar las variables de prueba destructivas (`SAVAGEWIND_ONLINE_DIE_TEST`, `_KILL_TEST`, `_BOSS_KILL`) con el
  guardado real.

## Riesgos y preguntas abiertas

- Riesgo: los santuarios y las Bestias Divinas son justo las zonas "no probadas" (DOC:984-986, 1013-1014) y las de
  mayor riesgo (mapa 07 A2).
- Riesgo: dos juegos en este PC usan ~9,5 GB de 15,7 GB de RAM; un cuelgue por falta de memoria no es un fallo del
  modo online. Anotar la memoria libre en cada prueba.
- Riesgo: M3 y M8 del mapa 07 (vida de los `InstParamPack` y metadatos de evento) no están verificados y necesitan
  Ghidra.
- Preguntas ya contestadas por el dueño (Q-03-1 y Q-03-2): ver ADR-011 (decisiones 46 y 47) en `../decisiones/`.

## Fase y dependencias

- **Fase**: F1 (línea base y arreglos rápidos) y F2 (cimientos). H-1 se resuelve en F3.
- **Depende de**: CU-01, CU-02.
- **Lo necesitan**: todos los casos de F3 a F5.
