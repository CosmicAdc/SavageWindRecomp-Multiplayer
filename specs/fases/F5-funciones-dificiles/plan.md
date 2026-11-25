# F5 — Funciones difíciles: plan

Lee antes `spec.md`. Referencias del checkout `pr294-online` (HEAD `350c5f8`): `OA:n` = `runtime/online_actors.c`,
`DOC:n` = `docs/ONLINE.md`, `MOD:n` = `tools/online_mod.py`. Tras F2 el código de `online_actors.c` está en módulos
`runtime/oa_*.c`: se busca por nombre de función.

## 0. Cómo se trabaja en esta fase

1. **Una función cada vez**, en el orden de §10. El dueño ya fijó lo principal `[DUEÑO]` (ADR-008, decisión 20):
   **pasarse los santuarios juntos (bloque C) va antes que el tinte 3D (A) y que la historia (E)**. Las secciones
   de este plan conservan su letra y su número (otras tareas las citan); el orden de trabajo es el de §10, no el
   de las secciones.
2. Cada función sigue el mismo ciclo: **pregunta → investigación → ADR (viable / no viable, camino elegido) →
   construcción → prueba en el juego**. Nadie construye antes del ADR.
3. Las investigaciones con Ghidra las hace el dueño con su copia (`tools/ghidra/README.md`, `tools/ghidra/gq.py`).
   Los agentes preparan las consultas, la lógica pura y las pruebas.
4. El resultado de una investigación con Ghidra se entrega siempre como **sitio por patrón** (receta del mapa 01
   §4: `tools/game_sites.py pattern <main> <dirección> <clase>` genera la línea `Site(...)`), con una descripción en
   palabras propias. Nunca desensamblado ni pseudocódigo en el repo.
5. Todo lo que cambie el comportamiento va tras una opción **apagada por defecto**.
6. Las respuestas del dueño (`../../decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`,
   `ADR-008-respuestas-del-dueno-ronda-2.md`, `ADR-009-respuestas-del-dueno-ronda-3.md`) mandan sobre este plan. El
   límite sigue en 2 jugadores: lo nuevo se diseña para N y se prueba con 2 (ADR-007, decisión 12).

## 1. A — Tinte 3D del segundo Link (CU-13)

### Hoy

- El traductor de sombreadores emite, en los sombreadores de fragmento que escriben color, un bloque
  `uniform Tint { vec4 ltint[2]; }` en el enlace 19 (`runtime/nvn/maxwell/translate.c:2310`) y al final mezcla el
  color del **objetivo 0** hacia el tinte con peso `ltint[0].w` (`:2424-2425`). El contrato lo documenta
  `runtime/nvn/maxwell/shader.h:15` y `:89` ("applied to color target 0").
- El renderizador lo enlaza en cada dibujo pero siempre con ceros: `float tint[8] = {0};` con el comentario "no
  game-specific highlight sets one yet" (`runtime/nvn/vulkan/exec.c:9161`); se deduplica (`DEDUPE_TINT`, `:7487`),
  así que el coste es nulo.
- El renderizador **no sabe a qué actor pertenece un dibujo**. Identifica sombreadores por un hash
  (`g_cur_fs_hash`), pero el Link local y el del compañero comparten sombreador.
- El juego es de sombreado diferido. La documentación del proyecto dice que el color base del G-buffer está en el
  objetivo 1 (mapa 04 §4b cita `docs/SAVAGE-WIND.md`), y qué contiene el objetivo 0 **no está verificado**.
- El Link del compañero es el actor `OnlinePlayer`, hecho con la lista de modelos de `PauseMenuPlayer` (MOD:277-286),
  y su ropa son actores `OnlineArmor_*` con el modelo de cada pieza. Si esos modelos comparten o no texturas y
  buffers con los del Link local **no está verificado**.
- Del lado del juego existen: el parámetro de tinte de armadura `ArmorDyeColor`, que ya viaja y se aplica al crear
  la pieza pero "not seen yet" (OA:6069-6077, DOC:1003-1008), y dos escritores de parámetros de material
  (`FN_SPECIAL_FX_B` y `FN_SPECIAL_FX_A_UNIT`, OA:1458 y OA:4011) que producen el brillo dorado de Magnesis y el
  pulso rojo de Stasis: valores escalares, no un color libre.

### Qué se quiere `[DUEÑO]`

