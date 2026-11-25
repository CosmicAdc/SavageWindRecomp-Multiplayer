# F3 — Red robusta: tareas

Cada tarea está escrita para ejecutarse **sola**, por un modelo que no ha leído nada más. Si una tarea te obliga a
adivinar, para y pregunta: no inventes.

## Convención

```
- [ ] T3xx [P] [área] descripción            ([P] = se puede hacer en paralelo; sin [P] = en serie)
  files:           ficheros que la tarea puede crear o modificar (y solo esos)
  verify:          orden que demuestra que funciona (sale con 0)
  depende de:      tareas o fases que deben estar fusionadas antes
  quién:           dueño (la persona) / agente
  contexto:        qué hace el código hoy, con fichero:línea, y por qué hace falta el cambio
  pasos:           lista numerada y concreta
  hecho cuando:    resultado observable
  no hagas:        trampas de esta tarea
  modelo sugerido: alto / medio / bajo
```

`alto` = exige razonar sobre concurrencia, memoria del juego, seguridad o ingeniería inversa.

## Reglas que valen para todas las tareas (léelas una vez)

- **ADR-006 manda sobre los pasos de cada tarea.** El subagente solo escribe código y documentos, hace commit y marca
  todo como "NO verificado". **No compila, no ejecuta pruebas, no lanza el juego ni Docker, no descarga dependencias
  (ENet, miniupnpc, imágenes) y no hace push.** Todo paso de abajo que diga "ejecuta", "mide", "compila", "descarga" o
  "regenera" lo hace **el orquestador** (o el dueño), de uno en uno, y anota la salida en `specs/bitacora/`. El
  subagente solo puede usar `grep` y `git diff`. El push al fork ocurre solo al cerrar la fase.
- Repositorio: el fork, rama de trabajo creada desde `coop/main`. Rutas siempre **relativas a la raíz del repo**.
  Nunca escribas rutas absolutas ni nombres de usuario en código, pruebas o documentos.
- Las líneas `fichero:línea` son del PR #294 original (HEAD `350c5f8`). Si F2 movió el código, busca la función por
  su nombre con `grep -n`.
- Escenarios de `tools/nettest/`: cada uno es un fichero `tools/nettest/scenarios/<nombre>.sh`; `run.sh <nombre>` lo
  descubre por el nombre (lo implementa T301). Una tarea que añade un escenario lo declara en su `files:` y no edita
  `run.sh`, así dos tareas no se pisan.
- No edites `generated/`, `extracted/`, `online_mod/` ni nada de `third_party/` (única excepción: añadir miniupnpc
  como diga el ADR de T306). No subas ficheros del juego.
- Decisiones del dueño que mandan sobre cualquier tarea (`specs/decisiones/`): ENet y miniupnpc aceptadas, con
  licencia compatible con MIT y anotadas en `docs/third-party.md` (ADR-007, decisión 11); límite de 2 jugadores por
  ahora (diseña para N, prueba con 2), contraseña opcional y el código de invitación puede contener la IP (ADR-007,
  decisión 12); plazo de gracia de reconexión de 10 minutos (ADR-008, decisión 18); si la conexión directa falla, el
  juego solo muestra un error claro: sin guía de red virtual (Tailscale, ZeroTier) ni servicio de presentación o
  perforación de NAT (ADR-009, decisión 23); ronda 4 (**ADR-011**, decisiones 29-47): el anfitrión es siempre la
  misma persona y, si cae, la sesión termina sin traspaso (decisiones 29 y 32); lo que el invitado hace en el mundo
  durante un corte se pierde y conserva solo su inventario (31); los menús no pausan y la colisión entre jugadores
  está activada, con opción del anfitrión para apagarla (44, 45); recorrido de prueba de la Meseta de los Albores (46).
  **Dato de planificación (ADR-011, decisión 47):** F3 **no se cierra** sin la prueba con dos PC por Internet (T341). Si
  no hay segundo PC, esa prueba queda como pendiente explícito en la bitácora y **no se sustituye por Docker**.
- No toques `tools/patches.py` ni `tools/game_sites.py` en F3 (cambian `GAMELIB_ID` y obligan a recompilar el juego).
- No cambies estructuras de cable, números de mensaje ni `PROTOCOL` salvo que la tarea diga "dueño del protocolo".
- No bloquees nunca el hilo del juego ni el de presentación: nada de `recv`, `send`, `connect`, `getaddrinfo`, UPnP
  o esperas ahí.
- Todo campo que llega por red es hostil: comprueba longitud, rango, terminador de cadena y `isfinite` antes de usar.
- Código C11 que compile con `clang -Wall` en Windows (MSYS2 UCRT64), Linux y macOS. Sockets con `#ifdef _WIN32`
  como en `runtime/control.c:2-39`. Funciones internas `static`; las públicas con prefijo `online_`.
- Texto visible para el jugador: en inglés en el código, envuelto con `tr()`, dentro de un `.mm` de `runtime/ui/`, y entrégalo en la
  lista de textos nuevos (T333). No edites `runtime/ui/i18n/*.inc` si no eres el carril i18n.
- Órdenes en Windows: shell MSYS2 UCRT64 con `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`. Python del
  proyecto: `tools/venv/Scripts/python.exe` (en Linux `tools/venv/bin/python`).
- Comprobación mínima de cualquier cambio en `runtime/`: `make runtime-objs` sin avisos nuevos (la ejecuta el orquestador).
- Una tarea está hecha cuando su `verify` pasa (ejecutado por el orquestador) **y** queda una entrada en `specs/bitacora/` con la salida.

---

## Bloque 1 — Arnés, bot y medidas

- [ ] T301 [pruebas] Arnés de red en Docker con latencia y pérdida simuladas
  files: tools/nettest/compose.yml, tools/nettest/Dockerfile, tools/nettest/netem.sh, tools/nettest/run.sh, tools/nettest/scenarios/smoke.sh, tools/nettest/README.md
  verify: `sh tools/nettest/run.sh smoke` sale con 0 e imprime `smoke: OK` (dos bots se ven entre sí a través del servidor con el perfil `wifi`)
  depende de: F0 (imagen Docker de pruebas), F2 (servidor ejecutable sin el juego)
  quién: agente escribe; orquestador ejecuta (Docker)
  contexto: Hoy no hay ninguna prueba automática de red; el bot `tools/online_bot.c` se compila a mano (`cc -O2 -o build/online_bot tools/online_bot.c`) y no está en el `Makefile`. Las pruebas de F3 necesitan tres contenedores (servidor y dos bots) y poder estropear la red con `tc netem`.
  pasos:
    1. Crea `tools/nettest/Dockerfile` a partir de la imagen de pruebas de F0; añade el paquete `iproute2` (da `tc`).
    2. Crea `compose.yml` con tres servicios (`server`, `bot_a`, `bot_b`) en una red puente propia y `cap_add: [NET_ADMIN]`.
    3. El servicio `server` arranca el servidor sin juego. Prueba primero `build/savagewind --server 52530`; si el binario completo no arranca en el contenedor, usa el arnés de servidor de las pruebas de F2 y anótalo en el README.
    4. `netem.sh <perfil>` aplica en `eth0` uno de: `lan` (nada), `fibra` (`delay 20ms 4ms loss 0.2%`), `normal` (`delay 40ms 10ms loss 1%`), `wifi` (`delay 80ms 20ms loss 2%`), `movil` (`delay 150ms 40ms loss 5%`), `rafagas` (`delay 60ms 15ms loss gemodel 1% 20% 70% 0.1%`), `corte` (`loss 100%`), y `quitar`.
    5. `run.sh <escenario>` ejecuta `scenarios/<escenario>.sh` (descubrimiento por nombre), levanta el compose, aplica el perfil, espera, recoge los registros de los tres contenedores en `build/nettest/<escenario>/` y devuelve 0 o 1.
    6. Escenario `smoke` (`scenarios/smoke.sh`): perfil `wifi`, 20 s; pasa si cada bot registró al menos 200 STATE del otro.
    7. Escribe el README con cómo lanzar cada cosa y cuánta RAM usa.
  hecho cuando: `run.sh smoke` pasa tres veces seguidas y deja los registros en `build/nettest/smoke/`.
  no hagas: no uses `network_mode: host`; no metas ficheros del juego en la imagen; no llames a un directorio `input`, `save`, `keys`, `dump` o `generated` (los ignora git y los rechaza el CI); no dejes `netem` aplicado al terminar.
  modelo sugerido: medio

- [ ] T302 [P] [pruebas] Modos de fallo y de ataque en el bot
  files: tools/online_bot.c, tools/nettest/scenarios/bot_modes.sh
  verify: `sh tools/nettest/run.sh bot_modes` sale con 0 (lanza el bot una vez por modo y comprueba en el registro la línea `bot: mode <nombre> active`)
  depende de: T301, F2 (cabecera de protocolo compartida)
  quién: agente
  contexto: El bot solo tiene `BOT_FREEZE`, que deja de enviar pero **sigue leyendo** (`tools/online_bot.c:145`), así que no reproduce el fallo principal: un par que no lee congela a todos. Su buffer de recepción es de 8192 bytes y ENEMIES puede medir 9965 (mapa 06 §5.2). Faltan modos para probar autoridad, límites y reconexión.
  pasos:
    1. Sube el buffer de recepción a 32768 bytes (igual que el juego).
    2. Añade, leyendo variables de entorno como las existentes: `BOT_NOREAD=<s>` (a los s segundos deja de llamar a `recv` pero mantiene el socket abierto), `BOT_FLOOD=<tipo>,<hz>` (envía ese tipo de mensaje a esa frecuencia), `BOT_SPOOF=1` (envía ENEMIES, LOOTS y QUESTS sin ser anfitrión), `BOT_BADPASS=<n>` (hace n intentos con contraseña mala y sale), `BOT_PREAUTH=<n>` (abre n conexiones sin enviar HELLO y las mantiene), `BOT_DROP=<s>,<d>` (a los s segundos cierra el socket y reintenta a los d segundos), `BOT_STATS=1` (imprime cada segundo: STATE recibidos por ranura, intervalo máximo entre STATE en ms, mensajes de cada tipo).
    3. Cada modo imprime al activarse `bot: mode <nombre> active`.
    4. Crea el escenario `tools/nettest/scenarios/bot_modes.sh`.
  hecho cuando: los siete modos funcionan contra el servidor actual y `BOT_STATS` da números coherentes (unos 20 STATE por segundo del otro bot en perfil `lan`).
  no hagas: no copies a mano estructuras de cable (usa la cabecera compartida de F2); no cambies el comportamiento por defecto del bot (sin variables debe seguir andando en círculo); no añadas dependencias.
  modelo sugerido: medio

