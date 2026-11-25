# Contrato: quién manda sobre cada parte del mundo

**Autoridad** = quién tiene la última palabra sobre un dato cuando los dos juegos no coinciden. Este documento dice,
para cada tipo de estado: quién manda **hoy** en el PR #294 (con la prueba en el código), quién **debe** mandar según
nuestras decisiones, y qué falta para llegar ahí.

Es autosuficiente: no hace falta haber leído ningún otro documento para aplicarlo.

Abreviaturas: `OA:n` = `runtime/online_actors.c` línea n; `online.c:n` = `runtime/online.c`; `MOD:n` =
`tools/online_mod.py`; `DOC:n` = `docs/ONLINE.md`. Todas las líneas son del checkout del PR (commit `350c5f8`).
Nada se ha ejecutado: lo que no se pudo comprobar leyendo va marcado `[sin verificar]`.

---

## 1. Las decisiones de las que parte todo

Tomadas en el plan aprobado (ver `decisiones/ADR-002-mundo-del-anfitrion.md`):

- **D1. El mundo y el progreso son los del anfitrión.** El anfitrión es el jugador que crea la sesión. Su partida
  guardada es la que cuenta. Lo que ocurre en la sesión avanza **su** mundo.
- **D2. Cada jugador conserva lo suyo:** inventario, corazones y resistencia. Nada de eso se copia de un jugador a
  otro ni se pierde por unirse a una partida.
- **D3. Cooperativo por defecto; PvP (daño entre jugadores) solo si los dos lo aceptan.**

Dos palabras que se usan en la tabla:

- **Del mundo (anfitrión):** hay una sola verdad, la del anfitrión. El cliente la recibe y la muestra. El cliente
  puede **pedir** un cambio; el anfitrión decide.
- **Del jugador:** cada juego es dueño de lo suyo y solo **informa** al otro para que lo dibuje.

## 2. Cómo funciona hoy, en cinco frases

1. Cada jugador ejecuta **su propio juego completo con su propia partida guardada**; el otro aparece como actores
   añadidos (`OA:1-2`, `DOC:19-22`).
2. El servidor es un repetidor: reenvía casi todo sin mirar y **no sabe quién es el anfitrión**
   (`online.c:232-311`). Quién manda lo decide cada cliente con su variable local `g.hosting`.
3. El "anfitrión" se **supone** en la ranura 0 (`OA:2838-2840`, `OA:991`).
4. El progreso (misiones, santuarios, cofres…) **no es el del anfitrión: es la unión de lo que haya hecho
   cualquiera**, y se escribe en la partida guardada de todos (`OA:3851-3997`).
5. Varios de esos avances **regalan objetos** a quien no los tenía: orbes, contenedores de corazón, runas, semillas
   Kolog, paravela (`OA:3987-3997`).

Los puntos 4 y 5 son la mayor distancia entre lo que hay y lo que hemos decidido.

---

## 3. Tabla de autoridad

Columna "Brecha": **Ninguna** = ya cumple; **Validar** = el dueño es correcto pero falta comprobar lo que llega;
**Cambiar** = el dueño de hoy no es el decidido; **Por decidir** = nuestras decisiones no bastan para fijarlo y lo
resuelve la fase indicada.

### 3.1 El jugador

