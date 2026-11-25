# F5 — Funciones difíciles: especificación

Estado: borrador para revisión del dueño. Depende de F4 terminada. **Se trabaja una función cada vez**, no en
paralelo: casi todas necesitan que el dueño investigue con Ghidra sobre su propia copia del juego, o pruebas largas
dentro del juego.

Convenciones:

- Referencias `fichero:línea` del checkout del PR #294 (`pr294-online`, HEAD `350c5f8`). `OA:n` =
  `runtime/online_actors.c`; `DOC:n` = `docs/ONLINE.md`; `MOD:n` = `tools/online_mod.py`. Las referencias al
  renderizador que vienen del mapa 04 (rama `test`) se han vuelto a localizar en el PR donde se indica.
- "[sin verificar]" = no comprobado. En esta fase hay muchas: por eso cada bloque empieza con investigación.
- **Este documento no contiene direcciones, desplazamientos ni estructuras nuevas.** Las que cita ya están en el
  código del PR. Lo que falte se obtiene del Ghidra del dueño y entra como sitio por patrón. No se copia nada de
  proyectos de descompilación.

## 1. Objetivo

Resolver, de una en una, las funciones que no se pueden construir solo con lógica de red y de interfaz:

1. Que el segundo Link se distinga por un **color en su modelo 3D** (CU-13).
2. **Jugar juntos en otros mapas**: teletransporte entre mapas, santuarios y Bestias Divinas (CU-14).
3. **Momentos de historia solo con los dos presentes** (CU-14).
4. **Colisión** con el compañero.
5. **Más de 2 jugadores** sin que el ancho de banda se dispare.
6. **Menos direcciones fijas de 1.9.0** en el código.

## 2. Casos de uso que cubre

| CU | Hoy (verificado) | Qué entrega F5 |
|---|---|---|
| CU-13 Color en el modelo 3D del segundo Link | Nada en el renderizador. Existe un tinte por dibujo cableado pero siempre a cero (`runtime/nvn/vulkan/exec.c:9161`). El color de ranura solo se ve en el overlay 2D | Tinte del modelo del compañero, tras una opción apagada por defecto, o una alternativa documentada si no es viable |
| CU-14a Teletransporte entre mapas | Un jugador en otro mapa no tiene Link en el mundo de los demás (`same_map`, OA:1484-1486; decisión en OA:6013-6014). El teletransporte solo funciona dentro de un mapa (DOC:747-748) | Ir con el compañero aunque esté en un santuario o una Bestia |
| CU-14b Santuarios y Bestias juntos | Los que están en el mismo santuario se ven y sus objetos móviles se envían (DOC:996-997); los mecanismos se sincronizan pero no está probado montado ni dentro de una Bestia (DOC:984-986, 1013-1014) | Matriz de pruebas, arreglos y límites documentados |
| CU-14c Momentos de historia con los dos | Solo existe un bloqueo del botón A a 30 m de un jugador que está en el pedestal de la piedra o de una torre (OA:5981-6001; `runtime/hle/input.c:1207-1208`) | Una puerta de presencia general para los eventos que el dueño marque |

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
- Sincronizar el estado interno de los rompecabezas de santuario que no sean objetos, mecanismos o flags.
- Cambiar el transporte de red (lo decide el ADR de F3).
- Marcadores dentro del mapa del propio juego.

## 4. Requisitos

### A. Tinte 3D (CU-13)

- **R5-01** Con la opción activada, el modelo del compañero (cuerpo y ropa) se ve teñido con su color de jugador en
  todos los pases en que se dibuja el color de la escena; el Link local no cambia.
- **R5-02** Con la opción desactivada (valor por defecto) el resultado es idéntico píxel a píxel al de antes.
- **R5-03** No se cambia el traductor de sombreadores salvo que el ADR lo apruebe: cambiarlo invalida las cachés de
  sombreadores de todos los jugadores.
- **R5-04** Coste medido: fps y tiempos de fotograma antes y después en la misma escena.

### B–D. Otros mapas (CU-14)

- **R5-05** "Ir con mi compañero" funciona aunque esté en otro mapa; el viaje termina con los dos en el mismo mapa y
  visibles el uno para el otro.
- **R5-06** En un santuario jugado por los dos: se ven, los objetos y mecanismos se mueven igual en los dos juegos,
  la recompensa llega a ambos (política de F4), sin crash en la matriz de pruebas.
- **R5-07** Lo que no se sincronice queda escrito en la guía del jugador.
- **R5-08** Dentro de una Bestia Divina los dos ven moverse las mismas partes.

### E. Historia (CU-14)

- **R5-09** Existe una tabla, decidida por el dueño, de eventos que exigen a los dos jugadores.
- **R5-10** Un evento de la tabla no puede empezar si el compañero no está en el mismo mapa y a menos de la
  distancia fijada; quien lo intenta ve "Esperando a tu compañero".
- **R5-11** Cuando empieza, ambos lo ven (cada juego reproduce su propia escena) y el progreso se registra para los
  dos.