Un **tinte suave sobre todo el Link del compañero**; el Link propio se ve normal; siempre acompañado del nombre y
de la forma del marcador, para accesibilidad (ADR-007, decisión 8). Si el tinte no es viable, el resultado final
es **nombre en color y marcador con forma** (lo que ya entrega F4), y eso **no bloquea** ningún otro bloque de F5
(ADR-009, decisión 22).

### Preguntas de investigación (en este orden; cada una puede cerrar el bloque)

| Tarea | Pregunta | Herramienta |
|---|---|---|
| T503 | ¿Qué dibujos pintan los píxeles del compañero, en qué pases y con qué hash de sombreador? | `SAVAGEWIND_DEBUG_PICK=<x>,<y>` y `SAVAGEWIND_DEBUG_FRAME=<f>` (mapa 04 §5.3) |
| T504 | ¿Qué contiene cada objetivo de color del pase de G-buffer de un personaje? ¿En cuál está el color base? | `SAVAGEWIND_DEBUG_FRAME` (vuelca cada pase a imagen) y `SAVAGEWIND_DEBUG_PICK_RT=<i>` |
| T505 | ¿Hay algún dato del dibujo que distinga al compañero del Link local sin ingeniería inversa? (direcciones de textura, de buffer de vértices, de constantes; posición) | Registros `[draw]` del fotograma de depuración; identidad por dibujo que ya calcula `runtime/nvn/vulkan/mvec.c` |
| T506 | Solo si T505 no basta: ¿existe una función del juego que dibuje un modelo entero y se pueda envolver con un marcador, y de dónde sale el buffer de huesos de un modelo? | Ghidra (dueño) |
| T507 | Alternativa por el juego: ¿se ve el tinte de armadura en una pieza `OnlineArmor_*`? ¿Sirve el brillo de material como "aura" de jugador? | Dos juegos; `SAVAGEWIND_LOG_FS_CB` |

### Caminos de construcción (el ADR T508 elige uno)

1. **Por datos del dibujo** (si T505 da una señal fiable): en `draw_` de `exec.c`, justo donde hoy se pone el tinte
   a cero, una función propia `coop_tint_for_draw(...)` devuelve el tinte del jugador si el dibujo es suyo. La lista
   de "señales del compañero" la publica el hilo del juego en una estructura propia con mutex (regla del mapa 04
   §3.3: el hilo de traducción no espera a nadie).
2. **Por ámbito** (si T506 encuentra la función): un registro solo-host alrededor del dibujo del actor, como hace
   `nvn_ui_scope` para la interfaz (`runtime/nvn/cmdbuf.c`), y `exec_list` aplica el tinte dentro del ámbito.
3. **Por el juego** (si T507 funciona): sin tocar el renderizador.
4. **No viable**: se queda el nombre en color y el marcador con forma del overlay 2D (F4) y se documenta
   `[DUEÑO]` (ADR-009, decisión 22). No hace falta volver a preguntar al dueño para cerrar así el bloque.

Si el color base no está en el objetivo 0 (T504), los caminos 1 y 2 necesitan que el traductor tiña otro objetivo:
eso cambia el identificador del traductor e invalida las cachés de todos. Solo con aprobación expresa (la parte de
P5-4 que sigue abierta); sin ella, se cierra por el camino 4.

## 2. B — Teletransporte entre mapas (CU-14)

### Hoy

- Cada STATE lleva el mapa del jugador (`pose.map`, obtenido con `FN_MAP_TYPE` / `FN_MAP_NAME`, OA:1470-1486;
  ejemplos de valores en DOC:404-407). Un jugador en otro mapa no se crea (`same_map` en la decisión de OA:6013-6014).
- El teletransporte del runtime (`control_tp_request`, `runtime/control_tp.c:298`) escribe la posición de Link: no
  cambia de mapa. "A shrine's or a Divine Beast's needs walking in" (DOC:747-748).
- El único camino del runtime que cambia de mapa es de pruebas: reescribir la posición y el mapa guardados cuando el
  juego lee el guardado (`runtime/services/test_warp.c`, con `SAVAGEWIND_TEST_WARP`; mapa 03 §1.2).

### Diseño

| Camino | Qué hace | Necesita |
|---|---|---|
| 1. Cambio de mapa en caliente | Pedir al juego que cargue el mapa del compañero y coloque a Link en su punto de entrada | **Investigación T511 (Ghidra)** |
| 2. Recarga dirigida | Guardar, reescribir en memoria la posición y el mapa del guardado al leerlo (mecanismo de `test_warp`) y forzar una carga | Spike T512 (sin Ghidra): ¿se puede disparar la carga del último guardado desde el runtime? |
| 3. Guía | Marca hacia la entrada del santuario donde está el compañero; se entra andando | Nada (ya hay marcas de F4) |

