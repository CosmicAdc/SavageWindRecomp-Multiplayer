# 04 · Redes para juegos, desde cero

Objetivo: el vocabulario y los compromisos de la red en juegos, cada uno atado a cómo lo hace hoy el PR #294
(`runtime/online.c`) y a qué cambia el plan. Si vienes de APIs HTTP, la diferencia principal es esta: aquí el
dato **caduca en milisegundos** y llegar tarde es peor que no llegar.

Referencia exhaustiva: `../mapa/06-pr294-red-y-sesion.md`.

---

## 1. Topologías: quién habla con quién

| Modelo | Cómo es | Pros | Contras |
|---|---|---|---|
| **Cliente/servidor dedicado** | Un proceso sin juego manda; todos se conectan a él | Justo, difícil de trampear | Hay que alojarlo |
| **Par a par (P2P)** | Todos con todos | Sin servidor | N² conexiones; NAT difícil; nadie arbitra |
| **Anfitrión (*listen server*)** | El juego de un jugador hace además de servidor | Cero infraestructura | El anfitrión tiene ventaja y, si se va, se acaba |

**Hoy.** Es una estrella con anfitrión, con un giro: `online_host()` abre un hilo servidor y luego conecta su
propio cliente a `127.0.0.1` (`online.c:1347-1361`, `1165`). El servidor es casi un **relé tonto**: reenvía lo
que recibe (`srv_message`, `online.c:262-311`). Existe `--server` para lanzarlo sin juego
(`online.c:412-423`), pero entonces nadie hace de anfitrión del mundo y enemigos, botín y misiones quedan
inertes (mapa 06 §1.1).

**El plan.** Se queda con el modelo de anfitrión y conexión directa. Lo que cambia es que el servidor deja de
ser tonto (§3).

## 2. Los hilos de la red

| Hilo | Función | Ritmo |
|---|---|---|
| servidor | `srv_loop` (`online.c:343-401`): `select()` de 100 ms sobre el socket de escucha y hasta 8 clientes | por evento |
| cliente | `cl_thread` (`online.c:1160-1320`): `select()` de 10 ms, recibe, y vacía buzones de salida | 20 Hz estado, 1 Hz ping |
| juego | `rt_online_frame` (`online_actors.c:5721`): produce y consume buzones | cada fotograma |

`select()` es la primitiva clásica de "avísame cuando alguno de estos sockets tenga datos, o cuando pase este
tiempo". En Go no la ves porque el runtime la esconde detrás de las goroutines.

## 3. Autoridad: quién decide la verdad

**Autoridad** = qué máquina tiene la última palabra sobre un dato. Si dos juegos discrepan sobre si un cofre
está abierto, alguien tiene que ganar.

Hoy la autoridad está repartida y **la decide cada cliente** con su variable local `g.hosting`, no el servidor:

| Dato | Quién manda hoy | Dónde |
|---|---|---|
| Posición y pose de cada Link | su propio juego | `online.c:1279-1284` |
| Lista de enemigos, botín | el juego del anfitrión | `online.c:1265-1277`, `1241-1245` |
| Hora del día | el anfitrión | `online.c:373-379` |
| Misiones y flags | **la unión de todos**; el anfitrión hace el OR | `online_actors.c:3926-3932` |
| Daño PvP | el atacante lo declara y la víctima lo aplica | mapa 07 §2.2 |

El servidor **no comprueba** que un mensaje "de anfitrión" (`ENEMIES`, `LOOTS`, `QUESTS`) venga del anfitrión:
lo reenvía tal cual (`online.c:278-280`, `294-298`). Cada receptor decide si lo acepta mirando su propio
`g.hosting` (`online.c:1043`, `1072`, `1082`). Entre amigos funciona; ante un cliente defectuoso o malicioso,
no (hallazgo H-2).

**El plan.** CU-06: el mundo y el progreso son los del anfitrión, no la unión. F3: el anfitrión valida quién
puede enviar enemigos, botín y misiones. El contrato vivirá en `../contratos/autoridad-mundo.md`.

