# Bitácora · F4 T450 · Luna de Sangre del anfitrión: la escena también en el invitado

- Fecha: 2026-11-25
- Quién: agente Sonnet (`coop/T450`) bajo el orquestador (ADR-006): **solo lee código y escribe**. No compila, no
  ejecuta pruebas ni el juego, no toca código, parches ni sitios. Sin desensamblado ni descompilaciones.
- Fuentes: ADR-025 (n.º 5), `mapa/08` (fila "Luna de Sangre", HE-08, Q-EST-4, PE-5, HE-09), `mapa/07` §5 n.º 31,
  T411, `runtime/blood_moon.c`, `runtime/oa_flags.c`, `runtime/oa_world.c`, `runtime/oa_frame.c`, `runtime/oa_loot.c`,
  `runtime/hle/framepace.c`, `runtime/control_game.c`, `runtime/oa_control.c`, `docs/ONLINE.md`, `docs/CONTROL.md`.
- Marcas: **[L]** leído en el código de este checkout · **[H]** hipótesis, no comprobada.
- **Resumen honesto:** el proyecto ya sabe pedirle al juego **su propia** Luna de Sangre (escena incluida) sin lanzar
  ninguna escena a mano (`blood_moon.c`) [L]. Lo que **no** hace es usarlo en el modo en línea: hoy el invitado
  copia el temporizador del anfitrión y, en el momento en que el del anfitrión vuelve a 0, **le borra al invitado la
  Luna de Sangre sin escena** [L mecanismo, H efecto]. Si la petición de `blood_moon.c` reproduce la escena completa
  del juego es [H] y se mide en el juego (sección 6).

## 1. Qué dispara la Luna de Sangre del juego

1. **Temporizador.** El gestor de tiempo (`ksys::world::Manager +0x5c8`, el primero) cuenta en `+0xc8` grados de
   día (360 por día; la hora está en `+0xb8`), desde `WM_BloodyMoonTimer` al cargar. Pasado **2520 (7 días)** y **a
   medianoche** hay una Luna de Sangre y el contador vuelve a 0 (`oa_flags.c:15-20`; `docs/ONLINE.md:782-787`;
   dirección `0x7001425fa8` de 1.9.0 solo citada en comentarios) [L el comentario; que sea exactamente así es lo que
   dice el comentario, no lo he comprobado].
2. **El temporizador vive dos veces.** Cada fotograma el juego lo escribe en su valor de datos `WM_BloodyMoonTimer`
   (identificador en `tm+0x120`) y lo vuelve a leer de ahí; escribir solo la copia se deshace el mismo fotograma. Por eso
   `bloody_moon_set` llama al setter f32 del gestor de datos (`FN_GDT_SET_F32`) y además escribe la copia
   (`oa_flags.c:28-35`; `docs/ONLINE.md:786-787`) [L].
3. **Petición de "alivio de memoria" (la vía de `blood_moon.c`).** El juego tiene una Luna de Sangre de "pánico" que
   arranca sola si se le acaba la memoria de recursos: un bit de un solo uso (bit 12 de la palabra `+0x174` del
   gestor de recursos) que su actualización de escena comprueba cada fotograma. Activarlo deja "el tiempo, la escena y la
   reaparición de monstruos y materiales" al juego (`blood_moon.c:1-4`, `:86-89`) [L el código y el comentario; que
   la escena de pánico sea idéntica a la de medianoche es [H]]. La dirección del gestor se busca por patrón de
   instrucciones, no por dirección fija (`blood_moon.c:26-49`) [L].
4. **Quién la usa hoy:** `Cheats > Blood moon now` y la orden de control `bloodmoon` del juego
   (`control_game.c:92-99`, registrada en `:497`) [L]. `blood_moon_request` devuelve false si no hay mundo abierto
   (`blood_moon.c:63-68`); `blood_moon_frame` entrega el bit al juego solo cuando **no** hay título, carga, pausa/menú,
   evento ni consola, y el mundo del campo está cargado (`:72-90`) [L].
5. **No es un flag.** No hay un flag "hay Luna de Sangre" que se pueda poner [L: el código no maneja ninguno]. Lo que
   existe son los flags de reaparición (cofres, objetos, barriles…) que el juego apaga con la Luna; el proyecto los
   detecta por heurística (ver §3) [L].

## 2. Qué pasa hoy con la Luna de Sangre del anfitrión

