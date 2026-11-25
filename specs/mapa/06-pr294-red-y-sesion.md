# 06 · PR #294 "Online mode": capa de red y sesión

Mapa y revisión de la capa de RED y SESIÓN del PR #294 (autores byluki, mstampfli) sobre Savage Wind Recomp.

- Checkout revisado: `$SW/pr294-online` (carpeta hermana de solo lectura), rama `pr-294-online`, HEAD `350c5f8`
  (merge de `7d9753c` "Online … (protocol 38)" con `22d36c9`, la base en `test`).
- Alcance: `runtime/online.c`, `runtime/online.h`, `runtime/ui/online_ui.mm`, `tools/online_bot.c`,
  `tools/swonline.spec`, y los cambios del PR en `runtime/main.c`, `runtime/ui/menu.mm`, `runtime/ui/game_options.mm`,
  `runtime/ui/i18n/*`, `runtime/paths.c`, `runtime/hle/framepace.c`, `runtime/hle/input.c`, `Makefile`, `.gitignore`,
  `tools/setup_windows.ps1`, y las secciones de `docs/ONLINE.md` sobre jugar, funcionamiento, pruebas con dos juegos,
  reglas de sesión y "Not done yet".
- Fuera de alcance (otro agente): `runtime/online_actors.c`, `tools/online_mod.py`, parches y game sites. Solo se
  citan cuando son el consumidor directo de un dato de red.
- Método: lectura completa de los ficheros del área y `git diff 22d36c9 HEAD`. Nada se compiló ni se ejecutó (en esta
  máquina no hay compilador de C ni Python en el PATH). Los tamaños de mensajes están calculados a mano a partir de
  las estructuras `#pragma pack(1)`. Todo lo marcado "verificado" se leyó en el código; lo que no pude comprobar se
  dice expresamente.

Todas las referencias `fichero:línea` son relativas al checkout del PR.

---

## 0. Resumen ejecutivo

- Es un **relé TCP estrella** muy simple: un hilo servidor (dentro del juego del anfitrión o en `--server`) acepta
  hasta 8 conexiones y reenvía casi todo sin interpretarlo. El "anfitrión" con autoridad sobre el mundo no es el
  servidor, sino **el cliente de juego del anfitrión**, que se conecta a su propio servidor por `127.0.0.1`.
- El análisis de paquetes en `online.c` es correcto en cuanto a límites: **no encontré corrupción de memoria** ni
  ningún mensaje que lleve direcciones de memoria. Los problemas son de **diseño**: envíos bloqueantes en un único
  hilo (un par lento congela a todos), ninguna autoridad ni validación semántica en el servidor, contraseña y tráfico
  en claro, sin límites de tasa, sin reconexión, y estado de sesión que sobrevive entre sesiones.
- No hay ninguna prueba automática. Solo `es.inc` está traducido (12 idiomas sin textos de Online).
- El borrado de `.github/workflows/*` es un **artefacto de los merges de la rama del fork**, no un cambio funcional
  del PR (detalle en §8.4).

---

## 1. Arquitectura

### 1.1 Roles

| Rol | Cómo se arranca | Qué corre | Evidencia |
|---|---|---|---|
| Anfitrión (host) | UI "Create session" o `SAVAGEWIND_ONLINE=host` | Hilo servidor (`srv_thread`) + hilo cliente (`cl_thread`) conectado a `127.0.0.1` con `g.hosting = true` | `online.c:1347-1361`, `online.c:1165`, `main.c:525-529` |
| Cliente | UI "Join session" o `SAVAGEWIND_ONLINE=<dirección>` | Solo `cl_thread` | `online.c:1363-1368` |
| Servidor dedicado | `savagewind.exe --server [puerto] [--password p]` | Solo `srv_loop` en el hilo principal, sin juego; no vuelve | `main.c:440-446`, `online.c:412-423` |

Consecuencias verificadas del diseño:

- **El servidor no sabe quién es el anfitrión.** No hay ningún campo "host" en `Server`/`SClient`
  (`online.c:194-211`). La autoridad se decide en cada cliente con su `g.hosting` local (`online.c:1043`, `1056`,
  `1063`, `1066`, `1072`, `1082`, `1091`, `1109`).
- **El anfitrión se asume en la ranura 0.** `online_actors.c:2838-2840` toma las reglas de la sesión de `pl[0]`.
  Funciona porque el cliente del anfitrión es el primero en conectarse a su propio servidor, pero nada lo garantiza
  (no hay reserva de ranura; `online.c:361-362` da la primera libre).
- **En `--server` nadie tiene `g.hosting`**, así que nadie envía ENEMIES / LOOTS / QUESTS, y TIME no sale porque
  `g_local_hour` vale -1 en ese proceso (`online.c:43`, `375-379`). El servidor dedicado hoy solo sirve para
  posiciones, poses, equipo y chat; el mundo compartido queda inerte. `docs/ONLINE.md:15` no lo advierte.
- `--server` se procesa dentro del bucle de argumentos, antes de `rt_paths_init()` (`main.c:454`) y de
  `online_settings_load()` (`main.c:463`): no lee `online.ini`.

### 1.2 Hilos y sincronización

| Hilo | Función | Qué hace |
|---|---|---|
| Hilo de red servidor | `srv_thread` → `srv_loop` (`online.c:343-401`) | `select()` con 100 ms sobre listener + 8 sockets; acepta, lee, trocea en mensajes, reenvía con `send()` **bloqueante** |
| Hilo de red cliente | `cl_thread` (`online.c:1160-1320`) | `select()` 10 ms; recibe y aplica mensajes (`cl_message`); cada vuelta vacía colas de salida; STATE+SKEL a 20 Hz; PING a 1 Hz |
| Hilo del juego (bucle de fotograma) | `rt_online_frame(Cpu*)` (`online_actors.c:5721`), llamado desde el parche `_vfr_ratio` (`tools/patches.py:425-426`) | Muestrea a Link y publica con `online_set_local_*` / `online_send_*`; consume con `online_players`, `online_take_*`, `online_enemies`… |
| Hilo de presentación / UI | `menu_render` → `online_overlay_draw`, `online_tab_draw` (`menu.mm:2357`, `2594`) | Dibuja pestaña y overlay; llama `online_host/join/leave` (que hacen `pthread_join`) |
| Hilo de render Vulkan | `online_camera_set` (`exec.c:10999-11005`) | Publica la matriz vista-proyección para el overlay |
| Hilo principal | `main` | `online_settings_load`, autoarranque por `SAVAGEWIND_ONLINE` |

Cerrojos (no hay colas sin bloqueo; todo son buffers fijos protegidos por mutex):

- `g.lock` (`online.c:443`, `485`): todo el estado del cliente (`g.r[]`, chat, avisos, colas de entrada y salida,
  enemigos, botín, misiones, cámara) y además `g_skel` (`online.c:582-586`).
- `g_pose_lock` (`online.c:811`): pose local, equipo local y enemigos locales del anfitrión (`g_pose`, `g_equip`,
  `g_local_en`).
- El `Server` no tiene cerrojo: solo lo toca su hilo. Comparte con el resto `sv->stop` (volatile) y `g_local_hour`
  (volatile float, `online.c:43`).
- Variables leídas **sin** cerrojo desde otros hilos: `g.state` (`online.c:1202`, `1375`, `1377`, `1389`, `1435`,
  `1483`), `g.hosting` (`online.c:1262`, `1265`, `1376`), `g.id` (`1377`), `g_srv_running` (`1376`), `g.stop`
  (volatile). Ver §5.3.

### 1.3 Dónde ocurre el trabajo por fotograma

- **Juego → red**: `rt_online_frame` rellena `OnlinePose`, esqueleto, equipo, enemigos, botín y misiones y los deja
  en buffers; el hilo cliente los envía a su ritmo. El hilo de red **también lee memoria del juego directamente**:
  `sample_local` lee `PlayerInfo` y el estado de pausa (`online.c:828-831`) 20 veces por segundo, sin sincronizar con
  el hilo del juego.
