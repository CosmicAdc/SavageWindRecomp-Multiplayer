# CU-08 · Teletransportarse al compañero (mismo mapa)

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Un jugador pulsa una opción y aparece junto a su compañero, siempre que los dos estén en el mismo mapa. Es una ayuda
para no perderse y para reunirse rápido. No existe en el PR, pero las piezas ya están en la base del proyecto.

## Actores

- **Viajero**: quien se teletransporta.
- **Destino**: el compañero.

## Precondiciones

- Sesión conectada y los dos en el mundo, **en el mismo mapa**.
- El viajero está en juego normal: a pie, no montado, no en una escena, no cargando, vivo.
- El grupo de sitios opcional del teletransporte está disponible en este build (`tp_gate`; mapa 03 §1.2).

## Flujo principal

1. El viajero abre la pestaña Online (o usa un atajo) y elige **Ir con <nombre>**.
2. El juego comprueba que se puede y muestra "Viajando…".
3. El viajero aparece a un par de metros del compañero, en el suelo, sin daño de caída.
4. Si el destino está lejos, hay una espera corta mientras carga el terreno ("Cargando el suelo en el destino…").
5. Los dos se ven y siguen jugando.

**Variante P5 (invocar al compañero)**: el anfitrión elige "Traer a <nombre>"; al compañero le aparece una petición
"<nombre> quiere traerte. ¿Aceptar?"; si acepta, es él quien ejecuta el flujo anterior hacia quien le llamó.

## Flujos alternativos y de error

| Situación | Comportamiento esperado |
|---|---|
| Compañero en otro mapa (santuario, Bestia Divina) | Opción deshabilitada con el motivo "está en <mapa>". El viaje entre mapas es CU-14 |
| Compañero cargando o sin estado reciente (3 s) | Deshabilitado: "está cargando" |
| Compañero en una escena, diálogo o menú | Se permite [PROPUESTA]; el viajero aparece cerca sin interrumpirle |
| Compañero muerto o caído | Se permite, para poder revivirlo (CU-11) |
| Compañero en el aire, trepando, nadando o sobre lava | El viajero aparece en la última posición segura conocida del compañero (en suelo), no en el aire |
| Viajero montado, en escena, cargando o muerto | Deshabilitado con el motivo |
| Viajero en combate | Permitido [PROPUESTA], con un tiempo de espera entre usos para que no sea una huida gratis |
| Ya hay un teletransporte en curso | La petición se rechaza (`control_tp_request` devuelve `false`) |
| El terreno no termina de cargar | La máquina de teletransporte agota su plazo (120 s) y da fallo; se informa |
| Se corta la conexión durante el viaje | El viaje termina en el destino pedido; después aplica CU-05 |
| Invocación rechazada o sin respuesta en 20 s | No pasa nada; se avisa a quien la pidió |

## Estado actual en el PR: **FALTA** (piezas listas)

- No hay mensaje ni interfaz de teletransporte al compañero (mapa 06 §9).
- Piezas disponibles:
  - Posición y mapa del compañero: `OnlinePlayer.pos` (NETH:138) y `pose.map` (NETH:93); `same_map()` (OA:1484-1486).
  - API de teletransporte de la base: `control_tp_request(x, y, z)`, `control_tp_status`, `control_tp_link_pos`
    (`runtime/control.h:90-94`, `runtime/control_tp.c:280-322`). La petición es atómica y se puede hacer desde otro
    hilo. Es "la entrada natural para teleport al compañero" (mapa 03 §1.2).
  - Salto lejano con espera de carga de terreno mediante el parche `tp_gate` y aterrizaje sin daño durante 4 s
    (`runtime/control_tp.c:53-86`, `104-112`, `173-184`).
  - El lado online ya contempla un salto largo: si los actores remotos siguen vivos se conservan; si no, se olvidan y
    se rehacen tras 60 fotogramas (OA:5748-5761).
- **Obstáculo**: hoy la máquina de teletransporte solo avanza en modo desarrollador
  (`control_game_frame`: `if (control_enabled())`, `runtime/control_game.c:36-40`). En el montaje de prueba el modo
  desarrollador se activa solo porque `SAVAGEWIND_CONTROL` lo enciende (`runtime/control.c:751-758`); un jugador
  normal no lo tiene.
- El teletransporte **no cambia de mapa** (DOC:747-748; mapa 03 §1.2).

## Qué hay que construir

1. **Permitir el teletransporte en sesión sin modo desarrollador**: que `control_tp_frame` avance también cuando hay
   una petición cooperativa pendiente, sin abrir el resto de comandos de desarrollador.
2. **Acción "Ir con <nombre>"** en la pestaña Online y como atajo. Calcula el destino a partir de `OnlinePlayer.pos`
   (posición interpolada) con un desplazamiento de 1,5-2 m y llama a `control_tp_request`. La interfaz sondea
   `control_tp_status` para mostrar el progreso, igual que la pestaña Teleport existente.
3. **Comprobaciones previas** en el viajero: `same_map`, compañero no `ONLINE_F_LOADING`, viajero no en evento
   (`framepace_in_event`), no montado ni muerto (AS `Dead*`, como en OA:1738).
