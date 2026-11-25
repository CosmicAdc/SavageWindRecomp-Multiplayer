# CU-14 · Historia con los dos presentes, santuarios juntos y teletransporte entre mapas

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Los momentos importantes de la historia (una lista corta) piden que los dos jugadores estén allí; los santuarios y
las Bestias Divinas se pueden jugar juntos; y un jugador puede ir hasta su compañero aunque esté en otro mapa. Es el
caso más difícil: hay piezas sueltas en el PR, pero ningún mecanismo general, y varias partes necesitan ingeniería
inversa.

Son tres subcasos que se entregan por separado: **14a** historia con los dos presentes, **14b** santuarios y Bestias
juntos, **14c** teletransporte entre mapas.

**Prioridad (ADR-008, decisión 20):** el objetivo de fondo del dueño es **que los dos puedan pasarse los santuarios
juntos**. Por eso 14b va primero en F5, antes que el tinte 3D (CU-13) y que el bloque de historia (14a). Los
santuarios se pueden hacer juntos pero no es obligatorio: no son "momento de historia" (ADR-007, decisión 3).

## Actores

- **Iniciador**: quien intenta empezar el momento de historia o entra primero al santuario.
- **Compañero**: el otro jugador.
- **Anfitrión**: su mundo es el que avanza (CU-06).

## Precondiciones

- CU-06 (el progreso es del anfitrión) y CU-07 (recompensas) hechos y decididos.
- CU-08 (teletransporte en el mismo mapa) y CU-10 (avisos en pantalla) hechos.
- La **lista de "momentos de historia"** ya está decidida por el dueño (ADR-007, decisión 3; ver reglas).

## Flujo principal

**14a — Historia con los dos presentes**

1. El iniciador llega a un momento de historia de la lista (por ejemplo, examinar un pedestal o hablar con un
   personaje clave).
2. Si el compañero **no** está presente (mismo mapa y a menos de 30 m), el momento no arranca. El iniciador ve
   "Esperando a <nombre> para continuar" y el compañero ve "<nombre> te espera en <lugar>", con un marcador y la
   **oferta de teletransportarse** hasta él (CU-08 / 14c).
3. Cuando los dos están presentes, el momento arranca. En la versión 1 basta con estar allí: uno ve la escena; el
   otro está presente y recibe el progreso (ADR-009, decisión 21).
4. Al terminar, el progreso queda en el mundo del anfitrión y los dos reciben lo que corresponda (CU-07).
5. Si el compañero no viene o no puede venir, el **anfitrión** puede continuar solo de inmediato: el juego le pide
   una confirmación ("<nombre> no está aquí. ¿Continuar sin él?") y, si acepta, el momento arranca. No hay
   temporizador (ADR-007, decisión 6).

**14b — Santuarios y Bestias juntos**

1. Los dos entran en el mismo santuario (cada uno por la entrada).
2. Dentro se ven y ven los mismos objetos y mecanismos moviéndose.
3. Resuelven el rompecabezas entre los dos.
4. Al hablar con el monje, el santuario queda superado en el mundo del anfitrión y **los dos reciben el orbe**
   (CU-07; ADR-007, decisión 2). El monje no exige a los dos presentes: quien llegue puede hablar con él.

**14c — Teletransporte entre mapas**

1. Un jugador está en el campo y su compañero dentro de un santuario (o al revés).
2. Elige "Ir con <nombre>". Ve una pantalla de carga.
3. Aparece en el mismo mapa que su compañero, en la entrada o junto a él.

## Flujos alternativos y de error