- **Red → juego**: `cl_message` guarda instantáneas con la hora local de recepción (`online.c:910`); el hilo del
  juego pide `online_players()` (`online.c:1387-1432`), que interpola 100 ms por detrás (`INTERP_NS`,
  `online.c:45`) entre las dos instantáneas que rodean ese instante (anillo de 8, `SNAPS`). Esqueleto: anillo de 4,
  nlerp (`online.c:775-808`). Enemigos: las dos últimas listas (`online.c:736-761`).
- Un jugador sin STATE durante 3 s se marca "loading" y no se dibuja (`STALE_NS`, `online.c:47`, `1402-1405`).

### 1.4 Ciclo de vida

```
 ANFITRIÓN (proceso del juego)                               CLIENTE (otro proceso / otro PC)
 ┌───────────────────────────────────────────────┐           ┌──────────────────────────────────┐
 │ hilo UI: online_host()                        │           │ hilo UI: online_join()           │
 │   stop_all(); srv_open(puerto)  [listen :52520]│           │   stop_all(); start_client(false)│
 │   pthread_create(srv_thread)                  │           │   pthread_create(cl_thread)      │
 │   start_client(true) -> cl_thread             │           └───────────────┬──────────────────┘
 │                                               │                           │
 │  cl_thread(host) ── TCP 127.0.0.1 ──┐         │                           │ TCP (IPv4/IPv6, connect 6 s máx.)
 │                                     ▼         │                           ▼
 │  ┌──────────────── srv_thread: srv_loop (select 100 ms) ──────────────────────────────┐
 │  │ accept -> ranura libre i (used=true, heard=now)   [8 ocupadas: REJECT(FULL)+close] │
 │  │                                                                                    │
 │  │   C→S HELLO {magic "SWOL", version 38, name[24], pass[24]}                          │
 │  │   S→C REJECT {VERSION|PASSWORD|NAME} + cierre            (si falla)                 │
 │  │   S→C WELCOME {slot}                                                               │
 │  │   S→C JOIN{k,name} [+ STATE{k,último}] [+ EQUIP{k,último}]  por cada jugador ya k   │
 │  │   S→todos JOIN{i,name}   (también al nuevo: su propio nombre)                      │
 │  │                                                                                    │
 │  │   régimen: C→S STATE/SKEL 20 Hz, EQUIP, CHAT, PING 1 Hz, HIT, PICK, ESTATE,        │
 │  │            LOOT_ADD/TAKE, QUEST   (cliente no anfitrión)                           │
 │  │            C(host)→S ENEMIES 10 Hz, LOOTS, QUESTS                                  │
 │  │            S→otros: reenvío (con o sin ranura delante);  S→C PONG;  S→todos TIME 2 s │
 │  │                                                                                    │
 │  │   baja: recv<=0 | mensaje inválido | 10 s de silencio | buffer lleno               │
 │  │         -> srv_drop(i): close + S→todos LEAVE{i}                                   │
 │  └────────────────────────────────────────────────────────────────────────────────────┘
 │        ▲ g.lock / g_pose_lock ▲                 │
 │  hilo juego: rt_online_frame()  (publica pose,  │        cl_thread(cliente): 10 s sin oír nada
 │    enemigos, botín, misiones; consume remotos)  │          -> ONLINE_FAILED "Lost the connection"
 │  hilo render: online_camera_set()               │        recv<=0 -> "Disconnected by the server"
 │  hilo UI: overlay + pestaña Online              │        (no hay reintento; hay que pulsar Join otra vez)
 └───────────────────────────────────────────────┘

 Estados del cliente (online.h:22):
   OFF ──host/join──▶ CONNECTING ──WELCOME──▶ CONNECTED ──error/timeout/cierre──▶ FAILED
    ▲                      │ REJECT / no conecta ─────────────────────────────────▶ FAILED
    └────── online_leave() (stop_all) desde cualquier estado ──────────────────────────┘
```

- **Conexión**: `cl_connect` resuelve con `getaddrinfo(AF_UNSPEC)` y prueba cada dirección con `connect` no
  bloqueante y 6 s de espera (`online.c:1128-1158`).
- **HELLO / WELCOME / REJECT**: `online.c:235-261` (servidor), `online.c:971-983` (cliente). Motivos de rechazo:
  `R_FULL=1, R_VERSION=2, R_PASSWORD=3, R_NAME=4` (`online.c:57`). `R_FULL` se envía en el `accept`, antes de leer
  nada (`online.c:364-367`).
- **Desconexión**: el cliente pasa a `ONLINE_FAILED` y se queda ahí (`online.c:1198-1200`, `1318`). El anfitrión que
  pulsa "End session" hace `stop_all()`: cierra servidor y sockets (`online.c:398-400`); los demás ven "Disconnected
  by the server". No hay migración de anfitrión.
- **Reinicio del juego (RT_ON_RESET)**: **no existe ningún `RT_ON_RESET` en `online.c` ni en `online_actors.c`**
  (búsqueda `grep RT_ON_RESET runtime/`: solo otros módulos). Los hilos de red sobreviven al reinicio en sitio
  ("Quit to start screen", `main.c:149-168`): la conexión se mantiene y `sample_local` envía `ONLINE_F_LOADING`
  mientras no hay Link (`online.c:830`, `1282`). La lectura de memoria de invitado durante el reinicio no revienta
  porque en Windows la página descomprometida se vuelve a comprometer a ceros en el manejador de excepciones
  (`platform/windows.c:85-131`, `184-192`) y en Linux se remapea (`platform/linux.c:44-47`). El estado propio de
  `online_actors.c` ante un reinicio queda para el otro agente; desde esta capa, la regla de AGENTS.md "Reset
  per-boot state with RT_ON_RESET" no se cumple.

---

## 2. Protocolo completo (versión 38)

### 2.1 Trama

- TCP, little-endian. Cada mensaje: `u16 longitud` (lo que sigue: tipo + carga), `u8 tipo`, carga
  (`online.c:3-7`, `150-158`).
- Emisor: buffer de pila de `3 + 16384` bytes; un mensaje mayor se descarta en silencio (`send_msg` devuelve false,
  `online.c:151-153`). Máximo útil: 16 384 bytes de carga.
- Receptor: `RecvBuf` de 32 768 bytes por conexión (`online.c:161`); `next_msg` exige `l >= 1` y el mensaje completo
  (`online.c:172-180`). Un mensaje con longitud 0 o mayor que el buffer nunca se completa: el servidor corta al
  llenarse el buffer (`online.c:394`); el cliente corta porque `recv` con tamaño 0 devuelve 0 (`online.c:165-166`).
- Las cargas son **estructuras C empaquetadas copiadas con `memcpy`** (`#pragma pack(1)`, `online.c:59-108`,
  `online.h:44-76`, `216-240`): el formato de cable es el layout en memoria. Solo es válido entre máquinas
  little-endian con el mismo `float` (x86-64 y aarch64 lo son).
- Tipos desconocidos se ignoran sin cortar (`switch` sin `default`, `online.c:262-311`, `970-1122`).

### 2.2 Versionado

- `PROTOCOL 38` y `MAGIC 0x4c4f5753` "SWOL" (`online.c:39-40`). El servidor exige **igualdad exacta**
  (`online.c:240`); si no, `REJECT(R_VERSION)` → "Another version of Savage Wind".
- No hay negociación de capacidades ni campos opcionales: cualquier cambio de `WireState` obliga a subir la versión
  (38 versiones en unas tres semanas según el historial). El bot lleva el 38 escrito a mano (`online_bot.c:102`).
- Aparte existe la versión del mod de actores (`online_mod/v42`, `main.c:507`), que no se comprueba entre jugadores.
- Documentación desfasada: `docs/ONLINE.md:52` dice "protocol 9"; `docs/ONLINE.md:46` dice "tab 10" (hoy es la 13);
  `online_bot.c:82` y `docs/ONLINE.md:333` dicen que STATE mide 2969 bytes (hoy 3375).