4. **Posición segura**: que cada jugador publique su última posición "en suelo firme" (campo nuevo en el estado;
   cambia el protocolo) o, como primera versión, usar `pos` y confiar en el aterrizaje sin daño.
   [sin verificar: cómo saber de forma fiable que Link está en suelo firme; hoy solo se conocen los nombres de AS.]
5. **Validar el destino**: coordenadas finitas y dentro de límites razonables antes de llamar (viene de la red).
6. **Invocación con consentimiento (P5)**: mensaje nuevo de petición y respuesta; diálogo en el overlay; plazo.
7. **Tiempo de espera entre usos** configurable por el anfitrión (regla de sesión, como `collide`/`revive`).
8. Textos traducidos; ajuste en `online.ini`.

## Reglas y decisiones de diseño

- **[DUEÑO] D9**: el teletransporte al compañero es una ayuda de juego que debe existir.
- **[PROPUESTA] P5**: invocar al compañero, siempre con su consentimiento. Nunca se mueve a un jugador sin que acepte.
- **[PROPUESTA]** Solo dentro del mismo mapa en F4; entre mapas en F5 (CU-14).
- **[PROPUESTA]** Permitido en combate, con un tiempo de espera de 30 s entre usos.
- **[PROPUESTA]** Disponible para los dos jugadores por igual, y el anfitrión puede desactivarlo para la sesión.
- **[PROPUESTA]** El viajero no arrastra su caballo ni objetos que sostenga.

## Criterios de aceptación

- **CA-1** Dado el montaje común con A y B en el campo a más de 500 m (`swctl.py --port 7778 goto <lugar lejano>`),
  cuando B ejecuta la acción "Ir con Host" (por la interfaz o por un comando de control nuevo `coop tp`), entonces en
  menos de 30 s `swctl.py --port 7778 pos` está a menos de 5 m del `pos` de A.
- **CA-2** Dado el viaje anterior, entonces la vida de B no baja (sin daño de caída) y B no queda bajo el suelo:
  su altura difiere menos de 3 m de la de A.
- **CA-3** Dado el viaje anterior, entonces tras llegar `swctl.py --port 7778 actors OnlinePlayer` devuelve
  exactamente 1 actor (sin duplicados) en menos de 10 s.
- **CA-4** Dado B dentro de un santuario y A fuera, entonces la acción está deshabilitada en ambos y, si se fuerza
  por comando, responde con error sin mover a nadie.
- **CA-5** Dado B en una escena (`swctl.py --port 7778 event <flujo>`), cuando B pide viajar, entonces se rechaza con
  motivo y no se cuelga.
- **CA-6** Dado un juego sin modo desarrollador (sin `SAVAGEWIND_CONTROL` ni `SAVAGEWIND_DEV`), cuando el jugador usa
  "Ir con …" en sesión, entonces funciona; y fuera de sesión, la pestaña Teleport sigue oculta como hoy.
- **CA-7** (P5) Dado A que invoca a B, cuando B no responde en 20 s, entonces B no se mueve; cuando acepta, se cumple
  CA-1 para B.
- **CA-8** Dado un bot que publica una posición con NaN o a 1e9 m, cuando el jugador pide viajar hacia él, entonces
  se rechaza.

## Qué NO hacer

- No escribir la posición de Link directamente: usar `control_tp_request` (`runtime/control_tp.c:298`), que ya
  espera a que cargue el terreno y evita el daño de caída.
- No mover nunca al otro jugador sin su aceptación: cada juego solo teletransporta a su propio Link.
- No intentar cambiar de mapa con esta función (santuario, Bestia): es CU-14.
- No abrir el resto de comandos de desarrollador al quitar la restricción de `runtime/control_game.c:36-40`: solo la
  máquina de teletransporte y solo en sesión.
- No usar como destino una posición recibida por red sin comprobar que es finita, que el compañero está en el mismo
  mapa y que su estado tiene menos de 3 s.
- No teletransportar durante una escena, montado, cargando o muerto.
- No añadir mensajes de protocolo (invocación P5) sin el dueño de `online.h`.

## Riesgos y preguntas abiertas

- Riesgo: aparecer dentro de geometría o en el aire si el compañero está trepando, planeando o nadando.
- Riesgo: las cadenas de punteros de la posición real son de 1.9.0 (mapa 03 §1.2).
- Riesgo: teletransportarse durante una escena del propio viajero puede romperla; por eso se bloquea.
- Riesgo: abrir la máquina de teletransporte fuera del modo desarrollador toca código de la base (`control_game.c`),
  que puede chocar con cambios de upstream.
- **Pregunta al dueño**: ¿se puede usar en combate y cuántas veces (sin límite, con espera, o solo fuera de combate)?
- **Pregunta al dueño**: ¿debe costar algo (nada, un tiempo de espera, un objeto)?
- **Pregunta al dueño**: ¿la invocación (traer al otro) la puede pedir cualquiera o solo el anfitrión?
- **Pregunta al dueño**: ¿se permite viajar hacia un compañero que está en plena escena o diálogo?

## Fase y dependencias

- **Fase**: F4. El viaje entre mapas, en F5 (CU-14).
- **Depende de**: CU-02 (posición y mapa del compañero), CU-03.
- **Lo necesitan**: CU-11 (llegar hasta el caído), CU-14.
