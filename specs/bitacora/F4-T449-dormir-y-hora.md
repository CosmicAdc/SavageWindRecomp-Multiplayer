# Bitácora · F4 T449 · dormir y esperar con el anfitrión como dueño de la hora

- Fecha: 2026-11-25
- Quién: agente Sonnet (`coop/T449`) bajo el orquestador (ADR-006): **solo lee código y escribe**. No compila, no
  ejecuta pruebas ni el juego, no toca código, parches ni sitios.
- Fuentes: ADR-025 (n.º 3 y 4), `mapa/08` (fila de dormir, fila de hora, HE-07), CU-06, constitución.
- Marcas: **[L]** leído en el código de este checkout · **[H]** hipótesis, no comprobada.
- **Resumen honesto:** el código del proyecto **no sabe nada** de cómo el juego duerme o espera en la hoguera: no hay
  sitio, parche ni nombre de escena para ello [L]. Todo lo que dice cómo se corta o se mantiene esa escena es [H] y
  se resuelve con las pruebas de la sección 5. Lo que sí está claro es la mecánica actual de la hora.

## 1. Cómo funciona la hora hoy

1. **Leer la hora.** `test_warp_hour` devuelve `*TimeMgr+0xb8 / 15` (0..24), o `-1` si aún no hay mundo
   (`runtime/services/test_warp.c:299-305`, `time_of_day` en `:148-154`) [L]. Si la hora está "retenida"
   (`held_tod >= 0`), devuelve la retenida, no la del juego (`:303`) [L].
2. **Escribir la hora.** `test_warp_set_hour` escribe la hora en los dos búferes de `WM_Time` de GameDataMgr y en el
   valor de pantalla (`write_time`, `:225-237`), y levanta el tope de las 11:00 previo al parapente (`:233-235`) [L].
   Si el mundo aún no existe, deja `pending_tod` y lo aplica `test_warp_frame` cuando aparece (`:265-274`, `:314-324`) [L].
3. **Difusión.** El servidor (la máquina del anfitrión) manda `M_TIME` a todos cada 2 s con `g_local_hour`
   (`runtime/online.c:332-336`, `TIME_EVERY_NS` en `:49`) [L]. `g_local_hour` la fija `online_set_local_hour`
   (`online.c:720`), llamada solo desde `oa_flags_hour_frame` (`runtime/oa_flags.c:317-330`) [L].
4. **Aplicación en el invitado.** `online.c:1149-1159` guarda la hora recibida (validada 0..24,
   `online_validate.h:225`) con su instante; `online_host_hour` la entrega **una vez** y solo si tiene menos de 5 s
   (`online.c:723-728`) [L]. `oa_flags_hour_frame` (paso 7 de `rt_online_frame`, `oa_frame.c:172`) compara y, si la
   diferencia (envuelta a ±12 h) supera 0,05 h (3 min de juego), llama a `test_warp_set_hour(host)`
   (`oa_flags.c:320-326`) [L]. El anfitrión nunca recibe `M_TIME` (`!g.hosting`, `online.c:1150`) [L].
5. **Quién la difunde sin mundo (el fallo de HE-07).** El paso 7 solo se ejecuta si `online_actors_on()`,
   `frame_out_of_session` (paso 3) y `frame_load` (paso 4) no terminan antes el fotograma (`oa_frame.c:159-172`) [L].
   `frame_load` devuelve `true` sin mundo (`:71`). Así que en el título, cargando o en una carga, **nadie actualiza
   `g_local_hour`** y el servidor sigue emitiendo el último valor cada 2 s (`online.c:334-335`) [L]. Además
   `test_warp_hour` solo da `-1` si no existe el TimeMgr, y el TimeMgr puede seguir existiendo en una carga [H].
6. **La hora se para en eventos.** `framepace.c:84-87` dice que el reloj del juego se detiene durante un evento
   (cinemática, charla, fanfarria) [L, comentario del código]. En una cinemática del invitado, cada 2 s llega una
   hora del anfitrión y se aplica igualmente (el paso 7 no mira eventos) [L].

## 2. Respuestas al paso 1

### 2.1 ¿Cómo se detecta que el jugador local duerme o espera (posada, cama, hoguera)?

- **No se detecta hoy** [L]: ninguna ruta de `runtime/` ni `tools/` nombra una escena de cama, posada u hoguera.
  Las búsquedas de `Demo0…` solo dan escenas de la pizarra, torres y de la apertura (`oa_flags.c:79-157`,
  `oa_sample.c:627-631`, `quick_replace.c:318-330`, `framepace.c:113-121`).
