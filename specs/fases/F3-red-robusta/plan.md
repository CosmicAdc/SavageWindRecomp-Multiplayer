# F3 — Red robusta: plan

Lee antes `spec.md` de esta carpeta. Referencias `fichero:línea` del checkout `pr294-online` (HEAD `350c5f8`).
Los valores numéricos de este plan (topes, plazos) son **propuestas iniciales**: los fija el experimento de T303.

## 0. Punto de partida que F3 da por hecho

| De | Qué | Por qué lo necesita F3 |
|---|---|---|
| F1 | `SIGPIPE` ignorado / `MSG_NOSIGNAL`; `stop_all` limpia todo el estado de sesión; contraseña oculta en pantalla; terminador de `shield` en ESTATE | La reconexión sería insegura con estado viejo (hallazgo T-1, mapa 06 §5.3) |
| F2 | Definición única de mensajes compartida con `tools/online_bot.c`; pruebas de trama y de máquina de estados sin el juego; validación única de entradas de red; `online.c` sin `static` de función que impidan dos clientes en un proceso | Las pruebas de F3 se apoyan en ese arnés |
| F0 | Imagen Docker de pruebas (Linux, clang, Python) y `docker compose` con el bot | F3 lo amplía con `tc netem` |

Si F2 partió `runtime/online.c` en varios ficheros, donde este plan dice `online.c` se entiende "el fichero que
contenga esa función".

## 1. Arquitectura actual en una página

- Relé TCP en estrella. Un hilo servidor (`srv_thread` → `srv_loop`, `runtime/online.c:343-401`) con `select()` de
  100 ms sobre el socket de escucha y hasta 8 clientes; reenvía con `send()` bloqueante.
- El anfitrión es un cliente más que se conecta a su propio servidor por `127.0.0.1` con `g.hosting = true`
  (`online.c:1347-1361`). El servidor no sabe cuál de sus clientes es el anfitrión.
- Hilo cliente (`cl_thread`, `online.c:1160-1320`): `select()` de 10 ms, aplica mensajes, envía STATE+SKEL a 20 Hz y
  PING a 1 Hz.
- Trama: `u16 longitud, u8 tipo, carga`; cargas = estructuras C empaquetadas (`online.c:59-108`).
- Tipos desconocidos se ignoran (`switch` sin `default`, `online.c:262-311`, `:970-1122`). Esto permite añadir
  mensajes sin romper a quien no los conoce, pero la igualdad exacta de `PROTOCOL` (`:240`) lo impide igualmente.

## 2. Diseño por elemento

### A. Colas de envío no bloqueantes (R3-01 a R3-05)

**Hoy.** `send_all` (`online.c:140-148`) hace `send()` en bucle sobre un socket bloqueante. `srv_broadcast`
(`:213-216`) lo llama por cada cliente y descarta el resultado. Si la ventana TCP de un cliente se llena, el hilo
servidor no vuelve: nadie recibe nada, la expulsión por silencio (`:384`) no puede actuar y `stop_all` (`:1322-1335`)
se cuelga en `pthread_join`, en el hilo de UI. `BOT_FREEZE` no lo reproduce porque el bot sigue leyendo
(mapa 06 §7.2).

**Diseño.**

1. Todos los sockets pasan a no bloqueantes (el patrón multiplataforma ya existe: `nonblocking` y `socket_again` de
   `runtime/control.c:24-39`, y el socket UDP no bloqueante de `runtime/dsu.c`).
2. Cada `SClient` gana una cola de salida con dos partes:
   - **Fiable**: anillo de bytes (propuesta: 256 KiB). JOIN, LEAVE, WELCOME, REJECT, CHAT, HIT, LOOT_*, QUEST*,
     PICK, TIME, PONG.
   - **Reemplazable**: una casilla por (tipo, ranura de origen) para STATE, SKEL y ENEMIES. Escribir en una casilla
     ocupada sustituye el contenido y suma 1 al contador de descartes de ese cliente.
3. El bucle del servidor añade el conjunto de escritura a `select()`: vacía primero la cola fiable y después las
   casillas, siempre mensajes enteros (un mensaje a medio enviar se termina antes de empezar otro).
