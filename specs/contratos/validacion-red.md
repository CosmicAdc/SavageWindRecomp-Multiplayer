# Contrato: validación de todo lo que llega por red

Todo byte que llega por un socket es **hostil hasta que se demuestre lo contrario**. No hace falta mala intención:
basta una versión distinta, un fallo del otro juego o una partida guardada diferente. Este documento fija qué hay que
comprobar, dónde, y qué fallos tiene hoy el código.

Es autosuficiente: no hace falta haber leído ningún otro documento para aplicarlo.

Abreviaturas: `online.c:n` = `runtime/online.c` línea n; `OA:n` = `runtime/online_actors.c` línea n. Líneas del
checkout del PR #294 (commit `350c5f8`). Nada se ha ejecutado; lo no comprobado va marcado `[sin verificar]`.

---

## 1. Dónde entra la red y por dónde pasa

```
socket ──► recv_some ──► next_msg ──► cl_message / srv_message ──► colas e instantáneas ──► rt_online_frame ──► juego
           online.c:164   online.c:172   online.c:967 / 232          (protegidas por g.lock)   OA:5721
           └────────────── hilo de red ──────────────────────────┘                            └─ hilo del juego ─┘
```

- En el **servidor**, cada mensaje pasa por `srv_message` (`online.c:232-311`), que casi siempre lo reenvía sin
  mirar.
- En el **cliente**, cada mensaje pasa por `cl_message` (`online.c:967-1124`), y el estado de un jugador por
  `remote_snap` (`online.c:907-964`).
- El hilo del juego lo recoge con `online_players`, `online_take_*`, `online_enemies`, `online_loots`,
  `online_quests` y lo usa en `runtime/online_actors.c`.

**La validación va en la frontera**: en `cl_message` / `remote_snap` (cliente) y en `srv_message` (servidor). Lo que
sale de ahí hacia las colas ya debe ser de fiar.

## 2. Qué se comprueba ya hoy (no lo rompas)

| Comprobación | Dónde |
|---|---|
| Trama: cabecera completa, longitud ≥ 1, mensaje completo, sin desbordamiento posible | `online.c:172-180` |
| Toda ranura usada como índice es `< ONLINE_MAX` (8) | `online.c:972`, `985`, `996`, `1002`, `1009`, `1022`, `1032`, `1066` |
| ENEMIES: `n ≤ 32` y longitud suficiente | `online.c:1044-1045` |
| LOOTS: `n ≤ 64` y longitud suficiente | `online.c:1083-1084` |
| SKEL: `n + nbow ≤ 160` y longitud suficiente | `online.c:1011-1012` |
| Colas de entrada con tope (golpes 64, estados 64, botín 32 + 32, recogidas 16) | `online.c:1056`, `1063`, `1075`, `1093`, `1101` |
| `shots.n ≤ 8`, `ice_n ≤ 3`, `anim` dentro de su enumerado, `elemento` de HIT dentro de su enumerado | `online.c:918`, `924`, `912`, `1104` |
| Cadenas terminadas en cero: HELLO, STATE, EQUIP, ENEMIES, LOOT_ADD, LOOTS, PICK, chat | `online.c:239`, `914-959`, `1027-1028`, `1052`, `1058`, `1076`, `1086`, `1037` |
| El texto recibido nunca se usa como cadena de formato | `runtime/ui/online_ui.mm:29`, `199`, `246-247` |
| Ningún mensaje lleva direcciones de memoria; los identificadores se buscan en tablas propias | `OA:2302` (`own_find`) |
| El consumidor acota algunos valores: clima `≤ 8` (`OA:1013`), temporizador de Luna Carmesí entre 0 y 4000 (`OA:993`), daño PvP `> 0` (`OA:2914`) | líneas citadas |
| `isfinite` en el consumidor para objetos (`OA:4683-4685`), mecanismos (`OA:4913-4915`), caballo (`OA:6323-6324`), equipo suelto (`OA:6289-6291`), materiales (`OA:6356-6357`) | líneas citadas |

