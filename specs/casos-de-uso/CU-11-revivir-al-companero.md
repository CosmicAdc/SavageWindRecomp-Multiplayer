# CU-11 · Revivir al compañero caído

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Cuando un jugador se queda sin corazones no hay "game over": queda caído y su compañero puede levantarlo. Hoy el PR
hace otra cosa: una reanimación automática e instantánea por hada, sin que intervenga el compañero.

## Actores

- **Caído**: el jugador que se queda sin vida.
- **Rescatador**: el compañero.

## Precondiciones

- Sesión conectada con dos jugadores (CU-01, CU-03).
- Regla de sesión "revivir" activada por el anfitrión (hoy `revive`, activada por defecto: NET:110, UI:123-126).

## Flujo principal

1. El jugador pierde su último corazón.
2. En vez de la pantalla de fin de partida, queda **caído** en el sitio. Ve un contador y el texto "Esperando a
   <nombre>…".
3. El compañero recibe un aviso en pantalla y con sonido, y un marcador especial en la posición del caído (CU-10).
4. El compañero llega hasta él (andando o con el teletransporte de CU-08) y mantiene pulsado el botón de revivir
   unos segundos a menos de 2 m.
5. El caído se levanta con parte de sus corazones y sigue jugando en el mismo sitio.

## Flujos alternativos y de error

| Situación | Comportamiento esperado |
|---|---|
| El caído tiene un hada propia | Se usa primero, como en el juego normal (hoy ya es así: DOC:775-781) |
| El contador llega a cero | [PROPUESTA] El caído revive solo en el sitio con poca vida. Alternativa: reaparece junto al compañero |
| Los dos caen a la vez | [PROPUESTA] Fin de partida normal para los dos: cada juego carga su último guardado |
| El rescatador suelta el botón o le golpean | El progreso de revivir se reinicia |
| El rescatador está en otro mapa | No puede revivir; el contador sigue. El caído puede elegir "rendirse" |
| El caído cae en lava, agua profunda o vacío | [sin verificar: el juego tiene muertes que recolocan a Link; hay que ver cuáles pasan por la pregunta del hada] |
| Se desconecta el rescatador | El caído revive solo al agotar el contador, o pasa a fin de partida si la regla lo dice |
| Se desconecta el caído | En su juego ya no hay sesión: `rt_online_has_fairy` deja de responder "sí" (OA:2826) y tiene el fin de partida normal |
| El anfitrión desactiva la regla | Fin de partida normal del juego para quien muera |
| Muerte por PvP | Igual que cualquier otra muerte |
| El caído está en una escena | No aplica: en escena no se recibe daño [sin verificar] |

## Estado actual en el PR: **PARCIAL** (auto-reanimación por hada)

- El parche `has_fairy` (`tools/patches.py:616`, sitio en `tools/game_sites.py:205`) hace que la pregunta del juego
  "¿Link tiene un hada?" pase por `rt_online_has_fairy` (OA:2825-2831), que responde sí mientras hay sesión conectada
  y la regla está activa. Resultado: animación del hada, corazones de vuelta, sin recargar el mundo (DOC:775-781).
  Probado por los autores escribiendo la vida a 0: "revived by a fairy", vida a 12 cuartos, sin fin de partida.
- La regla es del anfitrión: viaja en el estado del jugador del sitio 0 (`pose.revive`) y se adopta en
  `players_collide` (OA:2838).
- **Fallo conocido**: esa adopción está después de un `return` temprano (OA:2837): con
  `SAVAGEWIND_ONLINE_NO_COLLIDE` o durante un evento la regla no se refresca (mapa 07 §2.2 y §6.3).
- Mientras el Link local está muerto (AS `Dead*`, `g_local_dead`, OA:1738) no se crean actores, y se esperan 180
  fotogramas al volver (OA:5873-5880), porque rehacerlos durante el fin de partida colgaba el juego (DOC:732-734).