| Estado | Quién manda HOY (prueba) | Quién DEBE mandar | Brecha |
|---|---|---|---|
| **Posición, rumbo y animación** | Cada jugador sobre sí mismo. Viaja en STATE a 20 Hz; el receptor lo coloca con `FN_SET_MTX` sin comprobarlo (`OA:1493-1841`, `OA:6054-6056`) | Del jugador | **Validar**: números finitos y dentro del mapa (F1) |
| **Esqueleto (121 huesos)** | Cada jugador. Mensaje SKEL a 20 Hz; se escribe hueso a hueso en la marioneta (`OA:1871-1893`, `OA:1901-1942`) | Del jugador | **Validar**: el decodificador acota por construcción `[sin verificar]`; comprobar número de huesos contra el modelo local (F2) |
| **Equipo visible** (armadura, arma, escudo, arco) | Cada jugador. Mensaje EQUIP con nombres de actor; el receptor los traduce con `armor.txt` / `gear.txt` del overlay (`OA:728-736`, `OA:6063-6094`, `OA:6275-6317`) | Del jugador | **Validar**: que un nombre que no esté en esas tablas se descarte `[sin verificar que hoy sea así]` (F1) |
| **Inventario** (bolsa) | Cada jugador: ningún mensaje lleva inventario. **Pero** la unión de flags mete objetos en la bolsa de los demás (`give_item`, `OA:3751-3759`, `OA:3987-3997`) | Del jugador (D2) | **Cambiar**: quitar los regalos automáticos; ver 3.2 (F4, CU-07) |
| **Corazones** (vida actual) | Cada jugador. Su vida está en `jugador + 0x848` y solo la escribe su propio juego: golpes de enemigos ajenos (`strike`, `OA:2696-2727`) y PvP (`OA:2907-2928`) los aplica la víctima | Del jugador (D2) | **Ninguna** en la propiedad. El daño que llega necesita validación (ver PvP) |
| **Contenedores de corazón** (vida máxima) | Cada jugador, **salvo** que al llegar el flag `Get_HeartUtuwa_*` de otro jugador se le da uno (`give_heart`, `OA:3997`). El intercambio en la estatua de la diosa es de cada uno (`DOC:481`) | Del jugador (D2) | **Cambiar**: no se regala por lo que hizo otro (F4, CU-07) |
| **Resistencia** | Cada jugador. Ningún código del modo en línea la toca (`mapa/07` §7: "nada trata la resistencia") | Del jugador (D2) | **Ninguna** |
| **Rupias** | Cada jugador: ningún mensaje las lleva. Los flags `*_PutRupee*` (rupias colocadas en el mundo) sí están entre los compartidos (`MOD:916-918`) | Del jugador (D2). Las rupias colocadas en el mundo, del mundo | **Ninguna** para la cartera; el flag sigue la regla de "objetos del suelo" |

### 3.2 Recompensas que hoy se reparten por flags

Hoy, cuando el flag de un avance llega por la unión, el juego que no lo tenía activa el flag **y además recibe el
objeto** (`OA:3986-3997`). Cada fila separa las dos mitades.

| Estado | Quién manda HOY (prueba) | Quién DEBE mandar | Brecha |
|---|---|---|---|
| **Orbes espirituales** | Flag `Clear_Dungeon*` por unión + un orbe a quien no lo tenía (`OA:3994`) | El santuario superado: del mundo (anfitrión). El orbe: del jugador | **Cambiar** + **Por decidir en F4** (CU-07): cómo gana su orbe el cliente que estuvo presente y qué pasa con el que no |
| **Runas de la pizarra** | Flags `IsGet_Obj_*` por unión + el objeto de la runa (`slate_rune`, `OA:3988`) | **Por decidir en F4**: son a la vez progreso e inventario | **Por decidir en F4** (CU-06/CU-07). Hasta entonces NO se cambia el comportamiento |
| **Pizarra Sheikah y paravela** | `IsGet_PortableUnit` da la pizarra (`OA:3987`); `FindDungeon_Finish` da la paravela, porque sin ella el otro no podía bajar de la meseta (`OA:3989-3992`) | **Por decidir en F4** | **Por decidir en F4**: son objetos sin los que no se puede seguir jugando juntos |
| **Semillas Kolog** | Flag `MainField_Npc_HiddenKorok*` por unión + una semilla; si el Kolog está a menos de 60 m se lanza su escena (`OA:3811-3847`, `OA:3993`) | El Kolog encontrado: del mundo (anfitrión). La semilla: del jugador | **Cambiar** + **Por decidir en F4** |
| **Habilidades de campeón** | Flag `IsGet_Obj_HeroSoul_*` por unión + la habilidad (`OA:3996`) | **Por decidir en F4** | **Por decidir en F4** |

### 3.3 Progreso del mundo

Todo esto viaja en QUEST (cliente → anfitrión) y QUESTS (anfitrión → clientes): un bit por línea de
`online_mod/quests.txt`, hasta 32 768. El anfitrión hace la operación **O** entre lo de todos y lo suyo y devuelve la
unión; cada juego activa lo que le falta (`quests_update`, `OA:3851-3997`; `online.c:664-688`).