## 3. Las reglas

Cada regla es comprobable leyendo el código. "DEBE" y "NO DEBE" son obligatorios.

### 3.1 Dónde y cuándo

- **V-1.** Todo campo recibido DEBE validarse **una vez, al recibir**, en el hilo de red, antes de entrar en
  ninguna cola o instantánea. Lugar: `cl_message` / `remote_snap` en el cliente, `srv_message` en el servidor, o
  una función de validación llamada desde ahí.
- **V-2.** El código del hilo del juego NO DEBE ser la única defensa. Las comprobaciones que ya existen en
  `runtime/online_actors.c` (sección 2) se conservan como segunda barrera; NO DEBES quitarlas.
- **V-3.** Un mensaje que falla una comprobación DEBE **descartarse entero**. NO DEBES "arreglar" el valor y seguir
  (excepción permitida: terminar una cadena en cero y recortar un contador a su máximo, como ya se hace).
  **Excepción para los mensajes con lista** (ENEMIES, LOOTS, ESTATE) **y para STATE** (ADR-025 n.º 1, T222): se
  descarta la **entrada** que falla, no el mensaje. En STATE el mensaje entero solo se descarta si falla su **núcleo**;
  cualquier otra entrada que falla se **anula** en la copia local, poniéndola en la marca de "nada" que ya usa ese
  campo, y se cuenta (§3.9). Anular no es corregir: el valor recibido nunca se sustituye por otro valor "plausible".
- **V-4.** Cada descarte DEBE contarse (un contador por tipo de mensaje) y registrarse en el log **como mucho una
  vez por segundo**, para que un emisor defectuoso no inunde el log.

### 3.2 Tamaños, contadores e índices

- **V-5.** La longitud DEBE compararse con la esperada. Mensajes de tamaño fijo: `longitud == tamaño`. Mensajes con
  contador: `longitud == cabecera + n × tamaño_de_elemento`. Hoy casi todo usa `>=`.
- **V-6.** Todo contador `n` DEBE compararse con el máximo de la tabla **antes** de usarse en un `memcpy` o un
  bucle.
- **V-7.** Todo valor usado como índice DEBE comprobarse contra el tamaño de la tabla que indexa: ranura `< 8`,
  `gear_bone` (0, `0xff`, o `≤` número de huesos conocidos), `pvp_target < 8`, `owner < 8` o el valor "de nadie",
  `fx_anim`, `wbone`/`sbone` en {0, 1, 2}, `kind` de disparo `< ONLINE_K_COUNT`, `mode` de material `≤ 2`.
- **V-8.** Los campos que identifican al emisor y que hoy no se acotan (`origin` de LOOT_ADD, `est_from` de ESTATE)
  DEBEN comprobarse `< 8`.

### 3.3 Cadenas y nombres

- **V-9.** Toda cadena DEBE terminarse en cero al recibir, **todas las de la estructura**, sin excepción.
- **V-10.** Los nombres y el chat DEBEN ser texto válido: sin bytes de control (< 32) y UTF-8 bien formado, antes de
  dibujarlos.
- **V-11.** Todo nombre de actor que vaya a usarse para **crear** un actor DEBE estar en una lista de permitidos
  antes de llegar a `request` / `request_p` / `create`. Las listas ya existen como tablas generadas en la máquina
  del jugador y como funciones:
  - enemigos: `enemy_actor(nombre)` devuelve la marioneta o nada (`OA:1993`, tabla `enemies.txt`);
  - botín: `is_loot(nombre)` (`OA:3246`);
  - equipo: `gear_actor(nombre)` (`OA:445`, tabla `gear.txt`); armadura: `armor_actor(nombre)` (`OA:502`, tabla
    `armor.txt`).
  Un nombre que no pasa NO DEBE llegar al juego.
