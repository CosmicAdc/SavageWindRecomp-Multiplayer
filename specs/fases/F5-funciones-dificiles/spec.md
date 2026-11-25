# F5 — Funciones difíciles: especificación

Estado: borrador para revisión del dueño. Depende de F4 terminada. **Se trabaja una función cada vez**, no en
paralelo: casi todas necesitan que el dueño investigue con Ghidra sobre su propia copia del juego, o pruebas largas
dentro del juego.

Convenciones:

- Referencias `fichero:línea` del checkout del PR #294 (`pr294-online`, HEAD `350c5f8`). `OA:n` =
  `runtime/online_actors.c`; `DOC:n` = `docs/ONLINE.md`; `MOD:n` = `tools/online_mod.py`. Las referencias al
  renderizador que vienen del mapa 04 (rama `test`) se han vuelto a localizar en el PR donde se indica.
- "[sin verificar]" = no comprobado. En esta fase hay muchas: por eso cada bloque empieza con investigación.
- **Decisiones del dueño.** Las tres rondas de respuestas (`../../decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`,
  `ADR-008-respuestas-del-dueno-ronda-2.md` y `ADR-009-respuestas-del-dueno-ronda-3.md`) mandan sobre este
  documento. Lo que sale de ellas va marcado `[DUEÑO]` con el ADR y el número de decisión.
- **Este documento no contiene direcciones, desplazamientos ni estructuras nuevas.** Las que cita ya están en el
  código del PR. Lo que falte se obtiene del Ghidra del dueño y entra como sitio por patrón. No se copia nada de
  proyectos de descompilación.

## 1. Objetivo

Resolver, de una en una, las funciones que no se pueden construir solo con lógica de red y de interfaz:

1. **Pasarse los santuarios juntos** (CU-14): que los dos se vean y jueguen dentro del mismo santuario. Es el
   objetivo prioritario de la fase `[DUEÑO]` (ADR-008, decisión 20) y va **antes** que el tinte 3D y que la
   historia. No es obligatorio hacerlos juntos: un santuario no es un momento de historia (ADR-007, decisión 3).
2. **Jugar juntos en otros mapas**: teletransporte entre mapas y Bestias Divinas (CU-14).
3. **Momentos de historia solo con los dos presentes** (CU-14).
4. Que el segundo Link se distinga por un **color en su modelo 3D** (CU-13).
5. **Colisión** con el compañero.
6. **Más de 2 jugadores** sin que el ancho de banda se dispare.
7. **Menos direcciones fijas de 1.9.0** en el código.

## 2. Casos de uso que cubre

| CU | Hoy (verificado) | Qué entrega F5 |
|---|---|---|
| CU-13 Color en el modelo 3D del segundo Link | Nada en el renderizador. Existe un tinte por dibujo cableado pero siempre a cero (`runtime/nvn/vulkan/exec.c:9161`). El color de ranura solo se ve en el overlay 2D | Tinte suave sobre todo el modelo del compañero (el Link propio se ve normal), tras una opción apagada por defecto `[DUEÑO]` (ADR-007, decisión 8). Si no es viable: nombre en color y marcador con forma, sin bloquear el resto de F5 `[DUEÑO]` (ADR-009, decisión 22) |
| CU-14a Teletransporte entre mapas | Un jugador en otro mapa no tiene Link en el mundo de los demás (`same_map`, OA:1484-1486; decisión en OA:6013-6014). El teletransporte solo funciona dentro de un mapa (DOC:747-748) | Ir con el compañero aunque esté en un santuario o una Bestia. Aceptado con pantalla de carga y guardado `[DUEÑO]` (ADR-008, decisión 20) |
| CU-14b Santuarios y Bestias juntos | Los que están en el mismo santuario se ven y sus objetos móviles se envían (DOC:996-997); los mecanismos se sincronizan pero no está probado montado ni dentro de una Bestia (DOC:984-986, 1013-1014) | **Prioritario** `[DUEÑO]` (ADR-008, decisión 20). Investigación de qué pasa hoy (T540), matriz de pruebas, arreglos y límites documentados |
| CU-14c Momentos de historia con los dos | Solo existe un bloqueo del botón A a 30 m de un jugador que está en el pedestal de la piedra o de una torre (OA:5981-6001; `runtime/hle/input.c:1207-1208`) | Una puerta de presencia para la lista corta del dueño (inicio: piedra y primera torre; final de cada Bestia Divina; recuerdos; jefes finales). Los santuarios quedan fuera `[DUEÑO]` (ADR-007, decisión 3) |