| Pieza | Qué hace | Marca |
|---|---|---|
| Emisor | `oa_sample.c:748-749` mete el temporizador propio en `pose.bloody_moon` (-1 si no hay); viaja en STATE (`online.c:829`, `:923`; validado 0..4000 o -1 por el consumidor, `online_validate.h:168`) | [L] |
| Seguidor | `oa_bloody_moon_follow` (`oa_flags.c:37-54`), paso 12 de `rt_online_frame` (`oa_frame.c:104-110`): si el invitado tiene `me > 0`, el anfitrión (ranura 0) no carga (`ONLINE_F_LOADING`) y la diferencia pasa de ±30 grados, **copia** el temporizador del anfitrión con `bloody_moon_set` | [L] |
| Qué pasa cuando el anfitrión tiene la suya | Su contador pasa de ≈2520 a 0. El invitado, con ≈2520, ve diferencia ≈-2520 y se **pone en 0 sin escena** y sin pasar por medianoche con el contador alto | [L mecanismo; efecto H] |
| Consecuencia | La Luna de Sangre del invitado **no ocurre**: no hay escena, ni reaparición de monstruos y materiales por el juego del invitado. Es el fallo que la decisión 5 corrige | [H] |
| Variante | Si el contador del invitado queda por delante del del anfitrión (se copia solo cuando la diferencia supera 30), podría cruzar 2520 a medianoche **antes** que el anfitrión | [H] |
| Flags de reaparición | `oa_quests_update` ve 64 o más flags de reaparición apagados de golpe y supone una Luna de Sangre "aquí": guarda en `held_off` los que la unión aún tenía (`oa_flags.c:206-217`). `had`, `held_off` son estáticas sin reinicio (mapa 08) | [L] |
| Sesión | `g_world` (2 048 objetos rotos con identificador, `oa_world.c:584`) **solo** se vacía con `online_state() != ONLINE_CONNECTED` (`:726`, `world_clear` `:613-616`); `oa_g_broken` (8 ranuras de la pose) sigue rotando la lista (`world_rotate`, `:716-721`); `oa_world.c:697-706` vuelve a romper cada objeto de la lista que reaparece. **Con la Luna de Sangre, lo roto antes se vuelve a romper** (los árboles no vuelven) | [L] |
| Botín | `oa_loot_forget` (`oa_loot.c:48-52`) se llama en `oa_frame.c:24` (limpieza general), no en una Luna de Sangre | [L] |
| Orden `bloodmoon` | Hay **dos**: la del juego (`control_game.c:497`) y la de `oa_control.c:163` (borra los flags compartidos). La segunda se rechaza por nombre duplicado: HE-09 | [L] |

## 3. Respuestas a las preguntas del paso 1

### 3.1 ¿Qué la dispara?
Temporizador `+0xc8` ≥ 2520 **y** medianoche (cuando existe), o la petición de alivio de memoria (bit 12). No hay flag
[L, ver §1]. En el modo en línea **el reloj ya es del anfitrión** (`oa_flags_hour_frame`, `oa_flags.c:317-330`), así que la
medianoche llega a los dos a la vez [L].

### 3.2 ¿Se puede provocar en el invitado con su propio mecanismo?
Dos caminos:

- **A. Petición de alivio (`blood_moon_request`).** Ya existe, ya espera a que no haya menú, carga ni escena y no
  inventa el temporizador ni toca el reloj (`blood_moon.c:1-4`). **Es la vía recomendada**: no lanza ninguna escena a
  mano, así que no repite el riesgo n.º 31 de lanzar un evento fuera de contexto (`docs/CONTROL.md:89`) [L].
  Incógnita [H]: si en el juego ocurre a cualquier hora (el juego la usa por pánico, sin esperar medianoche) y si el
  invitado la sigue viendo como una Luna "de verdad" (cielo, sonido, reaparición, contador a 0).
- **B. Llevar el temporizador del invitado a ≥2520 y dejar que llegue a medianoche.** Es el mismo reloj, más
  "natural", pero el invitado tendría la Luna **solo a la medianoche**, que puede estar a minutos o horas de la del
  anfitrión, y `oa_bloody_moon_follow` está copiando el temporizador del anfitrión (hay que apagarlo). Y si el
  anfitrión duerme o cambia la hora (T449), la medianoche se adelanta o se salta. Es más frágil [H].

**Recomendación: A**, con una condición: el invitado **no** debe quedarse sin su Luna de Sangre si el anfitrión
ya pasó la suya (caso del seguidor actual) ni tener una por su cuenta (variante de §2).