### 2.3 Mensajes

Tamaños de carga calculados: `WireHello` 54, `WireState` 3375, `WireEquip` 432, `OnlineEnemy` 311, `OnlineLoot` 97,
`OnlineShots` 469, `OnlineGrab` 100, `OnlineFxEvent` 24, `OnlineObj` 52, `OnlineMover` 80, `OnlineHeld` 81.
"Relé con ranura" = el servidor antepone `u8 ranura del emisor`. "Relé crudo" = lo reenvía tal cual, sin origen.

| # | Tipo | Sentido | Carga C→S | Lo que hace el servidor | Carga S→C | Ritmo | Líneas |
|---|---|---|---|---|---|---|---|
| 1 | HELLO | C→S | `u32 magic, u16 version, char name[24], char pass[24]` (54) | Valida, asigna nombre, responde | — | 1 vez | `online.c:105`, `235-261`, `1176-1179` |
| 2 | WELCOME | S→C | — | **Decide** la ranura | `u8 slot` | 1 vez | `249`, `971-974` |
| 3 | REJECT | S→C | — | **Decide** | `u8 motivo` | 1 vez | `243-245`, `364-367`, `975-983` |
| 4 | STATE | C→S→otros | `WireState` (3375) | Guarda el último; relé con ranura a los demás; lo da a quien entra | `u8 slot + WireState` (3376) | 20 Hz | `263-268`, `1001-1007`, `1279-1284` |
| 5 | JOIN | S→C | — | **Genera** | `u8 slot, char name[24]` | al entrar | `254`, `258`, `984-994` |
| 6 | LEAVE | S→C | — | **Genera** | `u8 slot` | al salir | `218-229`, `995-1000` |
| 7 | CHAT | C→S→todos | texto + NUL (≤120) | Trunca a 119 + NUL; relé con ranura a **todos**, emisor incluido | `u8 slot + texto` | a demanda (cola de 4) | `299-306`, `1031-1041`, `1309-1315` |
| 8 | PING | C→S | `u64` (ns del cliente) | Eco | — | 1 Hz | `307-309`, `1292-1295` |
| 9 | PONG | S→C | — | — | eco (≤8 bytes) | 1 Hz | `1114-1121` |
| 10 | EQUIP | C→S→otros | `char armor[3][48], gear[6][48]` (432) | Guarda el último; relé con ranura | `u8 slot + WireEquip` | al cambiar y cada 5 s | `269-274`, `1021-1030`, `1296-1308` |
| 11 | SKEL | C→S→otros | `u8 n, u8 nbow, (n+nbow)×12` (≤1922) | Relé con ranura si `2 ≤ len ≤ 1922` | `u8 slot + …` | 20 Hz (no en carga) | `53-56`, `275-277`, `1008-1020`, `1285-1290` |
| 12 | TIME | S→C | — | **Genera** desde `g_local_hour` del proceso | `float hora 0..24` | cada 2 s | `52`, `373-379`, `1108-1113` |
| 13 | ENEMIES | C(host)→S→otros | `float centro[3], u8 n, n×OnlineEnemy` (≤9965) | Relé crudo si `len ≥ 13`. **No comprueba que venga del anfitrión** | igual | 10 Hz | `278-280`, `1042-1054`, `1264-1278` |
| 14 | HIT | C→S→otros | `u32 id, i32 daño, u8 elemento` (9) | Relé crudo (9 bytes) | igual | por golpe (cola de 32) | `281-283`, `1100-1107`, `1203-1212` |
| 15 | ESTATE | C(no host)→S→otros | `u8 n, n×OnlineEnemy` (≤9953) | Relé con ranura | `u8 slot, u8 n, …` | cuando el juego publica | `284-286`, `1090-1099`, `1251-1263` |
| 16 | LOOT_ADD | C(no host)→S→otros | `OnlineLoot` (97) | Relé con ranura si `len ≥ 4` | `u8 slot + OnlineLoot` | por objeto (cola de 16) | `287-290`, `1055-1061`, `1247` |
| 17 | LOOT_TAKE | C(no host)→S→otros | `u32 gid` | Relé con ranura | `u8 slot + u32` | por recogida | `287-290`, `1062-1064`, `1248` |
| 18 | LOOTS | C(host)→S→otros | `u8 n, n×OnlineLoot` (≤6209) | Relé crudo | igual | al cambiar y cada 2 s | `294-298`, `1081-1089`, `1241-1249` |
| 19 | QUEST | C(no host)→S→otros | 4096 bytes (bitmap de 32 768 flags) | Relé con ranura, **sin comprobar longitud** | `u8 slot + 4096` | al cambiar y periódico | `291-293`, `1065-1070`, `1222-1230` |
| 20 | QUESTS | C(host)→S→otros | 4096 bytes (unión) | Relé crudo | igual | al cambiar | `294-298`, `1071-1073`, `1227-1229` |
| 21 | PICK | C→S→otros | `char name[40], float pos[3]` (52) | Relé crudo | igual | por recogida (cola de 8) | `294-298`, `1074-1080`, `1213-1221` |

Contenido de `WireState` (3375 bytes, `online.c:60-104`): posición de los pies y rumbo, flags (pausa / cargando),
ping propio, animación derivada del movimiento, 4 nombres de AS, hueso y matriz de 6 piezas de equipo, disparos y
explosiones, efectos, Magnesis y Stasis (nombre de actor + matrices), pilares de Cryonis, mapa, jefes cercanos,
estela del arma, 8 eventos de efectos/sonido, 24+16 propiedades ELink, clima, tintes, golpe PvP
(`pvp_seq/target/damage`), caballo y arreos, **las reglas del anfitrión** (`collide`, `revive`, `nopause`),
temporizador de Luna Carmesí, último enemigo golpeado, 8 objetos y 6 mecanismos en movimiento, 8 objetos rotos,
4 troncos, 5 materiales sostenidos.

### 2.4 Qué decide el servidor y qué solo retransmite

- **Decide**: ranura, rechazo, nombre saneado (`copy_name`, `online.c:187-191`), JOIN/LEAVE, expulsión por silencio,
  truncado del chat, la hora (TIME) solo cuando corre dentro del juego del anfitrión.
- **Retransmite sin mirar**: todo lo demás. No valida contenido, ni tasa, ni que ENEMIES/LOOTS/QUESTS vengan del
  anfitrión, ni que HIT/ESTATE/LOOT_*/QUEST lleguen solo al anfitrión (van a todos; cada cliente descarta según su
  `g.hosting`).
- Las reglas de sesión (colisiones, revivir, menús sin pausa) no son un mensaje: viajan en el STATE del jugador de la
  ranura 0 y cada cliente las adopta (`online_actors.c:2838-2840`).

### 2.5 Ancho de banda (calculado, no medido)

- Subida por cliente: STATE 3378 B × 20 = ~68 kB/s, más SKEL (la doc da ~1,6 kB × 20 = ~30 kB/s para 121 huesos,
  `docs/ONLINE.md:56-57`) → **~100 kB/s (~0,8 Mbit/s) por jugador**, sin compresión ni deltas: se envía el estado
  entero aunque nada cambie.
- El anfitrión añade ENEMIES (hasta 9968 B × 10 = ~100 kB/s en el peor caso), LOOTS y QUESTS.
- El servidor reenvía cada flujo a N-1 jugadores: con 2 jugadores es asumible (~0,1-0,2 MB/s de subida del
  anfitrión); con 8 son ~56 flujos, del orden de 5-6 MB/s de subida en casa del anfitrión. `docs/ONLINE.md:1001` ya
  lo lista como pendiente ("Less to send").

---

## 3. Transporte: consecuencias de usar TCP