4. Contrapresión: si la cola fiable supera el tope, o si pasan N s (propuesta: 5) sin que el socket acepte un byte
   teniendo datos pendientes, `srv_drop`. Es la misma salida que la expulsión por silencio de hoy.
5. `select() < 0` deja de hacer `continue` sin pausa (`online.c:356`): duerme 10 ms para no girar al 100 %.
6. Cliente: cola de salida equivalente (más pequeña). `stop_all` cierra el socket para despertar al hilo y limita la
   espera; si el hilo está dentro de `getaddrinfo` (no cancelable, `online.c:1134`), se le marca como huérfano y se
   deja terminar solo, sin `pthread_join` en el hilo de UI.

**Reutiliza.** Estructura de dos colas con mutex de `runtime/control.c:282-285` y `:543-559`; el patrón "API que
nunca bloquea + hilo que habla" de `runtime/discord_rpc.c:205-232`.

**No hacer.** No añadir un hilo por cliente: el diseño de un hilo con `select()` es correcto y más fácil de probar.

### B. Autoridad en el servidor (R3-06 a R3-09)

**Hoy.** `srv_message` reenvía ENEMIES (`online.c:278-280`), LOOTS, QUESTS y PICK (`:294-298`) de cualquier ranura
sin origen ("relé crudo"). QUEST se reenvía sin comprobar longitud (`:291-293`). El cliente anfitrión guarda el QUEST
de cualquier ranura `< ONLINE_MAX` (`:1065-1070`). El anfitrión se asume en la ranura 0 sin garantía
(OA:2838-2840; `online.c:361-362` da la primera libre).

**Diseño.**

1. `online_host` genera un secreto aleatorio de 16 bytes en memoria y lo pasa al cliente local (`start_client(true)`).
   El HELLO lo lleva. El servidor solo acepta ese secreto desde una conexión de bucle local (`127.0.0.1` / `::1`).
   Quien lo presenta ocupa la **ranura 0, reservada** (ningún otro la recibe nunca).
2. `Server` gana `host_slot`. Tabla de autoridad (una constante, probada con una prueba por fila):

   | Mensaje | Quién puede enviarlo | A quién lo reenvía el servidor |
   |---|---|---|
   | STATE, SKEL, EQUIP | Cualquiera | A los demás, con ranura |
   | CHAT | Cualquiera | A todos, con ranura |
   | ENEMIES, LOOTS, QUESTS | Solo anfitrión | A los demás |
   | TIME | Lo genera el servidor | A todos |
   | ESTATE, LOOT_ADD, LOOT_TAKE, QUEST | Solo no anfitrión | **Solo al anfitrión**, con ranura |
   | HIT | Cualquiera | A todos (el dueño del enemigo puede ser cualquier jugador: comentario de `online.c:281`) |
   | PICK | Cualquiera | A los demás |
   | PING | Cualquiera | Eco al emisor |

3. El servidor aplica la validación única de F2 antes de reenviar, y longitud exacta para QUEST/QUESTS (4096 bytes)
   y HIT (9 bytes).
4. En el cliente: se ignora un segundo WELCOME (`online.c:971-974` hoy lo acepta y cambia `g.id`); `origin` y
   `est_from` se acotan a `< ONLINE_MAX` (mapa 06, H-10).
5. Las reglas de sesión siguen viajando en el STATE de la ranura 0; con la reserva dejan de depender del orden de
   llegada.

**Límite conocido.** El cliente sigue confiando en el servidor al que se une (H-10). Con conexión directa el
servidor es el anfitrión, que por decisión del dueño manda sobre el mundo.

### C. Límites de tasa y de conexiones sin autenticar (R3-10, R3-11)

**Hoy.** `sv->c[i] = (SClient){.s = s, .used = true, …}` en el `accept` (`online.c:359-370`); `heard` se renueva con
cualquier byte (`:383-388`); el buffer de 32 KiB tarda en llenarse (`:394`). Ocho conexiones mudas bloquean la sesión.

**Diseño.**

1. Lista separada de conexiones **pendientes** (propuesta: 8 a la vez, 2 por IP, plazo de 3 s para completar el
   HELLO, buffer de 256 bytes). Solo un HELLO válido promueve a ranura de jugador.