### 3.3 ¿Cómo se entera el invitado de que el anfitrión tuvo la suya?
No hay mensaje. Con lo que ya viaja (`pose.bloody_moon`) basta un detector de "vuelta a 0":
`prev_host ≥ 2000` y `host_now < prev_host - 1000` (histéresis; las cotas del consumidor ya admiten valores sobre
4000, `F2-T222`) [H, diseño]. Casos que **no** son Luna de Sangre y no deben disparar: el anfitrión carga otra
partida (`ONLINE_F_LOADING`, ya excluido en `oa_flags.c:46-47`), el anfitrión escribe el temporizador a mano
(`SAVAGEWIND_ONLINE_BM_TEST`, `bloodmoon` del juego), una partida cargada con otro valor [H].
Alternativa sin heurística: un campo/evento nuevo en la red ("Luna de Sangre N.º k") con contador de época, que además
resuelve el §3.5. Cuesta un cambio de protocolo (T213/T222 validación) [H].

### 3.4 ¿Qué pasa si el invitado está en…?
Todas las filas son [L] el comprobante y [H] la conducta del juego salvo indicación.

| Estado del invitado | Cómo se ve en el código | Qué hacer |
|---|---|---|
| Menú, mapa, consola | `framepace_paused()` = estado de pausa ≠0 o pantalla extra (`framepace.c:72-77`, `:240`); `menu_input_captured()` | **Esperar** (ya lo hace `blood_moon_frame`, `blood_moon.c:76-78`) |
| Escena o conversación | `framepace_in_event()` (evento activo, `:92`) | **Esperar**. No lanzar otra encima: n.º 31 |
| Cargando | `framepace_loading_ui()` (pantalla 0x55, `:179`); en juego en línea además `g_settle` 60/180 fotogramas tras carga (`oa_frame.c:77`, `:84`) | **Esperar** y re-evaluar; tras una carga el invitado ya trae su propio reloj: no repetirla |
| Título | `framepace_title_ui()` (`:172`) | No hay mundo; `blood_moon_request` ya devuelve false (`blood_moon.c:64`). Un pedido en espera se **descarta** o se aplica tras entrar (decisión §5) |
| Santuario, jefe, Bestia Divina (otro "estado del mundo") | `in_field()` exige `world+0x7d9 ≠ 0` y `+0x6b8 == 1` (`blood_moon.c:56-61`); fuera del campo, falso | **Esperar** a que vuelva al campo abierto [H: que +0x6b8≠1 sea exactamente "santuario" no está comprobado] |
| Muerto / pantalla de fin de partida | `oa_g_local_dead` (`oa_sample.c:739`; `oa_frame.c:80`); durante el fin de partida el módulo ya no hace nada (n.º 47) | **Esperar**; después de revivir, 180 fotogramas más (`oa_frame.c:84`) |
| Durmiendo / esperando junto a la hoguera | evento activo; adicionalmente el juego puede estar cambiando la hora (T449) | **Esperar** (cuenta como escena) |
| Sin mundo cargado / en otro mapa | `oa_same_map` solo para el clima; el temporizador es del mundo entero | Igual que título |

Hay una regla común: **el pedido de Luna de Sangre queda "debido" en el invitado** y se aplica cuando se cumplan
todas las condiciones de `blood_moon_frame` más "no muerto" y "no `g_settle`". Con un tope de espera (por ejemplo 10
minutos) tras el cual se cierra en silencio y se pone el contador en el del anfitrión, para no dejar el pedido para siempre
[H, diseño; el tope lo decide el dueño].

### 3.5 ¿Qué hay que vaciar de la sesión?
- **`g_world` y `oa_g_broken`.** La Luna de Sangre del juego devuelve árboles, cajas, minerales, barriles… a su
  lugar; con la lista de sesión, cada objeto que reaparece se **vuelve a romper** (`world_enforce`, `oa_world.c:697-706`).
  Hay que vaciar `g_world`/`g_widx`/`g_nworld` (hoy `world_clear` es estática y solo al desconectar, `:613`, `:726`) y las
  8 ranuras `oa_g_broken` [L el hecho; H el efecto en el juego].
- **Cuándo vaciar para que no se contamine.** Cada jugador reenvía su `oa_g_broken` rotando `g_world` (`world_rotate`);
  quien aún conserva la lista **vieja** se la devuelve al que ya la vació, que la vuelve a aprender
  (`world_add` por identificador, `:597-610`). Hay que vaciar **a la vez** en cuanto se detecta (no al llegar la
  escena) y, o bien ignorar identificadores entrantes unos segundos, o llevar un número de época en la red [H, diseño].
