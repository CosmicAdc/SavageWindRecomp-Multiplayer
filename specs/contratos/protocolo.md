# Contrato: protocolo de red

- **Parte 1** describe el protocolo **tal como es hoy** en el PR #294 (versión 38, commit `350c5f8`). Todo sale del
  código, con referencia `archivo:línea`; los tamaños están calculados a mano a partir de las estructuras empaquetadas
  y comprobados por suma de campos, **no** medidos con un compilador `[sin verificar con sizeof]`.
- **Parte 2** lista los **cambios previstos**, por fase. Ahí no se inventa ningún formato: lo que no está decidido
  dice "por definir".

Dueño único de este documento y de `runtime/online.h`: la tarea de protocolo de cada fase (constitución, regla 15).

Abreviaturas: `online.c:n` = `runtime/online.c`; `online.h:n` = `runtime/online.h`; `OA:n` =
`runtime/online_actors.c`. "C" = cliente, "S" = servidor. Recuerda que el anfitrión de la partida es **también** un
cliente: su juego se conecta a su propio servidor por `127.0.0.1` (`online.c:1347-1361`).

---

# Parte 1 — El protocolo hoy (versión 38)

## 1.1 Transporte

| Aspecto | Valor | Referencia |
|---|---|---|
| Transporte | TCP, un único flujo por cliente para todo | `online.c:1-7` |
| Puerto | 52520 por defecto; configurable | `online.h:18` |
| Topología | Estrella: cada cliente habla solo con el servidor, que reenvía | `online.c:213-216` |
| Jugadores | Hasta 8 (`ONLINE_MAX`) | `online.h:17` |
| Orden de bytes | Little-endian; las cargas son estructuras de C empaquetadas copiadas con `memcpy` | `online.c:59-108` |
| `TCP_NODELAY` | Activado en ambos lados | `online.c:124-127` |
| Envíos | Bloqueantes, sin cola de salida ni plazo | `online.c:140-148` |
| Cifrado / autenticación | Ninguno; la contraseña viaja en claro | `online.c:105`, `241` |

Como el formato de cable es el layout en memoria, solo es válido entre máquinas little-endian con el mismo `float`
(x86-64 y aarch64 lo son) (`mapa/06` §2.1).

## 1.2 Trama

Cada mensaje:

```
+----------------+----------+-----------------------+
| u16 longitud   | u8 tipo  | carga (longitud - 1)  |
+----------------+----------+-----------------------+
  longitud = bytes que siguen a estos dos (tipo + carga); mínimo 1
```

- Emisor: `send_msg` monta el mensaje en un buffer de `3 + 16384` bytes; uno mayor **se descarta en silencio**
  (devuelve `false`) (`online.c:150-158`).
- Receptor: buffer de 32 768 bytes por conexión; `next_msg` exige `longitud >= 1` y el mensaje completo
  (`online.c:161`, `172-180`).
- Longitud 0 o mensaje que no cabe: nunca se completa. El servidor expulsa al llenarse el buffer
  (`online.c:394`); el cliente corta porque `recv` con tamaño 0 devuelve 0 (`online.c:165-166`).
- Tipo desconocido: se ignora sin cortar (los `switch` no tienen `default`) (`online.c:262-311`, `970-1122`).

## 1.3 Versión

- `MAGIC 0x4c4f5753` (los bytes "SWOL") y `PROTOCOL 38` (`online.c:39-40`).
- El servidor exige **igualdad exacta** de ambos; si no, `REJECT(R_VERSION)` y el cliente muestra "Another version
  of Savage Wind" (`online.c:240`, `975-983`).
- No hay negociación, ni campos opcionales, ni lista de capacidades: cualquier cambio de una estructura obliga a
  subir el número.
- **No** se intercambia ni se comprueba: la versión del juego (1.9.0), la compilación del runtime, `GAMELIB_ID`, la
  versión del overlay de actores (`online_mod/v42`, `runtime/main.c:507`) ni la lista de mods.