| Aspecto | Estado | Evidencia |
|---|---|---|
| Bloqueo de cabeza de línea | **Presente y sin mitigar.** Todo (estado a 20 Hz, esqueleto, chat, eventos) va por un único flujo fiable. Un paquete perdido retiene todo lo posterior y luego llega en ráfaga. Como la marca de tiempo de cada instantánea es la hora local de recepción (`online.c:910`, `1013`, `1047`), una ráfaga produce instantáneas casi simultáneas y el interpolador salta | `online.c:907-964`, `1406-1415` |
| Nagle | `TCP_NODELAY` en los sockets aceptados y en el del cliente | `online.c:124-127`, `363`, `1154` |
| Buffers | Sin `SO_SNDBUF`/`SO_RCVBUF`. Recepción 32 KiB por conexión, emisión 16 KiB por mensaje en pila. **No hay cola de salida**: `send_all` bloquea hasta enviar todo | `online.c:140-158` |
| Envíos | **Bloqueantes** en los dos lados; sin `SO_SNDTIMEO`; sin `select` de escritura | `online.c:140-148`, `1153` |
| Tiempos de espera | Conexión 6 s por dirección; silencio 10 s en ambos lados (`TIMEOUT_NS`); jugador "rancio" a 3 s. No hay plazo para el HELLO | `online.c:46-48`, `384`, `1144`, `1198` |
| Keepalive | No hay `SO_KEEPALIVE`. A nivel de aplicación: PING cada 1 s y su PONG | `online.c:1292-1295` |
| IPv4 / IPv6 | Servidor: socket IPv6 con `IPV6_V6ONLY=0` (doble pila) y, si no hay IPv6, IPv4. Escucha en **todas las interfaces**, sin opción de dirección de enlace. Cliente: `AF_UNSPEC`. La lista de "direcciones de este PC" solo muestra IPv4 | `online.c:318-333`, `1131`, `1469` |
| Puerto | 52520/TCP por defecto, configurable (UI, `online.ini`, `SAVAGEWIND_ONLINE_PORT`) | `online.h:18`, `online.c:1518`, `1535-1536` |
| Descubrimiento LAN | **Ausente** (no hay UDP ni broadcast en el código) | — |
| UPnP / NAT-PMP / STUN / relé | **Ausente.** La UI pide redirigir el puerto o usar Tailscale / ZeroTier / Radmin | `online_ui.mm:57-59` |
| Reconexión | **Ausente.** Tras un fallo el estado es `ONLINE_FAILED` hasta que el usuario pulsa Join. No hay token de sesión ni reserva de ranura: quien vuelve es un jugador nuevo en la primera ranura libre | `online.c:1198-1200`, `1318` |
| Migración de anfitrión | **Ausente** | — |
| Cifrado / autenticación | **Ausente** (ver §4) | — |
| SIGPIPE (Linux/macOS) | `send()` sin `MSG_NOSIGNAL` ni `SO_NOSIGPIPE`, y el runtime no ignora SIGPIPE (`grep SIGPIPE runtime/` solo da `control.c:652-669`, que sí se protege). Escribir a un par que cerró puede matar el proceso | `online.c:143` |

Valoración para nuestro objetivo (2 jugadores, conexión directa): TCP es defendible para arrancar porque simplifica
NAT y fiabilidad de eventos, pero pide como mínimo envíos no bloqueantes con cola y descarte del estado viejo,
marcas de tiempo del emisor y reconexión. Separar más adelante un canal no fiable (UDP) para STATE/SKEL quitaría el
bloqueo de cabeza de línea.

---

## 4. Seguridad: análisis de la entrada de red no confiable

El servidor acepta conexiones entrantes de Internet (puerto redirigido) y por defecto **sin contraseña**
(`online.c:110`, `241`). Cada cliente además confía en el servidor al que se une.

### 4.1 Lo que está bien (verificado)

- **Trama**: `next_msg` comprueba cabecera, longitud mínima y mensaje completo con aritmética `int` sin
  desbordamiento posible (longitud ≤ 65 535, buffer 32 768) (`online.c:172-180`).
- **Índices de ranura**: todos los `p[0]` usados como índice se comprueban `< ONLINE_MAX` (`online.c:972`, `985`,
  `996`, `1002`, `1009`, `1022`, `1032`, `1066`). Los clientes no eligen su ranura: la pone el servidor.
- **Contadores**: ENEMIES `n ≤ 32` y longitud suficiente (`1044-1045`); LOOTS `n ≤ 64` (`1083-1084`); ESTATE
  limitado por longitud y por la cola de 64 (`1093`); SKEL `n + nbow ≤ 160` y longitud (`1011-1012`); colas de
  entrada con tope (`1056`, `1063`, `1075`, `1101`); `shots.n` e `ice_n` recortados (`918`, `924`).
- **Cadenas**: HELLO termina nombre y contraseña (`239`); el cliente termina todas las cadenas de STATE, EQUIP,
  ENEMIES, LOOT, PICK y chat (`914-959`, `1027-1028`, `1052`, `1058`, `1076`, `1086`, `1037`). Una excepción en §4.2
  (H-7).
- **Sin cadenas de formato controladas por la red**: avisos y estado usan literales fijos como formato y el dato
  como argumento (`online_ui.mm:29`, `199`); el chat se pinta con `AddText` sin formato (`online_ui.mm:246-247`).
- **Ningún mensaje lleva direcciones ni desplazamientos de memoria.** Los identificadores (`OnlineEnemy.id`,
  `gid`, `hash`) se buscan en tablas del consumidor (p. ej. `own_find`, `online_actors.c:2302`), no se desreferencian.
  En esta capa **no hay escritura arbitraria en memoria del invitado**.
- El anfitrión ignora ENEMIES / LOOTS / QUESTS / TIME ajenos (`!g.hosting`), y los no anfitriones ignoran
  LOOT_ADD / LOOT_TAKE / QUEST / ESTATE (`g.hosting`).

### 4.2 Hallazgos

