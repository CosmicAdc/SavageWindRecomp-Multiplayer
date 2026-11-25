# Bitácora · F4 T454 · el compañero no ve la animación del hada que revive (N2)

- Fecha: 2026-11-25
- Quién: agente Sonnet (`coop/T454`) bajo el orquestador (ADR-006): **solo lee código y escribe**. No compila, no
  ejecuta pruebas ni el juego, no toca código. Paso 1 de T454 (investigación).
- Marcas: **[L]** leído en el código (con `fichero:línea`); **[H]** hipótesis, sin comprobar en juego.
- Nada de desensamblado ni de proyectos de descompilación: solo el código del repositorio y su documentación.

## Resumen

1. El hada **no existe en el código de la red**: no hay ni una línea de `runtime/` que sepa qué actor o efecto es.
   Lo único que el repositorio hace es responder "sí" a la pregunta del juego "¿tiene Link un hada?" y dejar que el
   juego haga el resto en la máquina del que muere [L].
2. La **caída y el levantarse sí viajan** (los ve A) porque van en el esqueleto y en el estado de animación (`as[0]`
   empieza por `Dead`) [L]. Lo que falta es todo lo que el juego dibuja **fuera del cuerpo** de Link: el actor y/o
   el efecto del hada y su sonido [H].
3. El canal ya existente de efectos (`fxev`) **solo mira cuatro emisores de B** (Link, su arma, su escudo, su arco)
   y sus sonidos [L]. Si el hada es otro actor, su efecto nunca entra en el anillo. Hay una variable de entorno que
   registra los emisores ajenos, pensada justo para esto [L].
4. **No se puede saber en el repositorio** qué actor o efecto es. Hace falta una prueba en juego (más abajo, PE-T454-1).

## Qué ocurre hoy, paso a paso [L]

| # | Qué pasa | Dónde |
|---|---|---|
| 1 | El juego, al morir Link, pregunta si tiene hada (1.9.0 `0x7000cec510`; x0 = el jugador; en el juego normal mira si hay `Animal_Insect_F` en la bolsa) | `tools/game_sites.py:202-205` |
| 2 | El parche `has_fairy` cambia su `and w0, w0, #1` final por una llamada a `rt_online_has_fairy` | `tools/patches.py:616-619` |
| 3 | `rt_online_has_fairy`: si ya tenía hada propia, la devuelve (se gasta la suya); si hay sesión conectada y la regla `revive` está activa, responde 1 y escribe `[online] Link died: revived by a fairy (online play)` | `runtime/oa_rules.c:104-111` |
| 4 | La regla es la del anfitrión; el invitado la copia de `pl[0].pose.revive` | `runtime/oa_rules.c:118`, `runtime/oa_sample.c:746` |
| 5 | A partir de ahí es **código del juego sin tocar**: reproduce su revivida (efecto, corazones, sin recarga de mundo) | `docs/ONLINE.md:775-781` |
| 6 | En B se marca `oa_g_local_dead` cuando `pose.as[0]` empieza por `Dead` y el frame se detiene en el paso 9 (`frame_wait`), **después** de muestrear y de `oa_fx_update` | `runtime/oa_sample.c:739`, `runtime/oa_frame.c:80,174-176` |
| 7 | Al volver B, el bucle espera 180 fotogramas (3 s) antes de reanudar sus tareas | `runtime/oa_frame.c:81-86` |

Consecuencia: la animación de B (caer, estar en el suelo, levantarse) la ve A porque es el esqueleto de B; el hada
es otra cosa y no se envía.

## Cómo viajan los efectos hoy [L]

- **Captura (en B)**. El parche `xlink_emit` (`tools/patches.py` `_xlink_emit`; sitio en `tools/game_sites.py:206`)
  llama a `rt_online_xlink_emit(ui, loc)` por **cada** efecto o sonido que emite cualquier actor del juego
  (`runtime/oa_sample.c:41-98`).
- **Filtro de emisores** (`runtime/oa_sample.c:49, 82-94, 226-229`). Solo se miran `FX_WATCH = 6` instancias: el
  ELink de Link, de su arma, de su escudo y de su arco, y el SLink de Link y de su arma (sonidos). Los demás
  emisores **se descartan** y, solo si `SAVAGEWIND_ONLINE_FX_ALL=1`, se escriben en el registro como
  `[online] fx other <ui>: <nombre>` (`oa_sample.c:84-93`). Esa línea existe para "averiguar qué actor reproduce un
  efecto" [L: comentario en la línea 84].