- El bot de pruebas lleva el 38 y una copia de `WireState` escritos a mano (`tools/online_bot.c:31-78`, `102`).
- Documentación desfasada en el propio PR: `docs/ONLINE.md:52` dice "protocol 9" y da 2969 bytes para STATE
  (hoy 3375) (`mapa/06` §2.2).

## 1.4 Saludo (handshake) y ciclo de vida

```
Cliente                                   Servidor
   | --- conexión TCP ------------------->|  accept: ocupa la primera ranura libre (used = true)
   |                                      |  si las 8 están ocupadas: REJECT(FULL) y cierra
   | --- HELLO {magic, version, name, pass} ->|
   |                                      |  comprueba, por este orden: magic/version, contraseña, nombre no vacío
   | <-- REJECT {motivo} + cierre --------|  si falla
   | <-- WELCOME {ranura} ----------------|  si pasa
   | <-- JOIN {k, nombre} [STATE] [EQUIP] |  uno por cada jugador k que ya estaba, con su último estado y equipo
   | <-- JOIN {ranura propia, nombre} ----|  el JOIN del recién llegado va a todos, él incluido
   |                                      |
   |  ... régimen permanente ...          |
   |                                      |
   |  baja: cierre, mensaje inválido, 10 s de silencio o buffer lleno
   |                                      |  -> LEAVE {ranura} a los demás
```

Referencias: `accept` y REJECT(FULL) `online.c:359-369`; HELLO/WELCOME/REJECT `online.c:235-261`; lado cliente
`online.c:1176-1179`, `971-983`; baja `online.c:218-229`, `383-394`.

Motivos de REJECT (`online.c:57`): `1 FULL`, `2 VERSION`, `3 PASSWORD`, `4 NAME`.

Estados del cliente (`online.h:22`): `OFF → CONNECTING → CONNECTED`; cualquier error lleva a `FAILED`, y de ahí solo
se sale pulsando otra vez Unirse o Salir. No hay reintento automático (`online.c:1198-1200`, `1318`).

Detalles que importan:

- La ranura se ocupa **antes** del HELLO y no hay plazo para enviarlo (`online.c:359-370`).
- El servidor no guarda quién es el anfitrión. Se **supone** que es la ranura 0 porque su cliente es el primero en
  conectarse (`mapa/06` §1.1; `OA:2838-2840`, `OA:991`).
- Plazos: conexión 6 s por dirección; silencio 10 s en ambos lados; un jugador sin STATE durante 3 s deja de
  dibujarse (`online.c:45-48`, `1144`).
- El nombre se copia con `copy_name`, que sustituye por `?` los bytes menores de 32 (`online.c:187-191`).

## 1.5 Catálogo de mensajes

Convenciones: **relé con ranura** = el servidor antepone `u8 ranura del emisor` a la carga; **relé crudo** = la
reenvía tal cual, sin decir de quién viene. "Otros" = todos menos el emisor. Tamaños en bytes de carga (sin los 3 de
cabecera).