- **R5-12** Con el compañero desconectado, la tabla no bloquea nada (se puede seguir jugando solo si el anfitrión lo
  permite: decisión P5-2).

### F. Colisión

- **R5-13** Los jugadores no se atraviesan y, si es viable, uno puede subirse encima del otro. Si no es viable, se
  conserva el empuje actual y se documenta.

### G. Más de 2 jugadores

- **R5-14** Con 4 jugadores, la subida del anfitrión no supera el doble de la de 2 jugadores cuando los jugadores
  están repartidos por el mapa.
- **R5-15** Ningún supuesto de "el anfitrión es la ranura 0 y solo hay otro" queda sin prueba.

### H. Direcciones fijas

- **R5-16** Las direcciones absolutas de funciones del juego en los módulos `oa_*` pasan a ser sitios por patrón en
  grupos opcionales; si un patrón no se encuentra, la función en línea correspondiente se desactiva sola en vez de
  saltar a código equivocado.
- **R5-17** `tools/venv/Scripts/python.exe tools/test_game_sites.py` y `tools/game_sites.py find` pasan con todos los
  sitios nuevos en estado `ok`.

## 5. Criterios de salida (medibles)

Cada bloque se cierra por separado. Un bloque también puede cerrarse como **"no viable"** si la investigación lo
demuestra: entonces el criterio es un ADR con la evidencia y la alternativa.

| # | Bloque | Criterio | Cómo se mide |
|---|---|---|---|
| S1 | A | El compañero se ve teñido | Dos capturas de la misma escena, opción apagada y encendida: solo cambian los píxeles del compañero |
| S2 | A | Sin regresión con la opción apagada | Captura idéntica a la de antes del cambio (comparación de ficheros) |
| S3 | A | Coste | Tabla de fps y tiempos de fotograma antes y después |
| S4 | B | Teletransporte entre mapas | 5 viajes campo → santuario y 5 santuario → campo: 10 llegadas, 0 cuelgues |
| S5 | C | Santuarios | Matriz de 8 santuarios (4 tipos de mecanismo) recorrida a dúo: 0 crashes; divergencias anotadas |
| S6 | D | Bestia | Una Bestia recorrida a dúo hasta mover sus partes: ambos ven el mismo movimiento |
| S7 | E | Puerta de historia | Para 5 eventos de la tabla: con el compañero lejos no empieza; con el compañero cerca empieza en ambos |
| S8 | F | Colisión | Correr contra el compañero 20 veces: 0 traspasos; o ADR "no viable" |
| S9 | G | Ancho de banda | Medida en Docker con 2, 4 y 8 bots, antes y después: tabla de bytes/s en el servidor |
| S10 | G | 4 jugadores reales o 2 + 2 bots | Sesión de 20 min sin crash |
| S11 | H | Direcciones | Recuento de literales `0x70…` en `runtime/oa_*.c`: de 62 a 0 para funciones; los punteros de datos que queden, listados con motivo |
| S12 | Todos | Regresión | Recorridos de F1 y de F4 siguen pasando |

## 6. Riesgos

| # | Riesgo | Mitigación |
|---|---|---|
| 1 | No se pueden identificar los dibujos del compañero: comparte sombreadores con el Link local y el renderizador no sabe a qué actor pertenece un dibujo (mapa 04 §4b) | Tres vías de investigación ordenadas de barata a cara; alternativa sin renderizador (parámetros de material del juego) |
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

## 7. Preguntas abiertas para el dueño

| # | Pregunta | Propuesta | Bloquea |
|---|---|---|---|
| **P5-1** | **¿Qué es un "momento de historia"?** ¿Solo la historia principal (recuerdos, Bestias, Ganon)? ¿También torres, santuarios, Grandes Hadas, misiones secundarias con escena? | Empezar con una lista corta: pedestal de la piedra, torres, entrada y jefe de cada Bestia, Espada Maestra, castillo | T501 → todo el bloque E |
| P5-2 | Si el compañero no está conectado, ¿el anfitrión puede avanzar la historia solo? | Sí | T520 |
| P5-3 | ¿Qué distancia cuenta como "presente"? | 30 m y mismo mapa (la que ya usa el bloqueo de A) | T520 |
| P5-4 | Si no se puede teñir el modelo sin tocar el traductor, ¿se acepta invalidar las cachés de sombreadores o se prefiere un brillo/contorno alternativo? | Alternativa primero | T508 |
| P5-5 | ¿Orden de los bloques? | H (direcciones) → B/C (mapas y santuarios) → E (historia) → A (tinte) → G (N jugadores) → F (colisión) → D (Bestias) | Planificación |
| P5-6 | ¿Cuántos jugadores como objetivo real: 3, 4 u 8? | 4 | T526-T532 |
| P5-7 | Teletransporte entre mapas por recarga de guardado (lento, ~una pantalla de carga): ¿aceptable si no hay otra vía? | Sí, como primera entrega | T513 |