- **Herramientas existentes para detectarlo:**
  1. `framepace_in_event()` y `framepace_event_name()` (`framepace.c:87-108`): evento activo y su nombre
     (`evento_mgr +0x1d2b8`, cadena en `+0x68`) [L]. Cada evento nuevo se registra como `[event] <nombre>`
     (`framepace.c:142-145`) [L]. **Es la vía natural**: dormir y esperar son, casi seguro, eventos con nombre
     (`Demo…`) [H] — por eso la hora no avanza durante ellos (apartado 1.6) y por eso el "salto se deshace en 2 s".
  2. La animación de Link (AS) se envía en la pose y el código reconoce familias por nombre (`*Sleep*`, `*Relax*`…
     para enemigos, `docs/ONLINE.md:797`); no hay una lista para Link dormido [L]. Habría que mirar si la cama o la
     espera dejan un AS reconocible [H].
  3. El menú de espera de la hoguera ("¿cuánto esperar?") es una pantalla de UI; `framepace.c:150-178` ofrece
     `screen_at(id)` y `SAVAGEWIND_LOG_SCREENS=1` las lista [L]. El id no está documentado: hay que medirlo [H].
- **Propuesta de detección:** un nuevo campo de estado "durmiendo/esperando" que salga de `framepace_event_name()`
  (comparación con la lista de nombres que se midan, PE-T449-1) y, como respaldo, de la aparición de la pantalla de
  espera. Se envía en `OnlinePose.effects` (hay bits libres: `ONLINE_E_KAGO=1`, `KAGO_COUNT=6`, `GALE=0x08`,
  `DEMO=0x80`, `online.h:127-130`) o en un campo nuevo; añadir campo cambia el cable (F3).

### 2.2 ¿Se puede mantener al invitado en ese estado hasta que llegue la hora nueva?

- **Hoy no hay mecanismo** y **no se sabe** si el juego deja alargar la escena [H]. Lo que sabemos:
  - El invitado ya tiene la capacidad de **escribir la hora** (`test_warp_set_hour`), de **retenerla**
    (`test_warp_hold_hour`, para el reloj con `hold_time`, `test_warp.c:276-297`) y de **lanzar un evento por nombre**
    (`FN_EVT_START`/`oa_slate_event`, `oa_flags.c:153-157`; la orden `event` de `docs/CONTROL.md:89`) [L].
  - El juego cierra la escena de dormir cuando termina su flujo; el hecho de que "el salto se deshace en ~2 s"
    sugiere que el juego reescribe la hora al acabar (o que la escena lleva su propio reloj) [H].
- **Tres estrategias, de más a menos invasiva** (todas por validar en PE-T449-2/3):
  - **A. Dejar dormir al invitado con su reloj y no tocarle la hora mientras duerme.** Es la de menor riesgo: el
    invitado duerme "de mentira" y al terminar la escena el reloj se iguala al del anfitrión (paso 7 normal). Cuesta
    una cosa: el invitado ve su propia noche pasar y luego salta a la hora del anfitrión, no "espera". Sirve como
    **primera versión** porque no toca escenas.
  - **B. Mantener la escena abierta.** Si existe una espera dentro del flujo de la escena (por ejemplo, la pantalla
    de "¿hasta qué hora?" o un `Wait` del demo), pararla hasta que llegue la hora. Exige un sitio nuevo (ver 4) [H].
  - **C. Repetir la escena.** Volver a lanzar el evento de dormir si termina antes de que llegue la hora. Riesgo
    conocido: **un evento lanzado fuera de contexto puede colgarse** (`mapa/07` §5 n.º 31; `Demo022_0` en
    `docs/CONTROL.md:89`) [L].
- **Recomendación:** empezar por A, pasar a B solo si PE-T449-2 muestra un punto de espera utilizable, y **no usar C**.

### 2.3 ¿Qué pasa si el anfitrión no duerme (cancelar)?

- Hoy no pasa nada especial: el anfitrión sigue difundiendo su hora, que no cambia, y el invitado vuelve a ella en
  ≤2 s si la había movido (`oa_flags.c:320-326`) [L]. Eso **ya es** el "se deshace" de mapa 08.
- **Diseño propuesto (decisión 3 de ADR-025: "si el anfitrión no duerme, el invitado puede cancelar"):**
  - Mientras el invitado esté en el estado "esperando al anfitrión", muestra un aviso con la opción de **cancelar**
    (cualquier botón de salir de la escena, o una orden propia).
  - Sin cambio de cable: "el anfitrión durmió" se **deduce** cuando llega una hora que difiere en más de un umbral
    (p. ej. > 0,5 h) de la que había hace 2 s, y se detecta el salto. Una hora normal avanza ≈0,0x h cada 2 s [H,
    comprobar el ritmo con `time` en `state`]. Esto evita pedir un mensaje nuevo, pero es frágil si el anfitrión
    cambia la hora por `time set` o por la cámara de fotos (`photo.c:123-129`): también cuentan como "elegir hora" [L].
  - Alternativa más limpia (cambia el cable, F3): un mensaje nuevo `M_TIME` con una marca "salto" o un contador de
    saltos, de modo que el invitado sepa que fue un salto deliberado.