- **Filtro de nombres** (`oa_sample.c:248-257`): se envían los efectos de Link (who 0) y arma (who 1) y los sonidos
  (who 2/3 tras restar 2), salvo `Blur*`, `Kago_Shield*`, `Kago_Wind`, `MaterialEnv*`, `FieldEnv*`, los sonidos
  continuos (`continuous_sound`, `oa_sample.c:151-173`) y los nombres de 21 caracteres o más (`OnlineFxEvent.name`
  mide 21, `runtime/online.h:65`).
- **Estado enviado**: `pose.fxev[ONLINE_FX_EVENTS = 8]` (los 8 últimos), `pose.fx` (estela del arma, 24 bytes),
  `pose.fx_props[24]` (propiedades del ELink de Link), `pose.ground_props` (suelo) (`runtime/oa_sample.c:742-744,
  827-833`; copiado a la red en `runtime/online.c:818, 912`; tamaños en `runtime/online.h:56-58`).
- **Reproducción (en A)**, `runtime/oa_puppet.c:371-508`: copia `fx_props` al ELink de la marioneta (404-409); por
  cada evento nuevo (por número de secuencia) llama a `FN_EFFECT_EMIT` sobre el actor-marioneta o el arma que tiene
  en la mano (`who & 1`), o `FN_XLINK_EMIT` si es sonido (472-500); los termina a los `FX_EVENT_NS` (412-419) y
  mata antes uno igual aún vivo (479-485). Requiere que la marioneta de B exista y esté corriendo (`oa_running`,
  línea 443 para el suelo; `VALID(on)` en 473 para los demás).
- Hay un caso aparte para efectos que **no** salen del ELink de Link: el suelo (`ReactionField`, who 4), que el
  juego emite desde un actor suyo; se resolvió buscando ese actor entre todos (`oa_sample.c:181-191`) y enviando
  nombre, posición y propiedades (`oa_sample.c:63-80, 192-224`; reproducido en `oa_puppet.c:441-471`). **Es el
  molde más cercano a lo que probablemente necesite el hada.**

## Qué puede ser el hada: tres hipótesis [H]

Ninguna se puede confirmar leyendo el repositorio, porque no hay una sola mención al actor ni al efecto.

- **H-A, el hada es un efecto del ELink de Link** (por ejemplo, una luz que emite el propio jugador al revivir).
  Entonces ya pasaría por `rt_online_xlink_emit` como who 0 y ya se enviaría... salvo que la excluya algún filtro:
  nombre de 21 o más caracteres, prefijo `MaterialEnv`/`FieldEnv`, o que lo emita otro emisor distinto del ELink
  principal. **Debe de haber una causa concreta**; no es "no está implementado" sino "algo la descarta". Aun
  llegando, A solo la reproduciría si la marioneta existe y corre en ese momento (`oa_puppet.c:443,473`).
- **H-B, el hada es un actor aparte** (un NPC/objeto que el juego crea al revivir, con su propio ELink/SLink y
  su propia animación, que vuela hasta Link). Es la que mejor encaja con el comentario del encargo de la tarea
  ("el hada es un actor y un efecto locales"). Entonces su emisión se descarta en `oa_sample.c:86` y, aunque se
  enviara el efecto, A no vería el **actor** volando. Hay dos formas de reproducirlo: (1) hacer en A un actor
  equivalente en la posición de la marioneta de B (el sistema ya fabrica actores por nombre con `oa_piece_update`,
  `oa_core.c:412`, `FN_CREATE` en `oa_core.c:367`) o (2) emitir solo el efecto de luz sobre la marioneta.
