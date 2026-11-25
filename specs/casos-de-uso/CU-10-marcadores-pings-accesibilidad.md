# CU-10 · Marcadores, pings, nombre en color y accesibilidad

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Cada jugador sabe siempre dónde está su compañero, a qué distancia y cómo va de vida, puede señalar un punto del
mundo ("ping") y enviar frases rápidas, y todo ello se distingue sin depender solo del color. Hoy existen el nombre,
la distancia y la flecha de borde; lo demás falta.

## Actores

- **Jugador local**: ve los marcadores y lanza pings.
- **Compañero**: aparece marcado y recibe los pings.

## Precondiciones

- Sesión conectada (CU-01) y compañero visible o localizable (CU-02).
- Para marcadores en el mundo: cámara de la escena disponible (`online_camera_get`, NETH:194).

## Flujo principal

1. Sobre el compañero se ve su **nombre, su forma y su color**, la distancia y una fila pequeña de **corazones**,
   siempre visible y destacada cuando su vida baja del 25 %.
2. Si el compañero queda fuera de la pantalla, una flecha en el borde apunta hacia él con nombre y distancia.
3. En la lista de jugadores (izquierda, bajo los corazones) se ve su estado: jugando, en pausa, cargando, en otro
   mapa (cuál), en una escena, caído.
4. El jugador pulsa el botón de **ping**: aparece una marca en el punto al que mira, visible para los dos durante
   unos segundos, con un sonido.
5. El jugador abre la rueda de **frases rápidas** y elige una de las ocho ("Ven aquí", "Espera", "Necesito ayuda",
   "Gracias", "Sí", "No", "Vamos", "Mira esto"): el compañero la ve en pantalla en su idioma.
6. El color y la forma de cada jugador se asignan solos por orden de entrada. En Opciones se pueden cambiar, y
   también el tamaño del texto y los avisos sonoros.

## Flujos alternativos y de error

| Situación | Comportamiento esperado | Hoy |
|---|---|---|
| Compañero en otro mapa | Sin marcador en el mundo; la lista dice en qué mapa está | El marcador se omite si está a más de 3000 m (UI:266); la lista no dice el mapa |
| Compañero cargando | Sin marcador; "cargando" en la lista | UI:263 lo omite; la pestaña dice "loading" (UI:89) |
| Compañero en pausa | Marcador atenuado y "(en pausa)" | Figura atenuada (UI:280) y "(paused)" en la lista (UI:231) |
| Compañero muerto o caído | Marcador distinto y aviso (CU-11) | No hay estado de vida en el protocolo |
| Estoy en un menú, pausa o pantalla de título | No se dibujan marcadores del mundo | UI:186, UI:256 |
| Sin cámara reciente (más de 0,5 s) | No se dibujan marcadores del mundo | UI:256 |
| Demasiados pings seguidos | Límite: se ignoran los que superen N por segundo | — |
| Ping en un punto no válido (NaN, lejísimos) | Se descarta al recibir | — |
| Desconexión | Los marcadores y pings del que se fue desaparecen | La lista se vacía al salir (NET:1322-1335) |
| Dos jugadores con el mismo nombre | Se distinguen por color y forma | Hoy solo por color (mapa 06 H-9) |

## Estado actual en el PR: **PARCIAL**

Existe:

- Overlay `online_overlay_draw` (UI:181-304), llamado desde `runtime/ui/menu.mm:2594`.
- Lista de jugadores con un círculo de color, bajo los corazones (UI:217-235).
- Nombre y distancia sobre el compañero (UI:273-287); flecha en el borde si está fuera de vista (UI:289-302);
  figura esquemática solo si no tiene actor real (UI:278-284).
- Proyección mundo → pantalla con la cámara capturada en el renderizador (`project`, UI:135; `online_camera_get`,
  NETH:194; captura en `runtime/nvn/vulkan/exec.c`, mapa 07 §4).
- 8 colores fijos por número de sitio (`k_colors`, UI:20-23). El jugador no elige el suyo y el color no viaja por la
  red.
- Avisos de "se unió / se fue" (UI:188-203), últimas 6 líneas de chat (UI:237-250).
- Interruptores "Show players" y "Show names" (UI:105-108).
- Chat de texto, solo desde la pestaña Online (UI:94-97; `online_chat_send`, NETH:180).

Falta:

- **Vida del compañero**: ningún mensaje la lleva (en `runtime/online.h` solo hay vida de jefes y enemigos:
  NETH:91, NETH:218).
- **Pings**: PING/PONG del protocolo es solo latencia (mapa 06 §2.3).
- **Frases rápidas**, chat en juego sin abrir el menú, emotes (mapa 06 §6.2).
- **Formas** además de colores; elección de color; integración con la opción de visión de color del proyecto
  (`runtime/colour_vision.c`, `runtime/colour_vision.h`) (mapa 06 §6.2).
- **Marcador en el mapa del juego**: no hay mecanismo; el mapa es un layout nativo y el runtime no sabe qué
  coordenadas del mundo cubre (mapa 04 §4a).