| Situación | Comportamiento esperado |
|---|---|
| El compañero está en otro mapa | 14a: no arranca; aviso y opción de viajar. 14c: es el caso normal |
| El compañero está en un menú o en pausa | Cuenta como presente si está cerca [PROPUESTA] |
| El compañero está en un diálogo con un NPC (D5) | Cuenta como presente; el momento arranca cuando termine su diálogo [PROPUESTA] |
| El compañero ya está en una escena | El iniciador espera; hoy solo pasa en pedestales Sheikah (A bloqueado) |
| El compañero está muerto o caído | No cuenta como presente; primero CU-11 |
| El compañero se desconecta mientras se espera | El anfitrión puede continuar solo de inmediato, tras confirmarlo; sin plazo (ADR-007, decisión 6). El sitio del compañero sigue reservado 10 minutos (CU-05) |
| Quien inicia el momento es el invitado y el anfitrión no está presente | No arranca; la confirmación para continuar solo es únicamente del anfitrión |
| Uno de los dos entra solo a un santuario y habla con el monje | Se permite sin espera: los santuarios no son momento de historia (ADR-007, decisión 3). El otro recibe el orbe igualmente |
| Uno se desconecta en mitad de la escena | La escena termina en el juego del que sigue; el progreso se aplica al otro al volver (CU-05) |
| El anfitrión sale | Fin de sesión; el invitado no avanza la historia del anfitrión |
| El momento arranca por entrar en una zona, no por un botón | No hay forma conocida de retenerlo: **[sin verificar]**, requiere ingeniería inversa. Mientras tanto, esos momentos quedan fuera de la lista |
| Uno se salta la escena | En la versión 1 no aplica: solo uno la ve (Q-14-5, abierta) |
| El evento se lanza fuera de contexto en el otro juego | Riesgo de cuelgue conocido (DOC:751-752): por defecto **no** se lanza; se aplican solo los flags |
| Dentro de un santuario, un mecanismo no tiene cuerpo físico | No se mueve y se registra (arreglo `has_body`, CU-03) |
| 14c con el destino en una Bestia Divina o en una escena | Deshabilitado hasta probarlo |
| Versión distinta | No hay sesión (CU-01) |

## Estado actual en el PR: **FALTA** (hay piezas)

**14a — piezas existentes**

- Un juego que está en el evento del pedestal de la piedra o de una torre (`Obj_DRStone_A_01`, `Demo700_0`,
  `Demo701_0`, `Demo004_0`, `Demo164_0`) lo publica con el bit `ONLINE_E_DEMO` (OA:1625-1633; NETH:133). No se marca
  ningún otro evento ("not a chest, a talk or any other", DOC:458-461).
- Mientras otro jugador a menos de 30 m está en uno de esos eventos, el botón A local se retiene
  (`g_online_block_a`, OA:5981-6001; `runtime/hle/input.c:1206-1209`). Es para que no empiecen lo mismo a la vez: lo
  contrario de "esperar al compañero", pero el mecanismo (retener A según el estado del otro) es reutilizable.
- Saber si el juego local está en un evento y cuál: `framepace_in_event()` (`runtime/framepace.h:14`) y
  `framepace_event_name()`.
- Lanzar un evento sobre un actor: `start_event()` (OA:3699-3716), usado para el Kolog cercano (`korok_event`,
  OA:3811-3847) y, de forma opcional, para el pedestal de la piedra (`SAVAGEWIND_ONLINE_SLATE_EVENT`, DOC:452-456).
  Riesgos anotados: libera los metadatos justo después de pedir el evento y usa búferes estáticos (mapa 07 M8); un
  evento fuera de contexto puede colgar (DOC:751-752).
- **El progreso se propaga por flags aunque el otro no esté**: quien no estaba recibe los flags y, en su caso, los
  objetos, sin ver ninguna escena (OA:3986-3997; DOC:447-451). No hay condición de presencia ni escena sincronizada
  (mapa 06 §9).

**14b — piezas existentes**

- Cada estado lleva el mapa del jugador (`pose.map`, NETH:93; `local_map`, OA:1475-1482). Jugadores en el **mismo**
  santuario se ven; en mapas distintos no (`same_map`, OA:1484-1486; OA:6014; DOC:402-407, 996-997).
- Objetos y mecanismos del nivel se envían (mapa 07 §2.1, filas 22-23). El rompecabezas **no** está en flags: cada
  entrada lo empieza de nuevo (DOC:996).
- Activar, entrar y superar un santuario son flags compartidos; al llegar `Clear_Dungeon*` se entrega un orbe
  (OA:3994). Probado por los autores en los cuatro santuarios de la meseta con un jugador haciendo todo y el otro
  solo recibiendo (DOC:465-478).
- **No probado**: ascensor montado de verdad, piezas de una Bestia Divina, Bestia vista desde fuera
  (DOC:984-986, 1013-1014). Riesgo A2: `FN_BODY_TO` sin `has_body()` en mecanismos (OA:4928-4929).

**14c — nada**

- `teleport` / `goto` funcionan solo dentro de un mapa: "a shrine's or a Divine Beast's needs walking in"
  (DOC:747-748). El único camino que cambia de mapa en toda la base es reescribir la posición guardada al cargar el
  `.sav`, solo para pruebas (`SAVAGEWIND_TEST_WARP`, `runtime/services/test_warp.c:86-143`; mapa 03 §1.2).
- Falta "la función de cambio de escena o un guardar y recargar" (mapa 03 fila 13).