| Estado | Quién manda HOY (prueba) | Quién DEBE mandar | Brecha |
|---|---|---|---|
| **Flags de misiones** (principales, de santuario, secundarias) | **La unión de todos.** Un cliente con la partida avanzada vuelca todo en la del anfitrión, sin vuelta atrás (`OA:3880-3997`; `MOD:845-860`) | Del mundo (anfitrión) (D1) | **Cambiar** (F4, CU-06): el estado inicial es el del anfitrión; el cliente no aporta su partida, solo lo que ocurra durante la sesión y el anfitrión acepte |
| **Santuarios** (activado, entrado, superado) | Unión de `Open_/Enter_/Clear_Dungeon*` (`MOD:876`). El rompecabezas de dentro **no** está en flags: cada entrada lo reinicia (`DOC:996`). Dos jugadores en el mismo santuario se ven | Del mundo (anfitrión) (D1) | **Cambiar** (F4). Jugar juntos el interior: no probado, F5 (CU-14) |
| **Torres** | Unión de `MapTower_*` y sus escenas (`DOC:453-454`) | Del mundo (anfitrión) (D1) | **Cambiar** (F4) |
| **Bestias Divinas** | Unión de `Clear_Remains*`, `Remains*` y relacionados (`MOD:879-881`, `MOD:913`). Interior con piezas móviles: no probado (`DOC:984-986`) | Del mundo (anfitrión) (D1) | **Cambiar** (F4). Interior: F5 (CU-14) |
| **Cofres** | Unión de `<mapa>_TBox_*`. Abierto en un juego, abierto en todos; el contenido es de quien lo abrió (`MOD:883-893`) | El cofre abierto: del mundo (anfitrión). El contenido: de quien lo abre | **Cambiar** (el dueño del flag) + **Por decidir en F4**: si el otro jugador recibe algo |
| **Objetos recogidos o rotos del mapa** (objetos sueltos, menas, barriles, cajas), tiendas, interruptores, muros | Unión de sus flags de reaparición (`MOD:894-925`). Desaparecen en el otro juego al recargar esa zona | Del mundo (anfitrión) (D1) | **Cambiar** (F4) |

### 3.4 Seres y objetos vivos

| Estado | Quién manda HOY (prueba) | Quién DEBE mandar | Brecha |
|---|---|---|---|
| **Enemigos: lista** | El anfitrión mantiene un registro de 64 y envía hasta 32, 10 veces por segundo (ENEMIES) (`g_reg`, `OA:2318`; `online.c:1264-1278`). **Pero** el servidor reenvía ENEMIES de cualquiera (`online.c:278-280`) | Del mundo (anfitrión) (D1) | **Validar** (F3): el servidor solo acepta ENEMIES del anfitrión |
| **Enemigos: simulación** | Cada enemigo tiene un **dueño** (una ranura): su juego ejecuta la IA y envía su estado (ESTATE); los demás muestran una marioneta. El dueño cambia por cercanía con histéresis (`OA:2536-2623`) y por agresión (`OA:2520-2563`) | El reparto lo decide el anfitrión; la simulación puede delegarse en el más cercano | **Validar** (F3): hoy el anfitrión copia el ESTATE de un cliente sobre su registro, nombre incluido (`OA:2398`) |
| **Enemigos: daño** | Quien golpea una marioneta envía HIT al dueño, que resta la vida (`OA:3096-3128`, `OA:2302-2309`). El daño lo declara el atacante; un valor negativo **cura** (`OA:2308`) | El dueño aplica; el anfitrión valida | **Validar** (F1: signo y rango; F3: emisor) |
| **Enemigos: muerte** | Flag `<mapa>_<actor>_<HashId>` activado en todos los juegos (`mark_dead`, `OA:2343-2357`) | Del mundo (anfitrión) (D1) | **Cambiar** (F4): lo confirma el anfitrión |
| **Jefes** (Ganon, Iras) | Cada juego simula el suyo; se **suma el daño** que hace cada jugador (`OA:4251-4288`). Las fases, cada juego por su cuenta | Del mundo (anfitrión) (D1) | **Cambiar**, difícil: F5. Hasta entonces se mantiene |
| **Botín de enemigos y objetos caídos** | Lista de 64 en el anfitrión (LOOTS); cualquiera añade con LOOT_ADD y retira con LOOT_TAKE; los demás crean **el actor real por el nombre recibido** (`OA:3237-3649`, `OA:3604`) | La lista: del mundo (anfitrión). Lo recogido: de quien lo recoge | **Validar** (F1: nombre en lista de permitidos; F3: emisor) |
| **Armas en el suelo** (PICK) | Quien la recoge lo anuncia; los demás la borran (`OA:5458-5560`). Relé sin origen (`online.c:294-298`) | El objeto en el suelo: del mundo (anfitrión). El arma recogida: de quien la recoge | **Validar** (F3) |
| **Objetos del mundo** (cajas, rocas, troncos) | Quien lo pone en movimiento; va dentro de STATE (`objs`), identificado por HashId (`OA:4510-4785`) | Quien lo mueve, mientras lo mueve; el reposo, del anfitrión | **Validar** (F1: `has_body`, números acotados). Arbitraje del anfitrión: **Por decidir en F4** |
| **Mecanismos** (ascensores, puertas) | Quien lo mueve; empate → ranura menor (`OA:4787-4980`). Sin comprobar que el actor tenga cuerpo físico (`OA:4928-4929`) | Del mundo (anfitrión) (D1) | **Validar** (F1: `has_body`) + **Cambiar** (F5, CU-14) |
| **Objetos rotos y árboles talados** | Quien rompe lo anuncia en STATE (`broken`); lista de sesión de 2048 que se reaplica cada segundo (`OA:4982-5456`). Escribe vida 0 en el actor con ese HashId (`OA:5178-5180`) | Del mundo (anfitrión) (D1) | **Validar** (F1) + **Cambiar** (F4) |
| **Runas en uso** (Magnesis, Stasis, Cryonis) | Quien la usa; el objeto se busca por nombre y posición (`OA:4333-4498`, `OA:4012-4162`) | Del jugador que la usa | **Validar** (F1: vida del Link remoto antes de usarlo, `has_body`, números acotados) |
| **Bombas, flechas** | Quien dispara; los demás solo ven el modelo y el efecto, el daño se queda en el juego del que dispara (`OA:1183-1296`, `OA:6378-6412`) | Del jugador que dispara | **Ninguna** en la propiedad |
| **Caballos** | El jinete (`OA:1758-1820`, `OA:6318-6340`). Montar de verdad: no probado (`DOC:676`) | Del jinete | **Ninguna** |