- Avisos importantes que hoy solo van al registro, por ejemplo "A held back" (OA:5999 aprox., bloque OA:5981-6001).
- Traducciones: solo español (mapa 06 §6.3).

## Qué hay que construir

Todo en el overlay 2D; no toca `online_actors.c` salvo publicar dos datos nuevos. Fichero principal:
`runtime/ui/online_ui.mm`. Patrón a copiar para elementos fijos en pantalla: `perf_overlay_draw`
(`runtime/ui/menu.mm:1997`) y `photo_overlay_draw` (`runtime/ui/menu.mm:1935`). Todo elemento nuevo que se dibuje con
el menú cerrado debe añadirse a `menu_draws()` (`runtime/ui/menu.mm:2500`).

1. **Vida en el estado**: dos campos nuevos en `OnlinePose` / `WireState` (vida actual y máxima, en cuartos de
   corazón). Fuente en el emisor: la vida que ya lee el PR en `player + 0x848` (`ENEMY_LIFE`, usada en OA:2907);
   la máxima [sin verificar: mapa 03 §1.1 cita los flags `MaxHartValue` / `CurrentHart`; hay que decidir la fuente].
   Cambia el protocolo: lo hace el dueño de `online.h`.
2. **Marcador ampliado**: junto al nombre (UI:273-287), dibujar forma + corazones. La vida del compañero se ve
   **siempre, en pequeño**, y se destaca (tamaño, contorno o parpadeo, no solo color) por debajo del 25 %
   (ADR-009, decisión 27). Formas por orden de entrada: círculo, triángulo, cuadrado, rombo… en la lista (UI:229)
   y en el marcador.
3. **Estado en la lista**: usar `pose.map` (NETH:93) para "en <mapa>", `ONLINE_E_DEMO` y un bit nuevo "en evento"
   (de `framepace_in_event`), AS `Dead*` para "caído".
4. **Ping**: mensaje nuevo `MARK {tipo, pos[3]}` con relé a los demás; al recibir, validar que es finito y está a
   menos de 3000 m; dibujarlo con `project()` (UI:135) y la misma flecha de borde. Punto de destino, primera versión:
   un punto a D metros delante de la cámara o la posición de Link [PROPUESTA]; un trazado de rayo real contra el
   terreno necesita ingeniería inversa (mapa 07 §7).
5. **Frases rápidas**: lista cerrada de **ocho** identificadores (no texto libre) enviada como `QUICK {id}`, en
   este orden: "Ven aquí", "Espera", "Necesito ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto" (ADR-009,
   decisión 28). Cada juego la muestra traducida con `tr()`. Entrada con una rueda o menú rápido en el overlay.
5b. **Botón del ping y de la rueda (Q-10-5, lo único abierto)**: tarea de investigación, no de diseño. Revisar en
   `docs/controls.md` y `runtime/input_bind.h` (y `runtime/hle/input.c`) qué botones y teclas están libres y
   proponer una combinación al dueño. Hasta su respuesta, el ping y la rueda se lanzan solo desde la pestaña Online
   o por comando de control.
6. **Color y forma**: se asignan por orden de entrada y cada jugador puede cambiarlos en Opciones (ADR-009,
   decisión 26). El ajuste va en `online.ini` (`NET:1506-1554`) y se envía en el HELLO o en el estado; el servidor
   resuelve duplicados. Paleta comprobada para daltonismo.
7. **Accesibilidad**: tamaño de marcador y texto configurable; contorno oscuro en todo texto (ya lo hace `label`,
   UI:173); aviso sonoro opcional en unión, salida, ping y caída [sin verificar: cómo reproducir un sonido desde el
   runtime sin usar recursos del juego]; avisos en pantalla para lo que hoy solo va al registro.
8. **Traducir** todas las cadenas nuevas en `runtime/ui/i18n/*.inc` y pasar `tools/i18n_check.py`.
9. **Marcador en el mapa del juego**: fuera de F4; se deja como investigación de F5.

## Reglas y decisiones de diseño

- **[DUEÑO] D2**: lo que distingue a los jugadores es el color.
- **[DUEÑO] (ADR-009, decisión 26; Q-10-2; P8)** El color y la forma del marcador **se asignan por orden de
  entrada y se pueden cambiar en Opciones**.
- **[DUEÑO] (ADR-009, decisión 27; Q-10-4; P1)** La vida del compañero está **siempre visible, pequeña**, y
  destacada por debajo del 25 %.
- **[DUEÑO] (ADR-009, decisión 28; Q-10-1; P7)** Las frases rápidas son **ocho**: "Ven aquí", "Espera", "Necesito
  ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto".
- **[DUEÑO] (ADR-009, decisión 22)** El nombre en color y el marcador con forma de este caso son la **salida
  aceptada** si el tinte 3D de CU-13 no resulta viable. Por eso este caso no puede depender de CU-13.
- **[DUEÑO] (ADR-011, decisión 41; Q-10-3; P1)** **No hay marcador en el mapa del propio juego en F4**: flecha en
  pantalla, distancia y nombre del mapa del compañero. Se estudia después.
- **[PROPUESTA] P2**: pings.
- **[PROPUESTA]** Las frases rápidas son una lista cerrada traducible, no texto libre (funciona entre idiomas y no
  necesita teclado).