Fuera de los CU numerados, pedidos por el dueño: colisión con el compañero, Bestias Divinas en movimiento, más de 2
jugadores, reducción de direcciones fijas.

## 3. Alcance

### 3.1 Dentro

| # | Elemento | ¿Ingeniería inversa nueva? |
|---|---|---|
| A | Tinte 3D del segundo Link | Investigación con herramientas de depuración del renderizador; Ghidra solo si no basta |
| B | Teletransporte entre mapas | **Sí** (o una alternativa lenta por recarga de guardado) |
| C | Santuarios juntos | Probablemente no; es sobre todo probar y arreglar |
| D | Bestias Divinas: dentro y vistas en movimiento desde fuera | Probablemente sí para "desde fuera" |
| E | Momentos de historia con ambos presentes | **Sí** para eventos que arrancan por zona; no para los de botón |
| F | Colisión con el compañero | Experimento con el generador de actores; puede necesitar Ghidra |
| G | Escalar a más de 2 jugadores | No |
| H | Convertir direcciones fijas en sitios por patrón | Mecánico para funciones; Ghidra (referencias cruzadas) para punteros de datos |

### 3.2 Fuera

- Otras versiones del juego distintas de 1.9.0 (H lo facilita, no lo entrega).
- Sincronizar el contenido de una escena de vídeo fotograma a fotograma.
- Que los dos **vean** la misma escena a la vez: la versión 1 solo exige presencia y progreso; verla a la vez es
  una mejora posterior, sin tareas activas `[DUEÑO]` (ADR-009, decisión 21).
- Exigir a los dos presentes en los 120 santuarios o ante su monje `[DUEÑO]` (ADR-007, decisión 3).
- Un temporizador de desconexión para la historia: basta la confirmación del anfitrión `[DUEÑO]` (ADR-007,
  decisión 6).
- Sincronizar el estado interno de los rompecabezas de santuario que no sean objetos, mecanismos o flags.
- Cambiar el transporte de red (lo decide el ADR de F3).
- Marcadores dentro del mapa del propio juego.

## 4. Requisitos

### A. Tinte 3D (CU-13)

- **R5-01** `[DUEÑO]` (ADR-007, decisión 8) Con la opción activada, el modelo del compañero (cuerpo y ropa) se ve
  con un **tinte suave** de su color de jugador en todos los pases en que se dibuja el color de la escena; el Link
  local no cambia. El tinte acompaña al nombre y a la forma del marcador (F4), no los sustituye.
- **R5-02** Con la opción desactivada (valor por defecto) el resultado es idéntico píxel a píxel al de antes.
- **R5-03** No se cambia el traductor de sombreadores salvo que el ADR lo apruebe: cambiarlo invalida las cachés de
  sombreadores de todos los jugadores.
- **R5-04** Coste medido: fps y tiempos de fotograma antes y después en la misma escena.
- **R5-04b** `[DUEÑO]` (ADR-009, decisión 22) Si el tinte no es viable, el compañero se distingue por el nombre en
  color y el marcador con forma de F4. Ese resultado cierra el bloque A y **no bloquea** ningún otro bloque.

### B–D. Otros mapas (CU-14)

- **R5-05** "Ir con mi compañero" funciona aunque esté en otro mapa; el viaje termina con los dos en el mismo mapa y
  visibles el uno para el otro. `[DUEÑO]` (ADR-008, decisión 20): se acepta que pase por una pantalla de carga con
  guardado.
- **R5-06** En un santuario jugado por los dos: se ven, los objetos y mecanismos se mueven igual en los dos juegos,
  la recompensa llega a **los dos, siempre** `[DUEÑO]` (ADR-007, decisión 2; quien se une más tarde la recibe con la
  tarea de ponerse al día al entrar de F4), sin crash en la matriz de pruebas. Completarlo no exige a los dos
  presentes.
- **R5-07** Lo que no se sincronice queda escrito en la guía del jugador.
- **R5-08** Dentro de una Bestia Divina los dos ven moverse las mismas partes.

### E. Historia (CU-14)

- **R5-09** `[DUEÑO]` (ADR-007, decisión 3) La tabla de eventos que exigen a los dos jugadores es la lista corta:
  inicio (piedra y primera torre), final de cada Bestia Divina, recuerdos y jefes finales. Los santuarios y su
  monje **no** están en la tabla.
- **R5-10** `[DUEÑO]` (ADR-007, decisión 4) Un evento de la tabla no empieza si el compañero no está **en el mismo
  mapa y a menos de 30 m**. Quien lo intenta ve "Esperando a tu compañero", y el juego **ofrece teletransportar**
  al que falta (con su aceptación, como toda invocación: ADR-008, decisión 16).