- **Botín.** `oa_loot_forget` (`oa_loot.c:48-52`) vacía muertes/botín propio/ajeno: llamarla en la Luna, porque los
  enemigos reaparecen y el dueño de lo que cayó ya no es el mismo [H, no necesariamente necesario: se mide].
- **Marionetas y dueños de enemigos.** Salen por la vía genérica "the game removed … Link" (mapa 08) [L: leído en el
  mapa, no en el código otra vez].
- **Flags de reaparición.** Los apaga el propio juego con su Luna: la heurística de las 64 banderas
  (`oa_flags.c:206-217`) ya lo detectaría en cada juego. Con T411 esa heurística debe desaparecer o convivir
  (T411 paso 7: "no toques la parte de Luna Carmesí"). El `held_off` hay que reiniciarlo al vaciar [L el código; H la
  forma].
- **No hay que vaciar** el inventario, los corazones ni la resistencia: la Luna de Sangre no los toca [H].

## 4. Diseño propuesto

1. **Un solo módulo de decisión** (propuesta: `runtime/coop_bloodmoon.h` + `.c` con la parte pura, probable con una
   prueba en `tools/`): `bm_host_wrapped(prev, now)` y una máquina de estados `IDLE → OWED → WAITING_PLAY → LAUNCHED`.
2. **Detección** en `oa_bloody_moon_follow` (que ya recibe `pl`): si el anfitrión ha pasado por 0, el invitado pasa a
   `OWED` y **deja de copiar** su temporizador hacia abajo mientras esté en `OWED`/`WAITING_PLAY`.
3. **Lanzamiento** con `blood_moon_request()` más una comprobación propia de "no muerto, no `g_settle`".
   El camino es el del juego (bit 12): ninguna escena se lanza a mano. Tras la escena el temporizador del invitado
   es 0 por el propio juego y el seguidor vuelve a funcionar.
4. **Vaciado** (`g_world`, `oa_g_broken`, `oa_loot_forget`, `held_off`) **en cuanto se pasa a `OWED`** y también en el
   anfitrión cuando detecta **su propio** salto del temporizador (misma función sobre `*oa_bloody_moon_timer()`).
   Para eso `world_clear` pasa a ser pública (`oa_world.c`).
5. **Hilo.** `blood_moon_frame` se llama desde el gancho de fotograma **presentado** (`device.c:3292-3293`), no desde el
   hilo del juego; la constitución (regla 6) pide el hilo del juego. El `or` atómico del bit lo hace tolerable, pero para
   el modo en línea conviene hacer la entrega desde `rt_online_frame` [L el sitio de llamada; H el riesgo real].
6. **Reinicio.** Todo estado nuevo con `RT_ON_RESET` (`AGENTS.md`).
7. **Aviso.** Una línea de aviso al invitado ("la Luna de Sangre del anfitrión: empieza cuando salgas del menú") con
   traducción (`runtime/ui/i18n/*.inc`, `tools/i18n_check.py`).
8. **No se toca**: el reloj (`oa_flags_hour_frame`), ni `held_off/revival` de T411, ni se inventan eventos `Demo`.

## 5. Tareas propuestas (para `tasks.md`)

| Id | Tipo | Qué | Archivos | Depende |
|---|---|---|---|---|
| T450-a | [lógica] | Detector `bm_host_wrapped` y máquina de estados en función pura con prueba sintética (casos: salto real, carga del anfitrión, escritura a mano, valores >4000) | `runtime/coop_bloodmoon.h` (+`.c`), `tools/test_*` | — |
| T450-b | [mundo] | El invitado pasa a `OWED` y deja de copiar hacia abajo; lanza con `blood_moon_request` cuando todo lo permite (menú, evento, carga, santuario, muerto, `g_settle`); tope de espera | `runtime/oa_flags.c`, `runtime/oa_frame.c` | a, PE-T450-1..3 |
| T450-c | [mundo] | Vaciar `g_world`/`oa_g_broken`/botín/`held_off` en la Luna (propia y del anfitrión), `world_clear` pública; guarda de época o ventana de ignorar | `runtime/oa_world.c`, `runtime/oa_loot.c`, `runtime/oa_flags.c` | a |
| T450-d | [hilo] | Mover la entrega de `blood_moon_frame` al hilo del juego en modo en línea (o confirmar que el `or` atómico basta) | `runtime/blood_moon.c`, `runtime/oa_frame.c` | PE-T450-1 |
| T450-e | [texto] | Aviso traducible | `runtime/ui/i18n/*.inc` | b |
| T450-f | [herramienta] | Resolver HE-09 (renombrar la `bloodmoon` de `oa_control.c`, p. ej. `bloodmoonflags`) y documentarlo; ver §6 para qué orden usa la prueba | `runtime/oa_control.c`, `docs/CONTROL.md` | — (HE-09) |