- **V-12.** Los nombres usados para **buscar** un actor existente (Magnesis, Stasis, PICK, mapa, AS) NO crean nada,
  pero DEBEN ser cadenas terminadas y de longitud acotada, y la acción resultante DEBE cumplir V-18.

### 3.4 Números

- **V-13.** Todo `float` recibido DEBE cumplir `isfinite()`. Sin excepciones: posiciones, rumbo, las 12 componentes
  de cada matriz, hora, temporizadores, fotogramas de animación.
- **V-14.** Todo `float` DEBE además estar **acotado**. `isfinite` no basta: 1e30 es finito. Cotas de partida,
  tomadas de lo que el propio código ya usa:
  - posición y traslación de matrices: valor absoluto `≤ 100000` (la cota que el emisor ya aplica a su propia
    posición, `online.c:832-834`);
  - hora: `0 ≤ h ≤ 24`;
  - temporizador de Luna Carmesí: en la capa, solo finito y `|x| ≤ 100000` (fuera: se anula a `-1`, desconocido); la
    ventana `0..4000` la aplica el consumidor (`oa_flags.c`, `oa_bloody_moon_follow`). T222: antes la capa tiraba el
    STATE entero por encima de 4000 aunque el juego puede pasar de ahí (hallazgo I-1);
  - clima: `0..8` o `255` (`runtime/online.h`, campo `weather`);
  - componentes de rotación/escala de una matriz: cota **por fijar en F2** (hasta entonces, al menos finitas).
- **V-15.** Todo entero con signo cuyo sentido sea "una cantidad" DEBE ser `> 0` y tener un tope: daño de HIT, daño
  de PvP. El tope concreto es **por fijar en F1** a partir del mayor poder de ataque de `power.txt`. Vida de
  enemigo: `-1` (no está) o `≥ 0`.
- **V-16.** Un identificador (id de enemigo, `gid` de botín, HashId) NO necesita rango, pero el consumidor DEBE
  buscarlo en su tabla y descartar si no existe. NO DEBE usarse como índice ni como puntero.

### 3.5 Lo que nunca se acepta

- **V-17.** NO DEBE existir ningún mensaje con direcciones de memoria, desplazamientos dentro de estructuras del
  juego, rutas de archivo, nombres de archivo ni nombres de funciones. Si una tarea "necesita" enviar uno, está mal
  planteada.
- **V-18.** Un dato recibido NO DEBE decidir **por sí solo** una escritura en un campo de un actor del juego. Antes
  de escribir DEBE comprobarse localmente que el actor es del tipo esperado: `has_body(actor)` (`OA:73`) antes de
  `FN_BODY_TO`; lista de nombres admitidos antes de escribir la vida en `+0x848`; `alive()` (`OA:91`) antes de usar
  cualquier actor guardado.

### 3.6 Autoridad (quién puede enviar qué)

- **V-19.** El servidor DEBE saber qué conexión es la del anfitrión y aceptar ENEMIES, LOOTS, QUESTS y TIME **solo**
  de ella. (Cómo se identifica: por definir en F3; ver `contratos/protocolo.md`.)
- **V-20.** El servidor DEBE entregar HIT, ESTATE, LOOT_ADD, LOOT_TAKE y QUEST solo a su destinatario, no a todos.
- **V-21.** El anfitrión DEBE comprobar que quien pide un cambio puede hacerlo: que el enemigo golpeado existe y
  está cerca de quien golpea, que el botín retirado existe, que el jugador no está en otro mapa.
- **V-22.** Un mensaje de sesión (WELCOME, JOIN) recibido cuando no toca (un segundo WELCOME a media sesión) DEBE
  ignorarse.

### 3.7 Límites de consumo

- **V-23.** El servidor DEBE aplicar un tope de frecuencia y de bytes por cliente y por tipo de mensaje, y un plazo
  para recibir el HELLO. Umbrales: por fijar en F3.