- Los enemigos de un jugador muerto pasan a otro jugador (`can_own`, OA:2367-2370; OA:2548, OA:2571-2575).
- **No existe**: estado "caído", contador, interacción de revivir, aviso al compañero, ni la vida en el protocolo
  (CU-10).
- Herramientas de prueba que ya hay: `SAVAGEWIND_ONLINE_DIE_TEST=<s>` mata al Link local a los s segundos
  (OA:5922-5924); `SAVAGEWIND_ONLINE_LIFE_LOG`.

## Qué hay que construir

Se propone en dos entregas, porque la segunda necesita ingeniería inversa (mapa 07 §7: "retener la muerte sin game
over").

**Entrega 1 — sin ingeniería inversa (F4)**

1. Arreglar el refresco de la regla: mover las líneas OA:2838-2839 antes del `return` de OA:2837 (o a una función
   propia en `oa_rules`).
2. Condicionar `rt_online_has_fairy` (OA:2825-2831): responde sí solo si hay **otro jugador vivo** en la sesión
   (ningún otro con AS `Dead*`) y no se ha superado un límite de reanimaciones seguidas. Si los dos están muertos,
   responde no → fin de partida normal.
3. Publicar "he caído" en el estado (un bit nuevo en `pose.effects` o campo propio; cambia el protocolo) y avisar al
   compañero con texto, sonido y marcador (CU-10).
4. Penalización configurable por el anfitrión tras la reanimación automática (por ejemplo, volver con 3 corazones)
   [sin verificar: la reanimación del hada repone la vida que decide el juego; para cambiarla habría que escribir la
   vida después, en `player + 0x848`, como hace PvP en OA:2907-2921].

**Entrega 2 — caído real con rescate (F4 tardío o F5, requiere Ghidra)**

5. Encontrar cómo mantener a Link caído sin que el juego avance al fin de partida ni lo borre (sitio nuevo en
   `tools/game_sites.py` + parche en `tools/patches.py`; obliga a recompilar a los dos jugadores).
6. Máquina de estados en `oa_rules`: `vivo → caído (contador) → reviviendo (progreso) → vivo | fin de partida`.
   La decisión la toma el juego del **caído** (su vida es suya, D4); el rescatador solo envía "estoy reviviendo a X".
7. Interacción: botón mantenido a menos de 2 m. Leer el botón de `g_game_buttons` (`runtime/hle/input.c:1202-1205`)
   y bloquear A para que no dispare otra acción, como hace `g_online_block_a` (`runtime/hle/input.c:1206-1209`).
8. Validar en el caído: el rescatador está en el mismo mapa (`same_map`, OA:1484-1486), a menos de 2 m según su
   posición recibida, y vivo.
9. Barra de progreso y contador en el overlay.

## Reglas y decisiones de diseño

- **[PROPUESTA] P3**: revivir al compañero caído. Aceptada como propuesta; los detalles de abajo están por confirmar.
- **[DUEÑO] D4**: los corazones son de cada jugador: revivir no transfiere vida del rescatador.
- **[DUEÑO] D8**: es un juego cooperativo por defecto; morir no debería separar a los jugadores.
- **[PROPUESTA]** Contador de 30 s; revivir cuesta 3 s de botón mantenido; se vuelve con 3 corazones.
- **[PROPUESTA]** Si caen los dos, fin de partida normal.
- **[PROPUESTA]** La entrega 1 es suficiente para cerrar F4; la entrega 2 se decide tras probar la 1.
- **[PROPUESTA]** Un hada propia se sigue usando primero y no cuenta como reanimación del compañero.

## Criterios de aceptación

- **CA-1** (regla) Dado el montaje común con `revive` activado en A y `SAVAGEWIND_ONLINE_NO_COLLIDE=1` en B, cuando
  A cambia la regla a desactivada, entonces en menos de 2 s B la adopta (registro o comando `coop rules`), demostrando
  el arreglo de OA:2837-2839.
- **CA-2** (entrega 1) Dado A y B vivos y juntos, cuando B muere (`SAVAGEWIND_ONLINE_DIE_TEST=30` en B), entonces B no
  llega a la pantalla de fin de partida, el registro de B contiene `revived by a fairy (online play)`, y A recibe el
  aviso de caída en menos de 1 s (`coop overlay`, CU-10).
- **CA-3** (entrega 1) Dado A ya muerto, cuando B muere, entonces B tiene el fin de partida normal del juego.
- **CA-4** Dado B solo en la sesión (sin compañero), cuando muere, entonces fin de partida normal.
- **CA-5** Dado B que muere y revive, entonces 10 s después `swctl.py --port 7777 actors OnlinePlayer` devuelve 1
  actor y ninguno de los dos juegos se ha colgado (enlaza con CU-03 CA-7).
- **CA-6** (entrega 2) Dado B caído, cuando A mantiene el botón 3 s a menos de 2 m, entonces B vuelve a estar vivo con
  la vida definida en la regla; cuando A está a 5 m, el progreso no avanza.
- **CA-7** (entrega 2) Dado B caído, cuando pasan 30 s sin rescate, entonces ocurre lo que diga la regla del contador
  y queda registrado.
- **CA-8** (entrega 2) Dado un bot que envía "estoy reviviendo" desde otro mapa o a 100 m, entonces B no revive.
- **CA-9** Dado B con un hada propia en la bolsa (`give Animal_Insect_F 1` [sin verificar que `give` acepte ese
  objeto]), cuando muere, entonces se gasta su hada y no cuenta como reanimación cooperativa.

## Qué NO hacer

- No crear, borrar ni tocar actores remotos mientras el Link local está muerto ni en los 180 fotogramas siguientes
  (OA:5873-5880): eso ya colgó el juego.
- No escribir la vida de Link desde un hilo que no sea el del juego.
- No dejar que el rescatador decida la vida del caído: la decide el juego del caído tras validar distancia y mapa.
- No quitar el camino del hada propia del jugador.
- No añadir parches o sitios (`tools/patches.py`, `tools/game_sites.py`) sueltos: se agrupan, porque cada cambio
  obliga a recompilar el juego a los dos jugadores.
- No inventar direcciones ni offsets para "retener la muerte": si no está en los mapas, es trabajo de Ghidra con la
  copia del dueño y se documenta.
- No copiar de proyectos de decompilación.

## Riesgos y preguntas abiertas

- Riesgo alto: la zona de código alrededor de la muerte es la que más cuelgues ha dado (DOC:732-734; mapa 07 A5).
- Riesgo: la entrega 2 puede no ser viable sin mucha ingeniería inversa; por eso hay entrega 1.
- Riesgo: reanimación ilimitada quita toda dificultad al juego.
- **Pregunta al dueño**: ¿te vale como primera versión la reanimación automática "mientras el compañero siga vivo",
  o quieres desde el principio que el compañero tenga que ir a levantarte?
- **Pregunta al dueño**: ¿qué pasa si el contador se agota (revivir solo con poca vida, reaparecer junto al
  compañero, o fin de partida)?
- **Pregunta al dueño**: ¿qué pasa si caen los dos?
- **Pregunta al dueño**: ¿con cuántos corazones se vuelve, y hay límite de reanimaciones o tiempo de espera?
- **Pregunta al dueño**: ¿revivir cuesta algo al rescatador (un hada, comida, nada)?

## Fase y dependencias

- **Fase**: F4 (entrega 1). Entrega 2: F4 tardío o F5.
- **Depende de**: CU-03 (estabilidad alrededor de la muerte), CU-10 (aviso y marcador, vida en el estado), CU-08
  (llegar al caído).
- **Lo necesitan**: CU-12 (qué pasa al morir en PvP).