- **[PROPUESTA]** Ningún dato se comunica solo por color: siempre color + forma + texto.
- **[PROPUESTA]** Límite de 2 pings por segundo y 5 s de vida por ping.

## Criterios de aceptación

Las comprobaciones visuales usan captura de ventana (`SAVAGEWIND_WINDOW_SHOT`), no el `shot` del control (mapa 04).
Para comprobaciones mecánicas se añade un comando de control de solo lectura `coop overlay` que devuelve en JSON lo
que el overlay dibujaría (nombre, distancia, vida, forma, pings activos); es parte de este caso.

- **CA-1** Dado el montaje común con A y B a 20 m, entonces `swctl.py --port 7778 coop overlay` lista a "Host" con
  distancia 20 ± 2 m y vida actual y máxima iguales (±1 cuarto) a las de A.
- **CA-2** Dado lo anterior, cuando A pierde vida, entonces el valor en B cambia en menos de 1 s.
- **CA-3** Dado A que lanza un ping, entonces `coop overlay` en B lista 1 ping a la posición enviada durante 5 ± 1 s y
  después ninguno.
- **CA-4** Dado un bot que envía 50 `MARK` en un segundo, entonces B conserva como máximo los permitidos por el
  límite y no baja de fotogramas de forma visible.
- **CA-5** Dado un `MARK` con NaN o a más de 3000 m, entonces B no lo lista.
- **CA-6** Dado A que envía la frase rápida nº 3 con el juego en español y B en otro idioma, entonces cada uno la
  muestra en su idioma.
- **CA-7** Dado cada uno de los tres modos de visión de color del proyecto (`SAVAGEWIND_CVD_SIMULATE=protan`,
  `deutan`, `tritan`; `runtime/colour_vision.c:54`), entonces en una captura de ventana los dos jugadores se
  distinguen por forma y nombre aunque el color se confunda (revisión visual a mano, anotada en `bitacora/`).
- **CA-8** `python tools/i18n_check.py` termina sin errores con las cadenas nuevas.
- **CA-9** Dado B en un santuario y A fuera, entonces la lista de A muestra a B "en <mapa>" y no hay marcador de B en
  el mundo.
- **CA-10** Dado A con la vida completa, entonces `coop overlay` en B la lista como visible y no destacada; cuando
  la vida de A baja del 25 %, entonces en menos de 1 s la lista como destacada.
- **CA-11** La rueda ofrece exactamente las ocho frases de la decisión 28, en ese orden, y las ocho tienen
  traducción en los 13 ficheros de `runtime/ui/i18n/*.inc`.
- **CA-12** Dado un anfitrión y un invitado que no han tocado Opciones, entonces el anfitrión tiene el primer color
  y la primera forma y el invitado los segundos; cuando el invitado elige otros en Opciones, entonces A lo ve
  cambiado en menos de 2 s y la elección sigue ahí en la sesión siguiente.

## Qué NO hacer

- No dibujar nada desde el hilo del juego ni desde el hilo de red: el overlay se dibuja en `menu_render`.
- No añadir un elemento al overlay sin añadirlo a `menu_draws()` (`runtime/ui/menu.mm:2500`).
- No usar fuentes, iconos ni sonidos sacados del juego.
- No pasar texto recibido por red como cadena de formato (usar `AddText` o `"%s"`).
- No tocar `runtime/nvn/vulkan/exec.c` para esto: la cámara ya se publica.
- No intentar dibujar dentro del mapa nativo del juego en F4.
- No añadir texto visible sin traducción ni cambiar `online.h` por tu cuenta (dueño único).
- No leer memoria del juego desde el hilo de UI más allá de lo que ya se hace (UI:258-259); los datos nuevos llegan
  por `online_players()`.

## Riesgos y preguntas abiertas

- Riesgo: las etiquetas tiemblan o van retrasadas si la cámara es de otro fotograma, sobre todo con generación de
  fotogramas (mapa 04 §6, riesgo 5).
- Riesgo: ImGui siempre se dibuja encima del HUD del juego; un marcador puede tapar corazones o diálogos.
- Riesgo: cada campo nuevo en el estado sube la versión de protocolo y hay que repetirlo en el bot.
- Preguntas ya contestadas por el dueño (Q-10-1, Q-10-2 y Q-10-4): ver ADR-009 (decisiones 26, 27 y 28) en
  `../decisiones/`.
- Pregunta ya contestada por el dueño (Q-10-3): ver ADR-011 (decisión 41) en `../decisiones/`.
- **Q-10-5 (abierta, no es de diseño)**: ¿con qué botón se lanza el ping y se abre la rueda? Hay que revisar qué
  botones están libres (punto 5b) y proponer una combinación al dueño.

## Fase y dependencias

- **Fase**: F4 (se puede empezar en paralelo desde F2: no comparte código con `online_actors.c`).
- **Depende de**: CU-02. La vida y los pings necesitan un cambio de protocolo coordinado.
- **Lo necesitan**: CU-09 (marcar el objeto soltado), CU-11 (aviso de caído), CU-12 (indicador de PvP), CU-13.