Regla mental: **nunca confíes en el cliente** para nada que afecte a otro jugador. Un flag de misión enviado por
un invitado acaba hoy en el guardado de todos, de forma irreversible.

## 4. TCP frente a UDP

| | TCP | UDP |
|---|---|---|
| Entrega | garantizada y en orden | ni garantizada ni ordenada |
| Unidad | flujo de bytes (tú pones los límites) | datagramas sueltos |
| Si se pierde un paquete | se retransmite y **todo lo posterior espera** | lo pierdes y sigues |
| Trabajo para ti | poco | reconstruir lo que necesites |

**Bloqueo de cabeza de línea** (*head-of-line blocking*): TCP entrega en orden. Si se pierde el paquete 5, los
paquetes 6, 7 y 8 ya llegaron pero la aplicación no los ve hasta que el 5 se retransmite. Para una posición a
20 Hz eso es absurdo: cuando por fin llega el estado viejo, ya no sirve, y llegan cuatro de golpe.

**Hoy.** Todo va por **un** flujo TCP: estado, esqueleto, chat, eventos (`online.c:3-7`). Detalles correctos:
`TCP_NODELAY` activado (`online.c:124-127`) para que el sistema no agrupe mensajes pequeños (algoritmo de
Nagle). Detalle que agrava la pérdida: cada instantánea se sella con la hora **local de recepción**
(`online.c:910`), así que una ráfaga tras una retransmisión produce instantáneas casi simultáneas y el
interpolador da un salto.

**Como TCP es un flujo**, hace falta **enmarcar** (*framing*): `u16 longitud`, `u8 tipo`, carga
(`online.c:150-158`). El receptor acumula bytes y corta mensajes completos (`next_msg`, `online.c:172-180`).
Léela: es la función más importante de la capa y tiene nueve líneas.

**El plan.** F3 documenta en un ADR si migrar de TCP a UDP con ENet, tras medir con latencia y pérdida
simuladas. Para 2 jugadores TCP es defendible como arranque; lo imprescindible es dejar de bloquear (§7).

## 5. Ticks, instantáneas, interpolación y extrapolación

- **Tick**: un paso de envío. Aquí `SEND_HZ 20` (`online.c:44`): una **instantánea** (*snapshot*) del estado de
  tu Link cada 50 ms.
- El juego dibuja a 30-120 fotogramas por segundo. Entre dos instantáneas hay que inventar posiciones.
- **Interpolación**: dibujar al otro jugador **en el pasado**, entre dos instantáneas que ya tienes. Suave y
  siempre correcto, a cambio de un retraso fijo.
- **Extrapolación** (*dead reckoning*): suponer que sigue como iba. Sin retraso, pero se equivoca cuando gira o
  se para, y hay que corregir a saltos.

**Hoy: interpolación pura, 100 ms por detrás.**

- `INTERP_NS 100000000` (`online.c:45`).
- Cada jugador remoto tiene un anillo de 8 instantáneas (`SNAPS`, `online.c:426-427`).
- `online_players()` busca las dos instantáneas que rodean "ahora menos 100 ms" y mezcla linealmente
  (`online.c:1406-1418`). Fíjate en tres decisiones:
  - si la distancia entre ambas supera 30 m, no interpola: es un teletransporte (`:1415`);
  - el ángulo se interpola por el camino corto con `remainderf` (`:1417`);
  - la pose "discreta" (qué animación, qué lleva) se toma de la más nueva, sin mezclar (`:1421`).
- El esqueleto tiene su propio anillo de 4 y mezcla rotaciones con **nlerp** (interpolación lineal de
  cuaterniones, normalizada después): `online_skeleton`, `online.c:775-808`. El signo `s` de la línea 796 evita
  ir "por el lado largo" de la rotación.
- No hay extrapolación: si no hay instantánea posterior, se usa la última.
- Un jugador sin noticias 3 s se marca "cargando" y deja de dibujarse (`STALE_NS`, `online.c:47`,
  `1402-1405`).