2. Cubo de fichas por cliente y tipo. Tasas nominales conocidas hoy: STATE y SKEL 20 Hz, ENEMIES 10 Hz, PING 1 Hz,
   QUEST cada 2 s, EQUIP al cambiar y cada 5 s (mapa 06 §2.3). Propuesta: capacidad = 2 × nominal por segundo,
   ráfaga de 1 s. CHAT: 4 por 5 s.
3. Infracciones: descartar y contar; más de N en 10 s → expulsión.
4. El cubo es un módulo puro con reloj inyectado (`runtime/online_ratelimit.c`), probado sin sockets.

### D. Contraseña (R3-12, R3-13)

**Hoy.** `WireHello.pass[24]` en claro (`online.c:105`), `strcmp` (`:241`), `password = %s` en `online.ini`
(`:1549-1551`), y por línea de órdenes en `--server` (`runtime/main.c:440-445`).

**Diseño.**

1. Mensaje nuevo CHALLENGE (S→C): 16 bytes aleatorios por conexión. El HELLO responde con
   `HMAC-SHA256(clave, desafío || nombre)`, donde `clave = SHA-256(sal de sesión || contraseña)`.
2. `online.ini` del anfitrión guarda `password_salt` y `password_hash`, nunca el texto. El campo de la UI se vacía
   al guardar. El invitado no guarda la contraseña salvo que marque "recordar" (pregunta P3-2).
3. Comparación en tiempo constante. Tope de intentos por IP (propuesta: 5, después 60 s de espera que se duplica).
4. SHA-256 ya existe en el runtime sin dependencias: `runtime/update_crypto.c` (mapa 05 §3.4). HMAC se escribe
   encima (pocas líneas) en `runtime/online_auth.c`. Números aleatorios: fuente del sistema
   (`BCryptGenRandom` en Windows —`bcrypt` ya se usa en `SW Update`— y `getrandom`/`/dev/urandom` en Linux y macOS)
   **[sin verificar si el runtime ya tiene un envoltorio común: buscarlo antes de escribir uno]**.

**Lo que no arregla.** No hay cifrado: el resto del tráfico se puede leer e inyectar desde el camino de red. Se
documenta en la guía de conexión.

### E. Comprobación estricta de versión (R3-14, R3-15)

**Hoy.** Solo `magic` y `version` (`online.c:240`). Existen, sin usarse en red: `g_game_version`
(`runtime/gamelib.c:76`, `runtime/game_sites.h:113`), el identificador de la biblioteca del juego
(`runtime/gamelib.c:43-59`), la versión del overlay (`online_mod/v42`, `runtime/main.c:507`) y el comprobador de DLC
(`rt_dlc_check`, `runtime/dlc_check.h:38`).

**Diseño.** `runtime/online_version.c` reúne una estructura `OnlineBuildInfo`:

| Campo | Fuente | Motivo de rechazo propio |
|---|---|---|
| Protocolo | `PROTOCOL` | "Otra versión del modo cooperativo" |
| Versión del modo cooperativo (cadena corta del fork) | Constante nueva | Ídem, con las dos versiones en el texto |
| Compilación del código del juego | Identificador de `gamelib.c` | "El código del juego se compiló con otra versión: los dos debéis recompilar con la misma" |
| Versión del juego | `g_game_version` | "Otra versión del juego (hace falta 1.9.0 en los dos)" |
| Overlay `online_mod` | Sello `v<N>` | "Los actores del modo en línea son de otra versión: bórralos y se regeneran" |
| DLC | `rt_dlc_check` | "Uno tiene el DLC y el otro no" |

El REJECT lleva motivo + los dos valores para que la UI los muestre. El orden de comprobación es: protocolo →
versiones → contraseña (así un intento con versión distinta no gasta intentos de contraseña).

**Investigación previa (T307).** `GAMELIB_ID` incluye los indicadores del compilador y existe `GAMELIB_ID_COMPAT`
para CPU sin AVX2/FMA. Pregunta: ¿dos PC con el mismo código fuente pero distinta CPU producen identificadores
distintos? Si es así, se envían los dos identificadores de cada compilación y basta con que coincida uno, o se
calcula un identificador solo de las fuentes. No se construye E hasta responderla.