`[DUEÑO]` (ADR-008, decisión 20): el camino 2 está **aceptado** tal cual, con su pantalla de carga y su guardado
(en el perfil cooperativo de F4, nunca en la partida individual del invitado). Si la recarga corta la sesión, el
sitio del jugador queda reservado 10 minutos (ADR-008, decisión 18), de sobra para volver a entrar.

Se construye el primero que resulte viable; el 3 queda siempre como mínimo. En todos, el destino es **la entrada del
mapa**, no la posición exacta del compañero (un punto arbitrario dentro de un santuario puede estar tras una puerta
cerrada).

## 3. C y D — Santuarios y Bestias Divinas juntos

`[DUEÑO]` (ADR-008, decisión 20): **que los dos puedan pasarse los santuarios juntos** es el objetivo de fondo de
la fase. El bloque C es el **primero** que se trabaja (§10) y va antes que el tinte 3D y que la historia. No basta
con poder llegar al mapa del compañero: los dos deben verse y jugar dentro. Hacerlos juntos es posible, no
obligatorio: el monje de un santuario no exige a los dos (ADR-007, decisión 3). Las Bestias (bloque D) siguen al
final.

### Hoy

- Quien está en un santuario o en una Bestia Divina **no aparece** en el mundo de los demás (`docs/ONLINE.md`,
  sección "Players on other maps"), y la tabla de huecos del mapa 07 dice de "Shrines/Divine Beasts together":
  "Partial, untested, high". Lo que sigue en esta lista sale de leer la documentación y el código, no de haberlo
  visto: por eso el bloque empieza con T540.

- En el mismo santuario los jugadores se ven; sus bolas y cajas van como objetos del mundo (DOC:996-997).
- Mecanismos (`movers`): cada juego envía los que mueve y los demás los colocan (OA:4787-4980; DOC:971-986). Probado
  solo en la entrada de un santuario; "a lift ridden for real and a Divine Beast's parts aren't tested yet".
- El rompecabezas no está en flags: cada entrada lo empieza de nuevo (DOC:996).
- Enemigos: los comparte el anfitrión desde **su** juego; un jugador en otro mapa "has none of its enemies"
  (DOC:406-407; `can_own`, OA:2367-2370). Consecuencia probable: en un santuario donde no está el anfitrión, cada
  invitado simula sus propios enemigos **[inferencia, sin verificar]**.
- Una Bestia vista en movimiento desde fuera: pendiente (DOC:1013-1014).

### Diseño

0. **Investigación previa** (T540), antes de planificar nada más: con dos instancias en un PC, qué ocurre hoy
   cuando los dos entran al mismo santuario. Resultado en `specs/bitacora/`. Solo necesita F1 (dos juegos
   funcionando), así que puede hacerse en cuanto se abra la fase, o antes.
1. **Matriz de pruebas** (T514), con lo aprendido en T540 y antes de tocar código: 8 santuarios que cubran 4 tipos de mecanismo (ascensor,
   plataformas móviles, bolas/cajas con interruptor, aparato de giroscopio) y uno de combate. Para cada uno: qué se
   ve igual, qué diverge, qué falla.
2. **Arreglos** según lo que salga. El único que se puede anticipar leyendo el código es el de enemigos sin
   anfitrión (T515): que el servidor trate como "dueño de la lista" al jugador de menor ranura presente en ese mapa.
3. **Recompensa**: el orbe es para los dos, siempre `[DUEÑO]` (ADR-007, decisión 2). Si uno no estaba o entró
   tarde, lo iguala la tarea de ponerse al día al entrar de F4 (ADR-008, decisión 13); F5 solo lo comprueba.
4. **Bestias** (T516, T517): misma matriz dentro; "vista desde fuera" es investigación.

## 4. E — Momentos de historia con los dos presentes (CU-14)

### Hoy

- Un juego que está en el evento del pedestal de la piedra o de una torre lo anuncia con el bit `ONLINE_E_DEMO`
  (`runtime/online.h`, enumeración de efectos); la lista de eventos está escrita en el código (OA:1625-1633;
  DOC:458-459 cita `Obj_DRStone_A_01`, `Demo700_0`, `Demo701_0`, `Demo004_0`, `Demo164_0`).
