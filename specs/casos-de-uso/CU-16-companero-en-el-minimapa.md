# CU-16 · El compañero en el minimapa

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md). Decisión del dueño que lo crea:
[ADR-022](../decisiones/ADR-022-companero-en-el-minimapa.md) (que **amplía** ADR-011, decisión 41).

## Resumen

Hoy el jugador solo sabe dónde está su compañero por la flecha de borde de pantalla y por la lista del overlay
(CU-10). Este caso lo muestra además en el **minimapa** del juego: una **flecha pequeña con el color y la forma del
compañero**, que apunta hacia donde mira, dibujada **encima** del minimapa por la capa de interfaz del runtime (la de
los marcadores de CU-10). Si el compañero queda **fuera del alcance del minimapa**, una marca pegada al borde del
círculo, en su dirección, con la distancia al lado. No se toca el layout del juego ni el guardado.

Para saber dónde dibujar hace falta conocer el minimapa: posición, tamaño, escala de metros a píxeles, si gira con la
cámara o lleva el norte arriba. Eso **no se conoce hoy**: se mide primero (T445) y después se programa (T446).

## Actores

- **Jugador local**: mira su minimapa y ve a su compañero.
- **Compañero**: aparece como flecha en el minimapa del otro (y viceversa).
- **Capa de interfaz del runtime** (hilo de presentación): dibuja; no llama al juego ni a la red.

## Precondiciones

- Sesión conectada (CU-01), compañero localizable en el mismo mapa (CU-02) y con posición y rumbo recientes.
- CU-10 hecho hasta la capa de marcadores (T425: color y forma por jugador, `coop_hud.mm`).
- T445 hecha: se conocen las medidas del minimapa. Sin ellas este caso no se construye.
- Versión del juego **1.9.0** (es la única para la que `ui_layout_map_mode` reconoce el mapa).

## Flujo principal

1. A y B están en el mismo mapa del mundo, a 120 m uno de otro, con el HUD normal y el minimapa visible.
2. En el minimapa de A aparece una **flecha pequeña** con el color y la forma de B, en el punto que corresponde a la
   posición de B respecto a A, orientada según hacia dónde mira B (relativo a como se orienta el minimapa).
3. B camina: la flecha de B se mueve por el minimapa de A y gira con B, sin saltos, al ritmo del minimapa nativo.
4. B se aleja más allá del alcance del minimapa: la flecha pasa a una **marca pegada al borde del círculo**, en la
   dirección de B, con la distancia ("350 m") al lado. Al volver al alcance, otra vez la flecha.
5. A abre el mapa grande de pausa o el minimapa anima hacia él: la flecha desaparece mientras tanto (el mapa grande
   queda fuera de F4, opción B del ADR). Al volver al minimapa, reaparece.
6. Con 16:9 y con ultrawide la flecha cae en el mismo sitio del minimapa (el de su proporción de pantalla).

## Flujos alternativos y de error

| Situación | Comportamiento esperado |
|---|---|
| Compañero **fuera del alcance** del minimapa | Marca pegada al borde en su dirección, con la distancia al lado (decisión 3 del ADR) |
| Compañero en **otro mapa**, **santuario** o **mazmorra** | Nada en el minimapa (la lista de CU-10 dice dónde está). Sin marca de borde |
| Compañero **cargando** o sin posición reciente (más de 0,5 s sin datos o sin cámara) | Nada |
| **Minimapa oculto** (el juego no lo dibuja, p. ej. en escena o con el HUD del juego quitado) | Nada |
| **Minimapa animando** hacia el mapa grande, o mapa grande abierto (`ui_layout_map_mode` ≠ 0) | Nada |
| **HUD apagado** (opción del proyecto) o **modo pro** del HUD | Nada: si el jugador quitó el HUD, no se dibuja encima de donde estaba |
| **Varias personas** (más de un compañero) | Una flecha por compañero, cada una con su color y forma; si dos caen en el mismo píxel, se dibuja la del índice menor encima y la otra desplazada 1 radio. Se diseña para N, se prueba con 2 |
| **Menús abiertos** (pausa, inventario, pestañas del proyecto, pantalla de título) | Nada |
| **Pausa del compañero** | Flecha atenuada, como el marcador de CU-10 |
| Desconexión del compañero | La flecha desaparece con él |
| Posición con NaN o fuera de rango | Se descarta (ya se valida al recibir) |
| Versión del juego distinta de 1.9.0, o medidas no fiables en este formato de pantalla | No se dibuja nada y se registra una vez el motivo: **mejor nada que una flecha en el sitio equivocado** |
| Ajuste "Show partner on minimap" apagado | Nada |