- [ ] T303 [P] [medidas] Línea base de TCP bajo latencia y pérdida
  files: tools/nettest/measure.sh, tools/nettest/report.py, specs/bitacora/F3-T303-linea-base-tcp.md (la entrada la completa el orquestador con las medidas)
  verify: `sh tools/nettest/measure.sh tcp` sale con 0 y escribe `build/nettest/tcp/resumen.csv` con una fila por perfil
  depende de: T301, T302
  quién: agente escribe `measure.sh` y `report.py`; **orquestador** ejecuta las medidas (Docker) y rellena la bitácora
  contexto: Todo viaja por una conexión TCP: un paquete perdido retiene lo posterior y llega en ráfaga, y la hora de cada instantánea es la de recepción (`runtime/online.c:910`), así que el interpolador salta. No hay ninguna medida; el dueño debe decidir con datos si se cambia de transporte.
  pasos:
    1. (Orquestador, al ejecutar `measure.sh`.) Para cada perfil (`lan`, `fibra`, `normal`, `wifi`, `movil`, `rafagas`) y 3 repeticiones de 120 s: levanta servidor y dos bots con `BOT_STATS=1`.
    2. Haz que `measure.sh` obtenga del bot (`BOT_STATS`, T302) la hora de recepción en ms de cada STATE; si falta una columna, pídela a T302, no edites el bot aquí.
    3. `report.py` calcula por perfil: intervalo entre STATE p50/p95/p99/máximo; atascos por minuto (intervalos > 150 ms); bytes/s por nodo; latencia de un CHAT (hora de envío en el texto, hora de recepción).
    4. Añade el perfil `subida` (`rate 1mbit` en el servidor más `normal`).
    5. Prueba de recuperación: `corte` durante 3 s y tiempo hasta el primer STATE nuevo.
    6. Escribe la entrada de bitácora con la tabla y las órdenes exactas.
  hecho cuando: existe `resumen.csv` con las 7 filas y la bitácora con la tabla.
  no hagas: no midas con el juego real (aquí solo bots); no promedies las 3 repeticiones sin dar también el peor valor; no cambies código del runtime.
  modelo sugerido: medio

