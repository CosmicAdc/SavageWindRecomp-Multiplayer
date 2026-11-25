# F5 — Funciones difíciles: plan

Lee antes `spec.md`. Referencias del checkout `pr294-online` (HEAD `350c5f8`): `OA:n` = `runtime/online_actors.c`,
`DOC:n` = `docs/ONLINE.md`, `MOD:n` = `tools/online_mod.py`. Tras F2 el código de `online_actors.c` está en módulos
`runtime/oa_*.c`: se busca por nombre de función.

## 0. Cómo se trabaja en esta fase

1. **Una función cada vez**, en el orden que fije el dueño (pregunta P5-5).
2. Cada función sigue el mismo ciclo: **pregunta → investigación → ADR (viable / no viable, camino elegido) →
   construcción → prueba en el juego**. Nadie construye antes del ADR.
3. Las investigaciones con Ghidra las hace el dueño con su copia (`tools/ghidra/README.md`, `tools/ghidra/gq.py`).
   Los agentes preparan las consultas, la lógica pura y las pruebas.
4. El resultado de una investigación con Ghidra se entrega siempre como **sitio por patrón** (receta del mapa 01
   §4: `tools/game_sites.py pattern <main> <dirección> <clase>` genera la línea `Site(...)`), con una descripción en
   palabras propias. Nunca desensamblado ni pseudocódigo en el repo.
5. Todo lo que cambie el comportamiento va tras una opción **apagada por defecto**.

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
4. **No viable**: se queda el color en el overlay 2D (F4) y se documenta.

Si el color base no está en el objetivo 0 (T504), los caminos 1 y 2 necesitan que el traductor tiña otro objetivo:
eso cambia el identificador del traductor e invalida las cachés de todos. Solo con aprobación expresa (P5-4).

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

Se construye el primero que resulte viable; el 3 queda siempre como mínimo. En todos, el destino es **la entrada del
mapa**, no la posición exacta del compañero (un punto arbitrario dentro de un santuario puede estar tras una puerta
cerrada).

## 3. C y D — Santuarios y Bestias Divinas juntos

### Hoy

- En el mismo santuario los jugadores se ven; sus bolas y cajas van como objetos del mundo (DOC:996-997).
- Mecanismos (`movers`): cada juego envía los que mueve y los demás los colocan (OA:4787-4980; DOC:971-986). Probado
  solo en la entrada de un santuario; "a lift ridden for real and a Divine Beast's parts aren't tested yet".
- El rompecabezas no está en flags: cada entrada lo empieza de nuevo (DOC:996).
- Enemigos: los comparte el anfitrión desde **su** juego; un jugador en otro mapa "has none of its enemies"
  (DOC:406-407; `can_own`, OA:2367-2370). Consecuencia probable: en un santuario donde no está el anfitrión, cada
  invitado simula sus propios enemigos **[inferencia, sin verificar]**.
- Una Bestia vista en movimiento desde fuera: pendiente (DOC:1013-1014).

### Diseño

1. **Matriz de pruebas** (T514) antes de tocar código: 8 santuarios que cubran 4 tipos de mecanismo (ascensor,
   plataformas móviles, bolas/cajas con interruptor, aparato de giroscopio) y uno de combate. Para cada uno: qué se
   ve igual, qué diverge, qué falla.
2. **Arreglos** según lo que salga. El único que se puede anticipar leyendo el código es el de enemigos sin
   anfitrión (T515): que el servidor trate como "dueño de la lista" al jugador de menor ranura presente en ese mapa.
3. **Bestias** (T516, T517): misma matriz dentro; "vista desde fuera" es investigación.

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

### Diseño

1. **Definición** (T501, dueño): qué eventos exigen a los dos. Es la pregunta P5-1 y bloquea todo el bloque.
2. **Catálogo** (T518): jugar con `[event]` en el registro y construir la tabla `nombre de evento → cómo arranca
   (botón / zona / otro evento)`.
3. **Lógica pura** (T520) `runtime/coop_story.c`: tabla de eventos y `story_gate(evento, compañeros) →
   permitir / esperar`, con las reglas de P5-2 y P5-3.
4. **Puerta por botón** (T521): generaliza el bloqueo actual. Hoy se retiene A cuando *otro* está en el pedestal;
   la puerta nueva retiene A cuando *yo* estoy junto a un punto de arranque de la tabla y mi compañero no está
   presente. Problema: saber que "estoy junto a un punto de arranque" antes de que el evento empiece. Sin ingeniería
   inversa solo se puede con una tabla de posiciones de esos puntos, anotadas por el dueño al hacer el catálogo
   (dejar empezar el evento y cancelarlo después no es una opción: no se conoce una forma limpia de cancelar). Por
   eso T521 depende del catálogo y se limita a los puntos con posición conocida.
5. **Puerta por zona** (T522): necesita la investigación T519: ¿por qué función pasan las peticiones de inicio de
   evento y se puede **aplazar** una sin romper el estado?
6. **Arranque para el otro**: cada juego reproduce su propia escena cuando su jugador la dispara; como ambos están
   presentes, el segundo la dispara en cuanto el primero termina (el bloqueo de A actual ya ordena esto). No se
   lanza la escena por la red salvo en los casos que ya hace el PR (piedra, Kolog).
7. **Interfaz** (T523): aviso "Esperando a tu compañero" y marca hacia el punto.

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
`coop_statecodec.c` con sus pruebas), las medidas en Docker y la redacción de consultas para Ghidra.

## 9. Coordinación de la versión de protocolo

- Un salto al principio de F5 (T502) con lo que se sabe seguro: indicadores de historia en STATE (esperando,
  en evento de la tabla), mensaje STORY (evento, estado), color 3D activado, y un STATE reducido para el filtrado.
- El STATE por bloques (T530) es un cambio de cable grande: **segundo salto**, único, coordinado por el dueño del
  protocolo, con el bot y las pruebas actualizados en el mismo cambio.
- Nada más cambia la versión. Las investigaciones no tocan el cable.

## 10. Orden propuesto (pendiente de P5-5)

1. H (T533-T537): baja el riesgo de todo lo demás y no depende de decisiones.
2. B y C (T511-T515): es lo que más se nota al jugar.
3. E (T501, T518-T523).
4. A (T503-T510).
5. G (T526-T532).
6. F (T524-T525) y D (T516-T517).