## Estado actual en el PR: **FALTA** (y el minimapa no está medido)

- No existe ninguna capa sobre el minimapa. El overlay (`runtime/ui/online_ui.mm`) dibuja marcadores sobre el mundo
  con la cámara capturada por el renderizador (`online_camera_get`), no sobre elementos del HUD del juego.
- El runtime sabe reconocer el modo de la pantalla del mapa: `ui_layout_map_mode(screen)` en `runtime/ui_layout.c`
  (1 = mapa grande, 0 = minimapa, -1 = animando o desconocido; solo 1.9.0). **No** sabe dónde está el minimapa en
  pantalla ni qué cubre.
- El escalado y los bordes del HUD (`g_settings.hud_edges`, `g_settings.hud_size`; `runtime/nvn/hud_scale.h`) mueven y
  cambian el tamaño del minimapa: forman parte de lo que hay que medir.
- Los datos del compañero que hacen falta ya llegan en el estado: posición (`OnlinePlayer.pos`), mapa (`pose.map`) y,
  si CU-10 lo publica, rumbo; si el rumbo no está en el estado, T446 lo toma de la orientación que ya envía
  `sample_pose` **[sin verificar: se comprueba al empezar T446; si falta, para y se pregunta, porque cambiaría el
  cable y eso es de T409]**.
- Color y forma por jugador: T425 (CU-10).

## Qué hay que construir

1. **T445 (medir, investigación).** Dos partes. *Código* (agente Opus `minimapa-t445`, ya en curso): cómo dibuja el
   juego el minimapa y qué expone el runtime (`ui_layout.c`, `hud_scale.h`, la pantalla AppMap). *Juego* (orquestador,
   con capturas de ventana): con el HUD por defecto, en 16:9 y en ultrawide, con `hud_size` y `hud_edges` por defecto
   y alterados, anotar: centro y radio del círculo en proporción de pantalla; metros del mundo que cubre el radio
   (escala metros→píxeles) y si cambia con el zoom del minimapa; si gira con la cámara (el "arriba" del minimapa es
   la dirección de la cámara) o lleva el norte arriba; cuándo está oculto (escenas, HUD quitado) y cómo se ve
   animando hacia el mapa grande. Resultado: una entrada de bitácora con tabla y capturas, y, si hace falta, una
   herramienta `tools/coop/minimapa_medir.py` que lee capturas y devuelve las medidas.
2. **Módulo puro `runtime/coop_minimap.c/.h` (T446).** Sin punteros del juego ni ImGui ni reloj. Entradas: posición y
   rumbo del compañero, posición y rumbo del jugador local, rumbo de la cámara, y los parámetros medidos (centro,
   radio y metros por radio, modo de giro). Salidas: posición en píxeles dentro del círculo, ángulo de la flecha, o
   "fuera de alcance" con el punto recortado al borde y el ángulo hacia el compañero. Matemática de recorte
   (`min(radio, distancia·escala)`), giro con la cámara o norte arriba, tolerancia al origen con NaN.
3. **Datos medidos.** Constantes por proporción de pantalla (centro y radio como fracción de ancho y alto), una tabla
   para cada formato medido. Si T445 demuestra que se pueden leer del layout, se leen de ahí y las constantes quedan
   como valor de reserva.
4. **Dibujo (T446).** En la capa de UI de CU-10 (`runtime/ui/coop_hud.mm`, llamado desde `online_overlay_draw`), con
   `coop_shape_draw` de T425 para la forma y color, y un triángulo orientado para el rumbo. Solo si el modo del mapa
   es minimapa (`ui_layout_map_mode == 0`), el HUD y el ajuste están activos y los datos son recientes.
5. **Ajuste.** "Show partner on minimap" (activado por defecto) en la pestaña Online, en `online.ini`; texto en
   `textos-nuevos.txt` y traducido por T437.
6. **Comando de control** `coop overlay` (de CU-10): añade por jugador el campo `minimap` con `{visible, x, y, angulo,
   borde, distancia}` en proporciones de pantalla, para poder medir sin mirar a mano.

## Reglas y decisiones de diseño

- **[DUEÑO] (ADR-022, decisión 1)** Se muestra al compañero en el minimapa con la opción A: dibujado encima por la
  capa de UI del runtime, sin tocar el layout del juego ni el guardado.
- **[DUEÑO] (ADR-022, decisión 2)** Flecha pequeña con el color y la forma del compañero (los 8 colores de CU-10), que
  apunta hacia donde mira.