¿Por qué 100 ms y no 50? Con envíos cada 50 ms, 100 ms de colchón aguantan la pérdida o el retraso de una
instantánea sin quedarse sin datos. Es el compromiso clásico (lo explican Gambetta y Valve, `recursos.md`).

## 6. Compresión: cuánto se envía

Sin compresión ni deltas, se manda el estado entero aunque nada cambie: `WireState` son 3375 bytes, 20 veces
por segundo, más el esqueleto. El mapa 06 §2.5 calcula ~100 kB/s de subida por jugador.

El esqueleto sí está comprimido, y es un buen ejemplo de técnica estándar (`online.c:53-54`, `531-554`):

- **"Los tres menores" de un cuaternión.** Una rotación son 4 números con módulo 1, así que uno se puede
  recalcular. Se envía cuál es el mayor (2 bits) y los otros tres en 15 bits cada uno: 6 bytes en vez de 16.
- **Punto fijo.** Cada traslación va como entero de 16 bits en unidades de 1/8192 m, en vez de `float`.

`skel_decode` (`online.c:556-569`) deshace ambas cosas. Es código puro, sin juego: candidato perfecto a prueba
unitaria en F2 (ejercicio E12).

**El plan.** Reducir tráfico es necesario para más de 2 jugadores (F5): el servidor reenvía cada flujo a N-1
jugadores y el ancho de banda crece con el cuadrado.

## 7. Envío bloqueante y contrapresión

`send_all` (`online.c:140-148`) llama a `send()` en bucle hasta enviar todo, sobre un socket **bloqueante**.
Si el receptor deja de leer (se le cuelga el juego, pierde la wifi sin cerrar), su ventana TCP se llena y
`send()` no vuelve.

En el servidor ese hilo atiende a todos (`srv_broadcast`, `online.c:213-216`). Consecuencia: **un jugador lento
congela la sesión entera**, y la expulsión por 10 s de silencio (`online.c:384`) no puede actuar porque el hilo
está atrapado en `send`. Hallazgo H-1, el más grave de la capa.

**Contrapresión** (*backpressure*) es qué haces cuando produces más rápido de lo que el otro consume. En Go lo
resuelves con un canal con búfer y un `select` con `default`. Aquí toca hacerlo a mano.

**El plan (F3).** Sockets no bloqueantes, una cola de salida por cliente y, cuando se llena, **descartar el
estado viejo** (el siguiente lo sustituye) y **conservar los eventos**. Modelos ya existentes en el repo: los
sockets de `runtime/control.c` y `runtime/dsu.c`.

## 8. Fiabilidad: no todo merece el mismo trato

| Tipo de dato | Ejemplo | Si se pierde uno | Trato ideal |
|---|---|---|---|
| Estado continuo | posición, huesos | da igual, viene otro | no fiable, solo el último |
| Evento | golpe, objeto recogido, chat | **se pierde para siempre** | fiable y ordenado |
| Estado lento | misiones, lista de botín | se corrige al reenviar | reenvío periódico |

**Hoy** TCP da fiabilidad a todo, la necesite o no. Aun así el PR ya distingue por diseño:

- eventos en **colas** acotadas: `HIT` (32), `PICK` (8), `LOOT_ADD` (16) (`online.c:603-615`, `690-698`). Si la
  cola está llena, el evento se descarta en silencio;
- estado lento con **reenvío**: botín al cambiar y cada 2 s (`online.c:1241-1245`), equipo al cambiar y cada 5 s
  (`:1296-1307`). Así quien entra tarde se pone al día;
- eventos incrustados en el estado con **número de secuencia** que solo crece (`pvp_seq`, `fxev[].seq`,
  `online.h:65`, `105`): el receptor ignora los que ya vio. Es un truco para que un evento sobreviva a la
  pérdida de instantáneas sin canal fiable.

Una biblioteca como **ENet** (UDP con canales fiables y no fiables, licencia MIT) te da esto hecho. Es la
candidata del ADR de F3.

