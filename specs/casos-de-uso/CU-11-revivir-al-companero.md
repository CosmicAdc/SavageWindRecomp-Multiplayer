# CU-11 · Revivir al compañero caído

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Cuando un jugador se queda sin corazones y su compañero sigue vivo no hay "game over". En la **primera entrega**
la reanimación es **automática**, como hace hoy el PR (reanimación instantánea por hada, sin que intervenga el
compañero); si caen los dos, fin de partida normal. El **rescate manual** (quedar caído y que el compañero te
levante) queda planificado para después, con prioridad baja (ADR-007, decisión 7; ADR-008, decisión 15).

## Actores

- **Caído**: el jugador que se queda sin vida.
- **Rescatador**: el compañero.

## Precondiciones

- Sesión conectada con dos jugadores (CU-01, CU-03).
- Regla de sesión "revivir" activada por el anfitrión (hoy `revive`, activada por defecto: NET:110, UI:123-126).

## Flujo principal

**Entrega 1 — reanimación automática (la que se construye ahora)**

1. El jugador pierde su último corazón mientras su compañero sigue vivo en la sesión.
2. En vez de la pantalla de fin de partida, se reanima solo en el sitio, con la animación del hada.
3. El compañero recibe un aviso en pantalla de que ha caído y se ha levantado (CU-10).
4. Si los dos caen a la vez, hay fin de partida normal: los dos recargan su último guardado.

**Entrega 2 — rescate manual (planificada para después, prioridad baja)**

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
| Los dos caen a la vez | Fin de partida normal para los dos: cada juego carga su último guardado (ADR-008, decisión 15) |
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

Va en dos entregas, porque la segunda necesita ingeniería inversa (mapa 07 §7: "retener la muerte sin game
over"). El dueño ha decidido que la primera sea la automática y que el rescate manual quede planificado para
después (ADR-007, decisión 7): la investigación en Ghidra del "estado caído" no se elimina, pero pasa a prioridad
baja y no bloquea el cierre de F4.

**Entrega 1 — sin ingeniería inversa (F4)**

1. Arreglar el refresco de la regla: mover las líneas OA:2838-2839 antes del `return` de OA:2837 (o a una función
   propia en `oa_rules`).
2. Condicionar `rt_online_has_fairy` (OA:2825-2831): responde sí solo si hay **otro jugador vivo** en la sesión
   (ningún otro con AS `Dead*`). **Sin límite de reanimaciones** (**[DUEÑO]** ADR-011, decisión 33: sin límite de usos
   mientras el compañero siga vivo). Si los dos están muertos,
   responde no → fin de partida normal.
3. Publicar "he caído" en el estado (un bit nuevo en `pose.effects` o campo propio; cambia el protocolo) y avisar al
   compañero con texto, sonido y marcador (CU-10).
4. **Sin penalización** tras la reanimación automática (**[DUEÑO]** ADR-011, decisión 33): revive con la vida que da el
   hada del juego. No se escribe la vida después y no hay ajuste del anfitrión.

**Entrega 2 — caído real con rescate (planificada para después, prioridad baja; requiere Ghidra)**

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

- **[DUEÑO] (ADR-007, decisión 7; Q-11-1; P3)** Revivir es **automático en la primera entrega**, como hace hoy el
  código. El **rescate manual queda planificado** para después, con prioridad baja.
- **[DUEÑO] (ADR-008, decisión 15; Q-11-2)** Si caen los dos a la vez, **fin de partida normal**: los dos recargan
  el último guardado.
- **[DUEÑO] D4**: los corazones son de cada jugador: revivir no transfiere vida del rescatador.
- **[DUEÑO] D8**: es un juego cooperativo por defecto; morir no debería separar a los jugadores.
- **[PROPUESTA]** (entrega 2) Contador de 30 s; revivir cuesta 3 s de botón mantenido.
- **[DUEÑO] (ADR-011, decisión 34; Q-11-3)** (entrega 2) Con el contador agotado, el caído **revive en el sitio con 3
  corazones**.
- **[DUEÑO] (ADR-011, decisión 33; Q-11-4)** (entrega 1) Se vuelve con la vida que da el hada del juego, **sin límite de
  usos** mientras el compañero siga vivo.
- **[DUEÑO] (ADR-011, decisión 35; Q-11-5)** Revivir **no cuesta nada** a quien rescata.
- La entrega 1 es suficiente para cerrar F4 (consecuencia de la decisión 7).
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
- Riesgo (aceptado por el dueño, ADR-011, decisión 33): reanimación ilimitada mientras el compañero siga vivo quita
  dificultad al juego.
- Preguntas ya contestadas por el dueño (Q-11-1 y Q-11-2): ver ADR-007 (decisión 7) y ADR-008 (decisión 15) en
  `../decisiones/`.
- Preguntas ya contestadas por el dueño (Q-11-3, Q-11-4 y Q-11-5): ver ADR-011 (decisiones 33, 34 y 35) en
  `../decisiones/`.

## Fase y dependencias

- **Fase**: F4 (entrega 1, automática). Entrega 2 (rescate manual): planificada para después, con prioridad baja;
  no bloquea el cierre de F4.
- **Depende de**: CU-03 (estabilidad alrededor de la muerte), CU-10 (aviso y marcador, vida en el estado), CU-08
  (llegar al caído).
- **Lo necesitan**: CU-12 (qué pasa al morir en PvP).