- **[DUEÑO] (ADR-022, decisión 3)** Fuera del alcance: marca pegada al borde del círculo, en su dirección, con la
  distancia al lado.
- **[DUEÑO] (ADR-022, decisión 4)** Se mide ya (T445) y se programa en F4, junto a CU-10.
- **[DUEÑO] (ADR-022, decisión 5)** Mapa grande de pausa (opción B): investigación para después, sin fecha (T447).
  Opción C (pines o marcas reales del juego): descartada, se guardan en la partida.
- **[DUEÑO] (ADR-022; amplía ADR-011, decisión 41)** El minimapa sí; el mapa grande, no en F4. Flecha de borde de
  pantalla, distancia y nombre del mapa de R4-35 se mantienen.
- **[PROPUESTA]** Ningún dato solo por color: la flecha lleva color **y** forma (CU-10) y la marca de borde, número.
- **[PROPUESTA]** Solo posición horizontal; la altura no cuenta (Q-16-1).
- **[PROPUESTA]** Mejor nada que mal puesto: sin medidas fiables para el formato de pantalla, no se dibuja.

## Criterios de aceptación

Las comprobaciones visuales usan captura de ventana (`SAVAGEWIND_WINDOW_SHOT`), no el `shot` del control. Para
medir, la prueba compara el centro de la flecha en la captura con la posición esperada que calcula `coop overlay`
(campo `minimap`). **Error en píxeles admitido: ±6 px a 1080p de alto; en otras resoluciones, ±0,6 % del alto de
pantalla.** Montaje: el común, con A y B en el mismo mapa, B colocado con `swctl.py --port 7778 teleport` a la
distancia indicada.

- **CA-1** (medidas) Dado T445 hecha, entonces `specs/bitacora/<fecha>-F4-T445-minimapa.md` contiene la tabla de
  centro, radio, metros por radio y modo de giro para 16:9 y ultrawide, con al menos una captura por formato, y dice
  en una línea si la medida es fiable o no.
- **CA-2** (lógica) Dado el módulo puro, cuando se ejecuta `tests/test_coop_minimap.c`, entonces sale con 0 y cubre:
  compañero en el centro, a la mitad del radio, justo en el borde, fuera de alcance (recorte al borde con el ángulo
  correcto), giro de la cámara con 0°/90°/180°, norte arriba, NaN, y distancia cero.
- **CA-3** (posición, 16:9) Dado A y B a 60 m con B al norte de A y el minimapa visible en 1920×1080, cuando se
  captura la ventana, entonces el centro de la flecha de B está a menos de ±6 px de la posición esperada.
- **CA-4** (posición, ultrawide) Dado lo mismo en 3440×1440 (o en la ventana ultrawide que anote T445), entonces el
  error es ≤ ±0,6 % del alto.
- **CA-5** (rumbo) Dado B que gira 90° en el sitio, entonces en menos de 1 s la flecha de B en el minimapa de A gira 90°
  ± 10° respecto a la del orden de giro del minimapa medido.
- **CA-6** (borde) Dado B a una distancia mayor que el alcance medido del minimapa, entonces `coop overlay` lista
  `borde: true` con la distancia al lado ±2 m, y la captura muestra la marca sobre el borde del círculo (±6 px) en la
  dirección de B ±10°.
- **CA-7** (otro mapa) Dado B en un santuario o en otro mapa, entonces `coop overlay` lista `minimap.visible: false`
  y la captura no muestra ninguna flecha ni marca en el minimapa de A.
- **CA-8** (minimapa oculto o animando) Dado A que abre el mapa grande de pausa, entonces mientras
  `ui_layout_map_mode` no sea 0 no hay flecha (`minimap.visible: false` en menos de 0,2 s) y, al volver, reaparece
  en menos de 0,5 s.
- **CA-9** (HUD apagado y modo pro) Dado el HUD del proyecto apagado o en modo pro, entonces no se dibuja la flecha ni
  la marca.
- **CA-10** (menús) Dado un menú abierto (pestañas del proyecto, inventario, pantalla de título), entonces no se
  dibuja nada en el minimapa.
- **CA-11** (varias personas) Dado un bot que simula un tercer jugador cerca de B, entonces se ven dos flechas con
  colores y formas distintos y ninguna tapa del todo a la otra.
- **CA-12** (colores y daltonismo) Dado cada uno de los tres modos de visión de color del proyecto
  (`SAVAGEWIND_CVD_SIMULATE=protan`, `deutan`, `tritan`), entonces en la captura la flecha del compañero se
  distingue de la de otro por forma y, fuera de alcance, por el número, aunque el color se confunda (revisión visual,
  anotada en `bitacora/`).