- **H-C, el hada va con una escena o un evento** (`Demo...`): el juego podría pasar a un evento mientras dura la
  revivida. Efectos de eso: `framepace_in_event()` es verdadero en B (`runtime/hle/framepace.c:92`) y varios
  módulos se saltan su trabajo en eventos (por ejemplo `oa_players_collide`, `oa_rules.c:117`; la hora, T457).
  Hallazgo importante: `oa_sample_frame` **no muestrea mientras `framepace_paused()`**
  (`oa_sample.c:872`); `framepace_paused()` mira solo el menú de pausa/mapa y el título (`hle/framepace.c:79-84,240`),
  no los eventos, así que un evento no corta el muestreo [L]. Pero si la revivida abriera una pantalla de menú, sí.

Nota sobre los "Demo" del repositorio: los que aparecen (`Demo700_0`, `Demo701_0`, `Demo004_0`, `Demo164_0`,
`Demo017_0`, `oa_sample.c:627-631`, `oa_flags.c:79-157`) son escenas de la Pizarra y de las torres, **no** de la
revivida [L]. No hay ningún `Demo` conocido para el hada.

## Respuestas a las preguntas del paso 1

1. **¿Qué actor o efecto es el hada?** No se puede responder con el repositorio [L]. Lo que se sabe: es lo que el
   juego reproduce tras el `has_fairy`==1 (el objeto del inventario es `Animal_Insect_F`, `game_sites.py:203`, pero
   eso es el **objeto**, no el actor ni el efecto). Se averigua en juego con PE-T454-1.
2. **¿Se envía hoy?** No como tal [L]: lo emitido por emisores fuera de Link/arma/escudo/arco se descarta
   (`oa_sample.c:86-94`). Si fuera un efecto de Link, solo la excluirían los filtros de nombre [H-A].
3. **¿Cómo se reproduciría en la marioneta?** Dos molde ya existentes: el de los efectos `fxev` (`oa_puppet.c:
   472-508`) para un efecto de Link, y el de `ReactionField` (who 4, `oa_sample.c:176-224` +
   `oa_puppet.c:441-471`) para un efecto que sale de un actor ajeno. Para un actor, el de `oa_piece_update`
   (`oa_puppet.c:328, 534`).
4. **¿Hay que cambiar el formato de red?** Probablemente **no**: `fxev` ya tiene `who` de 8 bits con valores 0-4
   usados (`online.h:65`; `oa_sample.c:222,255`; `oa_puppet.c:441,472`) y nombre de 21 caracteres. Un `who` nuevo
   (por ejemplo 5 = "hada") cabe sin tocar el tamaño de nada. Si el nombre del efecto del hada mide 21 o más, **sí**
   haría falta ampliar `name[]`, y eso es formato de red: **pasa antes por T409** (instrucción de la tarea).
5. **¿Qué pasa con la regla `revive` desactivada o con hada propia?** Sin cambios: con hada propia el juego la
   gasta y la revivida es la normal; con la regla apagada no hay revivida y B va a fin de partida (CU-11, `oa_rules.c
   :104-111`). Un hada propia **también** debe verse (misma animación), así que el diseño no debe colgarse de
   `rt_online_has_fairy` sino de lo que el juego emite [H].

## Diseño propuesto [H], a confirmar con la prueba

Principio: **copiar lo que emite el juego, no inventar la animación**, como ya hacen las demás funciones de efectos.

1. **Detectar al emisor** (paso 1 de la prueba). Con el registro de emisores ajenos se obtiene el nombre del
   efecto del hada y de qué instancia sale.
2. **Si es del ELink de Link o de otra instancia fácil de enlazar** (H-A): ampliar `FX_WATCH` / el filtro, o quitar
   el nombre de la lista de exclusión, para que entre como un `fxev` normal. Coste: pequeño (`oa_sample.c`),
   sin formato nuevo.
3. **Si es un actor aparte** (H-B): copiar el patrón de `ReactionField` — localizarlo entre los actores
   (`oa_all_actors` + `oa_actor_name_of`, `oa_sample.c:181-191`), capturar sus emisiones (`rt_online_xlink_emit`
   con su `ui`), enviarlas con un `who` nuevo (5) y reproducirlas en A sobre la posición de la marioneta
   (`oa_puppet.c:441-471` como plantilla). Si además se quiere ver el **actor** volando, ver el punto 4.
4. **Actor visible volando** (opcional, más caro): enviar su nombre y su matriz como ya se envía una escena de
   objetos (`pose.objs`, `oa_world.c`) o un jinete (`pose.horse`); crearlo en A con `oa_piece_update` y quitarlo al
   acabar la revivida. Más riesgo (alta de actores en A, tiempos). **Recomendación**: empezar por el efecto
   (puntos 2 o 3) y dejar este para después si el efecto no basta.