### 3.5 Entorno

| Estado | Quién manda HOY (prueba) | Quién DEBE mandar | Brecha |
|---|---|---|---|
| **Hora del día** | El anfitrión: su servidor envía TIME cada 2 s y los demás la adoptan si difiere en más de 3 minutos de juego (`online.c:373-379`; `OA:5851-5864`). Con servidor dedicado nadie la envía | Del mundo (anfitrión) (D1) | **Validar** (F1): que la hora sea un número finito entre 0 y 24 |
| **Clima** | **No es el anfitrión**: es el del primer jugador de la sesión (ranura más baja) que esté a menos de 500 m y en el mismo mapa (`weather_follow`, `OA:1004-1024`). Sí comprueba el rango (0..8) | Del mundo (anfitrión) (D1) | **Cambiar**, pequeño (F4). Con 2 jugadores juntos ya coincide con el anfitrión |
| **Luna Carmesí (temporizador)** | El jugador de la ranura 0: los demás adoptan su temporizador si difiere en más de 30 (`bloody_moon_follow`, `OA:984-1001`). Acepta solo valores entre 0 y 4000 | Del mundo (anfitrión) (D1) | **Cambiar**, pequeño (F3): identificar al anfitrión de verdad en vez de "ranura 0" |
| **Luna Carmesí (efecto sobre los flags)** | Regla local: un juego que ve 64 o más flags de reaparición borrados a la vez "tuvo una Luna" y deja de reaplicar los de la unión (`OA:3913-3924`). Cargar una partida más antigua a media sesión **se confunde** con una Luna | Del mundo (anfitrión) (D1) | **Cambiar** (F4): desaparece al dejar de usar la unión |

### 3.6 Reglas de la sesión, PvP y revivir

| Estado | Quién manda HOY (prueba) | Quién DEBE mandar | Brecha |
|---|---|---|---|
| **Reglas** (colisión entre jugadores, revivir, menús sin pausa) | El jugador de la ranura 0: viajan dentro de su STATE y cada cliente las adopta (`OA:2838-2840`) | El anfitrión | **Cambiar**, pequeño (F3): que sea el anfitrión identificado, no "ranura 0" |
| **PvP: activación** | Cada jugador con su interruptor, apagado por defecto; el daño solo se aplica si la **víctima** lo tiene activo (`OA:2914`; `online.c:110`) | Acuerdo de los dos (D3) | **Cambiar** (F4, CU-12): acuerdo explícito y visible |
| **PvP: daño** | **El atacante declara** (`pvp_seq`, `pvp_target`, `pvp_damage` en su STATE) y la víctima aplica daño menos su defensa, o bloquea si se cubre (`OA:2881-2928`). Se ignora si el daño es ≤ 0; no hay tope ni comprobación de distancia en la víctima `[sin verificar]` | La víctima aplica; el anfitrión valida (D3) | **Validar** (F4, CU-12): distancia, tope de daño, frecuencia |
| **Revivir** | Regla del anfitrión (ranura 0): mientras está activa, el juego cree que Link tiene un hada y lo reanima al instante (`rt_online_has_fairy`, `OA:2825-2831`). El compañero no interviene | Regla del anfitrión; el acto de revivir, entre los dos jugadores | **Cambiar** (F4, CU-11): estado "caído" y que el compañero te levante. Diseño **por definir en F4** |