**Versionado.** `PROTOCOL 38` y un número mágico (`online.c:39-40`); el servidor exige igualdad exacta
(`online.c:240`). Como el formato de cable es la estructura C copiada con `memcpy` (`online.c:59-108`),
cualquier campo nuevo rompe la compatibilidad. El plan añade una comprobación estricta que incluye compilación
y versión del juego.

## 9. Propiedad de entidades

Cuando una entidad (un enemigo) existe en dos juegos, uno debe **simularla** y el otro **mostrarla**. Eso es
*entity ownership*.

**Hoy.** Cada enemigo tiene un **propietario** (campo `owner`, `online.h:221`): su juego ejecuta la IA de
verdad y envía su estado; los demás borran el suyo y muestran una marioneta. El anfitrión lleva el registro y
reparte la propiedad con histéresis para que dos jugadores no se lo pasen sin parar (mapa 07 §2.2,
"Enemigos"). Un golpe a una marioneta viaja como `HIT` hacia el dueño (`online.c:281-283`).

Es la parte más delicada del PR y la que más bugs explica. Idea para llevarte: **propiedad con histéresis y
traspaso explícito**, nunca "el que esté más cerca en este fotograma".

## 10. Compensación de lag, y por qué no lockstep ni rollback

- **Predicción en el cliente**: tu propio personaje responde al instante y el servidor corrige. Aquí no hace
  falta: cada jugador simula a su Link en su propio juego.
- **Compensación de lag**: el servidor rebobina el mundo al instante en que el atacante vio el golpe. Aquí no
  existe; el PvP es "el atacante declara" (§3).
- **Lockstep**: todos ejecutan la **misma** simulación con las **mismas** entradas, tick a tick. Solo se envían
  los mandos. Exige determinismo perfecto.
- **Rollback**: como lockstep, pero se avanza prediciendo y, si la entrada real difiere, se rebobina y se
  re-simula. Exige guardar y restaurar el estado completo en microsegundos.

**Por qué no encajan en este juego:**

1. **No es determinista entre máquinas.** Hilos del juego, tiempos de carga, y paso de tiempo variable (el
   runtime mete la duración real del fotograma en el motor: `tools/patches.py:410-413`). Dos PCs divergen en
   segundos.
2. **El estado es enorme y no se puede copiar.** Un mundo abierto con gigabytes de memoria, física y streaming
   de mapa. No hay "guardar estado" barato, y no controlamos el código para añadirlo.
3. **Cada jugador tiene su propio mundo cargado.** Dos jugadores a un kilómetro tienen cargados trozos
   distintos del mapa.

Por eso el PR hace lo único viable: **replicación de estado** (cada juego simula lo suyo y envía resultados) con
marionetas. Los proyectos de referencia para juegos de mundo y un jugador hacen lo mismo (sm64coopdx, Anchor;
`recursos.md`).

## 11. NAT, puertos y cómo encontrarse

- **NAT**: tu router comparte una IP pública entre todos tus dispositivos. Desde fuera no se puede iniciar una
  conexión hacia tu PC salvo que el router sepa a quién entregarla.
- **Redirección de puertos** (*port forwarding*): le dices al router "lo que llegue al puerto 52520, a este PC".
- **UPnP / NAT-PMP / PCP**: protocolos para que un programa pida esa redirección él solo. A menudo desactivados.
- **Perforación** (*hole punching*): los dos lados envían a la vez para que cada router crea que la conexión es
  de salida. Necesita UDP y un tercero que presente a las partes (STUN). Falla con NAT estrictos.
- **Relé**: si todo falla, un servidor intermedio reenvía. Cuesta dinero.
- **Red virtual** (Tailscale, ZeroTier): hacen todo lo anterior por ti y os ponen "en la misma LAN".