5. **Sonido**: ya hay canal (`who` 2/3 en `oa_sample.c:248-249`); el sonido del hada debería ir por el emisor que
   lo produzca. Si el hada no es una de las seis instancias vigiladas, el sonido tampoco llega.
6. **Momento**: B envía durante la muerte (el muestreo y `oa_fx_update` corren antes de `frame_wait`,
   `oa_frame.c:174-176` frente a `:177`) [L]. **En A** `oa_puppet_frame` es el paso 18 y se salta si A también está
   muerto o en espera de 180 fotogramas [L, `oa_frame.c:76-87`]; en ese caso A tampoco vería nada, que es el
   comportamiento esperado.
7. **Pruebas sin juego**: ninguna de lo anterior se puede probar sin el juego (depende de nombres de efectos
   del propio juego). Lo que sí se puede es una prueba unitaria de cualquier filtro nuevo de nombres si se aísla
   como función pura (hoy `continuous_sound` es estática, `oa_sample.c:151`).

## Prueba que hay que hacer en el juego (para el orquestador)

Objetivo: descubrir **qué emite el hada y quién**. Se hace en la máquina de B (el que muere), A solo mira.

**PE-T454-1 (descubrir)**
1. En el equipo de B, arrancar con `SAVAGEWIND_ONLINE_FX_ALL=1`, `SAVAGEWIND_ONLINE_FX_LOG=1`,
   `SAVAGEWIND_ONLINE_ACTOR_LOG=1` y `SAVAGEWIND_ONLINE_LIFE_LOG=1` (los cuatro existen:
   `oa_sample.c:85,238`, `oa_core.c:137`, `oa_control.c:224`). Añadir `SAVAGEWIND_ONLINE_DIE_TEST=<segundos>` para
   que Link muera solo (`oa_control.c:218-223`: pone la vida a 0 a los `<s>` segundos); o bien dejarse matar.
2. Con A y B en la misma partida y cerca, esperar a que B muera y reviva por el hada (regla `revive` activa, B sin
   hada propia). En el registro de B buscar, entre `[online] Link died: revived by a fairy (online play)` y
   `[online] back after dying`:
   - líneas `[online] fx other <ui>: <nombre>` → efectos de **otros emisores** (el actor del hada) [H-B];
   - líneas `[online] fx Link: <nombre>` o `fx Link sound: <nombre>` → efectos de Link que **ya** se envían [H-A];
   - líneas `[online] AS ...` con `oa_core.c:137` → qué animaciones juega cada actor.
3. Con el mando de control (`docs/CONTROL.md:88`) y el juego aún en esa escena (o justo antes de morir), si el hada
   es un actor visible: `python tools/swctl.py actors Fairy` y `python tools/swctl.py actors Insect` (el comando
   `actors <parte del nombre>` lista actores vivos con su posición y distancia, 40 como máximo). Repetir cada 0,5 s
   durante la revivida. **Así se obtiene el nombre del actor aunque no emita efectos.** Para no depender de
   adivinar el nombre, probar también `actors Demo`, `actors Revive` y `actors Link`.
4. Si hay una escena/evento: apuntar `framepace_event_name()` (`hle/framepace.c:93`; lo usa el registro del
   marco de tiempo) o, si hay un comando, la salida de `swctl state` durante la revivida. Si no hay comando,
   decir que **no se pudo verificar** y no suponerlo.
5. Anotar versión del juego, FPS, y los nombres exactos. **No** copiar a la bitácora nada que sea contenido del
   juego más allá de los nombres cortos de efecto/actor necesarios (regla de no publicar activos).

**PE-T454-2 (comprobar el filtro)**
- Si el paso 1 muestra el nombre del efecto del hada entre los `fx Link`, comprobar en el registro de A si llega
  (`[online] <nombre de B>: <efecto> on his Link (...)`, `oa_puppet.c:503-507`, con `SAVAGEWIND_ONLINE_FX_LOG=1`
  también en A). Si **no** llega, la causa es el filtro de nombre o que la marioneta no corre en ese momento:
  mirar `oa_puppet.c:443,473`.