- [ ] T304 [P] [medidas] Prototipo de banco con UDP (ENet) para comparar
  files: tools/netbench/** (nuevo), tools/nettest/measure.sh (solo la rama `udp`), specs/bitacora/F3-T304-banco-udp.md
  verify: `sh tools/nettest/measure.sh udp` sale con 0 y escribe `build/nettest/udp/resumen.csv` con las mismas columnas que T303
  depende de: T303
  quién: agente escribe; orquestador descarga, compila y mide
  contexto: Pregunta a responder: con los mismos tamaños y ritmos de mensaje, ¿cuánto mejoran los atascos y la edad del estado usando un canal no fiable para el estado continuo y uno fiable para los eventos? ENet (licencia MIT) ofrece canales fiables y no fiables sobre UDP y está aceptada por el dueño como dependencia (ADR-007, decisión 11): esta tarea no espera ningún permiso. Esto es un banco aparte: no se integra en el juego.
  pasos:
    1. Crea un programa emisor y uno receptor en `tools/netbench/` que usen ENet con 2 canales: 0 no fiable secuenciado, 1 fiable.
    2. El emisor imita a un jugador: 3375 bytes a 20 Hz y 1600 bytes a 20 Hz por el canal 0; un evento de 120 bytes por segundo por el canal 1.
    3. Como 3375 bytes no caben en un datagrama de ~1200 bytes, mide dos variantes: (a) dejando que ENet fragmente; (b) partiendo el estado en 3 trozos independientes de ≤ 1150 bytes. Anota ambas.
    4. Mismos perfiles, repeticiones y métricas que T303, con el mismo `report.py`.
    5. ENet: el **orquestador** la descarga dentro de `build/` en el contenedor (el subagente no descarga nada; tampoco se añade al repositorio en esta tarea). El subagente deja en el README de `tools/netbench/` la versión y la orden exactas.
    6. Bitácora con la tabla comparada con T303.
  hecho cuando: hay dos CSV comparables (TCP y UDP) y la bitácora dice, por perfil, cuál gana y por cuánto.
  no hagas: no toques `runtime/`; no añadas ENet a `third_party/` ni al `Makefile` (eso solo ocurre si T305 decide migrar); no saques conclusiones: la decisión es de T305.
  modelo sugerido: medio

- [ ] T305 [decisión] ADR: seguir con TCP o migrar el estado continuo a UDP
  files: specs/decisiones/ADR-transporte-tcp-o-udp.md
  verify: el fichero existe, contiene la tabla de T303 y T304, la decisión y la firma del dueño (`Decidido por: <nombre>, <fecha>`); `git diff --check` sin errores
  depende de: T303, T304, T309
  quién: dueño (el agente redacta el borrador)
  contexto: F3 mide y decide; no migra. Usar ENet ya está permitido (ADR-007, decisión 11): la decisión depende solo de las medidas, y es la que cierra ADR-005. Regla propuesta en `plan.md` §L: migrar si en el perfil `wifi` TCP con colas no bloqueantes da más de 2 atascos por minuto o p99 > 200 ms y UDP da menos de la mitad.
  pasos:
    1. El orquestador repite T303 después de T309 (colas no bloqueantes) y añade esa tercera columna.
    2. Redacta: contexto, opciones (TCP con colas; UDP con ENet; UDP propio), medidas, coste de cada opción (dependencia nueva en tres plataformas, partir STATE), recomendación.
    3. El dueño elige y firma.
  hecho cuando: el ADR está firmado. Si la decisión es migrar, queda creada una fase o tarea aparte; F3 no la ejecuta.
  no hagas: no decidas por el dueño; no ocultes los perfiles donde tu opción preferida pierde.
  modelo sugerido: alto

- [ ] T306 [P] [investigación] Spike: apertura automática de puerto con miniupnpc
  files: tools/netbench/upnp_probe.c, tools/nettest/scenarios/upnp_probe.sh, specs/decisiones/ADR-upnp.md, specs/bitacora/F3-T306-upnp.md
  verify: `sh tools/nettest/run.sh upnp_probe` sale con 0 (la sonda abre y cierra una redirección contra `miniupnpd` en un contenedor)
  depende de: T301
  quién: agente (la prueba contra el router real la hace el dueño)
  contexto: Hoy no hay UPnP ni NAT-PMP; la pestaña Online solo dice que se redirija el puerto (`runtime/ui/online_ui.mm:57-59`). El dueño ya aceptó miniupnpc (ADR-007, decisión 11): no hay que elegir entre código propio y biblioteca ni pedir permiso. Lo que queda por saber es si funciona y cómo se incluye, porque una biblioteca enlazada es cara en este proyecto (tres plataformas, script de instalación, CI, `docs/third-party.md`).
  pasos:
    1. Pregunta exacta: ¿miniupnpc hace la secuencia completa (descubrimiento SSDP, `AddPortMapping`, `GetExternalIPAddress`, `DeletePortMapping`, y NAT-PMP si lo cubre) y cómo conviene incluirla: fuente en `third_party/` o paquete del sistema en cada plataforma?
    2. Escribe `upnp_probe.c` (programa suelto, un solo fichero) que haga esa secuencia con miniupnpc, con plazo total de 5 s, e imprima cada paso. El orquestador descarga miniupnpc dentro de `build/` en el contenedor (el subagente no descarga nada); no se añade al repositorio en esta tarea.
    3. Añade al compose un contenedor con `miniupnpd` como puerta de enlace de prueba y el escenario `scenarios/upnp_probe.sh`.
    4. El orquestador ejecuta la sonda en Docker; el dueño la ejecuta contra su router y pega la salida en la bitácora.
    5. Forma de la respuesta: ADR con tabla (forma de incluirla, versión y licencia leída en su fichero de licencia, funcionó en Docker sí/no, funcionó en el router real sí/no, coste de integración en Windows, Linux y macOS) y recomendación, más el texto de la entrada que T324 añadirá a `docs/third-party.md`.
  hecho cuando: el ADR existe con las dos pruebas (Docker y router real) anotadas y la licencia confirmada como compatible con MIT.
  no hagas: no integres nada en `runtime/` todavía; no analices XML con funciones sin límite de longitud; no confíes en la respuesta del router (tamaño máximo, plazo).
  modelo sugerido: alto

- [x] T307 [P] [investigación] Qué identificador de compilación se puede comparar entre dos PC
  hecha: agente Sonnet (integrada `4dd75f6`) — bitácora F3-T307-identificador-compilacion.md
  files: specs/bitacora/F3-T307-identificador-compilacion.md
  verify: `grep -c '^| [123] |' specs/bitacora/F3-T307-identificador-compilacion.md` da 3 (una fila de tabla por pregunta, con la orden usada y su salida en el cuerpo)
  depende de: F0
  quién: agente redacta la tabla vacía; orquestador y dueño ejecutan las órdenes (hace falta una segunda máquina o una compilación con otra arquitectura)
  contexto: El saludo solo compara `PROTOCOL` (`runtime/online.c:240`). Queremos rechazar a quien tenga otra compilación del código del juego. El identificador `GAMELIB_ID` se calcula de las fuentes del generador **más los indicadores del compilador** (`Makefile:136-142`) y existe `GAMELIB_ID_COMPAT` (`Makefile:150-152`) para CPU sin AVX2/FMA (`runtime/gamelib.c:43-59`).
  pasos:
    1. Pregunta 1: con el mismo commit, ¿`make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id` da lo mismo en Windows y en Linux (Docker)?
    2. Pregunta 2: ¿qué función de `runtime/gamelib.c` devuelve el identificador en ejecución y está disponible cuando no hay biblioteca cargada?
    3. Pregunta 3: ¿`savagewind --server <puerto>` arranca en un contenedor Linux sin GPU ni pantalla?
    4. Forma de la respuesta: una tabla (pregunta, respuesta sí/no, evidencia) y una recomendación de qué enviar en el saludo: un identificador, los dos, o uno nuevo solo de fuentes.
  hecho cuando: T314 puede implementarse sin suposiciones.
  no hagas: no edites el fichero `.id` ni el `Makefile`; no deduzcas la respuesta leyendo: el orquestador ejecuta las órdenes y pega la salida (ADR-006).
  modelo sugerido: medio

## Bloque 2 — Protocolo (dueño único)

- [ ] T308 [protocolo] Definir el cambio de cable de F3 en un solo salto de versión
  files: runtime/online.h, runtime/online.c (solo `PROTOCOL`, enumeración de mensajes y estructuras de cable), la cabecera compartida con el bot que dejó F2, tools/online_bot.c, tests/test_online_wire.c, specs/contratos/protocolo.md
  verify: `clang -Wall -Iruntime tests/test_online_wire.c -o build/test_online_wire && build/test_online_wire` sale con 0 (aserciones de tamaño y de ida y vuelta de cada estructura nueva)
  depende de: F2, T302 (las dos tareas editan `tools/online_bot.c`: T302 va primero)
  quién: agente designado como **dueño del protocolo** (solo uno en todo el proyecto)
  contexto: El protocolo es `PROTOCOL 38`, con igualdad exacta en el servidor (`runtime/online.c:39-40`, `:240`). Las cargas son estructuras C empaquetadas (`:59-108`). F3 necesita campos y mensajes nuevos; se definen todos de una vez para que los demás carriles trabajen sin tocar el cable.
  pasos:
    1. Sube `PROTOCOL` al siguiente número libre y anótalo en el registro de versiones de `specs/contratos/protocolo.md`.
    2. Añade los mensajes: CHALLENGE (S→C: 16 bytes aleatorios, identificador de sesión), AWAY (S→C: ranura, segundos de gracia), y deja HELLO, WELCOME y REJECT ampliados.
    3. HELLO ampliado: magia, protocolo, nombre, respuesta de autenticación (32 bytes), testigo de reanudación (16 bytes, ceros si no hay), secreto de anfitrión (16 bytes, ceros si no), y la estructura `OnlineBuildInfo` (versión del modo cooperativo, identificador de compilación, versión del juego, versión de `online_mod`, estado de DLC).
    4. WELCOME ampliado: ranura, ranura del anfitrión, testigo de sesión, plazo de gracia.
    5. REJECT ampliado: motivo (añade compilación, juego, overlay, DLC, demasiados intentos, reanudación inválida) y los dos valores que difieren como cadenas cortas.
    6. STATE: añade `uint16_t seq` y `uint32_t t_ms` del emisor.
    7. Deja los manejadores nuevos vacíos (el servidor y el cliente deben seguir funcionando como antes con el saludo ampliado relleno de ceros).
    8. `_Static_assert(sizeof(...) == N)` para cada estructura de cable; actualiza el bot.
  hecho cuando: juego y bot compilan, `run.sh smoke` pasa con la versión nueva y `protocolo.md` lista cada mensaje con tamaño y sentido.
  no hagas: no implementes lógica (eso es de T309-T319); no reordenes campos existentes sin necesidad; no dejes relleno sin poner a cero; no cambies la versión dos veces.
  modelo sugerido: alto

## Bloque 3 — Núcleo de red (en serie, dueño único de `runtime/online.c`)

- [ ] T309 [núcleo] Cola de envío por cliente en el servidor, sin bloqueo
  files: runtime/online.c, runtime/online_sendq.c, runtime/online_sendq.h, tests/test_online_sendq.c, tools/nettest/scenarios/h1_noread.sh
  verify: `clang -Wall -Iruntime tests/test_online_sendq.c -o build/test_online_sendq && build/test_online_sendq` sale con 0 **y** `sh tools/nettest/run.sh h1_noread` sale con 0
  depende de: T301, T302, T308
  quién: agente
  contexto: `send_all` (`runtime/online.c:140-148`) hace `send()` bloqueante en bucle y `srv_broadcast` (`:213-216`) lo llama para cada cliente dentro del único hilo servidor. Si un cliente deja de leer, el hilo se queda dentro de `send`: nadie recibe nada y la expulsión por silencio (`:384`) no puede actuar. Ocurre con una simple caída de red.
  pasos:
    1. Escribe `online_sendq.c` sin sockets: una cola con (a) anillo de bytes "fiable" de 256 KiB y (b) casillas "reemplazables" indexadas por (tipo, ranura de origen) para STATE, SKEL y ENEMIES. API: `sendq_push(q, tipo, origen, datos, n, reemplazable)`, `sendq_peek(q, &ptr, &n)`, `sendq_consume(q, n)`, `sendq_overflow(q)`, `sendq_dropped(q)`.
    2. Regla: un mensaje cuya cabecera ya empezó a salir se termina antes de empezar otro; primero lo fiable, luego las casillas.
    3. Prueba unitaria: orden, sustitución en casilla, desbordamiento, envíos parciales de 1 byte.
    4. En `online.c`: pon todos los sockets aceptados en modo no bloqueante; `srv_broadcast` pasa a `sendq_push`; en `srv_loop` añade el conjunto de escritura a `select()` y vacía cada cola con `send()` no bloqueante.
    5. Expulsa (`srv_drop`) si `sendq_overflow` o si pasan 5 s con datos pendientes sin que el socket acepte un byte.
    6. En `select() < 0` duerme 10 ms en vez de `continue` inmediato (`:356`).
    7. Escenario `scenarios/h1_noread.sh`: bot A con `BOT_NOREAD=5`, bot B con `BOT_STATS=1` más un tercer bot que emite; pasa si el intervalo máximo entre STATE de B no supera 150 ms y A aparece expulsado antes de 15 s.
  hecho cuando: las dos pruebas pasan y `run.sh smoke` sigue pasando.
  no hagas: no crees un hilo por cliente; no descartes nunca un mensaje fiable en silencio; no uses `SO_SNDTIMEO` como sustituto; no cambies estructuras de cable.
  modelo sugerido: alto

- [ ] T310 [núcleo] Cliente sin bloqueo y salida de sesión que no cuelga la UI
  files: runtime/online.c, tools/nettest/scenarios/leave_fast.sh
  verify: `sh tools/nettest/run.sh leave_fast` sale con 0 (mide que `online_leave` vuelve en < 1 s con el servidor congelado y con una dirección que no responde)
  depende de: T309
  quién: agente
  contexto: `stop_all` (`runtime/online.c:1322-1335`) hace `pthread_join` del hilo cliente y del servidor **desde el hilo de UI**. El hilo cliente puede estar hasta 6 s por dirección en `connect` y un tiempo indefinido en `getaddrinfo` (`:1128-1158`): la imagen se congela. El cliente también envía con `send_all` bloqueante.
  pasos:
    1. Da al cliente una cola de salida con `online_sendq` y socket no bloqueante; el envío ocurre en su `select()`.
    2. En `stop_all`: marca `stop`, cierra el socket para despertar al hilo y espera como máximo 500 ms. Si el hilo no ha terminado (está en `getaddrinfo`), márcalo "huérfano": terminará solo y liberará sus recursos al salir; no lo esperes.
    3. Un hilo huérfano no puede tocar `g` después de que empiece una sesión nueva: usa un contador de generación que el hilo compara antes de escribir (patrón de `runtime/control.c:623`).
    4. Sustituye las lecturas sin cerrojo de `g.state`, `g.hosting`, `g.id` por atómicos (`stdatomic.h`).
    5. Escribe la prueba `leave_fast` con el arnés de F2 (cliente en proceso) o con un programa de prueba pequeño.
  hecho cuando: salir o terminar sesión nunca tarda más de 1 s, y entrar/salir 50 veces seguidas no deja hilos ni sockets abiertos (compruébalo contando descriptores).
  no hagas: no uses `pthread_cancel` ni `TerminateThread`; no hagas `pthread_join` sin límite en el hilo de UI; no dejes que dos hilos cliente escriban en `g` a la vez.
  modelo sugerido: alto

- [ ] T311 [núcleo] El servidor sabe quién es el anfitrión y le reserva la ranura 0
  files: runtime/online.c, tests/test_online_host_slot.c, tools/nettest/scenarios/host_slot.sh
  verify: `sh tools/nettest/run.sh host_slot` sale con 0
  depende de: T310
  quién: agente
  contexto: No existe ningún campo "anfitrión" en `Server`/`SClient` (`runtime/online.c:194-211`). El anfitrión es un cliente que se conecta a su propio servidor por `127.0.0.1` (`:1347-1361`) y se **supone** que cae en la ranura 0, porque el servidor da la primera libre (`:361-362`) y `online_actors.c` lee las reglas de sesión de `pl[0]` (línea 2838-2840). Nada lo garantiza.
  pasos:
    1. En `online_host`, genera 16 bytes aleatorios con la fuente del sistema y guárdalos en `g_srv` y en una variable que lea el cliente local.
    2. El cliente local los envía en el campo "secreto de anfitrión" del HELLO (definido en T308).
    3. El servidor acepta ese secreto solo si la conexión viene de `127.0.0.1` o `::1` (usa `getpeername`) y coincide byte a byte en tiempo constante. Quien lo presenta recibe la ranura 0 y `sv->host_slot = 0`.
    4. Ningún otro cliente recibe jamás la ranura 0: la búsqueda de ranura libre empieza en 1.
    5. WELCOME informa de la ranura del anfitrión.
    6. Prueba: un bot que llega antes que el anfitrión obtiene la ranura 1; un bot remoto que envía un secreto (correcto o no) es rechazado.
  hecho cuando: la prueba pasa y en una sesión real el anfitrión es siempre la ranura 0.
  no hagas: no envíes el secreto fuera del proceso ni lo escribas en el registro o en `online.ini`; no uses `rand()`; no aceptes el secreto desde una dirección que no sea de bucle local.
  modelo sugerido: alto

- [ ] T312 [núcleo] Tabla de autoridad y validación en el servidor
  files: runtime/online.c, runtime/online_authority.c, runtime/online_authority.h, tests/test_online_authority.c, tools/nettest/scenarios/spoof.sh
  verify: `clang -Wall -Iruntime tests/test_online_authority.c -o build/test_online_authority && build/test_online_authority` sale con 0 **y** `sh tools/nettest/run.sh spoof` sale con 0
  depende de: T311, F2 (validación única de entradas)
  quién: agente
  contexto: `srv_message` reenvía ENEMIES (`runtime/online.c:278-280`), LOOTS, QUESTS y PICK (`:294-298`) de **cualquier** cliente, sin origen, y QUEST sin comprobar longitud (`:291-293`). Un invitado puede hacerse pasar por el anfitrión. El cliente anfitrión guarda el QUEST de cualquier ranura (`:1065-1070`).
  pasos:
    1. `online_authority.c` (puro): función `authority_check(tipo, es_anfitrion, longitud)` que devuelve una de: reenviar a los demás, reenviar solo al anfitrión, reenviar a todos, eco, descartar.
    2. Tabla: STATE/SKEL/EQUIP cualquiera → los demás. CHAT cualquiera → todos. ENEMIES/LOOTS/QUESTS solo anfitrión → los demás. ESTATE/LOOT_ADD/LOOT_TAKE/QUEST solo no anfitrión → solo al anfitrión. HIT y PICK cualquiera → los demás. PING → eco.
    3. Longitudes exactas: QUEST y QUESTS 4096 bytes, HIT 9, LOOT_TAKE 4, PICK 52; el resto con su mínimo y máximo.
    4. En `srv_message`, llama a la validación única de F2 y a `authority_check` antes de reenviar. Lo descartado suma 1 a `infracciones` del cliente.
    5. En el cliente: ignora un WELCOME si ya hay ranura (`:971-974`); acota `origin` y `est_from` a `< ONLINE_MAX`.
    6. Prueba unitaria: una aserción por fila de la tabla y por longitud límite. Escenario `spoof`: bot con `BOT_SPOOF=1`; el otro bot recibe 0 ENEMIES/LOOTS/QUESTS.
  hecho cuando: las dos pruebas pasan y una sesión real de dos jugadores sigue compartiendo enemigos, botín y misiones.
  no hagas: no cambies quién consume cada mensaje en `online_actors.c`; no elimines el filtro `g.hosting` del cliente (defensa en profundidad); no implementes aquí la política de flags del anfitrión (es F4).
  modelo sugerido: alto

- [ ] T313 [núcleo] Límite de tasa y conexiones sin autenticar
  files: runtime/online.c, tools/nettest/scenarios/flood.sh, tools/nettest/scenarios/preauth.sh
  verify: `sh tools/nettest/run.sh flood` y `sh tools/nettest/run.sh preauth` salen con 0
  depende de: T312, T320
  quién: agente
  contexto: La ranura se ocupa en el `accept`, antes del HELLO (`runtime/online.c:359-370`); `heard` se renueva con cualquier byte; no hay plazo para el HELLO. Ocho conexiones mudas bloquean la sesión. No hay contador de frecuencia: un cliente puede enviar mensajes de 16 KiB sin límite y el servidor los multiplica por N-1.
  pasos:
    1. Crea una lista de conexiones pendientes separada de las ranuras: máximo 8, máximo 2 por dirección IP, buffer de 512 bytes, plazo de 3 s para un HELLO completo.
    2. Solo un HELLO válido pasa la conexión a una ranura de jugador. Si no hay ranura, REJECT(FULL).
    3. Integra el cubo de fichas de T320: uno por cliente y tipo, con las tasas de `plan.md` §C (STATE y SKEL 40/s, ENEMIES 20/s, PING 2/s, QUEST 1/s, EQUIP 2/s, CHAT 4 cada 5 s, resto 20/s).
    4. Mensaje sobre la tasa: se descarta y suma infracción. Más de 50 infracciones en 10 s: `srv_drop`.
    5. Escenarios: `flood` (bot con `BOT_FLOOD=chat,200`: el otro recibe ≤ 5 CHAT en 5 s y el emisor acaba expulsado); `preauth` (bot con `BOT_PREAUTH=20`: un segundo bot normal entra en < 2 s).
  hecho cuando: las dos pruebas pasan y el anfitrión local nunca es limitado en una sesión normal de 10 minutos (0 infracciones en el registro).
  no hagas: no limites por tipo con un único cubo global; no registres una línea por cada mensaje descartado (limita el registro a 1 por segundo); no cuentes como infracción los mensajes de tipo desconocido.
  modelo sugerido: alto

- [ ] T314 [núcleo] Saludo con comprobación estricta de versión
  files: runtime/online.c, tools/online_bot.c, tools/nettest/scenarios/version_matrix.sh
  verify: `sh tools/nettest/run.sh version_matrix` sale con 0 (5 bots, cada uno con un campo distinto, reciben cada uno su motivo de rechazo)
  depende de: T307, T313, T325
  quién: agente
  contexto: El servidor solo compara magia y `PROTOCOL` (`runtime/online.c:240`). Dos jugadores con distinta compilación del código del juego, distinta versión del juego, distinto overlay `online_mod` o distinto DLC se conectan y luego fallan de formas raras. El runtime ya conoce esos datos (`runtime/gamelib.c:43-76`, `runtime/main.c:507`, `runtime/dlc_check.h:38`).
  pasos:
    1. Al crear sesión, el servidor guarda su `OnlineBuildInfo` (T325). El cliente rellena la suya en el HELLO.
    2. Orden de comprobación en el servidor: magia y protocolo → versión del modo cooperativo → compilación → versión del juego → overlay → DLC → (después) contraseña.
    3. Cada diferencia envía REJECT con su motivo y las dos cadenas (la del servidor y la del cliente).
    4. El cliente pone un estado legible: literal fijo en inglés como formato y los valores como argumentos (igual que `set_status` hoy).
    5. Usa para la compilación la regla que decidió T307.
    6. Añade al bot (`tools/online_bot.c`) las variables para falsear cada campo y escribe `scenarios/version_matrix.sh`.
  hecho cuando: cada una de las 5 diferencias produce su motivo y dos jugadores idénticos entran.
  no hagas: no uses datos de la red como cadena de formato; no reveles la contraseña ni si era correcta cuando falla la versión; no hagas la comprobación solo en el cliente.
  modelo sugerido: medio

- [ ] T315 [núcleo] Contraseña por desafío-respuesta y límite de intentos
  files: runtime/online.c, tools/nettest/scenarios/badpass.sh
  verify: `sh tools/nettest/run.sh badpass` sale con 0 **y** `grep -c "secreto-de-prueba" online.ini` da 0 tras guardar una sesión con esa contraseña
  depende de: T314, T321
  quién: agente
  contexto: La contraseña viaja en claro en HELLO (`runtime/online.c:105`), se compara con `strcmp` (`:241`), no hay límite de intentos y se guarda en claro en `online.ini` (`:1549-1551`).
  pasos:
    1. Al aceptar una conexión pendiente, el servidor envía CHALLENGE con 16 bytes aleatorios.
    2. El cliente calcula la respuesta con `online_auth` (T321) y la envía en el HELLO.
    3. El servidor verifica en tiempo constante contra `password_hash`/`password_salt`.
    4. Tabla de intentos por IP (32 entradas, la más antigua se recicla): 5 fallos → REJECT(demasiados intentos) durante 60 s, duplicando hasta 15 min.
    5. `--server ... --password p` (`runtime/main.c:440-445`) convierte la contraseña a sal + hash al arrancar.
    6. Escenario `badpass`: bot con `BOT_BADPASS=6`: los 5 primeros reciben REJECT(contraseña) y el sexto REJECT(demasiados intentos); un bot con la contraseña buena desde otra IP entra.
  hecho cuando: la prueba pasa y una captura de tráfico de la prueba no contiene la contraseña (`tcpdump` en el contenedor + `grep`).
  no hagas: no escribas la contraseña ni el hash en el registro; no compares con `strcmp`/`memcmp`; no bloquees por IP a `127.0.0.1`; no afirmes en la UI que la conexión está cifrada (no lo está).
  modelo sugerido: alto

- [ ] T316 [núcleo] Número de secuencia y hora del emisor en STATE; interpolar con la hora del emisor
  files: runtime/online.c, tests/test_online_interp.c
  verify: `clang -Wall -Iruntime tests/test_online_interp.c -o build/test_online_interp && build/test_online_interp` sale con 0
  depende de: T315
  quién: agente
  contexto: Cada instantánea recibida se marca con la hora local de recepción (`runtime/online.c:910`) y `online_players` interpola 100 ms por detrás entre dos instantáneas de un anillo de 8 (`:1387-1432`). Tras un atasco llegan varias juntas con casi la misma hora y el jugador remoto da un salto.
  pasos:
    1. El emisor rellena `seq` (suma 1 por STATE) y `t_ms` (milisegundos monótonos desde que conectó) definidos en T308.
    2. El receptor estima por jugador el desfase `recepción − t_ms` como el mínimo de los últimos 5 s y sitúa cada instantánea en `t_ms + desfase`.
    3. Descarta instantáneas con `seq` anterior o igual a la última (aritmética de 16 bits con vuelta).
    4. Pasa a `online_netstat` (T327) cada `seq` recibido para contar los que faltan.
    5. Valida `t_ms`: si salta más de 10 s hacia delante o hacia atrás, reinicia la estimación de ese jugador.
    6. Prueba con reloj falso: ráfaga de 5 instantáneas con la misma hora de recepción → posición interpolada continua; secuencia con vuelta de 65535 a 0; `t_ms` absurdo.
  hecho cuando: la prueba pasa y en dos instancias con `netem rafagas` el compañero no da saltos visibles (anótalo en bitácora).
  no hagas: no confíes en que los relojes de dos PC estén sincronizados; no uses la hora de pared; no cambies `INTERP_NS` en esta tarea.
  modelo sugerido: alto

- [ ] T317 [núcleo] Reconexión, lado servidor: reservar la ranura y reanudar
  files: runtime/online.c, tools/nettest/scenarios/resume_server.sh
  verify: `sh tools/nettest/run.sh resume_server` sale con 0
  depende de: T316
  quién: agente
  contexto: Cuando un cliente cae, `srv_drop` (`runtime/online.c:218-229`) libera su ranura y avisa LEAVE. Quien vuelve es un jugador nuevo en la primera ranura libre: no hay testigo de sesión.
  pasos:
    1. En WELCOME envía un testigo aleatorio de 16 bytes por jugador y el plazo de gracia (por defecto 600 s = 10 minutos, decisión del dueño en ADR-008, decisión 18; clave `grace` de `online.ini`).
    2. Al perder a un cliente autenticado (no al anfitrión): estado "ausente"; conserva nombre, último STATE y EQUIP, testigo y hora límite; envía AWAY a los demás.
    3. Un HELLO con un testigo válido y dentro de plazo recupera esa ranura: WELCOME con la misma ranura, luego JOIN/STATE/EQUIP de todos, como en una entrada normal.
    4. Vencido el plazo: LEAVE y ranura libre.
    5. Una ranura "ausente" no cuenta como libre para jugadores nuevos mientras quede otra.
    6. El testigo se compara en tiempo constante y se invalida al usarlo (se emite uno nuevo en el WELCOME).
    7. Escenario (el servidor de prueba arranca con `grace = 60` para no esperar 10 minutos): bot con `BOT_DROP=10,5` recupera su ranura; con `BOT_DROP=10,70` entra como jugador nuevo.
  hecho cuando: la prueba pasa y los demás bots reciben AWAY y después ningún LEAVE cuando la reanudación tiene éxito.
  no hagas: no reserves la ranura a una conexión que nunca se autenticó; no aceptes un testigo de otra sesión; no guardes testigos en disco; no ligues este plazo a la historia (el anfitrión puede continuar solo de inmediato, con confirmación: ADR-007, decisión 6).
  modelo sugerido: alto

- [ ] T318 [núcleo] Reconexión, lado cliente: reintentos automáticos
  files: runtime/online.c, runtime/online.h (solo el estado `ONLINE_RECONNECTING` y la función `online_reconnect_cancel`; pide permiso al dueño del protocolo), tools/nettest/scenarios/resume_client.sh
  verify: `sh tools/nettest/run.sh resume_client` sale con 0 (corte de 10 s con `netem corte`: el cliente real o el arnés de F2 vuelve a `CONNECTED` con la misma ranura)
  depende de: T317
  quién: agente
  contexto: Tras 10 s de silencio o al cerrarse el socket, `cl_thread` pone `ONLINE_FAILED` y termina (`runtime/online.c:1198-1200`, `:1318`). El jugador tiene que abrir el menú y pulsar Unirse.
  pasos:
    1. Añade el estado `ONLINE_RECONNECTING`. Si la sesión estaba `CONNECTED` y se pierde sin que el usuario saliera, el hilo cliente pasa a ese estado en vez de terminar.
    2. Reintenta con esperas de 1, 2, 4, 8, 15, 15… s hasta el plazo de gracia recibido en WELCOME. Cada intento envía HELLO con el testigo.
    3. Al reanudar: vacía las colas pendientes de la conexión anterior (golpes, estados de enemigos, botín, recogidas) y conserva el resto.
    4. REJECT(reanudación inválida) o plazo vencido → `ONLINE_FAILED` con texto claro y limpieza completa (la de `stop_all`).
    5. `online_reconnect_cancel()` detiene los reintentos (lo llamará la UI).
    6. El anfitrión no reintenta contra sí mismo: si su cliente local pierde la conexión, se termina la sesión.
  hecho cuando: la prueba pasa; durante el corte `online_state()` devuelve `ONLINE_RECONNECTING`.
  no hagas: no reintentes sin espera; no reintentes tras un REJECT de versión o contraseña; no apliques mensajes recibidos antes del nuevo WELCOME.
  modelo sugerido: alto

- [ ] T319 [núcleo] Reconexión, lado juego: no destruir al compañero durante la gracia
  files: el módulo de marionetas de `online_actors` que dejó F2 (en el PR original: `runtime/online_actors.c`), runtime/online.c
  verify: recorrido manual de dos instancias descrito en `verificacion.md` §CU-05, con entrada en `specs/bitacora/`
  depende de: T318
  quién: agente + dueño (la prueba es en el juego)
  contexto: Un jugador sin STATE durante 3 s deja de dibujarse y sus actores van a su pool (`STALE_NS`, `runtime/online.c:47`, `:1402-1405`). Cuando llega LEAVE, o cuando este juego sale de la sesión, sus actores se **borran** (DOC:726-730; `drop(i, left)` en OA:423-439). Recrear piezas tiene una limitación conocida: una segunda copia de una pieza hecha para un jugador nunca se dibuja (OA:338-342).
  pasos:
    1. Expón desde la capa de red un indicador por ranura "ausente" (recibido con AWAY) y uno global "este juego está reconectando".
    2. En el bucle por fotograma: una ranura ausente se trata como "cargando" (no se dibuja; piezas al pool) pero **no** se llama a `drop(..., true)`.
    3. Mientras este juego está en `ONLINE_RECONNECTING`, no ejecutes el borrado total de "fuera de sesión" (OA:5733-5746).
    4. Al llegar LEAVE o al fallar la reconexión, sí se borra, como hoy.
    5. Prueba en juego: corta la red del invitado 10 s; anota qué se ve en cada pantalla y que no hay crash.
  hecho cuando: tras el corte el compañero reaparece con su armadura y equipo en menos de 5 s, sin piezas duplicadas ni invisibles.
  no hagas: no guardes punteros del juego más allá de lo que ya hace el código; no crees actores mientras el Link local está muerto (cuelgue documentado, DOC:732-734); no llames al juego desde el hilo de red.
  modelo sugerido: alto

## Bloque 4 — Módulos puros (paralelo; cada uno en ficheros nuevos)

- [x] T320 [P] [módulo] Cubo de fichas con reloj inyectado
  reclamada: agente Sonnet modulos-f3 (integrada `1bea3c8`, adelantada) — verify 0 en Windows y Linux (CI Coop tests)
  files: runtime/online_ratelimit.c, runtime/online_ratelimit.h, tests/test_online_ratelimit.c
  verify: `clang -Wall -Iruntime tests/test_online_ratelimit.c runtime/online_ratelimit.c -o build/test_online_ratelimit && build/test_online_ratelimit` sale con 0
  depende de: ninguna
  quién: agente
  contexto: El servidor reenvía cualquier cantidad de mensajes sin contar (`runtime/online.c:262-311`). T313 necesita un limitador sencillo y probado.
  pasos:
    1. `typedef struct { double tokens, rate, burst; uint64_t last_ns; } RateBucket;`
    2. `void rate_init(RateBucket *b, double por_segundo, double rafaga);`
    3. `bool rate_take(RateBucket *b, uint64_t ahora_ns);` rellena según el tiempo pasado, con tope `burst`, y devuelve si había una ficha.
    4. Pruebas: ritmo exacto, ráfaga, reloj que no avanza, reloj que retrocede (no debe dar fichas negativas ni infinitas), periodo largo sin uso.
  hecho cuando: la prueba pasa y el fichero no incluye ninguna cabecera de sockets ni de `rt.h`.
  no hagas: no leas el reloj dentro del módulo; no uses variables globales; no toques `online.c`.
  modelo sugerido: bajo

- [~] T321 [P] [módulo] Autenticación: hash con sal, HMAC y comparación en tiempo constante
  reclamada: agente Opus auth-t321 (coop/T321, en curso; adelantada)
  files: runtime/online_auth.c, runtime/online_auth.h, tests/test_online_auth.c
  verify: `clang -Wall -Iruntime tests/test_online_auth.c runtime/online_auth.c runtime/update_crypto.c -o build/test_online_auth && build/test_online_auth` sale con 0 (incluye los vectores de prueba públicos de HMAC-SHA256 del RFC 4231)
  depende de: ninguna
  quién: agente
  contexto: La contraseña se compara con `strcmp` y se guarda en claro (`runtime/online.c:241`, `:1549-1551`). El runtime ya trae SHA-256 sin dependencias en `runtime/update_crypto.c` (lo usa el actualizador).
  pasos:
    1. Lee `runtime/update_crypto.h` y usa su SHA-256 tal cual.
    2. Implementa HMAC-SHA256 encima.
    3. `online_auth_key(sal[16], contraseña, clave[32])` = SHA-256(sal || contraseña).
    4. `online_auth_response(clave, desafío[16], nombre, salida[32])` = HMAC(clave, desafío || nombre).
    5. `online_auth_equal(a, b, n)` en tiempo constante (OR acumulado de XOR).
    6. `online_auth_random(buf, n)`: `BCryptGenRandom` en Windows; `getrandom` o `/dev/urandom` en Linux y macOS. Devuelve false si falla. Antes de escribirla, busca con `grep -rn "BCryptGenRandom\|getrandom\|urandom" runtime/` si ya existe una y reutilízala.
    7. Pruebas: vectores del RFC, ida y vuelta, contraseña vacía, nombre de 23 caracteres.
  hecho cuando: la prueba pasa en Windows y en Linux.
  no hagas: no inventes un algoritmo propio; no uses `rand()`/`srand()`; si `online_auth_random` falla, no sigas con ceros: devuelve error.
  modelo sugerido: alto

- [x] T322 [P] [módulo] Código de invitación (dirección + puerto ↔ texto)
  reclamada: agente Sonnet modulos-f3 (integrada `1bea3c8`, adelantada) — verify 0 en Windows y Linux (CI Coop tests)
  files: runtime/online_invite.c, runtime/online_invite.h, tests/test_online_invite.c
  verify: `clang -Wall -Iruntime tests/test_online_invite.c runtime/online_invite.c -o build/test_online_invite && build/test_online_invite` sale con 0
  depende de: ninguna
  quién: agente
  contexto: Para unirse hay que teclear IP y puerto (`runtime/ui/online_ui.mm`, sección "Join"). Un código corto que se copia y pega evita errores.
  pasos:
    1. Formato binario: 1 byte (versión 1 en los 4 bits altos; familia 4 o 6 en los bajos), dirección (4 o 16 bytes), puerto (2 bytes, big-endian), suma de control (2 bytes: CRC-16 de lo anterior).
    2. Texto: base32 con el alfabeto `0123456789ABCDEFGHJKMNPQRSTVWXYZ` (sin I, L, O, U), en grupos de 4 separados por guion.
    3. Al leer: ignora guiones y espacios, acepta minúsculas, trata `O` como `0` e `I`/`L` como `1`.
    4. `bool online_invite_encode(familia, dir, puerto, out, cap)` y `bool online_invite_decode(texto, &familia, dir, &puerto)`.
    5. Pruebas: ida y vuelta IPv4 e IPv6, un carácter cambiado → false, texto vacío, texto de 1000 caracteres, puerto 0 → false.
  hecho cuando: la prueba pasa; un código IPv4 mide 15 caracteres o menos sin contar guiones.
  no hagas: no metas la contraseña ni el nombre en el código; no reserves memoria dinámica; no leas más allá de `cap`.
  modelo sugerido: bajo

- [ ] T323 [P] [módulo] Descubrimiento de partidas en la red local
  files: runtime/online_discovery.c, runtime/online_discovery.h, tests/test_online_discovery.c, tools/nettest/scenarios/lan.sh
  verify: `clang -Wall -Iruntime tests/test_online_discovery.c runtime/online_discovery.c -o build/test_online_discovery && build/test_online_discovery` sale con 0 **y** `sh tools/nettest/run.sh lan` sale con 0
  depende de: T301, T308
  quién: agente
  contexto: No hay ningún código UDP en `runtime/online.c`: el invitado tiene que conocer la IP. `runtime/dsu.c` es el modelo de socket UDP no bloqueante del proyecto (`dsu_open` en `:183-213`, recepción acotada en `:232-254`), con funciones de paquete separadas del socket.
  pasos:
    1. Paquetes de tamaño fijo. Consulta: magia `SWLQ`, protocolo, 8 bytes aleatorios. Respuesta: magia `SWLR`, los mismos 8 bytes, protocolo, puerto TCP, jugadores, máximo, pide contraseña (0/1), resumen de versión (16 bytes), nombre del anfitrión (24 bytes terminados en 0).
    2. Funciones puras: `discovery_build_query`, `discovery_parse_query`, `discovery_build_reply`, `discovery_parse_reply`; cada `parse` exige longitud exacta y termina las cadenas.
    3. Lado anfitrión: `discovery_host_open(puerto)`, `discovery_host_poll(info)` (no bloqueante; como mucho 16 datagramas por llamada y 4 respuestas por segundo por IP).
    4. Lado cliente: `discovery_scan_start()` lanza un hilo propio que cada 500 ms, durante 5 s, envía la consulta a `255.255.255.255`, a la difusión de cada interfaz y a la multidifusión IPv6 `ff02::1`; `discovery_scan_results(lista, max)` copia hasta 16 resultados bajo mutex; `discovery_scan_stop()`.
    5. El cliente descarta respuestas cuyo valor aleatorio no coincide con el de su consulta.
    6. Escenario `lan`: cliente y servidor en la misma red de Docker → 1 resultado en < 2 s; en redes distintas → 0.
  hecho cuando: las dos pruebas pasan en Linux y `make runtime-objs` compila en Windows.
  no hagas: no emitas difusión periódica desde el anfitrión; no te unas automáticamente a nada; no llames a estas funciones desde el hilo del juego; no confíes en el nombre recibido (puede traer cualquier byte: sustituye los menores de 32).
  modelo sugerido: alto

- [ ] T324 [P] [módulo] Apertura automática de puerto (UPnP y NAT-PMP)
  files: runtime/online_upnp.c, runtime/online_upnp.h, tests/test_online_upnp.c, tools/nettest/scenarios/upnp.sh, docs/third-party.md (solo la entrada de miniupnpc), y la fuente de miniupnpc donde diga el ADR de T306
  verify: `sh tools/nettest/run.sh upnp` sale con 0 (abre, consulta la IP externa y cierra contra `miniupnpd`; y sin IGD devuelve "no disponible" en < 5 s)
  depende de: T306 (ADR con la forma de incluir miniupnpc)
  quién: agente
  contexto: No existe. El anfitrión debe abrir el puerto en el router a mano (`runtime/ui/online_ui.mm:57-59`). Se usa miniupnpc, aceptada por el dueño (ADR-007, decisión 11); el ADR de T306 dice cómo se incluye.
  pasos:
    1. API: `online_upnp_start(puerto)` lanza un hilo; `online_upnp_status(&estado, ip_externa, cap, motivo, cap2)` devuelve BUSCANDO / ABIERTO / NO_DISPONIBLE; `online_upnp_stop()` pide borrar la redirección con plazo de 2 s y no espera más.
    2. Secuencia con plazo total de 5 s: SSDP → `AddPortMapping` TCP con duración de 2 h (renovar cada hora) → `GetExternalIPAddress`; si falla, NAT-PMP contra la puerta de enlace.
    3. Clasifica la IP externa: si es privada (10/8, 172.16/12, 192.168/16) o de 100.64/10, estado ABIERTO con aviso "dirección compartida por el proveedor".
    4. Toda respuesta del router: tamaño máximo 16 KiB, plazo por operación, análisis de texto con longitudes comprobadas.
    5. Pruebas unitarias del análisis de respuestas con textos sintéticos (correcto, truncado, enorme, sin la etiqueta esperada).
    6. Añade la entrada de miniupnpc a `docs/third-party.md` (nombre, versión, licencia, origen), con el texto preparado en T306.
  hecho cuando: la prueba pasa, `docs/third-party.md` tiene la entrada y el dueño confirma en bitácora que abre el puerto en su router (T341).
  no hagas: no llames a nada de esto desde el hilo del juego ni el de presentación; no sigas URL de control que apunten fuera de la red local; no dejes la redirección puesta al salir; no añadas ninguna otra biblioteca; no cambies `RT_SRC` ni `LDFLAGS` del `Makefile` (pásale al integrador, T337, lo que haga falta enlazar).
  modelo sugerido: alto

- [x] T325 [P] [módulo] Reunir la información de versión de este juego
  hecha: agente Sonnet ag-t325 (integrada) — `online_version` + prueba (OK) en CI; `OnlineBuildInfo` provisional hasta T308; T314 llama a `online_build_init`. ADR-027 n.º 5-6: solo mismo sistema, DLC distinto se rechaza
  files: runtime/online_version.c, runtime/online_version.h, tests/test_online_version.c
  verify: `clang -Wall -Iruntime tests/test_online_version.c runtime/online_version.c -o build/test_online_version && build/test_online_version` sale con 0
  depende de: T307, T308
  quién: agente
  contexto: Los datos existen pero nadie los junta: versión del juego en `g_game_version` (`runtime/gamelib.c:76`), identificador de compilación (`runtime/gamelib.c:43-59`), sello del overlay `online_mod/v42` (`runtime/main.c:507`), estado de DLC (`rt_dlc_check`, `runtime/dlc_check.h:38`).
  pasos:
    1. `void online_build_info(OnlineBuildInfo *out)` rellena la estructura definida en T308 con esas cuatro fuentes más la constante de versión del modo cooperativo.
    2. `int online_build_compare(const OnlineBuildInfo *mio, const OnlineBuildInfo *suyo)` devuelve 0 o el motivo de rechazo del primer campo distinto, en el orden de T314.
    3. `void online_build_digest(const OnlineBuildInfo *, uint8_t out[16])` para el paquete de LAN.
    4. La prueba usa estructuras sintéticas (no necesita el juego): iguales, y una diferencia por campo.
  hecho cuando: la prueba pasa; `online_build_info` no bloquea ni lee disco más de una vez (guarda el resultado).
  no hagas: no leas ficheros del juego en cada llamada; no compares cadenas sin terminador; no decidas tú qué identificador comparar: usa la respuesta de T307.
  modelo sugerido: medio

- [~] T326 [P] [módulo] Direcciones: analizador común e IPv6 en la lista local
  reclamada: agente (integrada `62f04d9`) — online_addr con IPv6 y prueba en CI; FALTA la parte de la lista local en el juego
  files: runtime/online_addr.c, runtime/online_addr.h, tests/test_online_addr.c
  verify: `clang -Wall -Iruntime tests/test_online_addr.c runtime/online_addr.c runtime/online_invite.c -o build/test_online_addr && build/test_online_addr` sale con 0
  depende de: T322
  quién: agente
  contexto: `online_local_addresses` solo lista IPv4 (`runtime/online.c:1462-1480`, usa `AF_INET`). El campo "Dirección" se pasa tal cual a `getaddrinfo`; no admite `[IPv6]:puerto` ni códigos.
  pasos:
    1. `bool online_addr_parse(texto, host, cap, &puerto, puerto_por_defecto)` acepta: `nombre`, `nombre:puerto`, `1.2.3.4`, `1.2.3.4:puerto`, `[v6]`, `[v6]:puerto`, `v6` sin corchetes (sin puerto), y un código de invitación.
    2. `int online_addr_local(lista, max)` devuelve las direcciones de este PC con familia y tipo (IPv4 privada, IPv4 pública, IPv6 global, IPv6 de enlace local), usando `getifaddrs` en POSIX y `GetAdaptersAddresses` en Windows.
    3. Pruebas del analizador: los 8 formatos, puerto fuera de rango, texto de 500 caracteres, corchete sin cerrar.
  hecho cuando: la prueba pasa en Windows y Linux.
  no hagas: no resuelvas nombres (nada de `getaddrinfo`) en este módulo; no devuelvas direcciones de bucle local; no toques `online.c` (lo integrará el carril UI/núcleo).
  modelo sugerido: medio

- [x] T327 [P] [módulo] Estadísticas de calidad de red
  reclamada: agente Sonnet modulos-f3 (integrada `1bea3c8`, adelantada) — verify 0 en Windows y Linux (CI Coop tests)
  files: runtime/online_netstat.c, runtime/online_netstat.h, tests/test_online_netstat.c
  verify: `clang -Wall -Iruntime tests/test_online_netstat.c runtime/online_netstat.c -lm -o build/test_online_netstat && build/test_online_netstat` sale con 0
  depende de: ninguna
  quién: agente
  contexto: Solo existe la latencia propia medida con PING/PONG cada segundo (`runtime/online.c:1292-1295`, `:1114-1121`). No hay medida de pérdida ni de irregularidad.
  pasos:
    1. `NetStat` por jugador con ventana de 5 s.
    2. `netstat_ping(ns, ms)`, `netstat_state(ns, seq, ahora_ns)`.
    3. `netstat_get(ns, ahora_ns, &ping_ms, &ping_p95, &jitter_ms, &perdida_pct, &nivel)`.
    4. Pérdida efectiva = secuencias que faltan / secuencias esperadas en la ventana (16 bits con vuelta).
    5. Nivel: bueno (ping < 100 y pérdida < 2 %), regular, malo (ping > 250 o pérdida > 10 %), con histéresis de 2 s para no parpadear.
    6. Pruebas con reloj falso: sin datos, flujo perfecto, 10 % de huecos, vuelta de secuencia, 6 s sin datos (nivel malo).
  hecho cuando: la prueba pasa y el módulo no usa reloj propio ni sockets.
  no hagas: no reserves memoria dinámica; no dividas por cero cuando la ventana está vacía.
  modelo sugerido: bajo

## Bloque 5 — Interfaz (en serie, dueño único de `runtime/ui/online_ui.mm`)

- [ ] T328 [ui] Encontrar la partida: lista de red local, código de invitación y estado del puerto
  files: runtime/ui/online_ui.mm, runtime/online.c (solo llamadas a los módulos nuevos al crear y terminar sesión), specs/fases/F3-red-robusta/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos nuevos **y** `tools/venv/Scripts/python.exe tools/i18n_check.py --missing es` lista solo los textos entregados en `specs/fases/F3-red-robusta/textos-nuevos.txt`; más la captura de ventana descrita en `verificacion.md` §CU-04
  depende de: T322, T323, T324, T326, T310
  quién: agente
  contexto: La pestaña Online (`online_tab_draw`, `runtime/ui/online_ui.mm:38-130`) tiene "Create session" (puerto, contraseña, lista de IPv4) y "Join" (dirección, botón). El texto de ayuda dice que se redirija el puerto o se use una VPN (`:57-59`). No hay búsqueda, ni código, ni apertura de puerto.
  pasos:
    1. En "Join": botón "Search local network" que llama a `discovery_scan_start()`; debajo, la lista de `discovery_scan_results` (nombre, jugadores, candado si pide contraseña, aviso si la versión no coincide). Elegir una fila rellena la dirección.
    2. El campo de dirección acepta código, IP o nombre (usa `online_addr_parse`). Si el texto no se entiende, muestra "That is not an address or an invite code".
    3. En "Create session", con la sesión abierta: muestra el código de invitación (`online_invite_encode`) con botón "Copy", y el estado de `online_upnp_status` con los textos de `plan.md` §H.
    3b. Sustituye el texto de ayuda actual (`:57-59`) por uno que no recomiende ninguna VPN ni red virtual: el juego no ofrece nada cuando la conexión directa es imposible (ADR-009, decisión 23).
    4. Texto fijo sobre el cortafuegos de Windows bajo el botón de crear.
    5. Interruptor "Visible on the local network".
    6. Usa los widgets existentes `opt_*` de `runtime/ui/option_rows.h` (traducen solos) y el teclado en pantalla que ya trae `opt_text`.
    7. Añade cada texto nuevo en inglés a `textos-nuevos.txt`, una línea por texto.
  hecho cuando: con un bot anfitrión en la red local, la lista lo muestra y unirse funciona con mando y con teclado.
  no hagas: no llames a `getaddrinfo`, UPnP ni escaneo desde `online_tab_draw` (corre en el hilo de presentación): solo lee estados ya publicados; no muestres la contraseña; no edites `runtime/ui/menu.mm` ni los `.inc`; no nombres Tailscale, ZeroTier ni ningún servicio externo en ningún texto.
  modelo sugerido: medio

- [ ] T329 [ui] Reconexión y motivos de rechazo en pantalla
  files: runtime/ui/online_ui.mm, specs/fases/F3-red-robusta/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos nuevos; captura de ventana con `SAVAGEWIND_WINDOW_SHOT` durante un corte de red mostrando "Reconnecting…" (procedimiento en `verificacion.md` §CU-05)
  depende de: T318, T314, T328
  quién: agente
  contexto: Los errores son una sola línea de estado (`runtime/ui/online_ui.mm:25-31`) y tras un fallo hay que volver a pulsar Join. T318 añade el estado `ONLINE_RECONNECTING` y T314 motivos de rechazo con detalle.
  pasos:
    1. En el overlay (`online_overlay_draw`): mientras `online_state() == ONLINE_RECONNECTING`, una línea "Reconnecting… (attempt N)" con un icono que no dependa del color.
    2. En la pestaña: botón "Cancel" que llama a `online_reconnect_cancel()`, y tras `ONLINE_FAILED` un botón "Reconnect" que repite el último `online_join`.
    3. Junto a un jugador "ausente" en la lista: texto "reconnecting".
    4. Muestra el motivo de rechazo completo (qué difiere, tu valor y el del anfitrión). El formato es un literal fijo; los valores son argumentos.
    4b. Cuando un intento de unirse falla sin llegar al saludo (no hay respuesta, conexión rechazada): un mensaje de error claro que diga qué se intentó (dirección o código, puerto) y que no se pudo conectar. Nada más: ni guía de red virtual ni servicio de presentación (ADR-009, decisión 23).
    5. Añade los textos a `textos-nuevos.txt`.
  hecho cuando: cada motivo de rechazo de T314 se lee entero en la pestaña, el aviso de reconexión aparece y desaparece solo, y unirse a una dirección que no responde muestra el error claro del paso 4b.
  no hagas: no uses cadenas de la red como formato de `printf`/`ImGui::Text` (usa `TextUnformatted` o `"%s"`); no pintes texto de red sin limitar su longitud.
  modelo sugerido: medio

- [ ] T330 [ui] Indicador de calidad de red por jugador
  files: runtime/ui/online_ui.mm, specs/fases/F3-red-robusta/textos-nuevos.txt
  verify: `make runtime-objs` sin avisos nuevos; captura de ventana con dos instancias y `netem wifi` donde se lee latencia y pérdida (procedimiento en `verificacion.md` §Calidad de red)
  depende de: T316, T327, T329
  quién: agente
  contexto: La lista de jugadores del overlay muestra un círculo de color y, en la pestaña, la latencia o "loading"/"paused" (`runtime/ui/online_ui.mm:181-304`). El color es la única señal.
  pasos:
    1. Junto a cada jugador en el overlay: de 1 a 3 barras según el nivel de `netstat_get`; con nivel "malo", además un triángulo.
    2. En la pestaña: "NNN ms · P % lost".
    3. Interruptor "Show connection quality" (por defecto: solo cuando el nivel no es bueno).
    4. El dibujo va dentro de `online_overlay_draw`, que ya se llama cada fotograma (`runtime/ui/menu.mm:2594`); no hace falta tocar `menu.mm`.
    5. Textos a `textos-nuevos.txt`.
  hecho cuando: con `netem wifi` se ven 2 barras o menos y con `lan` 3 barras.
  no hagas: no distingas niveles solo por color; no tomes `gpu_lock` ni llames al renderer; no hagas cálculos caros por fotograma (lee el resultado ya calculado).
  modelo sugerido: bajo

- [ ] T331 [ui] `online.ini`: claves nuevas y contraseña sin texto claro
  files: runtime/online.c (solo `online_settings_load`, `online_settings_save` y `OnlineSettings`), runtime/online.h (solo `OnlineSettings`; avisa al dueño del protocolo), tests/test_online_settings.c
  verify: `clang -Wall -Iruntime tests/test_online_settings.c -o build/test_online_settings && build/test_online_settings` sale con 0 (ida y vuelta de un `online.ini` temporal, migración de uno antiguo con `password = abc`)
  depende de: T315, T330
  quién: agente
  contexto: `online_settings_load`/`save` (`runtime/online.c:1506-1554`) leen y escriben `name, host, port, password, show_names, show_markers, pvp, collide, revive, nopause` en `online.ini` del directorio de trabajo. La contraseña se guarda tal cual.
  pasos:
    1. Claves nuevas: `password_salt`, `password_hash` (hexadecimal), `lan_visible`, `upnp`, `grace`, `show_quality`, `remember_password`.
    2. Migración: si al cargar existe `password = <texto>`, calcula sal y hash, y al guardar escribe solo las claves nuevas.
    3. El invitado guarda la contraseña de unión solo si `remember_password = 1`.
    4. Valida rangos al cargar (`grace` 10..600, por defecto 600 = 10 minutos, ADR-008 decisión 18; booleanos 0/1; hexadecimal de longitud exacta).
    5. Escritura atómica: escribe a `online.ini.tmp` y renombra.
  hecho cuando: la prueba pasa y un `online.ini` antiguo sigue funcionando.
  no hagas: no muevas estos ajustes a `runtime/settings.c` (fichero caliente de otros carriles); no escribas nada si el nombre viene de `SAVAGEWIND_ONLINE_NAME` (comportamiento actual, `online.c:1544-1545`); no superes 127 caracteres por valor (límite de lectura).
  modelo sugerido: medio

## Bloque 6 — Idiomas (dueño único de `runtime/ui/i18n/*.inc`)

- [ ] T332 [P] [i18n] Traducir a 12 idiomas los 63 textos de Online que ya existen
  files: runtime/ui/i18n/de.inc, fr.inc, he.inc, it.inc, ja.inc, ko.inc, nl.inc, pt.inc, ru.inc, th.inc, zh-Hans.inc, zh-Hant.inc
  verify: `tools/venv/Scripts/python.exe tools/i18n_check.py` imprime `0 missing` para los 13 idiomas y sale con 0
  depende de: F0
  quién: agente
  contexto: El PR #294 añadió 63 entradas de Online solo a `runtime/ui/i18n/es.inc`. En los otros 12 ficheros hay 0 (comprobado: `grep -c "Create session" runtime/ui/i18n/*.inc` da 1 solo en `es.inc`). La clave de cada entrada es el texto en inglés: `T("English", "traducción")`.
  pasos:
    1. Por cada idioma, el orquestador ejecuta `tools/venv/Scripts/python.exe tools/i18n_check.py --missing <idioma>` y te pasa las líneas que faltan.
    2. Añade esas líneas al final del fichero del idioma, en un bloque con el comentario `// Online (co-op)`.
    3. Traduce conservando los `%s`/`%d` en el mismo orden y los nombres propios sin traducir. El texto de ayuda que nombra Tailscale, ZeroTier y Radmin VPN lo retira T328 (ADR-009, decisión 23): tradúcelo solo si sigue en el código cuando hagas esta tarea.
    4. Usa la terminología del propio fichero para palabras ya traducidas (busca cómo tradujo "Settings", "Port", "Password").
    5. Deja listo el cambio; el orquestador ejecuta el comprobador (`i18n_check.py`).
  hecho cuando: `0 missing` en los 13 y `make runtime-objs` compila (ambas cosas las comprueba el orquestador).
  no hagas: no cambies la cadena inglesa (es la clave); no reordenes ni reformatees líneas existentes; no dejes `T("...", "")` en frases enteras; no toques `es.inc` salvo que falte algo.
  modelo sugerido: bajo

- [ ] T333 [i18n] Traducir los textos nuevos de F3 a los 13 idiomas
  files: runtime/ui/i18n/*.inc, tools/i18n_check.py (solo si hay que añadir un fichero a `SOURCES`)
  verify: `tools/venv/Scripts/python.exe tools/i18n_check.py` → `0 missing` en los 13 y sale con 0
  depende de: T328, T329, T330, T332
  quién: agente
  contexto: Los carriles de UI dejaron sus textos en inglés en `specs/fases/F3-red-robusta/textos-nuevos.txt`. El comprobador solo escanea `runtime/ui/*.mm` (menos `console.mm`), `runtime/language.c`, `runtime/hle/bindings.c` y `runtime/launch_flags.c`: los textos de estado que nacen en `runtime/online.c` no los ve.
  pasos:
    1. Compara con `textos-nuevos.txt` la lista que dé `--missing es` (la ejecuta el orquestador y te la pasa). Si un texto de estado de `online.c` no aparece, comprueba que el literal figure también en `online_ui.mm` dentro de `tr("...")`; si no, añade ahí una tabla de textos de estado.
    2. Añade las líneas a los 13 `.inc` en el mismo bloque `// Online (co-op)`.
    3. Hebreo: el dueño o el orquestador comprueban en pantalla que el código de invitación y las direcciones IP se leen de izquierda a derecha.
  hecho cuando: `0 missing` y una captura de la pestaña Online en español, japonés y hebreo en bitácora.
  no hagas: no añadas `runtime/online.c` a `SOURCES` sin comprobar que no genera falsos positivos; no traduzcas el código de invitación.
  modelo sugerido: bajo

## Bloque 7 — Integración, pruebas y cierre

- [ ] T334 [pruebas] Suite de integración en Docker
  files: tools/nettest/scenarios/all.sh, tools/nettest/README.md
  verify: `sh tools/nettest/run.sh all` sale con 0 y termina en menos de 15 minutos
  depende de: T309-T319, T323, T324
  quién: agente
  contexto: Cada tarea del núcleo añadió un escenario suelto. Hace falta una orden única que los ejecute todos y sirva de regresión.
  pasos:
    1. Cada tarea ya dejó su `scenarios/<nombre>.sh`; comprueba que existen todos y crea `all.sh`. Escenarios: `smoke, bot_modes, h1_noread, leave_fast, host_slot, spoof, flood, preauth, version_matrix, badpass, resume_server, resume_client, lan, upnp`.
    2. `run.sh all` los lanza en orden, imprime una tabla `escenario | OK/FALLO | segundos` y devuelve 1 si alguno falla.
    3. Cada escenario limpia sus contenedores y su `netem` aunque falle (`trap`).
    4. Documenta en el README cómo depurar uno (dónde quedan los registros).
  hecho cuando: `all` pasa tres veces seguidas.
  no hagas: no dependas del orden entre escenarios; no uses esperas fijas largas donde puedas esperar a una línea de registro.
  modelo sugerido: medio

- [ ] T335 [P] [pruebas] Fuzzing de los manejadores de mensajes
  files: tests/fuzz_online_srv.c, tests/fuzz_online_cl.c, tools/nettest/fuzz.sh
  verify: `sh tools/nettest/fuzz.sh 120` sale con 0 (120 s por objetivo con libFuzzer y AddressSanitizer, sin fallos)
  depende de: T312, F2 (manejadores invocables sin sockets)
  quién: agente
  contexto: `srv_message` y `cl_message` de `runtime/online.c` analizan bytes de la red. El mapa 06 no encontró corrupción de memoria leyendo, pero no hay ninguna prueba automática que lo sostenga tras los cambios de F3.
  pasos:
    1. Objetivo 1: un `Server` en memoria con dos clientes simulados; la entrada del fuzzer se trocea en mensajes para `srv_message`.
    2. Objetivo 2: lo mismo con `cl_message`, en modo anfitrión y no anfitrión.
    3. Deja escrita `fuzz.sh`, que compila con `clang -fsanitize=fuzzer,address,undefined` dentro del contenedor Linux; **la compila y la ejecuta el orquestador**, no el subagente.
    4. Siembra el corpus con un mensaje válido de cada tipo generado por código (no con capturas reales).
  hecho cuando: 120 s por objetivo sin hallazgos; cualquier hallazgo se arregla o se abre como tarea.
  no hagas: no guardes en el repo capturas de sesiones reales; no desactives un sanitizador para "que pase".
  modelo sugerido: medio

- [ ] T336 [ci] Flujo de CI para las pruebas de red
  files: .github/workflows/coop-net.yml
  verify: `grep -c 'run.sh all' .github/workflows/coop-net.yml` da 1 y `git diff --check` sin errores; el orquestador lo comprueba en verde en el push de cierre de F3 (ADR-006: no hay push antes) o con `act` en local
  depende de: T334, T337
  quién: agente escribe el flujo; orquestador lo verifica (el dueño habilita Actions en el fork)
  contexto: El PR #294 borró los flujos de CI por un efecto de sus fusiones y F0 los restauró. Ninguno ejecuta pruebas de red.
  pasos:
    1. Disparo: PR y push que toquen `runtime/online*`, `tools/online_bot.c`, `tools/nettest/**`, `tests/test_online_*`.
    2. Trabajo en `ubuntu-latest`: compilar, `make test-online`, `sh tools/nettest/run.sh all`.
    3. Permisos `contents: read`; sin secretos.
  hecho cuando: el flujo corre en menos de 20 minutos y falla si se rompe un escenario (lo comprueba el orquestador con `act` o, tras el push de cierre de F3, con una rama de prueba que lo rompa a propósito; no hay push antes).
  no hagas: no edites los flujos existentes; no uses `pull_request_target`; no subas artefactos con ficheros del juego.
  modelo sugerido: bajo

- [ ] T337 [integración] Objetivos `make test-online*` y tabla de variables de entorno
  files: Makefile (un único bloque al final), runtime/launch_flags_table.c (regenerado, no editado a mano)
  verify: `make test-online` sale con 0 **y** `make test-launch-flags` sale con 0
  depende de: T309, T312, T316, T320-T327, T331
  quién: agente integrador (único que toca `Makefile` en F3)
  contexto: Cada módulo trae su prueba con la orden de compilación en la cabecera. El `Makefile` no tiene ningún objetivo de Online salvo `online-helper` (`Makefile:481-486`). Una variable `SAVAGEWIND_*` nueva deja desfasada la tabla generada `runtime/launch_flags_table.c` y rompe `make test-launch-flags`.
  pasos:
    1. Añade al final del `Makefile` un bloque `# co-op network tests` con una regla por binario `$(B)/test_online_*` y el objetivo `test-online` que los ejecuta todos; añádelo a `.PHONY`.
    2. Copia el estilo de `test-control` o `test-update-core` del propio `Makefile`.
    3. Pide al orquestador que regenere la tabla (`tools/venv/Scripts/python.exe tools/gen_launch_flags.py`); el subagente no ejecuta el generador.
  hecho cuando: los dos `verify` pasan en Windows (MSYS2) y en Linux (Docker).
  no hagas: no cambies `RT_SRC`, `LDFLAGS` ni nada de `GAMELIB_SRCS`; no edites a mano la tabla generada; no intercales reglas en mitad del fichero.
  modelo sugerido: bajo

- [ ] T338 [P] [docs] Guía de conexión para jugadores
  files: docs/coop-conexion.md
  verify: `git diff --check` sin errores y cada nombre de botón citado existe (`grep` de cada texto en `runtime/ui/online_ui.mm`)
  depende de: T328
  quién: agente
  contexto: `docs/ONLINE.md:8-17` explica crear y unirse en seis líneas. Falta la escalera completa para alguien sin conocimientos de redes.
  pasos:
    1. Escribe, en español llano, en este orden: misma casa (buscar en red local) → apertura automática y código de invitación → IPv6 → abrir el puerto a mano, con qué dirección usar en cada caso. La escalera termina ahí: la guía no recomienda redes virtuales (Tailscale, ZeroTier) ni servicios de presentación (ADR-009, decisión 23).
    2. Sección "Si no funciona": aviso del cortafuegos de Windows, dirección compartida por el proveedor (sin IPv6 en los dos lados, con esa conexión no se puede hospedar: dilo tal cual), versiones distintas (qué significa cada motivo de rechazo) y qué quiere decir el mensaje de error de conexión.
    3. Sección "Seguridad": el tráfico no está cifrado; el código revela tu IP; la contraseña es opcional pero recomendada si abres el puerto a Internet (ADR-007, decisión 12).
  hecho cuando: el dueño sigue la guía sin ayuda y conecta (anotado en bitácora).
  no hagas: no expliques cómo obtener el juego; no incluyas IP reales, nombres de usuario ni rutas de tu máquina; no prometas cifrado; no nombres productos de red virtual ni des pasos para instalarlos.
  modelo sugerido: bajo

- [ ] T339 [docs] Dejar claro qué hace y qué no hace `--server`
  files: runtime/main.c (solo el mensaje de arranque de `--server`), docs/coop-conexion.md
  verify: `build/savagewind.exe --server 52530` imprime al arrancar el aviso nuevo; `make runtime-objs` sin avisos
  depende de: T312, T338 (las dos escriben `docs/coop-conexion.md`). `--server` se construye con la propuesta de P3-4 (relé sin mundo); no espera al dueño
  quién: agente
  contexto: `savagewind.exe --server [puerto]` (`runtime/main.c:440-446`) arranca solo el relé. En ese modo ningún cliente es `g.hosting`, así que nadie envía enemigos, botín ni misiones, y TIME no sale porque `g_local_hour` vale -1 (`runtime/online.c:43`, `:373-379`). `docs/ONLINE.md:15` no lo advierte.
  pasos:
    1. Al arrancar `--server`, registra una línea: el servidor dedicado solo comparte posiciones, equipo y chat; el mundo compartido necesita que un jugador cree la sesión desde el juego.
    2. Con la autoridad de T312, comprueba que en `--server` no hay ranura de anfitrión y que ENEMIES/LOOTS/QUESTS se descartan.
    3. Documenta lo mismo en la guía.
  hecho cuando: el aviso aparece y un bot con `BOT_SPOOF=1` contra `--server` no consigue que otro bot reciba ENEMIES.
  no hagas: no intentes dar autoridad al servidor dedicado en F3; no cambies el orden de análisis de argumentos de `main.c`.
  modelo sugerido: bajo

- [ ] T340 [cierre] Recorrido de dos instancias en un PC y bitácora de F3
  files: specs/bitacora/F3-cierre.md
  verify: la bitácora contiene, para cada criterio S1-S16 de `spec.md`, OK o FALLO con evidencia (orden, salida o captura)
  depende de: todas las anteriores y **T341** (aunque lleve el número siguiente)
  quién: dueño (con un agente que prepara las órdenes)
  contexto: Un arreglo cuenta como hecho cuando se ha visto funcionar en el juego, no cuando compila (regla del plan aprobado). Dos juegos a la vez usan unos 9,5 GB de RAM: hay que cerrar Docker Desktop y el navegador.
  pasos:
    1. Sigue `verificacion.md` de esta carpeta de principio a fin.
    2. Repite el recorrido de F1 (CU-01 a CU-03, 30 minutos) con la versión nueva.
    3. Rellena la plantilla de bitácora.
  hecho cuando: todos los criterios están en OK, o cada FALLO tiene una tarea abierta. **F3 no se da por cerrada** sin la prueba de T341 con dos PC por Internet (ADR-011, dato de planificación): si no hay segundo PC, T340 deja esa prueba como pendiente explícito y F3 queda abierta; no se sustituye por Docker ni por pruebas simuladas.
  no hagas: no uses tu carpeta de guardado real (copia siempre); no marques OK sin evidencia.
  modelo sugerido: medio

- [ ] T341 [cierre] Prueba con router real, cortafuegos de Windows y dos PC
  files: specs/bitacora/F3-T341-red-real.md
  verify: la bitácora registra: aviso del cortafuegos visto y aceptado; resultado de la apertura automática en el router del dueño; unión por código desde otra red (móvil compartiendo datos o un amigo); unión por IPv6 si hay
  depende de: T328, T324, T338
  quién: dueño
  contexto: Docker no reproduce el cortafuegos de Windows ni un router doméstico. El aviso del cortafuegos aparece la primera vez que el programa escucha en un puerto; si se rechaza, nadie de fuera puede entrar y no hay ningún mensaje de error.
  pasos:
    1. En un perfil de Windows donde el programa no haya escuchado nunca: crea sesión y anota qué pregunta Windows.
    2. Anota el estado de apertura automática que muestra la pestaña y compáralo con la página del router.
    3. Únete desde fuera con el código. Si falla, sigue la guía de T338 y anota en qué peldaño funcionó.
  hecho cuando: hay al menos una unión con éxito desde fuera de la red local, o un diagnóstico claro de por qué no es posible con esa conexión.
  no hagas: no publiques tu IP ni el código en la bitácora (táchalos); no desactives el cortafuegos entero para probar.
  modelo sugerido: bajo