| ID | Severidad | Hallazgo | Evidencia |
|---|---|---|---|
| H-1 | **Alta** (DoS remoto, y fallo espontáneo) | **Un par que deja de leer congela la sesión entera.** El servidor es un único hilo y reenvía con `send()` bloqueante sobre sockets bloqueantes. Cuando se llena la ventana TCP de un cliente (basta dejar de leer, suspender el proceso o perder la red sin cierre), `send_all` no vuelve: nadie más recibe nada, y la expulsión por 10 s de silencio no puede actuar porque el hilo está dentro de `send`. Además `stop_all()` hace `pthread_join` de ese hilo, así que "End session" cuelga la UI del anfitrión. No hace falta mala intención: ocurre con una caída de red normal | `online.c:140-148`, `213-216`, `359-369`, `384`, `1325-1328` |
| H-2 | **Alta** (integridad de partidas) | **Sin autoridad en el servidor ni validación semántica.** (a) Cualquier cliente puede enviar ENEMIES, LOOTS, QUESTS y PICK, y el servidor los reenvía crudos: se hace pasar por el anfitrión ante los demás clientes. (b) Hacia el anfitrión, cualquier cliente puede enviar QUEST con los 32 768 bits a 1: el juego del anfitrión hace la unión y la reparte, y cada juego activa los flags compartidos que le falten (misiones, santuarios, cofres, Bestias Divinas…; `docs/ONLINE.md:429-494`, `985-992`), que acaban **en el guardado de todos**. También HIT con cualquier id y daño, LOOT_ADD con nombre de actor y posición arbitrarios, ESTATE. No son escrituras arbitrarias de memoria (son bits de una lista fija `quests.txt`), pero sí un cambio irreversible del progreso de otro jugador | `online.c:278-298`, `1055-1070`, `1090-1107`; `online_actors.c:3851-3936` |
| H-3 | **Media-alta** | **Contraseña y tráfico en claro.** La contraseña viaja sin cifrar en HELLO, se compara con `strcmp`, no hay límite de intentos ni espera entre ellos, se guarda en claro en `online.ini`, se muestra en claro en la UI (campo de texto normal, sin máscara) y en `--server` va por línea de órdenes. No hay cifrado ni integridad de ningún mensaje: cualquiera en el camino puede leer o inyectar | `online.c:105`, `241`, `1178`, `1549-1551`; `online_ui.mm:64-66`; `option_rows.mm:302-318`; `main.c:440-445` |
| H-4 | **Media** (DoS) | **Agotamiento de ranuras antes de autenticarse.** La ranura se ocupa en el `accept`, antes del HELLO; `heard` se renueva con cualquier byte; no hay plazo para el HELLO ni límite por IP. Ocho conexiones que envían un byte cada pocos segundos (por ejemplo, la cabecera de un mensaje que nunca completan) bloquean la sesión hasta llenar 32 KiB cada una, y se pueden reabrir. El anfitrión tampoco tiene ranura reservada | `online.c:359-370`, `383-388`, `394` |
| H-5 | **Media** (DoS) | **Sin límite de tasa y con amplificación.** El servidor reenvía cada mensaje de hasta 16 KiB a todos los demás, sin contar frecuencia. QUEST se reenvía sin comprobar longitud. Un cliente puede saturar la subida del anfitrión, y combinado con H-1, bloquearlo | `online.c:262-311`, `291-293` |
| H-6 | **Media** | **Valores numéricos sin validar.** Solo se terminan cadenas y se recortan tres contadores. Pasan al juego tal cual: posición y rumbo (sin `isfinite`; el emisor honesto sí filtra, `online.c:832-834`, pero el receptor no), todas las matrices, `daño` de HIT con signo (un valor negativo sube la vida: `online_actors.c:2308`), `pvp_damage`, `pvp_target`, `bloody_moon`, la hora de TIME (cualquier float, `online.c:1110`), `fx_anim`, `gear_bone`, hashes. Si alguno llega a provocar un fallo en el juego es asunto de `online_actors.c` (otro agente); desde la red no hay filtro | `online.c:907-964`, `1100-1113` |
| H-7 | **Baja-media** | **ESTATE no termina `shield`.** Al recibir ESTATE se terminan `name`, `as` y `weapon` pero no `shield` (ENEMIES sí lo hace). El anfitrión copia ese registro a su tabla (`online_actors.c:2398`); un `shield` de 40 bytes sin NUL se leería más allá del campo como cadena. Es lectura fuera de campo, no escritura | `online.c:1095` frente a `online.c:1052` |
| H-8 | **Baja-media** (Linux/macOS) | **SIGPIPE.** Ver §3: un `send` a un par que cerró puede terminar el proceso (juego del anfitrión o servidor dedicado) | `online.c:143` |
| H-9 | **Baja** | **Nombres y chat.** `copy_name` solo sustituye bytes < 32; no valida UTF-8, ni longitud mínima visible, ni duplicados (dos "Link" son indistinguibles salvo por color). El chat se trunca pero no se filtran caracteres de control ni UTF-8 inválido antes de pintarlo con ImGui. El cliente no vuelve a sanear los nombres de JOIN (confía en el servidor) | `online.c:187-191`, `299-306`, `990-991`, `1034-1038` |
| H-10 | **Baja** | **El cliente confía por completo en el servidor.** Un servidor malicioso puede hacer H-2 y H-6 contra quien se una, y reenviar WELCOME para cambiar `g.id` a mitad de sesión. `est_from` y `origin` no se acotan a `< ONLINE_MAX` (hoy solo se comparan, no indexan) | `online.c:971-974`, `1058`, `1096` |
| H-11 | **Baja** (local, no remoto) | `system()` con la ruta de los ficheros del juego entre comillas para generar `online_mod`: una ruta con comillas en `game_dir.txt` o `SAVAGEWIND_GAME_DIR` inyecta órdenes. Solo lo controla el propio usuario | `main.c:344-365`, `507-516` |
| H-12 | **Baja** | En POSIX, `select()` con descriptores ≥ `FD_SETSIZE` (1024) desborda `fd_set`; el juego abre muchos ficheros. Además no se comprueba el resultado de `setsockopt(IPV6_V6ONLY)`: si falla, el cliente del anfitrión (`127.0.0.1`) no puede entrar a su propio servidor | `online.c:326`, `345-355`, `1184-1188` |

Lo que **no** encontré: desbordamientos de buffer, índices fuera de rango, enteros que desborden un tamaño, o
mensajes que escriban en direcciones elegidas por el par, dentro de `online.c`. La superficie peligrosa real está en
cómo `online_actors.c` usa nombres de actores, matrices y hashes recibidos (otro agente).

---

## 5. Robustez

### 5.1 Manejo de errores

- Paquete mal formado en el servidor: HELLO corto, STATE corto o EQUIP corto → expulsión (`online.c:236`, `264`,
  `270`, `393-394`). El resto de tipos con longitud insuficiente se ignoran en silencio.
- Paquete mal formado en el cliente: se ignora; solo REJECT termina la sesión (`online.c:981`).
- Lecturas parciales: correctas en ambos lados (acumular, trocear, `memmove` del resto, `online.c:182-185`).
- Errores de envío: el servidor **ignora** el resultado de `send_msg` en los reenvíos (`online.c:215`); el par
  muerto se detecta después por `recv`. El cliente sí corta si falla un envío de datos, pero no por PING, EQUIP ni
  CHAT (`online.c:1294`, `1306`, `1315`).
- `select() < 0` en el servidor hace `continue` sin pausa (`online.c:356`): con un error persistente el hilo gira al
  100 % de CPU.
- Fallos de `socket`, `bind`, `listen` y `pthread_create` se tratan; `WSAStartup` y los `setsockopt` no.

### 5.2 Escenarios

| Escenario | Qué pasa hoy |
|---|---|
| Par lento o congelado | H-1: el servidor se bloquea en `send` y congela a todos. No hay descarte de estados viejos ni cola por cliente |
| Par que cae sin cerrar (y sin llenar la ventana) | Expulsado a los 10 s, LEAVE a los demás; a los 3 s ya no se le dibuja |
| El anfitrión sale o termina la sesión | Se cierran los sockets; los clientes pasan a FAILED "Disconnected by the server". Sin aviso previo, sin reconexión, sin migración. Lo que ocurre con los actores remotos es de `online_actors.c` |
| El cliente del anfitrión pierde su conexión local pero el servidor sigue | `g_srv_running` sigue a true y el servidor queda sin anfitrión, relé puro para los demás (`online.c:1318`, `1376`). La UI del anfitrión vuelve a ofrecer Create/Join |
| Reinicio a título estando conectado | La conexión sigue; se envía `ONLINE_F_LOADING`; sin `RT_ON_RESET` (§1.4) |
| Unirse a un puerto que acepta pero no responde | 10 s en CONNECTING y luego "Lost the connection" |
| Pulsar Join/Leave mientras conecta | `stop_all` espera al hilo: hasta 6 s por dirección resuelta, más `getaddrinfo`, que no es cancelable. Se ejecuta en el hilo de UI: la imagen se congela ese tiempo (`online.c:1322-1335`, `1134`) |
| Ráfaga tras pérdida de paquete | Instantáneas con casi la misma hora local: salto visible (§3) |
| Más de 26 enemigos cerca del anfitrión | El **bot** se desconecta: su buffer es de 8192 bytes y ENEMIES puede medir hasta 9965 (`online_bot.c:107`, `115-116`). El juego real no tiene ese problema (32 KiB) |

### 5.3 Seguridad entre hilos