- **V-24.** Un envío a un cliente NO DEBE poder bloquear a los demás.

### 3.8 Pruebas

- **V-25.** Cada regla de validación nueva DEBE llegar con una prueba automática sin el juego: un mensaje válido que
  pasa y, por cada comprobación, uno inválido que se descarta.
- **V-26.** Los datos de prueba DEBEN ser sintéticos, escritos a mano. NO DEBES usar capturas de una partida real.

### 3.9 STATE: núcleo y entradas (ADR-025 n.º 1, T222)

`online_validate_state` (`runtime/online_validate.h`) separa el STATE en dos partes. Se aplica sobre la copia local
que hace `cl_message`, después de terminar las cadenas (`online_terminate_state`).

- **V-27.** **Núcleo** → si falla un campo, el STATE se descarta entero (`dropped a STATE` en el log, contador
  `D_STATE`). Son: `pos[3]` y `yaw` (finitos, `|x| ≤ 100000`), `flags` (solo `PAUSED | LOADING`), `anim`
  (`< ONLINE_A_COUNT`), `as[4]` (terminadas) y las reglas de sesión `collide`, `revive`, `nopause` (0 o 1).
- **V-28.** **Entrada** → si falla, se anula esa entrada y el resto del STATE se aplica (`cleared an entry of a
  STATE` en el log la primera vez; contador aparte, `g_state_cleared_n`). La tabla dice qué forma cada entrada, qué la
  hace fallar y a qué "nada" se anula:

| Entrada | Falla si | Se anula a |
|---|---|---|
| `gear_bone[k]` + `gear_rel[k]` (k < 6) | hueso `> 8` y `≠ 0xff`; matriz no finita o `> 100000` | hueso 0 (no se ve), matriz a cero |
| `shots.n` | `> 8` | lista vacía: `n = 0`, los 8 disparos a cero |
| `shots.shot[k]` (id, kind, m) | `kind ≥ ONLINE_K_COUNT`; matriz mala | k `< n`: sale de la lista, los siguientes suben y `n` baja (el consumidor los busca por id); k `≥ n`: a cero |
| `shots.boom[k]` (seq, kind, pos) | `kind` fuera; posición mala | a cero (`seq` 0: ninguna) |
| `effects` | bits fuera de KAGO, KAGO_COUNT, GALE y DEMO | 0 |
| `grab`, `stop` (nombre, from, m) | nombre con caracteres fuera de `[A-Za-z0-9_]`; números malos | a cero (nombre `""`) |
| `ice_n` | `> 3` | 0, los 3 pilares a cero |
| `ice[k]` | matriz mala | como `shots.shot[k]` (el consumidor los empareja por posición) |
| `map` | sin terminar | `""` (desconocido) |
| `boss[k]` (hash, nombre, vida) | nombre malo; vida `< 0` | a cero (hash 0, `""`) |
| `fx` | sin terminar | `""` |
| `fx_anim[k]` + `fx_frame[k]` (k < 3) | animación `< -1` o `> 4095`; fotograma malo | animación `-1`, fotograma 0 |
| `fxev[k]` (seq, who, nombre) | `who > 5`; nombre sin terminar | a cero (`seq` 0) |
| `weather` | `> 8` y `≠ 255` | 255 (desconocido) |
| `dye[k]` (k < 3) | `< -1` | `-1` (ninguno) |
| `pvp_target` + `pvp_damage` | ranura `≥ 8`; daño `< 0` | 0 y 0; `pvp_seq` se conserva, así el golpe que nombra se pierde y no se aplica cuando vuelva |
| `horse` + `horse_m` + `horse_as` | nombre malo; matriz mala; AS sin terminar | a cero (`""`) |
| `tack[k]` + `tack_m[k]` (k < 2) | nombre malo; matriz mala | a cero |
| `bloody_moon` | no finito o `> 100000` | `-1` (desconocido) |
| `objs[k]` (hash, m) | matriz mala | a cero (hash 0) y su bit de `objs_trunk` a 0 |
| `movers[k]` (hash, m, as) | matriz mala; AS sin terminar | a cero (hash 0) |
| `held[k]` (nombre, mode, m) | nombre malo; `mode > 2`; números malos | a cero (`mode` NONE) |