- Otro jugador a menos de 30 m tiene su botón A retenido (`g_online_block_a`, OA:5981-6001;
  `runtime/hle/input.c:1207-1208`): evita que dos jugadores empiecen lo mismo a la vez. **No** exige presencia.
- El runtime sabe si hay un evento activo y su nombre (`framepace_in_event`, `framepace_event_name`,
  `runtime/hle/framepace.c`), y registra `[event] <nombre>` una vez por evento (mapa 03 §1.5).
- El progreso de un evento se propaga por flags aunque el otro no esté.
- Existe `start_event` para lanzar un evento en el otro juego (OA:3703-3713), con riesgos conocidos (libera sus
  metadatos justo después, mapa 07 M8; un evento fuera de contexto puede colgar, DOC:751-752).

### Reglas `[DUEÑO]`

- **Qué es un momento de historia** (ADR-007, decisión 3): lista corta. Inicio (piedra y primera torre), final de
  cada Bestia Divina, recuerdos y jefes finales. **Los santuarios no**: ningún bloqueo en los 120 santuarios ni
  ante su monje.
- **Qué es "presente"** (ADR-007, decisión 4): mismo mapa y a menos de 30 m. Si falta uno, el juego avisa y ofrece
  teletransportarlo.
- **Seguir sin el compañero** (ADR-007, decisión 6): el anfitrión puede continuar solo de inmediato, tras un
  diálogo de confirmación. Sin temporizador de desconexión. El plazo de reconexión de 10 minutos (ADR-008,
  decisión 18) es cosa aparte.
- **Versión 1** (ADR-009, decisión 21): basta con estar allí. Uno ve la escena; el otro está presente y recibe el
  progreso. Que los dos la vean a la vez es una mejora posterior, **sin tareas activas** en esta fase.

Con esto el bloque E **ya no espera ninguna respuesta del dueño**.

### Diseño

1. **Definición** (T501): resuelta por los ADR citados arriba. No queda nada que decidir antes de empezar.
2. **Catálogo** (T518): jugar con `[event]` en el registro y construir la tabla `nombre de evento → cómo arranca
   (botón / zona / otro evento)` para cada momento de la lista corta.
3. **Lógica pura** (T520) `runtime/coop_story.c`: tabla de eventos y `story_gate(evento, compañeros) →
   permitir / esperar`, con las reglas de arriba: 30 m y mismo mapa; el anfitrión puede saltarse la espera con su
   confirmación.
4. **Puerta por botón** (T521): generaliza el bloqueo actual. Hoy se retiene A cuando *otro* está en el pedestal;
   la puerta nueva retiene A cuando *yo* estoy junto a un punto de arranque de la tabla y mi compañero no está
   presente. Problema: saber que "estoy junto a un punto de arranque" antes de que el evento empiece. Sin ingeniería
   inversa solo se puede con una tabla de posiciones de esos puntos, anotadas por el dueño al hacer el catálogo
   (dejar empezar el evento y cancelarlo después no es una opción: no se conoce una forma limpia de cancelar). Por
   eso T521 depende del catálogo y se limita a los puntos con posición conocida.
   **Revisión del bloqueo existente** (ADR-007, consecuencia de la decisión 3): el bloqueo de A en pedestales que
   trae el PR cubre la piedra y las torres. Hay que contrastarlo con la lista corta: la exigencia de presencia vale
   solo para la piedra y la **primera** torre; en las demás torres el bloqueo actual se conserva únicamente como
   protección contra empezar lo mismo a la vez (que es lo que hace hoy), sin exigir presencia.
5. **Puerta por zona** (T522): necesita la investigación T519: ¿por qué función pasan las peticiones de inicio de
   evento y se puede **aplazar** una sin romper el estado?
6. **Un solo juego reproduce la escena** (versión 1): la ve quien la dispara. El otro está presente y recibe el
   progreso por los flags, como ya ocurre hoy. No se lanza la escena por la red salvo en los casos que ya hace el
   PR (piedra, Kolog). El bloqueo de A actual sigue evitando que los dos empiecen lo mismo a la vez.
7. **Interfaz** (T523): aviso "Esperando a tu compañero", marca hacia el punto, **oferta de teletransportar** al
   que falta (por el mecanismo de invocar con aceptación de F4: ADR-008, decisión 16) y, solo para el anfitrión,
   el **diálogo de confirmación** para continuar solo.

## 5. F — Colisión con el compañero

### Hoy