| ID | Severidad | Problema | Evidencia |
|---|---|---|---|
| T-1 | **Media** (error funcional) | **El estado de sesión no se limpia entre sesiones.** `stop_all` solo borra `g.r`, `id`, chat, cola de chat, `hosting` y `ping`. Sobreviven: `g.quests` y `quests_have` (la unión de misiones de la sesión anterior se entrega como válida al entrar en otra: `online_quests`, `online.c:682-688`), `g.loots`/`nloots`, `g.en[]`, `quest_of[]`, y las colas pendientes de golpes, estados, botín y recogidas. Un jugador que cambia de sesión puede aplicar en la nueva los flags de la anterior | `online.c:1322-1335` |
| T-2 | Baja-media | `quest_of[slot]` no se borra cuando un jugador sale y otro ocupa la ranura (JOIN solo limpia `g.r[slot]`): el nuevo hereda el bitmap del anterior hasta su primer QUEST | `online.c:988`, `1067`, `668-670` |
| T-3 | Baja-media | **Variables `static` dentro de funciones** que persisten entre sesiones y no tienen cerrojo: `motion_anim` (`524`), `sample_local` (`824-825`, `835`), temporizadores y buffers en `cl_thread` (`1232-1233`, `1252`, `1264`, `1267`, `1297-1298`), `next_time` en `srv_loop` (`374`). Impiden además tener dos clientes en un proceso (pruebas) | líneas citadas |
| T-4 | Baja (UB formal) | Lecturas sin cerrojo ni atómicos de `g.state`, `g.hosting`, `g.id`, `g_srv_running`, `g.thread_on`. En x86-64 son benignas, pero son carreras de datos según C11 | `online.c:1202`, `1262`, `1265`, `1375-1377`, `1389` |
| T-5 | Baja | `g_online` (ajustes) lo escribe la UI y lo leen el hilo de red (`1165-1166` bajo `g.lock`, que la UI no toma; `1177-1178` sin cerrojo) y el hilo del juego. La UI bloquea el campo Nombre en sesión, pero no los interruptores | `online_ui.mm:51-53`, `105-126` |
| T-6 | Baja | `online_host/join/leave` no son reentrantes ni tienen cerrojo; se llaman desde el hilo de UI y, en el arranque, desde el principal. Hoy no coinciden en el tiempo | `online.c:1347-1373`, `main.c:525-529` |
| T-7 | Baja | El hilo de red y el de UI leen memoria del invitado sin sincronizar (`PlayerInfo`, estado de pausa). La primera lectura es segura (§1.4); la segunda (`pinfo + 0x88`) depende de que el puntero leído sea válido. Con un valor a medias o basura fuera del rango del invitado habría fallo; no vi un camino real que lo produzca | `online.c:828-831`, `online_ui.mm:258-259` |

---

## 6. UI / UX

### 6.1 Qué ofrece hoy

- **Acceso**: pestaña "Online" del menú del runtime (F1), id 13, **solo dentro del juego**, no en la pantalla de
  inicio (`menu.mm:2242-2248`, `2357`); y una fila "Online" en las Opciones del propio juego que abre esa pestaña
  (`game_options.mm:383-384`, `2291-2293`; `menu.mm:102-106`).
- **Pestaña** (`online_ui.mm:38-130`): Estado; Nombre (bloqueado en sesión); Crear sesión (Puerto, Contraseña, botón
  que lista las IPv4 de este PC); Unirse (Dirección, botón); en sesión: lista de jugadores con latencia o
  "loading" / "paused", campo de Chat, botón Salir / Terminar sesión; En pantalla (Mostrar jugadores, Mostrar
  nombres); Combate (Daño entre jugadores; Colisiones, Menús sin pausa y Revivir, deshabilitados para el invitado
  porque manda el anfitrión).
- **Overlay** (`online_ui.mm:181-304`): avisos "se unió / se fue" 5 s; línea de estado al conectar o fallar; lista
  de jugadores con círculo de color bajo los corazones; últimas 6 líneas de chat que se desvanecen a los 12 s;
  figura esquemática, nombre y distancia sobre cada jugador (la figura solo si no tiene actor real,
  `online_has_actor`); flecha en el borde hacia los que están fuera de vista; nada a más de 3000 m.
- **Colores**: 8 colores fijos por número de ranura (`online_ui.mm:20-23`). Solo en el overlay; el jugador no elige
  el suyo y no afecta al modelo de Link.
- **Ajustes**: `online.ini` en la raíz de datos (directorio de trabajo tras `rt_paths_init`), claves `name, host,
  port, password, show_names, show_markers, pvp, collide, revive, nopause` (`online.c:1506-1554`). Variables de
  entorno: `SAVAGEWIND_ONLINE`, `_ONLINE_NAME`, `_ONLINE_PORT`, `_ONLINE_PVP`, `_ONLINE_COLLIDE`
  (`online.c:1531-1541`, `main.c:525-529`). Si el nombre viene de la variable de entorno, no se guarda nada
  (`online.c:1544-1545`). Valores por defecto: `collide`, `revive`, `nopause` activos; `pvp` apagado
  (`online.c:110`).

### 6.2 Carencias

- El chat solo se escribe desde el menú; no hay tecla rápida en juego, ni frases rápidas, ni emotes, ni marcadores
  ("pings") en el mundo o el mapa.
- No hay historial de servidores, ni invitación por código, ni descubrimiento en LAN, ni diagnóstico de puerto.
- No se expulsa ni se silencia a nadie; el anfitrión no ve direcciones.
- Los errores son una línea de texto; no hay reintento automático ni botón "Reconectar".
- Contraseña visible en pantalla (malo para quien retransmite).
- Accesibilidad: el color de ranura es la única distinción visual además del nombre; no se integra con la opción de
  visión de color del proyecto (commit `d275989`); tamaños de texto fijos relativos a la escala; sin lector de
  pantalla ni texto a voz para el chat. A favor: los campos de texto usan el teclado en pantalla del proyecto
  (`option_rows.mm:314-316`), así que son utilizables con mando.

### 6.3 Traducciones

- El PR añade 63 entradas **solo en `runtime/ui/i18n/es.inc`** (`git diff --stat`: 1 fichero, +65). Los otros 12
  idiomas (`de fr he it ja ko nl pt ru th zh-Hans zh-Hant`) no tienen ninguna de las cadenas de Online (comprobado
  con `grep` de "Create session" / "Join session" / "Host address": 0 coincidencias en cada uno).
- AGENTS.md pide actualizar las tablas y ejecutar `tools/i18n_check.py`. No pude ejecutarlo (sin Python en esta
  máquina); por la lectura, fallaría o listaría esas 63 cadenas en 12 idiomas.
- Los textos de estado se generan en inglés en `online.c` y se traducen al pintar (`tr(f)`, `online_ui.mm:25-31`):
  el mecanismo es correcto.

---

## 7. Pruebas

### 7.1 Qué existe

- **Ninguna prueba automática** de Online: no hay `tools/test_online*`, ni nada en `tests/`, ni objetivo `test-*`
  en el `Makefile` (`grep online Makefile` solo da `online-helper` y `dist`). Las pruebas descritas en
  `docs/ONLINE.md` son manuales, con dos juegos y lectura de logs.
- El bot (`tools/online_bot.c`) **no está en el `Makefile`**: se compila a mano.
- Además el PR elimina los flujos de CI (§8.4), así que la rama no ejecuta ni las pruebas que ya existían.

### 7.2 El bot

Compilar y lanzar (`online_bot.c:7-9`):

```
cc -O2 -o build/online_bot tools/online_bot.c            # Linux
cc -O2 -o build/online_bot.exe tools/online_bot.c -lws2_32   # Windows (MSYS2 UCRT64)
build/online_bot [host] [puerto] [nombre] [radio] [cabeza torso piernas] [arma escudo arco vaina carcaj]
# por defecto: 127.0.0.1 52520 Bot 4, conjunto hyliano, espada de viajero…
```

Comportamiento: envía HELLO (versión 38 fija), espera WELCOME, sigue al primer jugador del que recibe STATE y camina
en círculo a su alrededor (6 s andando, 4 s con la espada), saluda por chat una vez y envía EQUIP. Envía STATE a
20 Hz con `sleep(50 ms)`. No envía PING ni SKEL.

Variables: `BOT_SWAP=1|2|3` (cambia armadura y espada), `BOT_BOOM=1` y `BOT_BOOM_R` (bomba cada 10 s), `BOT_FAR=1`
(se aleja 150 m y vuelve), `BOT_FREEZE=<s>` (deja de hablar con la conexión abierta: prueba la expulsión por
silencio), `BOT_ICE=x,y,z`, `BOT_DEMO=1`, `BOT_GALE=1`, `BOT_SOUND=<nombre>` y `BOT_SOUND_WHO`,
`BOT_GRAB=<actor>,x,y,z`, `BOT_STOP=<actor>,x,y,z`.