**Hoy.** Nada automático. Puerto 52520/TCP (`online.h:18`), escucha en todas las interfaces, IPv6 con doble
pila si se puede (`online.c:318-333`). La interfaz le pide al usuario redirigir el puerto o usar una red
virtual (`runtime/ui/online_ui.mm:57-59`). No hay descubrimiento en LAN ni reconexión: tras un fallo el estado
es `ONLINE_FAILED` hasta que pulsas Join (`online.c:1198-1200`).

**El plan (F3, CU-04 y CU-05).** Descubrimiento en red local, código de invitación, apertura con UPnP, IPv6 y
reconexión. Perforación y relé quedan fuera: con TCP la perforación es poco fiable, y es un argumento más a
favor de UDP en el ADR.

## 12. Seguridad de aceptar conexiones entrantes

Abrir un puerto a Internet convierte tu juego en un servidor. Lo que entra es **entrada no confiable**.

**Lo que el PR hace bien** (mapa 06 §4.1): comprueba longitudes al enmarcar; todo índice de ranura se valida
`< ONLINE_MAX` (`online.c:972`, `985`, `1002`); termina en cero las cadenas recibidas (`online.c:913-959`);
recorta contadores (`:918`, `:924`, `:1045`, `:1084`); ningún mensaje lleva direcciones de memoria.

**Lo que falta**, por orden de importancia:

| Problema | Dónde | Fase |
|---|---|---|
| Un par lento congela a todos | `online.c:140-148` | F3 |
| Sin autoridad ni validación semántica en el servidor | `online.c:278-298` | F3 |
| Números sin validar: NaN, infinito, daño negativo | el emisor filtra (`online.c:832-833`), el receptor no (`:907-964`) | F1 |
| Nombres de actor que llegan por red y se usan para **crear actores** sin lista de permitidos | `online_actors.c:3604`, `2226` | F1 |
| Contraseña en claro en el HELLO, comparada con `strcmp`, sin límite de intentos | `online.c:105`, `241` | F3 |
| La ranura se ocupa al aceptar, antes de autenticarse; sin límite por IP ni de tasa | `online.c:359-370` | F3 |
| `SIGPIPE` en Linux puede matar el proceso | `online.c:143` | F1 |
| `ESTATE` no termina el campo `shield` | `online.c:1095` frente a `:1052` | F1 |
| Estado de sesión que sobrevive al salir | `stop_all`, `online.c:1322-1335` | F1 |

Principios que debes interiorizar:

1. **Valida en el borde, una vez, en un solo sitio.** Longitud, rangos, finitud (`isfinite`), cadenas
   terminadas, nombres contra una lista. El contrato irá en `../contratos/validacion-red.md`.
2. **Un dato de red nunca elige una dirección de memoria ni un nombre de recurso sin filtro.**
3. **Limita todo:** tamaño, tasa, intentos, tiempo hasta el primer mensaje.
4. **El cliente tampoco debe fiarse del servidor.** Quien se une a un anfitrión malicioso sufre lo mismo
   (H-10).
5. El socket de **control** (`SAVAGEWIND_CONTROL`) escucha solo en `127.0.0.1` y rechaza peticiones HTTP
   (`docs/CONTROL.md:19-20`): buen ejemplo de diseño defensivo dentro del mismo repo.

## 13. Autoevaluación

1. ¿Por qué el otro Link se ve 100 ms en el pasado?
2. Se pierde un paquete TCP con una posición. ¿Qué ve el otro jugador?
3. ¿Quién decide hoy si un mensaje `QUESTS` es legítimo?
4. ¿Qué dos requisitos de rollback no cumple este juego?
5. ¿Qué diferencia de trato deberían tener un `STATE` y un `HIT` en una cola de salida llena?

Respuestas: (1) interpolación entre dos instantáneas ya recibidas. (2) Un parón y luego un salto: todo lo
posterior espera a la retransmisión y llega junto. (3) Cada cliente, con su `g.hosting`; el servidor no.
(4) Determinismo y guardar/restaurar estado barato. (5) El `STATE` viejo se descarta; el `HIT` se conserva.

Siguiente: [05 · Anatomía del modo online](05-anatomia-del-modo-online.md).