- **Tiempo de espera máximo** (propuesta): si el anfitrión no ha dormido en N s, el invitado sale solo.

### 2.4 ¿Cómo se deja de difundir `g_local_hour` cuando el anfitrión no tiene mundo?

- Es un arreglo pequeño y seguro, **no necesita investigación** [L]:
  1. Llevar la bandera al servidor: en el paso 3 y 4 de `rt_online_frame` (cuando `!world`, `oa_frame.c:71`) llamar a
     `online_set_local_hour(-1)`. `srv_loop` ya omite el envío con `hour < 0` (`online.c:335`) [L].
  2. Cubre título, carga, muerte (`frame_wait` termina en el paso 9, **después** del 7, así que muerto sí se
     actualiza [L]) y escenas (en una escena el mundo existe y la hora se difunde, aunque parada [L]).
  3. Caso del anfitrión en **escena** (cinemática): su reloj está parado, así que `g_local_hour` es estable; el
     invitado sí iguala esa hora congelada. La decisión 4 de ADR-025 habla de "escena, cargando o en el título";
     para la escena hay dos opciones: **(a)** no difundir con `framepace_in_event()` (más fiel a "sigue con su
     reloj"); **(b)** difundir. Recomiendo (a), con el cuidado de que dormir **es** un evento [H]: ahí sí hace falta
     difundir el salto al terminar. Se resuelve con la regla "no difundir mientras el evento activo no sea de
     dormir/esperar, y difundir al terminar".
  4. Pasar `g_local_hour` a `-1` se desactiva solo con `RT_ON_RESET` (`online.c:1394` pone a cero `host_hour`; la
     variable `g_local_hour` es estática y **no** se reinicia ahí [L]): añadir su reinicio al `RT_ON_RESET` de
     `oa_flags.c` o de `online.c`.
- **Receptor (invitado):** además de lo anterior, que **no aplique** la hora del anfitrión mientras él mismo está en
  un evento propio que no sea dormir (cinemática, charla), para cumplir "sigue con su reloj" cuando el anfitrión
  vuelve a su mundo (`oa_flags_hour_frame` hoy no mira eventos, 1.6) [L]. Al terminar su evento, el paso 7 lo iguala.

## 3. Respuesta de la decisión 4 (reloj del invitado con el anfitrión en escena, cargando o en el título)

- Con el arreglo 2.4 (poner `-1`) el invitado deja de recibir `M_TIME` y su reloj sigue como el suyo [L, por el
  efecto de `online_host_hour`: devuelve `false` si la hora tiene más de 5 s (`online.c:725`)].
- Al volver el anfitrión al mundo vuelve el flujo normal y el invitado se iguala en ≤2 s (≤ 5 s de caducidad).
- Esto **no** necesita parche, sitio ni cambio de cable.

## 4. Diseño y tareas propuestas

**Principios:** el anfitrión es el único dueño de la hora (ADR-025 n.º 3); el invitado nunca la mueve; no se lanzan
eventos fuera de contexto; primero lo barato y seguro.

| Tarea | Qué | Dónde | Riesgo | Depende de |
|---|---|---|---|---|
| **T449-a** | No difundir `g_local_hour` sin mundo ni durante un evento que no sea dormir; reiniciar `g_local_hour` en `RT_ON_RESET`. | `runtime/oa_frame.c`, `runtime/oa_flags.c`, `runtime/online.c` | bajo | — |
| **T449-b** | El invitado no aplica la hora del anfitrión mientras él está en un evento propio ajeno a dormir. | `runtime/oa_flags.c` | bajo | T449-a |
| **T449-c** | Medir en el juego (sección 5): nombres de evento y pantallas de dormir y esperar; cómo acaba la escena y si reescribe la hora. | solo pruebas (orquestador) | — | — |
| **T449-d** | Estado "duermo/espero" detectado con `framepace_event_name()` y enviado a los demás; aviso "esperando la hora del anfitrión" con cancelación. Estrategia A (dejar dormir, igualar al acabar). | `runtime/oa_sample.c`, `runtime/oa_flags.c`, `runtime/online.h` (campo: cambia el cable → F3), textos de `runtime/ui/i18n/*.inc` | medio | T449-c |
| **T449-e** | Solo si T449-c halla un punto de espera: estrategia B (mantener la escena hasta la hora nueva). Puede exigir un sitio nuevo en `tools/game_sites.py` y un parche en `tools/patches.py`: **lo decide y escribe el dueño**, no un agente (regla del encargo). | por definir | alto | T449-c, T449-d |

**Sitio o parche nuevo (dejado por escrito para el dueño, sin crear):** si B es viable, hará falta el nombre
de la escena y su punto de espera. Con 1.9.0 como versión primaria, se añadiría con patrón versionado en
`tools/game_sites.py`, un parche con instrucciones comprobadas en `tools/patches.py` y direcciones por `g_site`
(AGENTS.md). Hasta que PE-T449-1 y 2 den el nombre, **no se puede escribir**.

## 5. Qué hay que medir en el juego (pruebas para el orquestador)

Montaje: dos juegos con `bash tools/coop/dos_juegos.sh arrancar` (A anfitrión en 7777, B invitado en 7778), como en
`mapa/08` §5. Órdenes de `tools/swctl.py` según `docs/CONTROL.md`. Cerrar Docker y el navegador antes (RAM).

**PE-T449-1 · nombres de evento y pantallas de dormir y esperar (un solo juego, sin sesión).**
1. `$PY tools/swctl.py --port 7777 goto stable 2` (o una posada: `places stable`); entrar y acercarse a la cama.
2. Pulsar A en la cama con `press a 8`; elegir la hora en el menú; anotar `state` antes y después
   (`hour`, `hour_held`).
3. Leer el log: la línea `[event] <nombre>` de la escena. Repetir con una hoguera (`goto` a un campamento,
   cocinar/encender y "esperar") y con la tienda del Prisma si procede.
4. Con `SAVAGEWIND_LOG_SCREENS=1`, anotar las pantallas que aparecen al elegir la hora.
- **Resultado esperado:** nombre(s) de evento y de pantalla; si la escena cambia la hora de golpe o la avanza.

**PE-T449-2 · ¿qué reescribe la hora al acabar la escena? (explica el "se deshace en 2 s").**
1. Igual que PE-T449-1 en el invitado B con sesión; en A, no dormir.
2. `state` en 7778 cada segundo durante la escena y 10 s después.
- **Resultado esperado:** si la hora salta con la escena y vuelve en ≤2 s (el paso 7 de `oa_flags_hour_frame`) o si
  el juego la reescribe. Para separarlos: repetir con `SAVAGEWIND_ONLINE` **sin** sesión en B (si la hora se queda,
  el culpable es `oa_flags_hour_frame`).

**PE-T449-3 · reloj parado durante la escena y aplicación de `M_TIME` dentro de ella.**
1. En la escena de dormir de B, ver en el log de B las líneas `[online] the host's time: …` durante la escena.
- **Resultado esperado:** si el paso 7 aplica la hora del anfitrión **dentro** de la escena (lo que hoy se espera
  [H]); con ello se confirma la necesidad de T449-b.

**PE-T449-4 · dormir del anfitrión con el invitado en el mundo (el caso de hoy).**
1. A duerme hasta una hora nueva (PE-T449-1 en 7777); en 7778 repetir `state` cada 2 s; `actors` y vista libre.
- **Resultado esperado:** B salta a la hora de A en ≤2 s, también en mitad de lo que haga (cinemática, menú).

**PE-T449-5 · anfitrión sin mundo.**
1. A: Sistema → Cargar (o ir al título); `time set 22` en A **antes**, para que la última hora sea distinta.
2. En B, `state` cada 2 s durante la carga de A (≥ 30 s).
- **Resultado esperado hoy:** B sigue recibiendo la última hora de A; tras T449-a, B sigue con su reloj.

**PE-T449-6 · ritmo del reloj, para fijar el umbral de "A durmió".**
1. `state` cada 2 s durante 2 min en un juego quieto: cuánto avanza `hour` cada 2 s. Con eso se fija el umbral de
   salto (propuesta: > 0,5 h en 2 s).

**Qué NO se ha verificado (todo):** nada de lo anterior se ha ejecutado. No se sabe cómo se llama la escena de
dormir ni la de espera, ni si el juego reescribe la hora al terminar, ni si la escena se puede alargar. Lo único
que sí está leído con certeza es la mecánica de difusión y aplicación de la hora (sección 1).

## 6. Resumen para el orquestador

- Se puede hacer ya, sin investigar más: **T449-a** y **T449-b** (la hora solo se difunde con mundo y fuera de
  eventos ajenos al dormir; el invitado no la aplica dentro de sus eventos).
- Primero hay que medir (PE-T449-1…6) para saber qué es "dormir" para el código; después T449-d con estrategia A.
- B (mantener la escena) y C (repetirla) están **sin demostrar**; C se desaconseja por el riesgo de cuelgue.