T450-a y T450-f pueden ir ya; el resto espera a la medición.

## 6. Qué hay que medir en el juego (para el orquestador)

Preparación común: dos juegos (A anfitrión, B invitado), partidas **de prueba**, `SAVAGEWIND_CONTROL=7777`
(A) y `7778` (B), modo desarrollo, `SAVAGEWIND_ONLINE_BM_TEST` solo en A. Ver `docs/CONTROL.md`; los puertos y el
arranque de dos juegos los pone el orquestador. Antes, T450-f o `help | grep -i bloodmoon` (PE-13) para saber qué
`bloodmoon` responde.

**PE-T450-1 · La vía de alivio, con un solo juego.** En B (sin sesión): `swctl.py --port 7778 bloodmoon` con la consola
cerrada, de día y de noche.
Esperado/medir: ¿sale la escena entera?; ¿cielo y sonido?; ¿reaparecen monstruos y materiales?; ¿el temporizador
(`mem read` en `tm+0xc8`, o log `[online] the host's Blood Moon timer`) vuelve a 0?; ¿a qué hora ocurre?
Resuelve: §1.3 y §3.2 A.

**PE-T450-2 · Lo que hace hoy el seguidor (control del experimento).** Con sesión: A con
`SAVAGEWIND_ONLINE_BM_TEST=2519` y `swctl.py --port 7777 time set 23.9`. Mirar en B el log: `the host's Blood Moon
timer`; ¿B tiene escena a medianoche? Esperado según el código: **no** (el contador de B salta a 0).
Resuelve: §2 "Qué pasa cuando el anfitrión tiene la suya".

**PE-T450-3 · Camino A en el invitado, con el anfitrión.** En A: `BM_TEST=2519`, `time set 23.9` y esperar a su Luna.
En B: `bloodmoon` (la del juego) con la consola cerrada. Medir: ¿se ve la escena?; ¿B queda con el temporizador en 0
y A también?; ¿los dos ven reaparecer lo roto?; ¿lo roto de `g_world` se vuelve a romper en B (línea `the world's
list:`)? Resuelve §3.5.

**PE-T450-4 · Estados.** Repetir PE-T450-3 con B en: (a) menú de pausa, (b) mapa, (c) hablando con un NPC, (d)
pantalla de carga, (e) dentro de un santuario (`places shrine`, `goto <santuario>` solo cruza un mapa: entrar
caminando), (f) muerto (`set god_mode false`, dejarse caer), (g) durmiendo, (h) cutscene larga. Anotar:
¿la petición queda pendiente (`blood_moon_pending`)?; ¿se aplica al volver?; ¿se cuelga algo? (n.º 31: observar pantalla
negra). Resuelve §3.4.

**PE-T450-5 · Contaminación de `g_world`.** Romper 3 árboles en A, conectar B, provocar la Luna en los dos y
comprobar en los logs de ambos que la lista queda vacía y que `world_rotate` no la repone (líneas
`the session's world list cleared`). Resuelve §3.5.

**PE-T450-6 · Tercer jugador (si se prueba con bot).** El bot no tiene modo de Luna (tools/online_bot.c:156 pone
`bloody_moon = -1`); probar solo con dos juegos reales.

## 7. NO verificado

- Todo lo de los marcados [H]; en particular que el bit 12 reproduzca la escena de medianoche (§3.2), qué valor tiene
  `+0x6b8` en un santuario (§3.4) y que el botín necesite vaciarse (§3.5).
- No se ha compilado ni ejecutado nada. No se han leído `oa_core.c` ni el protocolo (`online_wire.h`) más allá de los
  campos de `bloody_moon`; el cambio de protocolo de §3.3 no está medido.
- Las direcciones de 1.9.0 (`0x7001425fa8`, `0x7000b36c80`) las cito de los comentarios del código; no las he
  contrastado con nada.