Limitaciones verificadas: duplica `WireState` a mano (`online_bot.c:31-78`), sin cabecera compartida, así que cada
cambio de protocolo hay que repetirlo ahí; buffer de recepción de 8192 bytes (§5.2); `BOT_FREEZE` deja de enviar
pero **sigue leyendo**, de modo que no reproduce H-1.

### 7.3 Dos juegos en un PC (`docs/ONLINE.md:738-769`)

Dos procesos sin ventana, cada uno con su carpeta de guardado (copia, nunca la que está en uso), unos 9,5 GB de RAM
en total. Ejemplo en PowerShell, con las variables que cita la doc más `SAVAGEWIND_NO_LAUNCHER` de AGENTS.md:

```powershell
# Juego A (anfitrión)
$env:SAVAGEWIND_SAVE_DIR="C:\tmp\sw-save-A"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_HIDDEN="1"
$env:SAVAGEWIND_AUDIO_GAIN="0"; $env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="host"
$env:SAVAGEWIND_ONLINE_NAME="Host"; $env:SAVAGEWIND_CONTROL="7777"; .\savagewind.exe

# Juego B (cliente), en otra consola
$env:SAVAGEWIND_SAVE_DIR="C:\tmp\sw-save-B"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_HIDDEN="1"
$env:SAVAGEWIND_AUDIO_GAIN="0"; $env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="127.0.0.1"
$env:SAVAGEWIND_ONLINE_NAME="Client"; $env:SAVAGEWIND_CONTROL="7778"; .\savagewind.exe

# Conducirlos (docs/CONTROL.md)
tools\venv\Scripts\python.exe tools\swctl.py --port 7777 goto <lugar>
tools\venv\Scripts\python.exe tools\swctl.py --port 7778 actors Npc_
```

Órdenes de control añadidas para esto: `actors`, `flag`, `setflag`, `event`, `bloodmoon` (`docs/CONTROL.md`).
Registros opcionales: `SAVAGEWIND_ONLINE_ENEMY_AS_LOG`, `_SHOT_LOG`, `_GEAR_LOG`, `_POSE_LOG`, `_LIFE_LOG`,
`_AGGRO_LOG`, `_MENU_LOG`, `_MOVER_LOG`. Aviso de la doc: el juego puede relanzarse una vez al arrancar (un proceso
hijo), y los guiones que vigilan por PID deben buscar al hijo. Servidor dedicado para pruebas:
`savagewind.exe --server 52530 --password x`.

No ejecuté ninguno de estos comandos.

### 7.4 Qué se puede probar sin el juego

Hoy casi todo es `static` dentro de `online.c` y depende de `plat_now_ns`, `g_site` y `LOG`. Con una extracción
pequeña (por ejemplo `online_proto.c` puro + reloj inyectable) se pueden cubrir con pruebas unitarias al estilo de
`tools/test_*.c`:

1. **Trama**: `next_msg` / `consumed` / `send_msg` con lecturas partidas byte a byte, longitud 0, longitud máxima,
   mensaje mayor que el buffer.
2. **Codificación**: `skel_encode` / `skel_decode` ida y vuelta con tolerancia; cuaternión nulo; traslaciones
   saturadas.
3. **Tamaños de cable**: `_Static_assert(sizeof(WireState) == 3375)` y equivalentes, para que el bot y el juego no
   diverjan.
4. **Máquina de estados del servidor**: `srv_message` con un `Server` en memoria y sockets simulados (o
   `socketpair`): HELLO correcto, versión mala, contraseña mala, nombre vacío, sesión llena, doble HELLO, JOIN y
   LEAVE a terceros, reutilización de ranura.
5. **Manejadores del cliente**: `cl_message` por tipo con cargas cortas, contadores fuera de rango y ranuras ≥ 8;
   ideal como objetivo de fuzzing (libFuzzer / AFL) junto con `srv_message`.
6. **Interpolación**: `online_players`, `online_skeleton`, `online_enemies` con reloj falso: instantáneas en ráfaga,
   teletransporte (> 30 m), jugador rancio.
7. **Ajustes**: `online_settings_load/save` ida y vuelta, valores fuera de rango.
8. **Integración sin juego en CI**: servidor en bucle local + dos bots; comprobar relé, expulsión por silencio
   (`BOT_FREEZE`), y un bot que no lee para demostrar H-1.

---

## 8. Calidad y mantenibilidad

### 8.1 Tamaños y estructura

| Fichero | Líneas | Nota |
|---|---|---|
| `runtime/online.c` | 1554 | Servidor, cliente, colas, interpolación, cámara y ajustes en un solo fichero |
| `runtime/online.h` | 268 | API pública y estructuras de cable mezcladas |
| `runtime/ui/online_ui.mm` | 304 | Pestaña y overlay |
| `tools/online_bot.c` | 283 | Copia manual del protocolo |
| `runtime/online_actors.c` | 6468 | (otro agente) |
| `docs/ONLINE.md` | 1017 | Diario de ingeniería muy detallado, con partes desfasadas |

- **El estado del jugador está escrito cuatro veces**: `OnlinePose` (`online.h:78-128`), `WireState`
  (`online.c:60-104`), la copia campo a campo en `sample_local` (`online.c:845-883`) y en `remote_snap`
  (`online.c:907-964`), más la copia del bot. Añadir un campo exige tocar cinco sitios y subir la versión; es la
  fuente más probable de errores (H-7 es exactamente un olvido de este tipo).
- El estilo (comentarios densos, nombres, manejo de errores) es coherente con el resto del runtime.
- Acoplamiento con el runtime, pequeño y localizado: `g_site.player_info` / `pause_state`, `plat_now_ns`, `LOG`,
  `rt_resource`; hacia fuera, `exec.c` (cámara), `menu.mm`, `game_options.mm`, `input.c` (dos variables globales
  `extern`), `framepace.c` (un getter), `control_game.c`, `test_warp.c`. La API de `online.h` es el único puente con
  `online_actors.c`, lo que facilita sustituir el transporte sin tocar la parte de juego.

### 8.2 Cumplimiento de AGENTS.md

| Regla | Estado |
|---|---|
| Traducciones + `i18n_check.py` | **No**: solo `es.inc` (§6.3) |
| `RT_ON_RESET` para estado por arranque | **No** hay ninguno en `online.c` ni `online_actors.c` (§1.4) |
| "Keep each PR to one change" | **No**: además de Online incluye la función `game_dir.txt` / `SAVAGEWIND_GAME_DIR` (`main.c:344-365`, `paths.c:87-88`, `Makefile` `GAME_DIR`), órdenes de control nuevas, y cambios de render para menús en vivo |
| Cambios de comportamiento para todos, tras una opción apagada por defecto | **No**: en cada arranque, juegue o no en línea, `main.c:492-524` ejecuta con `system()` el generador `online_mod` si falta `online_mod/v42` y activa `SAVAGEWIND_ROMFS_OVERLAY=online_mod`. `make dist` pasa a exigir `make online-helper` (`Makefile:418`, `431`) |
| No subir ficheros de Nintendo ni derivados | Correcto: `online_mod*/`, `online.ini`, `game_dir.txt` y `run.log` están en `.gitignore` (+5 líneas) |
| Direcciones por `g_site` | Correcto en esta capa (`online.c:828-829`) |
| Base en `test` | Correcto (merge de `test` a 18 de noviembre) |

Otros detalles del diff: `tools/test_dsu.sh` pierde el bit de ejecución (modo 100755 → 100644), probablemente por
trabajar en Windows; `tools/setup_windows.ps1` añade `oead` a las dependencias; la receta `online-helper` del
`Makefile:485` quedó en una sola línea con tabuladores intercalados (funciona, pero no sigue la forma de
`gen-helper`).

### 8.3 Puntos calientes de conflicto con `test`