| # | Tipo | Sentido | Carga C→S | Qué hace el servidor | Carga S→C | Ritmo | Referencia |
|---|---|---|---|---|---|---|---|
| 1 | HELLO | C→S | `u32 magic, u16 version, char name[24], char pass[24]` = 54 | Valida y responde | — | 1 vez | `online.c:105`, `235-261` |
| 2 | WELCOME | S→C | — | Lo genera | `u8 ranura` = 1 | 1 vez | `online.c:249`, `971-974` |
| 3 | REJECT | S→C | — | Lo genera | `u8 motivo` = 1 | 1 vez | `online.c:243-245`, `364-367` |
| 4 | STATE | C→S→otros | `WireState` = 3375 | Exige longitud ≥ 3375 (si no, expulsa); guarda el último; relé con ranura | `u8 ranura + WireState` = 3376 | 20 Hz | `online.c:263-268`, `1001-1007`, `1279-1284` |
| 5 | JOIN | S→C | — | Lo genera | `u8 ranura, char name[24]` = 25 | al entrar alguien | `online.c:254`, `258`, `984-994` |
| 6 | LEAVE | S→C | — | Lo genera | `u8 ranura` = 1 | al salir alguien | `online.c:218-229`, `995-1000` |
| 7 | CHAT | C→S→**todos** | texto + NUL, ≤ 120 | Trunca a 119 + NUL; relé con ranura a todos, emisor incluido | `u8 ranura + texto` | a demanda (cola de 4) | `online.c:299-306`, `1031-1041` |
| 8 | PING | C→S | `u64` (reloj del cliente, ns) = 8 | Devuelve hasta 8 bytes | — | 1 Hz | `online.c:307-309`, `1292-1295` |
| 9 | PONG | S→C | — | — | eco de PING, ≤ 8 | 1 Hz | `online.c:1114-1121` |
| 10 | EQUIP | C→S→otros | `char armor[3][48], char gear[6][48]` = 432 | Exige longitud ≥ 432 (si no, expulsa); guarda el último; relé con ranura | `u8 ranura + WireEquip` = 433 | al cambiar y cada 5 s | `online.c:106`, `269-274`, `1021-1030`, `1296-1308` |
| 11 | SKEL | C→S→otros | `u8 n, u8 nbow, (n+nbow) × 12` ≤ 1922 | Relé con ranura si `2 ≤ longitud ≤ 1922` | `u8 ranura + …` | 20 Hz (no durante una carga) | `online.c:53-56`, `275-277`, `1008-1020`, `1285-1290` |
| 12 | TIME | S→**todos** | — | Lo genera a partir de la hora del juego que corre en su mismo proceso | `float hora` (0..24) = 4 | cada 2 s | `online.c:52`, `373-379`, `1108-1113` |
| 13 | ENEMIES | C(anfitrión)→S→otros | `float centro[3], u8 n, n × OnlineEnemy` ≤ 9965 | Relé crudo si longitud ≥ 13. **No comprueba el emisor** | igual | 10 Hz | `online.c:278-280`, `1042-1054`, `1264-1278` |
| 14 | HIT | C→S→otros | `u32 id, i32 daño, u8 elemento` = 9 | Relé crudo de 9 bytes si longitud ≥ 9 | igual | por golpe (cola de 32) | `online.c:281-283`, `1100-1107`, `1203-1212` |
| 15 | ESTATE | C(no anfitrión)→S→otros | `u8 n, n × OnlineEnemy` ≤ 9953 | Relé con ranura si longitud ≥ 1 | `u8 ranura, u8 n, …` | cuando el juego publica | `online.c:284-286`, `1090-1099`, `1251-1263` |
| 16 | LOOT_ADD | C(no anfitrión)→S→otros | `OnlineLoot` = 97 | Relé con ranura si longitud ≥ 4 | `u8 ranura + OnlineLoot` | por objeto (cola de 16) | `online.c:287-290`, `1055-1061` |
| 17 | LOOT_TAKE | C(no anfitrión)→S→otros | `u32 gid` = 4 | Relé con ranura si longitud ≥ 4 | `u8 ranura + u32` | por recogida (cola de 16) | `online.c:287-290`, `1062-1064` |
| 18 | LOOTS | C(anfitrión)→S→otros | `u8 n, n × OnlineLoot` ≤ 6209 | Relé crudo, sin comprobar longitud ni emisor | igual | al cambiar y cada 2 s | `online.c:294-298`, `1081-1089`, `1241-1249` |
| 19 | QUEST | C(no anfitrión)→S→otros | 4096 bytes (un bit por flag) | Relé con ranura, **sin comprobar longitud** | `u8 ranura + 4096` | cuando el juego publica (cada ~2 s) | `online.c:291-293`, `1065-1070`, `1222-1230` |
| 20 | QUESTS | C(anfitrión)→S→otros | 4096 bytes (la unión) | Relé crudo | igual | cuando el juego publica | `online.c:294-298`, `1071-1073` |
| 21 | PICK | C→S→otros | `char name[40], float pos[3]` = 52 | Relé crudo | igual | por recogida (cola de 8) | `online.c:294-298`, `1074-1080`, `1213-1221` |