- **CA-13** (sin tocar el guardado) Dado una sesión de 5 minutos con la flecha activa, entonces la suma de
  comprobación de la carpeta de guardado de A (copia) es igual antes y después, salvo lo que ya escribe el juego solo.
- **CA-14** (ajuste) Dado "Show partner on minimap" apagado, entonces no se dibuja nada, y encendido, vuelve.
- **CA-15** (i18n) `python tools/i18n_check.py` termina sin errores con la cadena nueva.

## Qué NO hacer

- No tocar el layout del juego, sus paneles ni el guardado; no crear pines, sellos ni marcas del juego (opción C,
  descartada).
- No dibujar desde el hilo del juego ni desde el de red: se dibuja en `menu_render`, en la capa de UI de CU-10.
- No leer memoria del juego desde el hilo de UI más allá de lo que ya hace `ui_layout_map_mode` (ni inventar
  desplazamientos de estructura): si T445 necesita más, para y pregunta al dueño.
- No poner píxeles absolutos: constantes por proporción de pantalla, o leídas del layout.
- No dibujar con el mapa grande abierto ni animando (opción B, T447, no es de F4).
- No cambiar `online.h` ni el protocolo por tu cuenta (dueño único: T409).
- No añadir texto visible sin traducción ni sin `textos-nuevos.txt`.
- No usar iconos, fuentes ni gráficos del juego.
- No copiar de proyectos de descompilación.

## Riesgos y preguntas abiertas

- Riesgo: el minimapa del juego puede cambiar de escala con el zoom, la altura o el tipo de zona, de modo que una
  escala fija sea falsa. T445 lo mide; si varía, T446 solo dibuja dentro de las zonas medidas.
- Riesgo: el HUD del proyecto (`hud_size`, `hud_edges`) y el ultrawide mueven el minimapa: la medida debe repetirse
  con los ajustes alterados.
- Riesgo: la cámara de otro fotograma o la generación de fotogramas hacen que la flecha vaya retrasada respecto al
  minimapa nativo. Se mide en CA-3 y CA-5.
- Riesgo: el rumbo del compañero puede no estar en el estado actual (ver "Estado actual"): costaría un campo en el
  cable (T409).
- Riesgo: ImGui dibuja encima del HUD y puede tapar un icono del minimapa.
- Riesgo: solo 1.9.0; otras versiones no se dibujan.
- **Q-16-1** ¿Se distingue la altura (compañero muy arriba o muy abajo)? **Por defecto: no**, solo posición horizontal.
- **Q-16-2** ¿Qué tamaño tiene la flecha? **Por defecto: 3 % del alto de pantalla (≈ 32 px a 1080p), y sigue el ajuste
  "Overlay text size" de CU-10 (80-160 %).**
- **Q-16-3** ¿Cómo se orienta la flecha si el minimapa lleva el norte arriba y no gira? **Por defecto: según lo que
  mida T445; si gira con la cámara, el rumbo se muestra relativo a la cámara; si lleva el norte arriba, en absoluto.**
- **Q-16-4** ¿Se puede apagar? **Por defecto: sí, "Show partner on minimap", activado.**
- **Q-16-5** Fuera del alcance, ¿la marca de borde muestra siempre la distancia? **Por defecto: sí, en metros
  redondeados ("350 m"), a la derecha de la marca, o a la izquierda si está en el semicírculo derecho; con más de
  10 000 m, "9999+ m".**
- **Q-16-6** ¿La flecha se atenúa si el compañero está en pausa? **Por defecto: sí, como el marcador de CU-10 (la
  mitad de opacidad).**
- **Q-16-7** ¿Con varias personas se dibujan todas? **Por defecto: sí, una por compañero (hasta ONLINE_MAX − 1); si
  hay solape, se desplaza 1 radio de flecha la de índice mayor.**
- **Q-16-8** ¿Qué hacer si T445 concluye que el alcance cambia con el zoom del minimapa y no se puede leer? **Por
  defecto: solo dibujar en el zoom por defecto y no dibujar fuera de él.**

## Fase y dependencias

- **Fase**: F4 (primera entrega), junto a CU-10. Tareas: T445 (medir, adelantada, en curso la parte de código),
  T446 (flecha), T447 (mapa grande, después, prioridad baja).
- **Depende de**: CU-10 hasta T425 (capa de marcadores y color/forma por jugador) y CU-02 (posición y rumbo del
  compañero).
- **Lo necesitan**: ninguno. **Amplía**: ADR-011, decisión 41.