## Qué hay que construir

**14a**

1. **Tabla de momentos de historia** (`contratos/` o un `.txt` junto a `quests.txt`) con la lista del dueño (ADR-007,
   decisión 3): inicio (piedra y primera torre), final de cada Bestia Divina, recuerdos y jefes finales. **Ningún
   santuario entra en la tabla.** Por cada momento, el nombre del evento o del actor que lo inicia y el radio de
   presencia (30 m). Empezar por los que ya se conocen por nombre (los cinco de OA:1629) y **revisar contra esta
   lista el bloqueo del botón A en pedestales que existe hoy**: lo que hoy se retiene y no está en la lista se
   anota y se decide si se conserva solo como protección contra arranques simultáneos.
2. **Publicar "estoy en un evento" de forma general**: un bit nuevo en el estado a partir de `framepace_in_event()`
   y, si hace falta, el nombre (`framepace_event_name()`). Cambia el protocolo.
3. **Puerta de presencia, versión 1 (sin ingeniería inversa)**: para los momentos que empiezan con el botón A junto a
   un actor conocido, retener A (mecanismo de `g_online_block_a`) mientras el compañero no esté en el mismo mapa y a
   menos de 30 m, y mostrar "Esperando a <nombre>" con la oferta de teletransporte para el ausente (ADR-007,
   decisión 4). Para saber que Link está junto al actor iniciador se puede
   reutilizar la búsqueda de actores por nombre que ya existe (`all_actors`, mapa 07 §8.1; comando `actors`).
   Limitación: retiene **todo** A cerca de ese actor.
4. **Puerta de presencia, versión 2 (Ghidra)**: interceptar el arranque de eventos en general (sitio nuevo cerca de
   la petición al gestor de eventos) para poder aplazar por nombre de evento, incluidos los que arrancan por zona.
5. **Qué ve el compañero**: versión 1 (la que se construye; ADR-009, decisión 21), nada de escena; está presente y
   al terminar recibe flags y objetos como hoy, con un aviso. Versión 2 (ver la misma escena a la vez, lanzándola
   en su juego con `start_event()` solo para eventos probados uno a uno): mejora posterior, **sin tareas activas**.
6. **Regla de avance** en `oa_flags`: los flags de un momento de la tabla solo se aceptan en el anfitrión si en ese
   instante los dos estaban presentes, o si el anfitrión confirmó continuar solo (punto 6b).
6b. **Continuar sin el compañero** (ADR-007, decisión 6): diálogo de confirmación en el juego del anfitrión,
   disponible de inmediato cuando el compañero no está presente (lejos, en otro mapa, desconectado). Sin
   temporizador de desconexión. Al aceptar se levanta la retención de A para ese momento y queda anotado en el
   registro.

**14b**

6c. **Investigación previa (primera tarea del bloque; ADR-008, decisión 20)**: con dos instancias en un PC, entrar
   los dos al mismo santuario y anotar en `../bitacora/` qué ocurre hoy: si se ven, si los mecanismos se mueven
   para los dos, qué pasa al hablar con el monje y qué falla. Según `docs/ONLINE.md` §"Players on other maps",
   quien está en un santuario o en una Bestia Divina no aparece en el mundo de los demás, y el mapa 07 lo da como
   parcial y sin probar. De su resultado sale la planificación del resto de 14b.