Los números de tipo salen del `enum` de `online.c:50` (empieza en `M_HELLO = 1`).

Quién acepta qué **en el cliente receptor** (el servidor reenvía a todos; cada cliente descarta según su variable
local `g.hosting`):

| Mensaje | Lo procesa | Condición en el código |
|---|---|---|
| ENEMIES, LOOTS, QUESTS, TIME | Solo quien **no** es anfitrión | `!g.hosting` (`online.c:1043`, `1082`, `1072`, `1109`) |
| LOOT_ADD, LOOT_TAKE, QUEST, ESTATE | Solo el anfitrión | `g.hosting` (`online.c:1056`, `1063`, `1066`, `1091`) |
| HIT, PICK, STATE, SKEL, EQUIP, CHAT | Todos | — |

HIT lo reciben todos; el juego de cada uno lo descarta si el enemigo no es suyo (`own_find`, `OA:2302`).

## 1.6 Estructuras de carga

Tamaños: `WireHello` 54, `WireState` 3375, `WireEquip` 432, `OnlineEnemy` 311, `OnlineLoot` 97, `OnlineShots` 469,
`OnlineGrab` 100, `OnlineFxEvent` 24, `OnlineObj` 52, `OnlineMover` 80, `OnlineHeld` 81.

### `WireState` (`online.c:60-104`) — el estado de un jugador, 20 veces por segundo

Se envía **entero** cada vez, cambie o no. Los campos, en orden de cable:

| Campo | Tipo | Qué es |
|---|---|---|
| `pos`, `yaw` | `float[3]`, `float` | Posición de los pies y rumbo (radianes) |
| `flags` | `u8` | `1` en pausa, `2` cargando (`online.h:23`) |
| `ping_ms` | `u16` | La latencia que el propio jugador mide |
| `anim` | `u8` | Animación genérica deducida del movimiento (`ONLINE_A_*`, 13 valores) |
| `as` | `char[4][32]` | Nombres de las animaciones (AS) que reproduce su Link; primera vacía = "sin pose" |
| `gear_bone`, `gear_rel` | `u8[6]`, `float[6][12]` | Para cada pieza de equipo: en qué hueso va (0 oculto, `0xff` suelta en el mundo) y su matriz |
| `shots` | `OnlineShots` | Hasta 8 bombas/flechas en vuelo (id, tipo, matriz) y 4 explosiones recientes |
| `effects` | `u8` | Bits `ONLINE_E_*` (escudo de Daruk, viento de Revali, "está en un pedestal") |
| `grab`, `stop` | `OnlineGrab` ×2 | Lo que sostiene con Magnesis / paró con Stasis: nombre de actor, dónde lo cogió, matriz |
| `ice_n`, `ice` | `u8`, `float[3][12]` | Pilares de Cryonis |
| `map` | `char[32]` | Mapa en el que está (`MainField/`, `CDungeon/Dungeon000`…) |
| `boss` | `{u32 hash, char name[28], i32 life}[2]` | Jefes cercanos |
| `fx`, `fx_anim`, `fx_frame` | `char[24]`, `i16[3]`, `float[3]` | Estela del arma |
| `fxev` | `OnlineFxEvent[8]` | Últimos efectos y sonidos (número de secuencia, quién, nombre) |
| `fx_props` | `u32[24]` | Propiedades de efectos de su Link |
| `weather` | `u8` | Clima donde está (0..8; 255 desconocido) |
| `dye` | `i8[3]` | Tinte de armadura |
| `pvp_seq`, `pvp_target`, `pvp_damage` | `u16`, `u8`, `i16` | Su último golpe a otro jugador: número, ranura de la víctima, daño |
| `horse`, `horse_m`, `horse_as` | `char[24]`, `float[12]`, `char[32]` | Montura |
| `tack`, `tack_m` | `char[2][24]`, `float[2][12]` | Silla y riendas |
| `ground_props` | `u32[16]` | Propiedades del suelo para efectos |
| `collide`, `revive`, `nopause` | `u8` ×3 | **Sus** ajustes de reglas; los del jugador de la ranura 0 son los de la sesión |
| `bloody_moon` | `float` | Su temporizador de Luna Carmesí (-1 desconocido) |
| `aggro_id`, `aggro_seq` | `u32`, `u16` | Último enemigo del anfitrión que golpeó |
| `objs` | `OnlineObj[8]` | Objetos del mundo moviéndose cerca (HashId, matriz) |
| `broken` | `u32[8]` | HashId de objetos que rompió hace poco |
| `trunks_gone` | `u32[4]` | Árboles cuyo tronco troceó |
| `movers` | `OnlineMover[6]` | Mecanismos moviéndose cerca (HashId, matriz, animación) |
| `objs_trunk` | `u8` | Bits: cuáles de `objs` son troncos |
| `held` | `OnlineHeld[5]` | Materiales que sostiene o acaba de lanzar |