---

## 4. Reglas para el código nuevo

Cada línea es comprobable leyendo el cambio. "DEBE" y "NO DEBE" son obligatorios.

- **A-1.** Antes de sincronizar un dato nuevo DEBES añadir su fila a la tabla de la sección 3, diciendo si es **del
  mundo (anfitrión)** o **del jugador**. Sin fila no hay código.
- **A-2.** Un dato **del mundo** DEBE tener un único emisor válido: el anfitrión. El cliente NO DEBE escribirlo en su
  juego por iniciativa propia; DEBE pedirlo y esperar la respuesta del anfitrión.
- **A-3.** Un dato **del jugador** NO DEBE ser modificado por lo que haga otro jugador, salvo por una función
  explícita de intercambio (dar un objeto, revivir, PvP aceptado).
- **A-4.** NO DEBES añadir llamadas a `give_item` (`runtime/online_actors.c:3751`) ni a `give_heart` disparadas por
  la llegada de un flag de otro jugador. Las que existen (`runtime/online_actors.c:3987-3997`) se revisan una a una
  en F4.
- **A-5.** NO DEBES decidir quién es el anfitrión con "ranura 0" en código nuevo. Usa `online_is_host()` para el
  juego local; para saber si **otro** es el anfitrión hace falta el mecanismo que definirá F3.
- **A-6.** NO DEBES combinar con una operación **O** (unión) el estado del mundo de varios jugadores. El patrón de
  `online_take_quests` (`runtime/online.c:664-674`) es el que hay que abandonar, no el que hay que copiar.
- **A-7.** El anfitrión DEBE validar toda petición de un cliente que cambie el mundo: que el cliente pueda hacerlo,
  que esté lo bastante cerca, que el valor esté en rango. Ver `contratos/validacion-red.md`.
- **A-8.** Un cambio irreversible en la partida guardada (activar un flag de progreso, dar un objeto único) NO DEBE
  aplicarse por un único mensaje sin validar.
- **A-9.** NO DEBES resolver por tu cuenta una fila marcada **Por decidir**. Párate y pide la decisión; se registra
  como ADR en `decisiones/`.
- **A-10.** Si tu tarea cambia quién manda sobre algo, DEBES actualizar la fila correspondiente en el mismo cambio,
  moviendo la descripción vieja a "HOY" con su nueva referencia de línea.
- **A-11.** NO DEBES confiar en el servidor dedicado (`--server`) para nada que necesite autoridad: en ese modo no
  hay anfitrión (`runtime/online.c:412-423`).

## 5. Errores típicos que no debes cometer

1. **"El servidor ya filtra, así que este mensaje viene del anfitrión."** Falso hoy: el servidor reenvía ENEMIES,
   LOOTS, QUESTS y PICK de cualquiera, y además sin decir de quién.
2. **Tratar "anfitrión" y "servidor" como lo mismo.** El servidor es un hilo repetidor; el anfitrión es un cliente
   más (su juego), conectado por `127.0.0.1`.
3. **Copiar el patrón de unión de flags para un dato nuevo** porque "ya funciona así".
4. **Regalar un objeto al otro jugador "para que no se quede atrás".** Es exactamente lo que D2 prohíbe; el caso de
   la paravela existe por un bloqueo real y se decide en F4, no por analogía.
5. **Escribir en la partida del anfitrión algo que el cliente traía de la suya.** El cliente entra en el mundo del
   anfitrión; no lo actualiza con su progreso.
6. **Dar por bueno el daño que declara el atacante** (PvP o golpes a enemigos).
7. **Suponer que el clima o las reglas ya son "del anfitrión".** El clima es del jugador de ranura más baja que esté
   cerca; las reglas, de la ranura 0.
8. **Confundir "ningún mensaje lleva el inventario" con "el inventario está aislado".** Los flags lo modifican.
9. **Olvidar que los dos juegos tienen partidas distintas.** Un flag que en el anfitrión está apagado puede estar
   encendido en el cliente desde antes de la sesión.
10. **Cambiar el comportamiento de una fila "Por decidir" porque parecía obvio.**