- **R5-11** `[DUEÑO]` (ADR-009, decisión 21) Versión 1: basta con estar allí. Uno ve la escena; el otro está
  presente y recibe el progreso. No se lanza la escena en el otro juego.
- **R5-12** `[DUEÑO]` (ADR-007, decisión 6) El anfitrión puede continuar solo **de inmediato**, tras un diálogo de
  confirmación, esté el compañero desconectado o simplemente lejos. No hay temporizador. El plazo de reconexión
  de 10 minutos (ADR-008, decisión 18) es independiente y no retrasa la historia.

### F. Colisión

- **R5-13** Los jugadores no se atraviesan y, si es viable, uno puede subirse encima del otro. Si no es viable, se
  conserva el empuje actual y se documenta.

### G. Más de 2 jugadores

- **R5-14** Con 4 jugadores, la subida del anfitrión no supera el doble de la de 2 jugadores cuando los jugadores
  están repartidos por el mapa.
- **R5-15** Ningún supuesto de "el anfitrión es la ranura 0 y solo hay otro" queda sin prueba.
- **R5-15b** `[DUEÑO]` (ADR-007, decisión 12) El límite sigue siendo de 2 jugadores por ahora: lo nuevo se diseña
  para N y se prueba con 2. El bloque G prepara el terreno; no sube el límite por sí mismo.

### H. Direcciones fijas

- **R5-16** Las direcciones absolutas de funciones del juego en los módulos `oa_*` pasan a ser sitios por patrón en
  grupos opcionales; si un patrón no se encuentra, la función en línea correspondiente se desactiva sola en vez de
  saltar a código equivocado.
- **R5-17** `tools/venv/Scripts/python.exe tools/test_game_sites.py` y `tools/game_sites.py find` pasan con todos los
  sitios nuevos en estado `ok`.

## 5. Criterios de salida (medibles)

Cada bloque se cierra por separado. Las filas siguen el orden de las letras de bloque, **no** el de trabajo: el
orden de trabajo está en `plan.md` §10 (C va antes que A y que E). Un bloque también puede cerrarse como **"no viable"** si la investigación lo
demuestra: entonces el criterio es un ADR con la evidencia y la alternativa.

| # | Bloque | Criterio | Cómo se mide |
|---|---|---|---|
| S1 | A | El compañero se ve teñido | Dos capturas de la misma escena, opción apagada y encendida: solo cambian los píxeles del compañero |
| S2 | A | Sin regresión con la opción apagada | Captura idéntica a la de antes del cambio (comparación de ficheros) |
| S3 | A | Coste | Tabla de fps y tiempos de fotograma antes y después |
| S4 | B | Teletransporte entre mapas | 5 viajes campo → santuario y 5 santuario → campo: 10 llegadas, 0 cuelgues |
| S5 | C | Santuarios (prioritario) | Bitácora de T540 con lo que pasa hoy; matriz de 8 santuarios (4 tipos de mecanismo) recorrida a dúo: 0 crashes; divergencias anotadas |
| S6 | D | Bestia | Una Bestia recorrida a dúo hasta mover sus partes: ambos ven el mismo movimiento |
| S7 | E | Puerta de historia | Para 5 eventos de la tabla: con el compañero lejos no empieza y se ofrece teletransportarlo; con el compañero cerca empieza y el progreso queda para los dos; con la confirmación del anfitrión empieza solo, sin espera |
| S8 | F | Colisión | Correr contra el compañero 20 veces: 0 traspasos; o ADR "no viable" |
| S9 | G | Ancho de banda | Medida en Docker con 2, 4 y 8 bots, antes y después: tabla de bytes/s en el servidor |
| S10 | G | 4 jugadores reales o 2 + 2 bots | Sesión de 20 min sin crash |
| S11 | H | Direcciones | Recuento de literales `0x70…` en `runtime/oa_*.c`: de 62 a 0 para funciones; los punteros de datos que queden, listados con motivo |
| S12 | Todos | Regresión | Recorridos de F1 y de F4 siguen pasando |

## 6. Riesgos