### F. Descubrimiento en red local (R3-16)

**Hoy.** No hay UDP en `online.c`. Patrón disponible: cliente UDP no bloqueante de `runtime/dsu.c`
(`dsu_open :183-213`, `receive :232-242`, trabajo acotado por sondeo `:254`) con ayudantes de paquete separados del
socket y probados (`tools/test_dsu.sh`).

**Diseño** (modelo de paquete de consulta en LAN, como en los proyectos de referencia):

1. El anfitrión escucha en UDP el mismo número de puerto que la sesión. No emite nada por su cuenta.
2. El cliente envía una **consulta** por difusión IPv4 (`255.255.255.255` y la difusión de cada interfaz) y por
   multidifusión IPv6 de enlace local, cada 500 ms mientras la lista está abierta (máximo 5 s).
3. El anfitrión responde por unidifusión con un paquete de tamaño fijo (< 200 bytes): magia, protocolo, resumen de
   `OnlineBuildInfo`, nombre del anfitrión, jugadores/máximo, si pide contraseña, puerto TCP.
4. El cliente valida cada campo (longitud exacta, cadenas terminadas, puerto en rango) y nunca se une solo.
5. El socket UDP se sondea desde el `select()` del hilo servidor (anfitrión) y desde un hilo de búsqueda propio
   (cliente). Límite de respuestas por segundo por IP.
6. Interruptor "Visible en la red local" (activado por defecto al hospedar).

### G. Código de invitación (R3-17)

Función pura en `runtime/online_invite.c`: `(familia, dirección, puerto) ↔ texto`.

- Base32 sin caracteres ambiguos, en grupos de 4 con guiones, con 1 byte de versión y suma de control.
- IPv4: 4 + 2 bytes + cabecera → unos 13 caracteres. IPv6: 16 + 2 bytes → unos 32 caracteres.
- Qué dirección se codifica: (1) la IP externa que devuelva UPnP/NAT-PMP si hubo éxito; (2) si no, la IPv6 global si
  existe; (3) si no, la IPv4 local, con el aviso "solo sirve en tu red local".
- El campo "Dirección" acepta indistintamente código, `IPv4:puerto`, `[IPv6]:puerto` o nombre.
- El código **no** lleva la contraseña ni nada secreto. Sí revela la IP: el texto de ayuda lo dice.

### H. UPnP y NAT-PMP/PCP (R3-18, R3-20)

**Hoy.** Nada. La UI muestra un texto fijo (`online_ui.mm:57-59`).

**Diseño.**

1. Hilo de trabajo que arranca al pulsar "Crear sesión", después de abrir el socket. Plazo total: 5 s.
2. Secuencia: descubrimiento SSDP del IGD → `AddPortMapping` TCP → `GetExternalIPAddress`. Si falla, NAT-PMP y PCP
   contra la puerta de enlace. Al terminar la sesión: `DeletePortMapping` (también con plazo; no retrasa la salida).
3. Estado publicado con mutex para la UI: buscando / abierto (IP externa) / no disponible (motivo).
4. Mensajes de alternativa, siempre con el siguiente peldaño:
   - "Puerto abierto automáticamente. Comparte este código: …"
   - "No se pudo abrir el puerto automáticamente. Opciones: abrir el puerto TCP <n> en el router, usar IPv6 si los
     dos tenéis, o una red virtual (Tailscale, ZeroTier)."
   - Si la IP externa del router es privada o de rango CGNAT: "Tu proveedor comparte tu dirección: el puerto abierto
     no sirve desde fuera. Usa IPv6 o una red virtual."