Lo importante para el diseño: **las reglas de la sesión no son un mensaje**; viajan dentro del STATE y cada cliente
adopta las del jugador de la ranura 0 (`OA:2838-2840`). Y el golpe PvP tampoco: es un contador dentro del STATE.

### `OnlineEnemy` (`online.h`, bloque "The host's enemies") — 311 bytes

`u32 id; char name[40]; float m[12]; char as[32]; char weapon[40]; float wm[12]; i32 life; char shield[40];
float sm[12]; u8 wbone, sbone; u8 owner; u32 hash`. `owner` es la ranura del jugador cuyo juego lo simula; `hash` es
su HashId del mapa (0 si no tiene).

### `OnlineLoot` — 97 bytes

`u32 gid, lid; u8 origin; char name[40]; float pos[3]; float rot[9]`. `gid` lo asigna el anfitrión; `origin` es la
ranura de donde vino (el receptor la sobrescribe con la ranura que antepuso el servidor, `online.c:1058`).

### SKEL

`u8 n` (huesos de Link), `u8 nbow` (huesos del arco), y por hueso 12 bytes: rotación en 6 (cuaternión, "los tres
menores") y traslación en 3 × `s16` (1/8192 m). Máximo 160 huesos (`online.c:53-56`, `online.h` `ONLINE_BONES`).

### QUEST / QUESTS

4096 bytes = 32 768 bits; el bit *k* corresponde a la línea *k* de `online_mod/quests.txt`, que genera
`tools/online_mod.py` en la máquina de cada jugador. **El significado de cada bit depende de que los dos jugadores
tengan el mismo `quests.txt`**, y eso hoy no se comprueba.

## 1.7 Quién decide y quién solo retransmite

- **Decide el servidor**: ranura, rechazo, nombre saneado, JOIN/LEAVE, expulsión por silencio, truncado del chat y la
  hora (TIME), esta última solo cuando corre dentro del juego del anfitrión.
- **Retransmite sin mirar**: todo lo demás. No valida contenido, frecuencia ni emisor (`mapa/06` §2.4).
- Con el servidor dedicado (`--server`) nadie tiene `g.hosting`: no hay ENEMIES, LOOTS, QUESTS ni TIME. Solo
  funcionan posiciones, poses, equipo y chat (`mapa/06` §1.1).

## 1.8 Interpolación en el receptor

- La marca de tiempo de cada instantánea es **la hora local de recepción**, no la del emisor (`online.c:910`).
- Jugadores: anillo de 8 instantáneas; se dibuja 100 ms por detrás (`INTERP_NS`), mezclando las dos que rodean ese
  instante (`online.c:1387-1432`). Esqueleto: anillo de 4. Enemigos: las dos últimas listas.
- Consecuencia: tras una pérdida, TCP entrega una ráfaga con horas casi iguales y el movimiento salta
  (`mapa/06` §3).

## 1.9 Consumo (calculado, no medido)

Cada jugador sube ~68 kB/s de STATE más ~30 kB/s de SKEL: unos **100 kB/s**. El anfitrión añade ENEMIES (hasta
~100 kB/s en el peor caso), LOOTS y QUESTS. El coste crece con el cuadrado del número de jugadores porque el
servidor reenvía cada flujo a todos los demás (`mapa/06` §2.5).

---

# Parte 2 — Cambios previstos

> Nada de esta parte existe todavía. Aquí solo se fija **qué** debe cambiar y **en qué fase**. Los formatos de
> campo, tamaños y números de mensaje nuevos **no están decididos**: donde dice "por definir en F3" (o la fase que
> toque), lo define la tarea dueña del protocolo en el `plan.md` de esa fase y lo escribe aquí antes de
> implementarlo.

## 2.1 F1 — Arreglos rápidos (sin cambiar el formato de cable)

Ninguno de estos cambia tamaños ni campos; la versión sigue siendo 38 salvo que la tarea descubra lo contrario.

| Cambio | Motivo | Formato |
|---|---|---|
| Rechazar NaN e infinito (y valores fuera de rango) al recibir | `mapa/06` H-6; `mapa/07` M4 | Sin cambio |
| Terminar `shield` al recibir ESTATE | `online.c:1095` (H-7) | Sin cambio |
| Lista de permitidos para nombres de actor recibidos | `OA:3604`, `OA:2226` (A4) | Sin cambio |
| Limpiar todo el estado de sesión en `stop_all` | `online.c:1322-1335` (T-1) | Sin cambio |
| Proteger los envíos frente a SIGPIPE | `online.c:143` (H-8) | Sin cambio |

## 2.2 F2 — Cimientos

| Cambio | Motivo | Formato |
|---|---|---|
| Una sola definición de las estructuras de cable, compartida con `tools/online_bot.c`, con comprobación de tamaño en compilación | El estado está escrito cuatro veces más la copia del bot (`mapa/06` §8.1) | Sin cambio de bytes previsto |
| Pruebas automáticas de trama, codificación y máquina de estados, sin el juego | No existe ninguna (`mapa/06` §7) | Sin cambio |
| Validación única de entradas | `contratos/validacion-red.md` | Sin cambio |

## 2.3 F3 — Red robusta (aquí sí cambia el protocolo; la versión sube)

| Cambio | Qué debe conseguir | Formato |
|---|---|---|
| **Comprobación estricta de versión** | El saludo compara, además del protocolo, la **compilación** del runtime y la **versión del juego**; un desajuste se rechaza con un motivo propio y un texto claro para el jugador | Campos nuevos del saludo: **por definir en F3**. Si entran también `GAMELIB_ID`, la versión del overlay y una huella de `quests.txt`: **por definir en F3** |
| **El anfitrión valida la autoridad** | El servidor sabe qué conexión es la del anfitrión; acepta ENEMIES, LOOTS, QUESTS y TIME solo de él; entrega HIT, ESTATE, LOOT_ADD, LOOT_TAKE y QUEST solo a quien corresponde; el anfitrión tiene ranura reservada | Cómo se identifica al anfitrión y si cambian los relés crudos para llevar origen: **por definir en F3** |
| **Envío no bloqueante por cliente** | Un cliente lento no congela a los demás; el estado viejo se descarta en vez de acumularse | Interno al servidor; sin cambio de formato previsto |
| **Límite de mensajes** | Tope de frecuencia y de tamaño por tipo y por cliente; plazo para el HELLO | Sin cambio de formato previsto; umbrales **por definir en F3** |
| **Contraseña** | Límite de intentos; no se guarda ni se envía en claro; oculta en pantalla | Mecanismo (por ejemplo desafío-respuesta): **por definir en F3** |
| **Reconexión** (CU-05) | Tras una caída, el jugador vuelve a su misma ranura sin que la sesión se pierda | Identificador de sesión, mensajes y plazos: **por definir en F3** |
| **Marca de tiempo del emisor** | La interpolación deja de depender de la hora de llegada | Campo y unidad: **por definir en F3** |
| **Descubrimiento en red local** (CU-04) | La partida aparece sola en la misma red | Transporte y formato del anuncio: **por definir en F3** |
| **Código de invitación** (CU-04) | Unirse sin dictar una IP | Codificación: **por definir en F3** |
| **Apertura automática de puerto** (CU-04) | UPnP donde el router lo permita | No es un mensaje del protocolo |
| **IPv6** | Mostrar y aceptar direcciones IPv6 de forma completa | Sin cambio de formato (el transporte ya es de doble pila, `online.c:318-333`) |
| **Posible paso a ENet/UDP** | Ver [ADR-005](../decisiones/ADR-005-tcp-vs-udp.md): se decide con medidas | Si se adopta, la trama de la sección 1.2 se sustituye; reparto de mensajes por canal: **por definir en F3** |

## 2.4 F4 — Funciones cooperativas

Cada una necesitará mensajes o campos nuevos. Ninguno está diseñado.

| Función | Caso de uso | Formato |
|---|---|---|
| Flags del mundo: manda el anfitrión, no la unión | CU-06 | Por definir en F4 |
| Recompensas personales separadas del progreso del mundo | CU-07 | Por definir en F4 |
| Teletransporte al compañero | CU-08 | Por definir en F4 |
| Dar un objeto directamente | CU-09 | Por definir en F4 |
| Marcadores (pings de mapa) y color del nombre | CU-10 | Por definir en F4 |
| Estado "caído" y revivir al compañero | CU-11 | Por definir en F4 |
| PvP por acuerdo, con daño validado | CU-12 | Por definir en F4 |

## 2.5 Reglas para cualquier cambio

1. Lo edita solo el dueño del protocolo, en un único cambio que toca a la vez `runtime/online.h`, `runtime/online.c`,
   `tools/online_bot.c` y este documento.
2. Si cambia un tamaño, el orden o el significado de un campo, `PROTOCOL` sube.
3. Todo campo nuevo trae su fila en `contratos/validacion-red.md` (qué se comprueba al recibirlo) y su fila en
   `contratos/autoridad-mundo.md` (quién manda sobre ese dato).
4. Nunca direcciones de memoria, desplazamientos ni rutas de archivo en un mensaje (constitución, regla 10).
5. La Parte 1 se actualiza en el mismo cambio: debe describir siempre lo que hay en el código.

---

# Parte 3 — Reglas comprobables (para quien ejecuta una tarea)

Este contrato es autosuficiente: no necesitas haber leído los mapas. Si tu tarea toca el protocolo, cumple **todas**
estas líneas. "DEBE" y "NO DEBE" son obligatorios.

## 3.1 Reglas

- **P-1.** Solo la tarea dueña del protocolo DEBE editar `runtime/online.h`, las estructuras `WireState`,
  `WireHello`, `WireEquip` (`runtime/online.c:59-108`), el `enum` de tipos de mensaje (`runtime/online.c:50`) y este
  documento. Cualquier otra tarea NO DEBE tocarlos: se detiene y lo pide.
- **P-2.** Si cambias el tamaño, el orden o el significado de un campo de cualquier estructura de cable, DEBES subir
  `PROTOCOL` (`runtime/online.c:40`) en el mismo cambio.
- **P-3.** En el mismo cambio DEBES actualizar la copia del bot (`tools/online_bot.c:31-78` y la versión en
  `tools/online_bot.c:102`). Comprobación: `grep -n "PROTOCOL\|version" runtime/online.c tools/online_bot.c` muestra
  el mismo número en los dos.
- **P-4.** Un campo nuevo de `WireState` DEBE añadirse en los cuatro sitios donde hoy está escrito el estado:
  `OnlinePose` (`runtime/online.h`), `WireState` (`runtime/online.c:60-104`), la copia de salida en `sample_local`
  (`runtime/online.c:845-883`) y la copia de entrada en `remote_snap` (`runtime/online.c:907-964`). Olvidar uno
  compila sin error y el campo llega a cero.
- **P-5.** Todo campo de texto recibido DEBE terminarse en cero al recibir, en `remote_snap` o en `cl_message`
  (`runtime/online.c:967-1124`), antes de que nadie lo lea.
- **P-6.** Un mensaje NO DEBE superar 16 384 bytes de carga: `send_msg` lo descarta sin avisar
  (`runtime/online.c:150-158`).
- **P-7.** Un mensaje NO DEBE contener direcciones de memoria, desplazamientos dentro de estructuras del juego, ni
  rutas de archivo. Los objetos se nombran por ranura, id, HashId o nombre de actor.
- **P-8.** NO DEBES asignar un número de tipo ya usado (1 a 21) ni reutilizar uno retirado. Los tipos nuevos van al
  final del `enum`.
- **P-9.** NO DEBES inventar el formato de nada marcado "por definir": lo define la tarea dueña en el `plan.md` de su
  fase y lo escribe en la Parte 2 **antes** de implementarlo.
- **P-10.** Si la Parte 1 deja de coincidir con el código tras tu cambio, DEBES corregir la Parte 1 en ese mismo
  cambio.
- **P-11.** Cada mensaje o campo nuevo DEBE traer su fila en `contratos/validacion-red.md` y en
  `contratos/autoridad-mundo.md`.

## 3.2 Errores típicos que no debes cometer

1. **Añadir un campo a `WireState` sin subir `PROTOCOL`.** Dos juegos con estructuras distintas se aceptan entre sí
   y leen basura.
2. **Añadir el campo en `sample_local` pero no en `remote_snap`** (o al revés). Así nació el fallo de `shield` sin
   terminar en ESTATE (`runtime/online.c:1095` frente a `runtime/online.c:1052`).
3. **Suponer que un relé crudo dice quién lo envió.** ENEMIES, HIT, LOOTS, QUESTS y PICK llegan **sin** ranura de
   origen; no puedes saber de quién son.
4. **Suponer que el servidor filtra por anfitrión.** No lo hace: reenvía a todos y cada cliente descarta con su
   `g.hosting` local.
5. **Tratar la ranura 0 como "el anfitrión" en código nuevo.** Hoy es una suposición sin garantía.
6. **Leer `len` como longitud exacta.** Casi todas las comprobaciones son `len >= mínimo`; un mensaje puede traer
   bytes de más.
7. **Usar la hora de llegada como hora del suceso.** La marca de tiempo es local al receptor
   (`runtime/online.c:910`); no sirve para ordenar sucesos entre máquinas.
8. **Confiar en que los dos jugadores tienen el mismo `quests.txt`.** El bit *k* de QUEST solo significa lo mismo si
   el archivo generado es idéntico, y hoy nadie lo comprueba.
9. **Dar por medidos los tamaños de este documento.** Están sumados a mano; antes de depender de uno, añade una
   comprobación de tamaño en compilación.
10. **Escribir aquí un formato "previsto" con bytes concretos sin que exista en el código.**