| # | Riesgo | Mitigación |
|---|---|---|
| 1 | No se pueden identificar los dibujos del compañero: comparte sombreadores con el Link local y el renderizador no sabe a qué actor pertenece un dibujo (mapa 04 §4b) | Tres vías de investigación ordenadas de barata a cara; alternativa sin renderizador (parámetros de material del juego); y, si nada sirve, nombre en color y marcador con forma (ADR-009, decisión 22) |
| 2 | El tinte existente solo afecta al objetivo de color 0 y el sombreado es diferido; qué objetivo guarda el color base **no está verificado** | Investigación T504 antes de escribir código |
| 3 | Cambiar el traductor invalida cachés de todos | R5-03; opción apagada por defecto |
| 4 | `exec.c` es el fichero más cambiado aguas arriba (58 de 150 commits, mapa 05 §7.1): cualquier gancho ahí chocará al sincronizar | Gancho mínimo, en una función propia, llamado desde un solo punto |
| 5 | Lanzar eventos fuera de contexto puede colgar el juego (`Demo022_0` en pantalla negra, DOC:751-752) | La puerta **retrasa o impide** el arranque; no lanza eventos en el otro juego salvo los ya probados |
| 6 | Retener un evento que arranca por zona puede dejar al juego en un estado a medias | Investigación T519 con prueba de 10 repeticiones antes de construir |
| 7 | `FN_BODY_TO` sobre actores sin cuerpo físico ya causó cuelgues (OA:67-72) y los mecanismos de santuario son la zona menos probada | Requiere el arreglo `has_body` de F1; la matriz de santuarios empieza por los más simples |
| 8 | Dar cuerpo físico a la marioneta puede hacer que el juego la trate como obstáculo en todas partes (IA de enemigos, cámara) | Experimento aislado con el generador; se puede abandonar |
| 9 | Cada lote de sitios nuevos cambia `GAMELIB_ID`: recompilar el juego (~15 min) los dos jugadores | Lotes grandes y pocos; nunca uno por dirección |
| 10 | Un patrón que encuentra otra dirección que la esperada | `tools/game_sites.py` ya detiene la generación en ese caso para versiones conocidas (mapa 01 §3.6) |
| 11 | El dueño es una sola persona y casi todo pasa por él | Una función cada vez; los agentes preparan consultas, pruebas y lógica pura mientras tanto |
| 12 | Con N jugadores aparecen fallos que con 2 no existen (propiedad de enemigos, pool compartido de la ranura 0) | Tarea específica de supuestos y prueba con bots |
| 13 | Hoy quien está en un santuario o en una Bestia no aparece en el mundo de los demás, y "santuarios juntos" figura como parcial y sin probar (ADR-008, consecuencias de la decisión 20) | T540 lo prueba con dos instancias antes de planificar nada más del bloque C |

## 7. Preguntas para el dueño

Las contestadas remiten a las tres rondas de respuestas (ADR-007, ADR-008 y ADR-009, en `../../decisiones/`).
Siguen abiertas P5-6 y la parte de P5-4 sobre las cachés de sombreadores.

| # | Pregunta | Estado y respuesta | Afecta a |
|---|---|---|---|
| P5-1 | ¿Qué es un "momento de historia"? | RESUELTA (ADR-007, decisión 3): lista corta: inicio (piedra y primera torre), final de cada Bestia Divina, recuerdos y jefes finales. Los santuarios no | Bloque E |
| P5-2 | Si el compañero no está, ¿el anfitrión puede avanzar la historia solo? | RESUELTA (ADR-007, decisión 6): sí, de inmediato, con una confirmación del anfitrión. Sin temporizador | T520, T521, T523 |
| P5-3 | ¿Qué distancia cuenta como "presente"? | RESUELTA (ADR-007, decisión 4): mismo mapa y a menos de 30 m; si falta uno, el juego avisa y ofrece teletransportarlo | T520, T523 |
| P5-4 | Si no se puede teñir el modelo sin tocar el traductor, ¿se acepta invalidar las cachés de sombreadores? | RESUELTA en parte (ADR-009, decisión 22): si el tinte no es viable, nombre en color y marcador con forma. Invalidar cachés sigue **ABIERTA**; por defecto: solo con ADR aprobado | T508 |
| P5-5 | ¿Orden de los bloques? | RESUELTA en parte (ADR-008, decisión 20): santuarios juntos (C) va antes que el tinte (A) y que la historia (E). El resto del orden sigue siendo propuesta: `plan.md` §10 | Planificación |
| P5-6 | ¿Cuántos jugadores como objetivo real: 3, 4 u 8? | **ABIERTA**. Por defecto: 4. El límite vigente es 2 (ADR-007, decisión 12): se diseña para N y se prueba con 2 | T526-T532 |
| P5-7 | Teletransporte entre mapas por recarga de guardado (una pantalla de carga): ¿aceptable? | RESUELTA (ADR-008, decisión 20): aceptado con pantalla de carga y guardado | T513 |