5. Cortafuegos de Windows: texto fijo bajo "Crear sesión" ("La primera vez Windows preguntará si permite la
   conexión: acepta en redes privadas").

**Spike previo (T306).** Código propio mínimo (SSDP + SOAP sobre HTTP plano + NAT-PMP, unos cientos de líneas)
frente a incluir una biblioteca permisiva en `third_party/`. Criterios: funciona con el router del dueño, con
`miniupnpd` en Docker, licencias, y coste de compilación en las tres plataformas (mapa 05 §6.4 explica por qué una
biblioteca enlazada es cara aquí).

### I. IPv6 (R3-19)

- `online_local_addresses` (`online.c:1462-1480`) pasa a listar también IPv6 globales (no las de enlace local ni
  temporales si se pueden distinguir).
- Comprobar el resultado de `setsockopt(IPV6_V6ONLY)` (`online.c:326`); si falla, abrir un segundo socket IPv4.
- Analizador común de direcciones (`runtime/online_addr.c`): `host`, `host:puerto`, `[v6]:puerto`, código.
- En POSIX, comprobar que el descriptor es `< FD_SETSIZE` antes de `FD_SET` (hallazgo H-12).

### J. Reconexión con reanudación de sesión (R3-21 a R3-24)

**Hoy.** `cl_thread` pone `ONLINE_FAILED` a los 10 s de silencio (`online.c:1198-1200`) o al cerrar el servidor
(`:1318`) y termina. El servidor hace `srv_drop` → LEAVE (`:218-229`). Al salir de sesión los actores remotos se
borran de inmediato (DOC:726-730).

**Diseño.**

1. WELCOME añade un **testigo de sesión** aleatorio (16 bytes) y el identificador de sesión del servidor.
2. Servidor: al perder a un cliente autenticado, su `SClient` pasa a estado "ausente" durante el plazo de gracia
   (propuesta: 60 s, pregunta P3-1): conserva nombre, último STATE y EQUIP; envía a los demás un aviso AWAY (mensaje
   nuevo) en vez de LEAVE. Si vence el plazo: LEAVE.
3. Cliente: estado nuevo `ONLINE_RECONNECTING`. Reintentos con espera 1, 2, 4, 8, 15 s (tope 15 s) hasta el plazo.
   HELLO con el testigo → el servidor devuelve la misma ranura, y reenvía JOIN de todos, sus STATE/EQUIP, y pide al
   anfitrión LOOTS y QUESTS completos.
4. Lado juego (`online_actors.c`): mientras un jugador está "ausente", su Link se trata como "cargando" (no se
   dibuja) pero **sus piezas no se destruyen**; hoy un jugador sin STATE 3 s ya manda sus actores al pool
   (`STALE_NS`, DOC:726-727), así que se reutiliza ese camino y solo se evita el `drop(i, left)` definitivo.
5. Al reanudar, el cliente descarta colas pendientes de la conexión anterior (golpes, botín, recogidas) pero conserva
   el estado de sesión válido. La limpieza total sigue ocurriendo en `online_leave` y al agotar el plazo.
6. Si quien cae es el anfitrión, el servidor muere con él: los invitados reintentan contra la misma dirección
   durante el plazo y, si el anfitrión vuelve a crear sesión, entran como jugadores nuevos (otro identificador de
   sesión). Sin migración de anfitrión.
7. Reinicio en caliente del juego ("Salir a la pantalla de inicio"): los hilos de red sobreviven (mapa 06 §1.4). No
   cambia en F3; se prueba que la reconexión no rompe ese caso.

### K. Indicador de calidad de red (R3-25, R3-26)

**Hoy.** PING/PONG a 1 Hz (`online.c:1292-1295`, `:1114-1121`); cada STATE lleva el ping del emisor; la lista de
jugadores muestra la latencia.

**Diseño.**

1. STATE añade `u16 seq` y `u32 t_emisor_ms`. Con `seq` el receptor cuenta los STATE que no llegaron (descartados
   por una cola reemplazable o perdidos si algún día hay UDP): **pérdida efectiva** en una ventana de 5 s.
2. `runtime/online_netstat.c` (puro): media y percentil 95 de latencia, variación del intervalo entre STATE,
   pérdida efectiva, y un nivel de 3 estados con histéresis.
3. Interpolación con `t_emisor_ms` (ajustado con un desfase de reloj estimado por jugador) en vez de la hora de
   recepción (`online.c:910`): una ráfaga tras un atasco deja de producir un salto.
4. Dibujo dentro de `online_overlay_draw` (`runtime/ui/online_ui.mm:181-304`), junto a la lista de jugadores:
   1-3 barras **y** texto; el nivel "malo" añade un triángulo de aviso. No depende solo del color. No hace falta
   tocar `menu_draws()` porque el overlay de Online ya está incluido (`runtime/ui/menu.mm:2594`).

### L. Decisión TCP frente a ENet/UDP (R3-27)

Es un experimento, no un cambio en el juego.

**Montaje (Docker, red puente propia, `--cap-add NET_ADMIN`):**

```
[bot A] ──┐
          ├── [servidor] ── tc netem en la interfaz de cada contenedor
[bot B] ──┘
```

- Variante TCP: el servidor real (`savagewind --server`, que se procesa antes de crear ventana,
  `runtime/main.c:440-446` **[sin verificar que el binario completo arranque en un contenedor sin GPU; si no, se usa
  el arnés de servidor de las pruebas de F2]**) y dos bots.
- Variante ENet: banco independiente en `tools/netbench/` que envía los mismos tamaños y ritmos (STATE 3375 bytes a
  20 Hz, SKEL ~1,6 kB a 20 Hz, ENEMIES hasta 9965 bytes a 10 Hz, eventos sueltos) por dos canales: uno no fiable
  secuenciado y otro fiable. ENet solo vive en ese banco.

**Perfiles `netem`:**

| Perfil | Orden |
|---|---|
| LAN | sin `netem` |
| Fibra | `delay 20ms 4ms loss 0.2%` |
| Normal | `delay 40ms 10ms loss 1%` |
| WiFi malo | `delay 80ms 20ms loss 2%` |
| Móvil | `delay 150ms 40ms loss 5%` |
| Ráfagas | `delay 60ms 15ms loss gemodel 1% 20% 70% 0.1%` |
| Subida corta | `rate 1mbit` en el servidor + perfil Normal |

**Métricas** (cada corrida de 120 s, 3 repeticiones):

| Métrica | Definición |
|---|---|
| Intervalo entre STATE recibidos | p50 / p95 / p99 / máximo (ms). Nominal 50 ms |
| Atascos | nº de intervalos > 150 ms por minuto (150 = 100 ms de interpolación + un intervalo) |
| Edad del estado | hora de recepción − hora del emisor, p50 / p95 (los contenedores comparten reloj) |
| Latencia de eventos fiables | CHAT y HIT: p50 / p95 / máximo |
| Ancho de banda | bytes/s de subida y bajada por nodo |
| Estados descartados | por cola reemplazable (TCP) o por red (UDP), % |
| Tiempo de recuperación | desde `loss 100%` durante 3 s hasta el primer STATE nuevo |

**Regla de decisión propuesta** (el dueño puede cambiarla en el ADR): se recomienda migrar STATE/SKEL/ENEMIES a un
canal no fiable si, en el perfil "WiFi malo", TCP con las colas de A da más de 2 atascos por minuto o p99 > 200 ms y
el prototipo UDP da menos de la mitad. En caso contrario se sigue con TCP. Si se migra, el plan de migración debe
resolver antes que STATE (3375 bytes) no cabe en un datagrama de ~1200 bytes.

### M. Traducciones (R3-28)

- Carril propio con dueño único de `runtime/ui/i18n/*.inc`.
- Paso 1 (ya se puede empezar): las 63 entradas de Online de `es.inc` a los otros 12 idiomas.
  `tools/venv/Scripts/python.exe tools/i18n_check.py --missing <idioma>` imprime las líneas que faltan.
- Paso 2 (al final): los textos nuevos de F3, que los demás carriles entregan como lista en inglés.
- Los textos de estado se generan en inglés en `online.c` y se traducen al pintar (`tr(f)`,
  `online_ui.mm:25-31`). **Atención:** `tools/i18n_check.py` no escanea `runtime/online*.c` (mapa 05 §1.5); los
  literales nuevos deben aparecer en un `.mm` de `runtime/ui/` o añadirse a `SOURCES` del comprobador.
- Hebreo se escribe de derecha a izquierda: revisar el código de invitación y las direcciones mezcladas con texto.

## 3. Ingeniería inversa necesaria

Ninguna. F3 no usa Ghidra ni toca `tools/patches.py` / `tools/game_sites.py`.

## 4. Carriles y propiedad de ficheros

| Carril | Ficheros (dueño único) | Tareas |
|---|---|---|
| **Protocolo** (1 persona/agente en todo el proyecto) | `runtime/online.h`, la constante `PROTOCOL` y las estructuras de cable, la cabecera compartida con el bot | T308 y cada cambio de cable posterior |
| **Núcleo** (serie) | `runtime/online.c` (servidor, cliente, `stop_all`) y, para T319, el módulo de marionetas de `online_actors` que deje F2 | T309-T319 |
| **Módulos puros** (paralelo, un worktree por tarea) | Ficheros nuevos: `runtime/online_ratelimit.*`, `online_auth.*`, `online_invite.*`, `online_discovery.*`, `online_upnp.*`, `online_version.*`, `online_addr.*`, `online_netstat.*` y su `tests/test_online_*.c` | T320-T327 |
| **UI** (serie) | `runtime/ui/online_ui.mm`; persistencia de `online.ini` | T328-T331 |
| **i18n** | `runtime/ui/i18n/*.inc`, `tools/i18n_check.py` | T332-T333 |
| **Pruebas y medidas** | `tools/online_bot.c`, `tools/nettest/**`, `tools/netbench/**`, flujo de CI nuevo | T301-T304, T334-T336 |
| **Integrador** | `Makefile` (un bloque `test-online-*` al final), `runtime/main.c` si hiciera falta, `runtime/launch_flags_table.c` (generado) | T337 |

Reglas:

- Un módulo puro no incluye `online.c` ni lo modifica: expone una cabecera pequeña que el carril Núcleo llama.
- `runtime/ui/menu.mm` y `runtime/settings.*` **no se tocan** en F3: la pestaña Online ya existe (id 13,
  `menu.mm:2245`, `:2357`) y los ajustes de Online viven en `online.ini`, no en `savagewind.ini`.
- Cada módulo puro lleva en la cabecera de su prueba la orden de compilación (convención de `tools/test_*.c`), para
  no depender del `Makefile` hasta que el integrador añada los objetivos.
- Variables `SAVAGEWIND_*` nuevas obligan a regenerar `runtime/launch_flags_table.c`
  (`tools/gen_launch_flags.py`): solo el integrador.

## 5. Coordinación de la versión de protocolo

1. **Un solo cambio de versión para toda F3**, definido al principio (T308): se escriben las estructuras y los
   números de mensaje nuevos (CHALLENGE, HELLO ampliado, WELCOME con testigo, REJECT con detalle, AWAY, campos
   `seq`/`t_emisor_ms` de STATE) aunque los manejadores estén vacíos.
2. El número concreto es "el siguiente libre" según el registro de versiones de `specs/contratos/protocolo.md`. Nadie
   más cambia `PROTOCOL`.
3. Los carriles trabajan contra la rama donde T308 ya está fusionada. Si un carril descubre que necesita un campo
   más, lo pide al dueño del protocolo, que agrupa las peticiones en una segunda ventana (como mucho una por
   semana de trabajo).
4. Cada cambio de cable actualiza a la vez: la cabecera compartida, el bot, las aserciones de tamaño
   (`_Static_assert`) y la tabla de `specs/contratos/protocolo.md`.
5. Compatibilidad: no se mantiene con versiones anteriores. La igualdad exacta sigue siendo la regla; lo que cambia
   es que el rechazo explica el porqué (E).

## 6. Orden recomendado

1. T301 (arnés) y T302 (bot) → sin ellos nada de lo demás tiene `verify`.
2. En paralelo: T303-T304 (medidas), T306-T307 (spikes), T308 (protocolo), T320-T327 (módulos puros), T332 (i18n).
3. Núcleo en serie: T309 → T310 → T311 → T312 → T313 → T314 → T315 → T316 → T317 → T318 → T319.
4. UI en serie, según vayan llegando los módulos: T328 → T329 → T330 → T331.
5. Cierre: T333 → T334 → T337 → T336 → T340 y T341 (dueño).
