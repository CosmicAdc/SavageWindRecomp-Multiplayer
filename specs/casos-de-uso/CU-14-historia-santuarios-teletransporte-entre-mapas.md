# CU-14 · Historia con los dos presentes, santuarios juntos y teletransporte entre mapas

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Los momentos importantes de la historia solo arrancan cuando los dos jugadores están allí; los santuarios y las
Bestias Divinas se pueden jugar juntos; y un jugador puede ir hasta su compañero aunque esté en otro mapa. Es el caso
más difícil: hay piezas sueltas en el PR, pero ningún mecanismo general, y varias partes necesitan ingeniería inversa.

Son tres subcasos que se entregan por separado: **14a** historia con los dos presentes, **14b** santuarios y Bestias
juntos, **14c** teletransporte entre mapas.

## Actores

- **Iniciador**: quien intenta empezar el momento de historia o entra primero al santuario.
- **Compañero**: el otro jugador.
- **Anfitrión**: su mundo es el que avanza (CU-06).

## Precondiciones

- CU-06 (el progreso es del anfitrión) y CU-07 (recompensas) hechos y decididos.
- CU-08 (teletransporte en el mismo mapa) y CU-10 (avisos en pantalla) hechos.
- Una **lista escrita de "momentos de historia"** aprobada por el dueño (ver preguntas abiertas).

## Flujo principal

**14a — Historia con los dos presentes**

1. El iniciador llega a un momento de historia de la lista (por ejemplo, examinar un pedestal o hablar con un
   personaje clave).
2. Si el compañero **no** está cerca (mismo mapa y a menos de X m), el momento no arranca. El iniciador ve
   "Esperando a <nombre> para continuar" y el compañero ve "<nombre> te espera en <lugar>", con un marcador y la
   opción de ir (CU-08 / 14c).
3. Cuando los dos están presentes, el momento arranca. Los dos ven la escena.
4. Al terminar, el progreso queda en el mundo del anfitrión y los dos reciben lo que corresponda (CU-07).

**14b — Santuarios y Bestias juntos**

1. Los dos entran en el mismo santuario (cada uno por la entrada).
2. Dentro se ven y ven los mismos objetos y mecanismos moviéndose.
3. Resuelven el rompecabezas entre los dos.
4. Al hablar con el monje, el santuario queda superado en el mundo del anfitrión y las recompensas se reparten según
   la regla de CU-07.

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
| El compañero se desconecta mientras se espera | [PREGUNTA] seguir solo tras un plazo, o esperar siempre |
| Uno se desconecta en mitad de la escena | La escena termina en el juego del que sigue; el progreso se aplica al otro al volver (CU-05) |
| El anfitrión sale | Fin de sesión; el invitado no avanza la historia del anfitrión |
| El momento arranca por entrar en una zona, no por un botón | No hay forma conocida de retenerlo: **[sin verificar]**, requiere ingeniería inversa. Mientras tanto, esos momentos quedan fuera de la lista |
| Uno se salta la escena | [PREGUNTA] ¿se salta para los dos? |
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

1. **Tabla de momentos de historia** (`contratos/` o un `.txt` junto a `quests.txt`): por cada momento, el nombre
   del evento o del actor que lo inicia, el radio de presencia y qué hacer con el ausente. Empezar por los que ya se
   conocen por nombre: los cinco de OA:1629.
2. **Publicar "estoy en un evento" de forma general**: un bit nuevo en el estado a partir de `framepace_in_event()`
   y, si hace falta, el nombre (`framepace_event_name()`). Cambia el protocolo.
3. **Puerta de presencia, versión 1 (sin ingeniería inversa)**: para los momentos que empiezan con el botón A junto a
   un actor conocido, retener A (mecanismo de `g_online_block_a`) mientras el compañero no esté en el mismo mapa y a
   menos de X m, y mostrar "Esperando a <nombre>". Para saber que Link está junto al actor iniciador se puede
   reutilizar la búsqueda de actores por nombre que ya existe (`all_actors`, mapa 07 §8.1; comando `actors`).
   Limitación: retiene **todo** A cerca de ese actor.
4. **Puerta de presencia, versión 2 (Ghidra)**: interceptar el arranque de eventos en general (sitio nuevo cerca de
   la petición al gestor de eventos) para poder aplazar por nombre de evento, incluidos los que arrancan por zona.
5. **Qué ve el compañero**: versión 1, nada de escena; al terminar recibe flags y objetos como hoy, con un aviso.
   Versión 2, lanzar la misma escena en su juego con `start_event()` solo para los eventos probados uno a uno.
6. **Regla de avance** en `oa_flags`: los flags de un momento de la tabla solo se aceptan en el anfitrión si en ese
   instante los dos estaban presentes (o si se cumple la excepción decidida por el dueño).

**14b**