- La marioneta es un actor de perfil DemoNPC sin cuerpo físico; al generar `OnlinePlayer` se quitan las partes de
  contacto y de cuerpos rígidos de la física (MOD:277-286 según el mapa 07 §1.2).
- Cada juego empuja a **su** Link fuera del otro: 0,7 m entre centros y 1,7 m de altura, escribiendo la posición
  real (`players_collide`, OA:2833-2872; DOC:690-694). No hay colisión con el mundo ni se puede subir encima.
- Pendiente declarado: "Bodies for the players' puppets" (DOC:1009-1010).
- Experiencia previa con marionetas de enemigo: con controlador de personaje "it stays where it stands" (DOC:576-577).

### Diseño

Investigación T524 (experimento con `tools/online_mod.py`, sin tocar el juego): generar `OnlinePlayer` con un cuerpo
rígido simple y moverlo cada fotograma con `FN_BODY_TO` como se hace con los objetos del mundo. Preguntas: ¿se
dibuja?, ¿el Link local choca con él?, ¿los enemigos lo tratan como obstáculo?, ¿baja los fps? Si funciona, T525
sustituye el empuje por el cuerpo; si no, el empuje se queda.

## 6. G — Más de 2 jugadores

### Hoy (cálculo del mapa 06 §2.5, no medido)

- Cada jugador sube STATE (3375 bytes × 20 por segundo ≈ 68 kB/s) y SKEL (121 huesos, ~1,6 kB × 20 ≈ 30 kB/s): unos
  **100 kB/s por jugador**, enteros aunque nada cambie.
- El servidor reenvía cada flujo a los N−1 restantes: el total crece con N². Con 8 jugadores, unos 56 flujos y del
  orden de 5-6 MB/s de subida del anfitrión. `docs/ONLINE.md:1001` lo lista como pendiente.
- El juego ya ignora a los jugadores de otro mapa o a más de 400 m (OA:6013-6018), pero el servidor se los envía
  igualmente.
- Supuestos frágiles: reglas de sesión leídas de `pl[0]` (OA:2838-2840); marionetas de enemigo, botín y pruebas
  usan el pool del jugador 0 (mapa 07 §6.3).

### Diseño (de menos a más invasivo)

| Paso | Qué | Ahorro esperado | Tarea |
|---|---|---|---|
| 0 | Medir con bots en Docker (2, 4, 8) | — | T526 |
| 1 | **Filtrado por interés** en el servidor: no reenviar STATE/SKEL de A a B si están en mapas distintos o a más de 450 m; enviar un STATE reducido cada 1 s para que la lista y las flechas sigan funcionando | Grande cuando los jugadores se separan | T527, T528 |
| 2 | **Tasa por distancia**: 20 Hz hasta 60 m, 10 Hz hasta 200 m, 5 Hz más allá | Medio | T528 |
| 3 | **STATE por bloques**: núcleo pequeño (posición, rumbo, indicadores) siempre; el resto (equipo, efectos, objetos, mecanismos…) solo cuando cambia, con reenvío periódico | Grande incluso juntos | T529, T530 |
| 4 | **SKEL solo huesos movidos** | Medio | T531 |
| 5 | Quitar supuestos de ranura 0 y de 2 jugadores | Corrección | T532 |

El servidor necesita leer posición y mapa del STATE de cada cliente (ya guarda el último, `SClient.last`,
`runtime/online.c:194-204`).

## 7. H — Menos direcciones fijas de 1.9.0

### Hoy (mapa 07 §3)

- `online_actors.c` usa **62 direcciones absolutas** (47 funciones y 15 punteros de datos) escritas como literales,
  frente a 16 sitios por patrón nuevos del PR. Dos funciones están definidas y sin uso (`FN_ARMOR_INIT`,
  `FN_SPECIAL_FX_A`).
- No hay comprobación de versión en ese fichero (F1 añade una puerta: no llamar si el juego no es 1.9.0).
- Regla del proyecto (AGENTS.md): patrones en `tools/game_sites.py`, y `g_site` en el runtime.

### Diseño

1. **Inventario** (T533): guion que lista cada literal `0x70…` con su nombre, línea, módulo y si es función o dato.
2. **Funciones** (T534, T535): para cada una, `tools/game_sites.py pattern <main> <dirección> code` genera el
   patrón desde el binario del dueño. Se añade a `SITES`, a `KNOWN['1.9.0']`, a un grupo opcional `online_*` por
   subsistema, y a `GAME_SITES_OPTIONAL` en `runtime/game_sites.h`. El literal se sustituye por `g_site.<nombre>` y
   cada subsistema comprueba al empezar que su grupo está presente.