7. Plan de pruebas de santuario con dos juegos (ya hay receta: "pedestal, elevator, guidance stone, monk, each step
   waiting until the other game logs the flag", DOC:759-761) y lista de santuarios representativos: uno de
   rompecabezas con objetos, uno de combate, uno con plataformas móviles.
8. Arreglos que salgan de ahí, empezando por `has_body` en `movers_update` (CU-03).
9. Recompensa del santuario según la regla de CU-07: orbe para los dos, siempre, también para quien no entró y
   para quien se une más tarde (ADR-007, decisión 2; hoy: orbe para todos los que reciben el flag).
10. Bestias Divinas: solo después de los santuarios; son "no probadas" en todo.

**14c**

11. Investigación con Ghidra: la función de cambio de mapa que usa el juego al entrar a un santuario. Sitio nuevo por
    patrón en `tools/game_sites.py`, nunca dirección fija nueva.
12. Alternativa sin esa función [sin verificar]: "guardar y recargar" con la posición y el mapa de destino
    escritos como hace `test_warp.c`. El dueño acepta que la primera versión pase por pantalla de carga y guardado
    (ADR-008, decisión 20). Toca el guardado del jugador: solo con perfil cooperativo (CU-06); en el anfitrión, su
    partida es la de la campaña.
13. Después de cambiar de mapa, el lado online ya trata la carga (`forget_all` y 60 fotogramas de espera,
    OA:5748-5761).

## Reglas y decisiones de diseño

- **[DUEÑO] D6**: los momentos de historia necesitan a los dos presentes para continuar.
- **[DUEÑO] D1**: los dos recorren la historia juntos.
- **[DUEÑO] D5**: cada jugador habla con los NPC por su cuenta: las conversaciones normales **no** son momentos de
  historia y no se bloquean.
- **[DUEÑO] D3**: el progreso que resulta es el del anfitrión.
- **[DUEÑO] (ADR-007, decisión 3; Q-14-1)** Los momentos de historia son una **lista corta**: inicio (piedra y
  primera torre), final de cada Bestia Divina, recuerdos y jefes finales. **Los santuarios no** (tampoco su monje),
  ni las misiones secundarias ni los diálogos normales. No hay ningún bloqueo en los 120 santuarios.
- **[DUEÑO] (ADR-007, decisión 4; Q-14-2)** "Presente" = **mismo mapa y a menos de 30 m** (el radio que ya usa el
  bloqueo de A, OA:5988). Si falta uno, el juego avisa y **ofrece teletransportarlo**.
- **[DUEÑO] (ADR-007, decisión 6; Q-14-4)** Se puede continuar la historia sin el compañero **de inmediato, con
  una confirmación del anfitrión**. Sin temporizador. El plazo de reconexión de CU-05 (10 minutos) es una cuestión
  aparte.
- **[DUEÑO] (ADR-009, decisión 21; Q-14-3)** En la versión 1 **basta con estar allí**: uno ve la escena; el otro
  está presente y recibe el progreso. Verla a la vez se intenta más adelante, sin tareas activas.
- **[DUEÑO] (ADR-008, decisión 20; Q-14-7)** El teletransporte a un compañero en otro mapa se acepta **con pantalla
  de carga y guardado**. Objetivo de fondo: **que los dos puedan pasarse los santuarios juntos**; no basta con
  llegar al mapa del compañero, los dos deben verse y jugar dentro.
- **[DUEÑO] (ADR-007, decisión 2; P4)** Las recompensas de santuario (orbes) son para los dos, siempre.
- **[DUEÑO] (ADR-008, decisión 20)** Orden de entrega: 14b primero (investigación y jugar santuarios juntos), antes
  que el tinte 3D (CU-13) y que 14a. El orden exacto de las tareas está en `../fases/F5-funciones-dificiles/plan.md`.
  14a versión 2 queda sin tareas activas.

## Criterios de aceptación

- **CA-1** (14a) Dado el montaje común con A junto a un pedestal de la lista y B a más de 100 m, cuando A pulsa A
  (`swctl.py --port 7777 press a`), entonces el evento no arranca (`framepace_in_event` falso; se consulta con un
  comando `coop story` nuevo), A ve "Esperando a Client" y B recibe el aviso (`coop overlay`, CU-10).
- **CA-2** (14a) Dado lo anterior, cuando B llega a menos de 30 m (`teleport`) y A pulsa A, entonces el evento
  arranca en A en menos de 2 s.
- **CA-2b** (14a) Dado CA-1, entonces B tiene en pantalla la oferta de teletransportarse hasta A; cuando la acepta,
  se cumple CU-08 CA-1 y después CA-2.
- **CA-2c** (14a) Dado CA-1 con A anfitrión, cuando A confirma "continuar sin Client", entonces el evento arranca
  en A en menos de 2 s sin esperar ningún plazo, el registro de A lo anota y B recibe el progreso al terminar.
  Dado el mismo caso con el invitado como iniciador, entonces no se le ofrece continuar solo.
- **CA-2d** (14a/14b) Dado A en un santuario y B fuera, cuando A habla con el monje, entonces no hay espera ni
  aviso de presencia, `Clear_Dungeon<n>` queda a 1 en los dos y B recibe su orbe.
- **CA-3** (14a) Dado A y B presentes y el evento terminado, entonces el flag correspondiente está a 1 en los dos
  (`swctl.py flag <nombre>`), y los objetos se han entregado según CU-07.
- **CA-4** (14a) Dado un NPC normal fuera de la lista, cuando A habla con él con B lejos, entonces el diálogo arranca
  sin espera (D5).
- **CA-5** (14a) Dado un bot que envía flags de un momento de la lista sin que el anfitrión estuviera presente,
  entonces el anfitrión no los acepta.
- **CA-6** (14b) Dado A y B dentro del mismo santuario de prueba, entonces cada uno tiene 1 `OnlinePlayer`
  (`actors OnlinePlayer`), y cuando A mueve un objeto del rompecabezas, su posición en B cambia en menos de 1 s
  (`actors <nombre del objeto>` en los dos).
- **CA-7** (14b) Dado el recorrido completo de tres santuarios de prueba con dos juegos, entonces no hay ningún
  cuelgue y cada paso queda anotado en `bitacora/`.
- **CA-8** (14b) Dado el santuario superado, entonces `Clear_Dungeon<n>` está a 1 en los dos y cada uno tiene un
  orbe más (regla de CU-07: para los dos, siempre).
- **CA-8b** (14b, investigación) Existe una entrada en `bitacora/` con lo observado hoy al entrar los dos al mismo
  santuario con dos instancias en un PC, antes de planificar el resto de 14b.
- **CA-9** (14c) Dado A en el campo y B dentro de un santuario, cuando A elige "Ir con Client", entonces en menos de
  60 s el mapa de A (línea `[online] map:` del registro, OA:1480) es el de B y los dos se ven.
- **CA-10** (14c) Dado el viaje anterior, entonces el guardado propio de A (fuera del perfil cooperativo) no ha
  cambiado.

## Qué NO hacer

- No lanzar eventos en el juego del compañero "por si acaso": un evento fuera de contexto puede colgar el juego
  (DOC:751-752). Solo eventos de la tabla, probados uno a uno.
- No bloquear los diálogos normales con NPC (D5).
- No dar por buena ninguna función de cambio de mapa sin verla en Ghidra con la copia del dueño; no inventar
  direcciones ni offsets.
- No añadir direcciones fijas nuevas: todo sitio nuevo va por patrón en `tools/game_sites.py`, y los cambios en
  `tools/patches.py` se agrupan (recompilar cuesta ~15 min a cada jugador).
- No escribir en el guardado real del jugador para cambiar de mapa.
- No empezar por las Bestias Divinas.
- No decidir por tu cuenta qué es un "momento de historia": es la lista del dueño (ADR-007, decisión 3).
- No poner ninguna puerta de presencia en los santuarios ni en sus monjes.
- No añadir temporizadores para "continuar sin el compañero": es una confirmación inmediata del anfitrión.
- No abrir tareas de sincronización de escenas (14a versión 2) en esta entrega (ADR-009, decisión 21).
- No copiar código ni tablas de proyectos de decompilación.
- No tocar `start_event` sin resolver antes la duda de vida de sus búferes (mapa 07 M8).

## Riesgos y preguntas abiertas

- Riesgo alto: interceptar el arranque de eventos en general necesita ingeniería inversa y es donde más fácil es
  colgar el juego.
- Riesgo: los momentos que arrancan por zona no se pueden retener con el botón A.
- Riesgo resuelto por decisión: el bloqueo permanente si el compañero no puede llegar lo evita la confirmación del
  anfitrión (ADR-007, decisión 6). Queda el caso del invitado como iniciador con el anfitrión ausente: espera.
- Riesgo: hoy no está probado qué pasa cuando los dos entran al mismo santuario; hasta la investigación del punto
  6c no se sabe cuánto trabajo es "santuarios juntos".
- Riesgo: santuarios y Bestias son zonas sin probar con el riesgo A2 de mapa 07.
- Riesgo: algunos efectos de flags solo se ven tras una carga (DOC:441-442).
- Preguntas ya contestadas por el dueño (Q-14-1, Q-14-2, Q-14-3, Q-14-4, Q-14-6 y Q-14-7): ver ADR-007
  (decisiones 2, 3, 4 y 6), ADR-008 (decisión 20) y ADR-009 (decisión 21) en `../decisiones/`.
- **Pregunta al dueño** (Q-14-5, abierta; por defecto: no aplica en la versión 1, porque solo uno ve la escena; en
  una versión 2, cada uno la suya): si uno se salta la escena, ¿se salta para los dos?

## Fase y dependencias

- **Fase**: F5 (una a una; requiere ingeniería inversa con la copia del dueño en Ghidra). 14b es el primer bloque
  de F5 y empieza por la investigación con dos instancias (punto 6c), que puede adelantarse a F4.
- **Depende de**: CU-06, CU-07, CU-08, CU-10; CU-05 para los cortes en mitad de un momento; CU-03 (`has_body`).
- **Lo necesitan**: ninguno.