- La rama local `test` (`2ae4b39`) va 4 commits por delante de la base `22d36c9`. `git merge-tree --write-tree HEAD
  test` da un árbol limpio: **hoy no hay conflictos**.
- Ficheros que tocan ambos lados desde la base, y por tanto los que chocarán primero: `Makefile`,
  `runtime/main.c`, `runtime/hle/framepace.c`, `runtime/hle/input.c`, `runtime/nvn/vulkan/exec.c`,
  `runtime/ui/game_options.mm`, `runtime/ui/menu.mm`, `runtime/ui/i18n/es.inc`.
- Los más frágiles:
  - `menu.mm`: el id numérico de la pestaña (13) y las tablas `names[]` / `order[]` (`menu.mm:2242-2245`); el
    propio mensaje del merge cuenta que upstream añadió Teleport (11) y Mods (12) y hubo que renumerar.
  - `game_options.mm`: `enum Action` y `build_items` (`game_options.mm:102`, `383`).
  - `main.c`: bucle de argumentos y el orden raíz del juego → overlay → autoarranque (`main.c:411-529`).
  - `exec.c` (12 203 líneas): captura de cámara junto a DLSS/XeSS (`exec.c:10832`, `10999`).
  - `input.c` `fill_state` (`input.c:1202-1209`).
  - `es.inc`: bloque añadido al final.
- El mensaje del merge avisa de que **upstream reescribió `test`**; los mantenedores publican en `test` como un
  commit limpio (AGENTS.md), así que conviene rebasar con frecuencia y mantener nuestros cambios en ficheros
  propios.

### 8.4 El borrado de `.github/workflows/*`

Hechos verificados con git:

- El diff contra la base borra `.github/dependabot.yml` y cinco flujos: `activity-feed.yml`, `checks.yml`,
  `linux-distros.yml`, `tests.yml`, `windows.yml`.
- La rama Online nació el 29 de octubre de 2026 (`2b13594`), **un día antes** de que upstream añadiera esos flujos
  (`56802c0`, 30 de octubre, "CI: tests without game files…").
- Ningún commit normal de la rama los borra. Faltan en el resultado de **cada merge** de `test` hacia Online
  (`a102f29`, `a7189cc`, `350c5f8`): el segundo padre los tiene y el resultado no. En cambio esos mismos merges sí
  incorporan flujos más nuevos (`crash-save.yml`, `quick_inventory_*.yml`) y cambios en `stale.yml` y `triage.yml`.
- Ningún mensaje de commit lo menciona; el del último merge dice que "los ficheros que Online nunca tocó toman la
  versión de test".

Conclusión: es un **artefacto del proceso de merge del fork** (los merges se resolvieron dejando fuera,
sistemáticamente, los mismos seis ficheros), no un cambio intencionado ni relacionado con el modo en línea. El
motivo exacto no consta en el repositorio (posibles: no ejecutar CI en el fork, o permisos del token), y no lo pude
determinar. Efecto práctico: la rama no ejecuta pruebas, ni compilación de Windows y Linux, ni la comprobación de
ficheros de Nintendo. En nuestro fork hay que **restaurarlos desde `test`** (que además trae `macos-dmg.yml`).

---

## 9. Lista de huecos frente a nuestros objetivos

| Objetivo | Estado | Evidencia y comentario |
|---|---|---|
| Cooperativo de 2 jugadores, la máquina del anfitrión es el servidor, conexión directa | **EXISTE** | `online_host` + `cl_thread` a `127.0.0.1` (`online.c:1347-1361`); puerto redirigido o VPN (`online_ui.mm:57-59`) |
| Más jugadores después | **PARCIAL** | `ONLINE_MAX 8` (`online.h:17`) funciona como relé; el coste es O(N²) sin compresión (§2.5) y hay supuestos frágiles (anfitrión = ranura 0) |
| Estabilidad | **PARCIAL** | Análisis de paquetes sólido, pero H-1 (bloqueo global), T-1 (estado entre sesiones), SIGPIPE, sin `RT_ON_RESET`, sin pruebas |
| Color por jugador | **PARCIAL** | 8 colores fijos por ranura, solo en overlay (`online_ui.mm:20-23`, `229`, `283`, `287`); no elegible, no viaja por la red, no se aplica al modelo. El tinte de armadura sí viaja (`dye`) pero "not seen yet" (`docs/ONLINE.md:1003-1008`) |
| Teletransporte al compañero | **FALTA** | No hay mensaje ni UI. Piezas disponibles: `OnlinePlayer.pos` y `pose.map` del compañero, y la orden `teleport` / `goto` del control (modo desarrollador, un solo mapa) |
| Enviar o soltar objetos al compañero | **PARCIAL** | Lo que se suelta o tira al suelo se comparte y el otro puede recogerlo (PICK, LOOT_*, materiales sostenidos, protocolos 34 y 38). No hay "dar" directo ni UI de intercambio |
| Revivir | **PARCIAL** | Regla `revive`: reanimación automática por hada, sin game over (`online.h:157`, `docs/ONLINE.md:775-783`). No hay estado "caído" ni que el compañero te levante |
| Pings (marcadores) | **FALTA** | PING/PONG es solo latencia. Existen chat y flecha hacia el compañero |
| Reconexión | **FALTA** | §3; además T-1 la haría insegura hoy |
| Accesibilidad | **PARCIAL** | Interruptores de nombres y figuras, entrada de texto con mando; solo español traducido; sin paleta para daltonismo, sin chat en juego, sin TTS (§6.2) |
| PvP opcional | **EXISTE** (frágil) | `g_online.pvp`, apagado por defecto y solo entre quienes lo activan (`online_ui.mm:110-112`, `online.c:864`, `1522`). El daño lo declara el atacante y nadie lo valida (H-2, H-6) |
| Mundo compartido propiedad del anfitrión | **PARCIAL** | Se comparten enemigos, botín, hora, clima, Luna Carmesí, objetos rotos y movidos, flags de misiones. Pero no es "del anfitrión": es la **unión** de lo que hace cualquiera, escrita en el guardado de todos, y la autoridad reside en un cliente, no en el servidor (§1.1, H-2). Con `--server` no hay autoridad |
| Cada jugador conserva inventario y corazones | **EXISTE** | "Each player plays their own game (their save…)" (`docs/ONLINE.md:20-22`); ningún mensaje lleva inventario ni vida propia. Matiz: los flags compartidos (incluidos habilidades de campeones, `docs/CONTROL.md` `setflag`) sí se propagan a todos |
| Momentos de historia con ambos presentes | **FALTA** | Solo hay piezas sueltas: bit `ONLINE_E_DEMO` ("está en una escena") y bloqueo del botón A cerca de quien la ve (`online.h:133`, `input.c:1206-1208`); el progreso se propaga por flags aunque el otro no esté. No hay condición de presencia ni escena sincronizada |

### 9.1 Trabajo previo recomendado en esta capa (por orden)

1. Envíos no bloqueantes con cola por cliente y descarte de STATE/SKEL viejos; plazo para el HELLO; ranura reservada
   al anfitrión; `MSG_NOSIGNAL` (H-1, H-4, H-8).
2. Autoridad en el servidor: marcar quién es anfitrión, aceptar ENEMIES/LOOTS/QUESTS/TIME solo de él y encaminar
   HIT/ESTATE/LOOT_*/QUEST solo hacia él; validar rangos numéricos al recibir (H-2, H-6, H-7).
3. Limpiar todo el estado en `stop_all` y al reutilizar ranura; quitar los `static` de función; atómicos para
   `state/hosting/id` (T-1 a T-4).
4. Una única definición del estado de cable, con `_Static_assert` de tamaños y cabecera compartida con el bot;
   pruebas unitarias y de fuzzing de la trama y de los manejadores; restaurar el CI.
5. Reconexión con token de sesión y marcas de tiempo del emisor; después, compresión por deltas o canal UDP.
6. Contraseña: al menos desafío-respuesta con hash y límite de intentos; idealmente cifrado del canal.
7. Traducir los 12 idiomas restantes y añadir `RT_ON_RESET`.