- **V-29.** Admiten cualquier valor (no son núcleo ni entrada): `ping_ms`, `pvp_seq`, `aggro_id`, `aggro_seq`,
  `broken`, `trunks_gone`, `objs_trunk`, los `hash`, `id` y `seq` de las entradas, `fx_props` y `ground_props`.
- **V-30.** `fx_props` (24 palabras) y `ground_props` (16) **se escriben tal cual en la memoria del juego**: en las
  propiedades del ELink del muñeco (`oa_puppet.c`) y del ReactionField local. Cada palabra es entera o `float` según la
  definición del ELink del juego, que nuestro código no conoce; comprobar `isfinite` sobre una palabra entera tiraría
  valores legítimos (`0xffffffff` = -1 tiene forma de NaN). Mientras no se sepa qué índices son `float` (medirlo en el
  juego), no se acotan: es un riesgo aceptado y anotado (revisión de F2, C-2).
- **V-31.** Cotas de T222 y de dónde salen: `fx_anim ≤ 4095` es una **hipótesis** conservadora (el emisor manda el
  índice como `int16`, `oa_sample.c` `anim_slots`; el tamaño de la tabla de animaciones de un modelo no consta en el
  código); `dye ≤ 127` es la cota del emisor (`oa_sample.c`: solo manda el valor de la ranura si está en `0..127`).

---

## 4. Qué comprobar en cada mensaje

"Hoy" resume lo que ya se hace. "Falta" es lo que exige este contrato. Los tamaños son de carga, en bytes.