3. **Punteros de datos** (T536): un puntero global no tiene patrón propio: hay que encontrar una instrucción que lo
   referencie (clase `ref` o `got`). Eso exige mirar referencias cruzadas en Ghidra (`gq.py xrefs`): tarea del dueño,
   con un agente preparando la lista.
4. **Limpieza** (T537): quitar las dos funciones sin uso y reunir los desplazamientos de estructura en una cabecera
   con una sola comprobación de versión.
5. **Lotes**: todo el bloque H son **dos o tres** cambios de `GAMELIB_ID` como mucho.

## 8. Carriles y propiedad de ficheros

En F5 casi no hay paralelismo; aun así, estos ficheros tienen dueño único:

| Fichero | Dueño |
|---|---|
| `runtime/online.h`, `PROTOCOL`, cabecera compartida con el bot | Dueño del protocolo (T502 y cualquier cambio posterior) |
| `tools/game_sites.py`, `tools/patches.py`, `runtime/game_sites.h` | Un solo agente por lote; nunca dos lotes abiertos a la vez |
| `runtime/nvn/vulkan/exec.c` | Solo T509, y solo una función nueva más una llamada |
| `runtime/ui/online_ui.mm`, `runtime/ui/coop_hud.mm` | Carril UI |
| `runtime/ui/menu.mm`, `runtime/settings.*` | Integrador (no deberían tocarse) |
| `runtime/ui/i18n/*.inc` | Carril i18n (T538) |
| `tools/online_mod.py` | Solo T524/T525 (cambiarlo obliga a subir su número de versión y a regenerar `online_mod/` en los dos PC) |

Se pueden hacer en paralelo sin riesgo: la lógica pura (`runtime/coop_story.c`, `coop_interest.c`,
`coop_statecodec.c` con sus pruebas), las medidas en Docker y la redacción de consultas para Ghidra. `coop_story.c`
(T520) ya no espera ninguna decisión, solo el catálogo T518. La investigación T540 no toca código, pero necesita el
turno de los dos juegos: no coincide con ninguna otra prueba en el juego.

## 9. Coordinación de la versión de protocolo

- Un salto al principio de F5 (T502) con lo que se sabe seguro: indicadores de historia en STATE (esperando,
  en evento de la tabla), mensaje STORY (evento, estado; incluye "el anfitrión continúa solo"), color 3D activado,
  y un STATE reducido para el filtrado.
- El STATE por bloques (T530) es un cambio de cable grande: **segundo salto**, único, coordinado por el dueño del
  protocolo, con el bot y las pruebas actualizados en el mismo cambio.
- Nada más cambia la versión. Las investigaciones no tocan el cable.

## 10. Orden de trabajo

`[DUEÑO]` (ADR-008, decisión 20): **C va antes que A y que E**. El resto del orden es propuesta (lo que queda
abierto de P5-5) y el dueño puede cambiarlo.

1. **C — Santuarios juntos** (T540, luego T514 y T515). Empieza por la investigación T540, que solo necesita F1.
2. **B — Teletransporte entre mapas** (T511-T513): completa el objetivo de los santuarios (llegar adonde está el
   compañero sin entrar andando). C no depende de B: a la matriz se entra andando.
3. **H — Direcciones fijas** (T533-T537): baja el riesgo de todo lo demás y no depende de decisiones. T533 lleva
   `[P]` y puede adelantarse; los lotes T534-T536 obligan a recompilar el juego, así que no se abren a mitad de la
   matriz de santuarios.
4. **E — Historia** (T518-T523). Ya no espera respuestas del dueño (T501 quedó resuelta por los ADR).
5. **A — Tinte 3D** (T503-T510). Si sale "no viable", se cierra con nombre en color y marcador con forma y no
   retrasa nada (ADR-009, decisión 22).
6. **G — Más de 2 jugadores** (T526-T532).
7. **F — Colisión** (T524-T525) y **D — Bestias** (T516-T517).

Dependencias que cambian con este orden:

- T514 depende ahora de T540. T515 y T516 siguen dependiendo de T514.
- T518 y T520 dejan de depender de una decisión pendiente: T501 está resuelta.
- Nada de C, B o E depende del bloque A.
- `tasks.md` lista los bloques en este mismo orden; los identificadores de tarea no cambian.