7. Plan de pruebas de santuario con dos juegos (ya hay receta: "pedestal, elevator, guidance stone, monk, each step
   waiting until the other game logs the flag", DOC:759-761) y lista de santuarios representativos: uno de
   rompecabezas con objetos, uno de combate, uno con plataformas móviles.
8. Arreglos que salgan de ahí, empezando por `has_body` en `movers_update` (CU-03).
9. Recompensa del santuario según la regla de CU-07 (hoy: orbe para todos los que reciben el flag).
10. Bestias Divinas: solo después de los santuarios; son "no probadas" en todo.

**14c**

11. Investigación con Ghidra: la función de cambio de mapa que usa el juego al entrar a un santuario. Sitio nuevo por
    patrón en `tools/game_sites.py`, nunca dirección fija nueva.
12. Alternativa sin esa función [PROPUESTA, sin verificar]: "guardar y recargar" con la posición y el mapa de destino
    escritos como hace `test_warp.c`. Toca el guardado del jugador: solo con perfil dedicado (CU-06).
13. Después de cambiar de mapa, el lado online ya trata la carga (`forget_all` y 60 fotogramas de espera,
    OA:5748-5761).

## Reglas y decisiones de diseño

- **[DUEÑO] D6**: los momentos de historia necesitan a los dos presentes para continuar.
- **[DUEÑO] D1**: los dos recorren la historia juntos.
- **[DUEÑO] D5**: cada jugador habla con los NPC por su cuenta: las conversaciones normales **no** son momentos de
  historia y no se bloquean.
- **[DUEÑO] D3**: el progreso que resulta es el del anfitrión.
- **[PROPUESTA] P4**: recompensas de santuario para los dos.
- **[PROPUESTA]** "Presente" = mismo mapa y a menos de 30 m (el radio que ya usa el bloqueo de A, OA:5988).
- **[PROPUESTA]** Lista inicial de momentos de historia: pedestal de la piedra, primera torre, final de cada
  santuario (monje), final de cada Bestia Divina, recuerdos, y los jefes finales. Las misiones secundarias no.
- **[PROPUESTA]** Válvula de escape: si el compañero lleva N minutos desconectado, el anfitrión puede continuar solo.
- **[PROPUESTA]** Orden de entrega: 14b (probar santuarios) → 14a versión 1 → 14c → 14a versión 2.

## Criterios de aceptación

- **CA-1** (14a) Dado el montaje común con A junto a un pedestal de la lista y B a más de 100 m, cuando A pulsa A
  (`swctl.py --port 7777 press a`), entonces el evento no arranca (`framepace_in_event` falso; se consulta con un
  comando `coop story` nuevo), A ve "Esperando a Client" y B recibe el aviso (`coop overlay`, CU-10).
- **CA-2** (14a) Dado lo anterior, cuando B llega a menos de 30 m (`teleport`) y A pulsa A, entonces el evento
  arranca en A en menos de 2 s.
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
- **CA-8** (14b) Dado el santuario superado, entonces `Clear_Dungeon<n>` está a 1 en los dos y el número de orbes de
  cada uno cumple la regla de CU-07.
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
- No decidir por tu cuenta qué es un "momento de historia": es la tabla aprobada por el dueño.
- No copiar código ni tablas de proyectos de decompilación.
- No tocar `start_event` sin resolver antes la duda de vida de sus búferes (mapa 07 M8).

## Riesgos y preguntas abiertas

- Riesgo alto: interceptar el arranque de eventos en general necesita ingeniería inversa y es donde más fácil es
  colgar el juego.
- Riesgo: los momentos que arrancan por zona no se pueden retener con el botón A.
- Riesgo: bloqueo permanente si el compañero no puede llegar (desconectado, atascado): hace falta válvula de escape.
- Riesgo: santuarios y Bestias son zonas sin probar con el riesgo A2 de mapa 07.
- Riesgo: algunos efectos de flags solo se ven tras una carga (DOC:441-442).
- **Pregunta al dueño**: ¿qué cuenta exactamente como "momento de historia"? (lista propuesta arriba)
- **Pregunta al dueño**: ¿qué significa "presente": cerca (¿a cuántos metros?), o basta con estar en el mismo mapa?
- **Pregunta al dueño**: ¿los dos deben **ver** la escena, o basta con que el segundo esté allí y reciba el progreso?
- **Pregunta al dueño**: si el compañero se desconecta o no puede venir, ¿se puede continuar solo tras un plazo?
- **Pregunta al dueño**: si uno se salta la escena, ¿se salta para los dos?
- **Pregunta al dueño**: en un santuario, ¿hace falta que entren los dos para poder completarlo, o uno puede hacerlo
  solo y el otro recibe la recompensa?
- **Pregunta al dueño**: ¿aceptas que el teletransporte entre mapas, en su primera versión, pase por una pantalla de
  carga con guardado en un perfil cooperativo?

## Fase y dependencias

- **Fase**: F5 (una a una; requiere ingeniería inversa con la copia del dueño en Ghidra). Las pruebas de 14b pueden
  empezar en F4.
- **Depende de**: CU-06, CU-07, CU-08, CU-10; CU-05 para los cortes en mitad de un momento; CU-03 (`has_body`).
- **Lo necesitan**: ninguno.