| Mensaje | Tamaño esperado | Hoy | Falta |
|---|---|---|---|
| HELLO | 54 exactos | Longitud `≥ 54`; magia y versión; termina nombre y contraseña; nombre no vacío (`online.c:235-246`) | Longitud exacta; plazo para recibirlo; límite de intentos de contraseña; nombre en UTF-8 válido; (F3) compilación y versión del juego |
| WELCOME | 1 | Ranura `< 8` (`online.c:972`) | Ignorarlo si ya se está conectado (V-22) |
| REJECT | 1 | Motivo desconocido → texto genérico (`online.c:975-983`) | Nada |
| JOIN | 25 | Ranura `< 8`; termina el nombre (`online.c:985-991`) | Sanear el nombre también en el cliente (hoy confía en el servidor) |
| LEAVE | 1 | Ranura `< 8` y en uso (`online.c:996`) | Limpiar el estado asociado a esa ranura (flags `quest_of[]`) |
| STATE | 3375 (+1 de ranura al recibir) | (T213/T222) Longitud `≥`; ranura en uso; termina todas las cadenas; núcleo malo → descarta; entrada mala → se anula (§3.9) | Longitud exacta; **todos** los `float` finitos y acotados (V-13, V-14); índices (V-7); `pvp_target < 8`; `pvp_damage` con tope; `bloody_moon` en rango; `weather` en rango; terminar `shots`/`grab`/`held`/`horse`/`tack` (ya se hace) y comprobar nombres contra listas cuando crean actores (`horse`, `tack`, `held`) |
| SKEL | `2 + (n + nbow) × 12` | `n + nbow ≤ 160` y longitud `≥` (`online.c:1011-1012`) | Longitud exacta; `nbow ≤ 32`; que el decodificador no pueda producir valores no finitos `[sin verificar]` |
| EQUIP | 432 (+1) | Longitud `≥`; termina las 9 cadenas (`online.c:1022-1028`) | Longitud exacta; cada nombre en `armor.txt` / `gear.txt` o vacío |
| CHAT | 1..120 | Recorta a 119 + cero en servidor y cliente (`online.c:299-306`, `1034-1037`) | Sin bytes de control; UTF-8 válido; tope de frecuencia |
| PING / PONG | 8 | PONG exige `≥ 8` (`online.c:1115`) | Descartar una latencia absurda (marca de tiempo futura) |
| TIME | 4 | Solo si no se es anfitrión (`online.c:1109`) | `isfinite` y `0 ≤ h ≤ 24`; en el servidor, solo generado por él |
| ENEMIES | `13 + n × 311` | `n ≤ 32`; longitud `≥`; termina 4 cadenas (`online.c:1044-1052`) | Longitud exacta; emisor = anfitrión (V-19); nombre en `enemies.txt`; `weapon`/`shield` en lista; matrices finitas y acotadas; `owner`, `wbone`, `sbone` en rango; centro finito |
| HIT | 9 | Longitud `≥ 9`; `elemento` en rango (`online.c:1101-1104`) | Daño `> 0` y con tope (V-15): hoy un daño negativo **cura** (`OA:2308`); entregar solo al dueño |
| ESTATE | `1 + n × 311` (+1) | Limitado por longitud y por la cola de 64; termina `name`, `as`, `weapon` (`online.c:1091-1096`) | **Terminar `shield`** (falta, `online.c:1095`); `n ≤ 32` y longitud exacta; `est_from < 8`; nombre en `enemies.txt`; el enemigo debe ser de quien lo envía; matrices finitas y acotadas |
| LOOT_ADD | 97 (+1) | Longitud `≥`; termina el nombre; pone `origin` (`online.c:1056-1059`) | Longitud exacta; `is_loot(nombre)`; posición y rotación finitas y acotadas; `origin < 8`; tope de frecuencia |
| LOOT_TAKE | 4 (+1) | Longitud `≥ 5` (`online.c:1063`) | Que el `gid` exista en la lista; que quien lo retira esté cerca |
| LOOTS | `1 + n × 97` | `n ≤ 64`; longitud `≥`; termina nombres (`online.c:1082-1087`) | Longitud exacta; emisor = anfitrión; `is_loot(nombre)`; posiciones finitas y acotadas |
| QUEST | 4096 (+1) | Cliente: longitud `≥ 4097`, ranura `< 8` (`online.c:1066`). Servidor: **ninguna** comprobación de longitud (`online.c:291-293`) | Longitud exacta en el servidor; que los dos jugadores tengan la misma tabla `quests.txt` (F3); el anfitrión decide qué bits acepta (F4) |
| QUESTS | 4096 | Longitud `≥ 4096`; solo si no se es anfitrión (`online.c:1072`) | Longitud exacta; emisor = anfitrión |
| PICK | 52 | Longitud `≥ 52`; termina el nombre (`online.c:1075-1077`) | Longitud exacta; posición finita y acotada; el relé debe llevar el origen |

---

## 5. Incumplimientos actuales que hay que arreglar

Gravedad: **Alta** = puede colgar el juego, congelar la sesión o estropear una partida guardada en uso normal;
**Media** = necesita una condición menos frecuente; **Baja** = improbable o solo degrada.

### 5.1 Fase F1 (arreglos rápidos)