**PE-T454-3 (el aceptado, tras implementar)**
- Verificación de la tarea: A muere con hada propia en la bolsa y B ve la revivida (variante CA-9 de CU-11);
  B muere sin hada y A ve la revivida (regla `revive`). Con `give Animal_Insect_F 1` (`docs/CONTROL.md:110`: el
  comando `give` busca por nombre del actor; **sin verificar** que acepte ese objeto, como ya dice CU-11).

## Propuesta de tareas (para añadir a `tasks.md` tras la prueba)

- **T454-a [efectos, orquestador]**: ejecutar PE-T454-1 y anotar nombre(s) y emisor(es) del hada. Sin código.
  Bloquea al resto.
- **T454-b [efectos, agente]**: según el resultado, (H-A) quitar la exclusión que la descarta en
  `runtime/oa_sample.c:248-257`, o (H-B) añadir el actor del hada a lo vigilado, con `who` 5 y reproducción en
  `runtime/oa_puppet.c` siguiendo el molde de `ReactionField`. Verificación: PE-T454-2 y PE-T454-3.
  Archivos: `runtime/oa_sample.c`, `runtime/oa_puppet.c`; `runtime/online.h` solo si hace falta un `who` nuevo (no
  cambia el tamaño). **No** cambiar el formato sin pasar por T409.
- **T454-c [efectos, agente, opcional]**: si el efecto solo no basta, que A vea el actor del hada volando (punto 4).
- **T454-d [pruebas, agente]**: prueba unitaria de la función de filtro de nombres de efectos si se extrae como
  función pura.

## Qué hay que medir en el juego

1. Qué actor y qué efectos (nombres) produce el hada y desde qué instancia (PE-T454-1).
2. Si hay evento o escena durante la revivida (afecta a si A debe seguir enviando/mostrando).
3. Duración de la revivida (segundos) para fijar el tiempo de vida del efecto en la marioneta
   (`FX_EVENT_NS`, `oa_puppet.c:414`).
4. Si el sonido del hada sale de una de las seis instancias vigiladas.
5. Que con hada propia (objeto del inventario) la animación y el efecto son los mismos que con la regla `revive`.

## NO verificado

- Todo lo marcado [H]. En especial: qué es el hada, si es un actor o un efecto de Link, qué nombre tiene, si hay
  evento. Se leyó solo el código del repositorio; no se compiló ni se lanzó nada.
- Que `give Animal_Insect_F` funcione (CU-11 ya lo marca igual).
- Que `oa_puppet.c` siga reproduciendo efectos sobre una marioneta mientras B está caído (el paso 18 corre, pero
  no se comprobó que la marioneta de un jugador "Dead" siga `oa_running`).
- Los números de línea son de `coop/T454` al 2026-11-25 (commit base `ef9a958`).

## Resultado de PE-T454-1 (2026-11-25, orquestador + dueño, build `dc16647`, 1.9.0, 60 fps)

B arrancado con `SAVAGEWIND_ONLINE_FX_ALL/FX_LOG/ACTOR_LOG/LIFE_LOG=1` y `SAVAGEWIND_ONLINE_DIE_TEST=180`, junto a A.
Log de B (`[online] our life 0` → `Link died: revived by a fairy (online play)` → `our life 12`), justo después:

| Emisor (instancia en B) | Nombres |
|---|---|
| `…6518` | `Appear`, `ResurrectionFairy` |
| `…63a8` | `Fairy_Light`, `FairyTail` |
| UI | `mc_HeartUp` |

Ninguno es `fx Link`: son **otros emisores** → **H-B confirmada** (el hada es un actor aparte). El dueño lo vio en B y
**no en A**. Nombres ≤ 17 caracteres: caben en `fxev.name` (21), **sin cambio de formato**. Falta el nombre del actor
(no se pidió `actors` a tiempo): se obtiene en la implementación con `actors Fairy` durante la revivida, o desde el
emisor en `oa_sample.c` (la instancia `ui` que emite `ResurrectionFairy`).
Siguiente: T454-b (who 5 = hada; vigilar los dos emisores mientras Link revive; reproducir en la marioneta con el
molde de `ReactionField`).