| Id | Gravedad | Qué pasa | Dónde |
|---|---|---|---|
| A1 | Alta | Sin puerta de versión: con otra versión del juego se llama a 62 direcciones fijas de 1.9.0 | `OA:225-229` (solo mira que exista el overlay); direcciones en `OA:30-57` y siguientes |
| A2 | Alta | `FN_BODY_TO` con una matriz recibida, sin comprobar `has_body`, en mecanismos, troncos y objeto cogido del botín | `OA:4928-4929`, `OA:4901-4902`, `OA:4611-4627`, `OA:4727-4738`, `OA:4405-4406` |
| A3 | Alta | Escribe vida 0 en `+0x848` de cualquier actor "rompible" cuyo HashId llegue en `broken` | `OA:5178-5180` (lista de prefijos en `OA:5025-5033`) |
| A4 | Alta | Crea actores con el nombre recibido, sin lista de permitidos | botín `OA:3604` (entra por `online.c:1056-1059`); enemigos `OA:2226` (nombre copiado de un ESTATE en `OA:2398`) |
| A5 | Alta | Usa el Link remoto antes de comprobar que sigue vivo en ese fotograma | uso en `OA:4307`, `4323`, `4326`, `4437`, `4439` (llamadas desde `OA:5976-5977`); comprobación en `OA:6036` |
| H-6 / M4 | Media | Números sin `isfinite` ni cota: posición y rumbo del jugador, matrices de equipo, disparos, enemigos y sus armas, pilares de hielo, Magnesis y Stasis, botín, explosiones, hora | `online.c:907-964`, `1108-1113`; consumo en `OA:6054-6056`, `6287`, `6397`, `2235`, `3087`, `3067-3070`, `4157`, `4406`, `4106`, `3616-3623`, `6457` |
| H-6 (daño) | Media | El daño de HIT tiene signo: un valor negativo sube la vida del enemigo | `online.c:1100-1107` → `OA:2308` |
| H-7 | Baja-media | ESTATE no termina `shield`; el anfitrión lo copia y lo lee como cadena | `online.c:1095` (compárese con `online.c:1052`) → `OA:2398` |
| T-1 | Media | `stop_all` no limpia flags, botín, enemigos ni colas: los flags de una sesión se aplican en la siguiente | `online.c:1322-1335`; efecto en `online.c:682-688` |
| T-2 | Baja-media | `quest_of[ranura]` no se borra cuando sale un jugador y otro ocupa su ranura | `online.c:988`, `1067`, `668-670` |
| H-8 | Baja-media | `send()` sin protección frente a SIGPIPE: en Linux y macOS puede matar el proceso | `online.c:143` |
| H-3 (parte) | Media | La contraseña se muestra en claro en pantalla | `runtime/ui/online_ui.mm:64-66` |

### 5.2 Fase F2 (cimientos)

| Id | Gravedad | Qué pasa | Dónde |
|---|---|---|---|
| M1 | Media | Ningún `RT_ON_RESET`: ~85 variables con punteros del juego sobreviven al reinicio, incluidas peticiones pendientes cuyo estado se desreferencia | `OA:300-301`, `408-410`, `1405-1413`, `5244-5252`; limpieza parcial en `forget_all`, `OA:316-336` |
| M5 | Media | Ganchos que escriben anillos sin protección desde cualquier hilo | `OA:820-825`, `868-898`, `935-945` |
| T-3 | Baja-media | Variables `static` dentro de funciones que guardan estado entre sesiones | `online.c:524`, `824-825`, `835`, `1232-1233`, `1252`, `1264`, `1267`, `1297-1298`, `374` |
| T-4 | Baja | Lecturas sin cerrojo de `g.state`, `g.hosting`, `g.id`, `g_srv_running` | `online.c:1202`, `1262`, `1265`, `1375-1377`, `1389` |
| T-7 | Baja | El hilo de red y la interfaz leen memoria del juego | `online.c:828-831`; `runtime/ui/online_ui.mm:258-259` |
| — | Media | Validación dispersa: cada mensaje se comprueba (o no) en un sitio distinto; el estado está escrito cuatro veces | `online.c:845-883`, `907-964`; `runtime/online.h` (`OnlinePose`); `tools/online_bot.c:31-78` |
| H-10 | Baja | `est_from` y `origin` no se acotan a `< 8`; un segundo WELCOME cambia la ranura propia a media sesión | `online.c:1096`, `1058`, `971-974` |
| H-9 | Baja | Nombres y chat sin comprobar UTF-8 ni caracteres de control en el cliente | `online.c:187-191`, `990-991`, `1034-1038` |

### 5.3 Fase F3 (red robusta)

| Id | Gravedad | Qué pasa | Dónde |
|---|---|---|---|
| H-1 | Alta | Un cliente que deja de leer congela la sesión entera: un solo hilo, `send()` bloqueante; además "Terminar sesión" cuelga la interfaz del anfitrión | `online.c:140-148`, `213-216`, `1325-1328` |
| H-2 | Alta | Sin autoridad en el servidor: cualquiera envía ENEMIES, LOOTS, QUESTS, PICK; un QUEST con todos los bits a 1 acaba en la partida guardada de todos | `online.c:278-298`; aplicación en `OA:3851-3997` |
| H-3 | Media-alta | Contraseña en claro en el cable y en `online.ini`, comparada con `strcmp`, sin límite de intentos | `online.c:105`, `241`, `1178`, `1549-1551` |
| H-4 | Media | La ranura se ocupa al aceptar la conexión, antes del HELLO, sin plazo: ocho conexiones mudas bloquean la sesión | `online.c:359-370`, `383-388`, `394` |
| H-5 | Media | Sin tope de frecuencia; el servidor amplifica cada mensaje a todos; QUEST sin comprobar longitud | `online.c:262-311`, `291-293` |
| H-12 | Baja | `select()` con descriptores ≥ 1024 en Linux desborda `fd_set`; no se comprueba el resultado de `setsockopt(IPV6_V6ONLY)` | `online.c:326`, `345-355`, `1184-1188` |
| — | Media | No se comprueba que los dos jugadores tengan la misma versión del juego, la misma compilación ni la misma tabla `quests.txt` | `online.c:240` (solo magia y protocolo) |

### 5.4 Fase F4 (junto con las funciones)

| Id | Gravedad | Qué pasa | Dónde |
|---|---|---|---|
| M7 | Media | Los flags son una unión y entregan objetos; cargar una partida antigua a media sesión se toma por una Luna Carmesí | `OA:3913-3924`, `OA:3987-3997` |
| PvP | Media | El daño lo declara el atacante y la víctima lo aplica sin tope ni comprobación de distancia `[sin verificar]` | `OA:2881-2928` |

---

## 6. Errores típicos que no debes cometer

1. **Comprobar `isfinite` y creer que basta.** Un `FN_BODY_TO` hacia 1e9 metros es finito. Acota siempre.
2. **Validar en el consumidor y no en la frontera.** El dato ya ha pasado por colas y copias; otro subsistema lo
   usará sin tu comprobación.
3. **Usar `>=` para la longitud** y aceptar bytes de más sin saber qué son.
4. **Terminar casi todas las cadenas de una estructura.** El fallo real de `shield` fue exactamente eso. Recorre la
   definición campo a campo.
5. **Pasar un nombre recibido a `request`, `request_p` o `create`** sin haberlo buscado antes en una tabla local.
6. **Tratar un entero con signo como si no pudiera ser negativo** (daño, vida, contadores).
7. **Confiar en un mensaje porque "solo lo envía el anfitrión".** Hoy el servidor no lo garantiza.
8. **Corregir un valor inválido en vez de descartar el mensaje.** Un valor recortado sigue siendo mentira.
9. **Registrar cada mensaje inválido en el log.** A 20 mensajes por segundo llenas el disco; cuenta y resume.
10. **Añadir un campo al protocolo sin añadir su fila en la sección 4.**
11. **Probar la validación con una captura de una partida real.** Está prohibido subirla y no cubre los casos
    malos; escribe los bytes a mano.
12. **Quitar una comprobación del hilo del juego porque "ya se valida al recibir".** Se conservan las dos.
13. **Usar un identificador recibido como índice de una tabla.** Se busca; no se indexa.
