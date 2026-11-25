# Bitácora · F4 T451 · Eventide y Prueba de la Espada: entrar los dos juntos

- Fecha: 2026-11-25
- Quién: agente Sonnet (`coop/T451`) bajo el orquestador (ADR-006): **solo lee código y escribe**. No compila, no
  ejecuta pruebas ni el juego, no toca código, parches ni sitios. Sin desensamblado ni descompilaciones.
- Fuentes: ADR-025 (n.º 7), `mapa/08` (filas Eventide y Prueba de la Espada, HE-16, Q-EST-6), `runtime/oa_sample.c`
  (`local_map`, `oa_same_map`), `runtime/oa_flags.c`, `runtime/oa_gamedata.c`, `runtime/oa_loot.c`,
  `runtime/oa_frame.c`, `runtime/oa_rules.c`, `runtime/oa_enemies.c`, `runtime/online.h`,
  `runtime/online_actors_internal.h`, `runtime/services/fs.c`, `runtime/game_place.c`, `docs/ONLINE.md`,
  `docs/CONTROL.md`, `tools/online_mod.py`.
- Marcas: **[L]** leído en el código de este checkout · **[H]** hipótesis, no comprobada.
- **Resumen honesto:** el repositorio **no tiene nada** sobre Eventide ni la Prueba de la Espada: ninguna búsqueda de
  `Eventide`, `FinalTrial`, `Trial` (como reto), `Demo` de entrada ni de quitar inventario da resultado en `runtime/`,
  `tools/patches.py` ni `tools/game_sites.py` [L]. Quién quita el inventario, cuándo y cómo lo devuelve lo hace **el
  juego** con sus propios datos, y nada de eso está en este código [H]. Por eso casi todo el paso 1 sale como [H] y
  se resuelve con las mediciones de la sección 6. Lo que sí es firme [L] es **qué hace hoy el modo en línea** cuando
  uno entra y el otro no (sección 3), y es suficiente para diseñar.

## 1. Lo que el código sí sabe

| Hecho | Dónde | Marca |
|---|---|---|
| El mapa propio se lee del juego: tipo (`FN_MAP_TYPE` 0x7001208304) y nombre (`FN_MAP_NAME` 0x70012082f8), cada uno un `SafeString`. Se guarda en `oa_g_map` como `"<tipo>/<nombre>"`; **el nombre solo se añade si el tipo no es `MainField`**, así que el campo es siempre `"MainField/"` | `oa_sample.c:481-488`; `online_actors_internal.h:125-129` | [L] |
| Cada cambio de mapa deja la línea `[online] map: <tipo>/<nombre>` en el registro | `oa_sample.c:486` | [L] |
| `oa_same_map(p)`: dos jugadores están en el mismo mapa si alguno no tiene mapa conocido o los textos son iguales | `oa_sample.c:490-492` | [L] |
| Ejemplos de tipos que el código ya conoce: `MainField`, `AocField`, `MainFieldDungeon`, `CDungeon` (esos cuatro, para marcar enemigos muertos) | `oa_enemies.c:339` | [L] |
| El mapa viaja en la pose de cada jugador (`OnlinePose.map[32]`) y llega a los demás | `online.h:93-94`; `online.c:813`, `:907` | [L] |
| **Eventide es `MainField`** según el mapa 08; no hay ningún código que lo contradiga ni lo confirme | `mapa/08:108`; ninguna búsqueda de `Eventide` en `runtime/` | [L que no hay código; H el tipo] |
| El tipo de mapa de la Prueba de la Espada **no está comprobado**; `AocField` es la candidata (el DLC), pero también podría ser un tipo propio o un interior | `mapa/08:107`, `:322` | [H] |
| `game_place.c` solo distingue por *pack* de interior (`Pack/<nombre>.pack`); su tabla de interiores generada trae santuarios y bestias divinas, y su tipo `INTERIOR_TRIAL` existe para pruebas de santuario | `game_place.c:9-23`, `:42-57` | [L] |
| El nombre del evento activo se puede leer (`framepace_in_event`, nombre tipo `"Demo014_0"` en un puntero a +0x68 → +0x74) | `hle/framepace.c:96-100` | [L] |
| El proyecto ya sabe **arrancar un evento del juego** en Link, como lo haría un `EventTag` (`oa_start_event`), y lo usa para la pizarra, los kolog y la escena de la Luna de Sangre | `oa_gamedata.c:55-62`; `oa_flags.c:81-107`; `docs/CONTROL.md` (`event`) | [L] |
| El proyecto ya sabe meter un objeto en el inventario con la ruta del juego (`FN_POUCH_CAN_GET` + `FN_POUCH_GET`) | `oa_gamedata.c:63-71` | [L] |
| **No hay** ningún código que lea, vacíe, guarde o restaure el inventario. Solo hay "dar un objeto" | búsqueda de `PorchItem`, `inventory`, `ClearPorch`: solo `oa_give_item` | [L] |
| `services/fs.c` guarda los ficheros del perfil de guardado tal cual los pide el juego (con reintentos si Windows los tiene bloqueados). **No tiene ninguna lógica de reto, copia ni inventario** | `fs.c:1798-1830`, `:2094-2117` | [L] |

## 2. Respuestas al paso 1

### 2.1 ¿Cómo se dispara la entrada (escena, flag, carga de mapa)?

- **Prueba de la Espada.** En el juego se empieza desde la Espada Maestra y hace una **carga de mapa** (pantalla de
  carga a otro mapa) [H, conocimiento general del juego, no del código]. Para este proyecto lo observable es el cambio
  de `oa_g_map` en el registro (`[online] map: ...`, `oa_sample.c:486`) [L que se registra; H que cambie a un valor
  distinto de `MainField/`].
- **Isla de Eventide.** No es una carga: es `MainField` [H por mapa 08]. El juego **al cruzar un límite de la isla**
  lanza una escena y le quita el inventario [H]. Eso se vería como un evento `Demo…` o flujo de evento
  concreto (se lee con `framepace_in_event`, `hle/framepace.c:96-100`) más un salto brusco en el inventario [H].
- **Flags.** Ambos retos escriben flags de progreso. La documentación dice que el progreso de la Prueba de la Espada
  (`FinalTrial*`) **ya forma parte de los 25 767 flags compartidos** de la unión (`docs/ONLINE.md:992`;
  lista `quests.txt` de hasta 32 768 nombres, `tools/online_mod.py:937`) [L el texto; H que los de Eventide estén en la
  lista: la lista sale del generador y no la he podido abrir aquí].
- **Conclusión:** el disparador es del juego y no está en el código. Hace falta medirlo (sección 6, M1 y M2).

### 2.2 ¿Cómo se guarda y devuelve el inventario?

- **Lo hace el juego.** El código no tiene nada que quite ni devuelva inventario [L, ver sección 1]. Cómo lo guarda
  el juego (¿copia en el guardado?, ¿datos de partida en memoria?, ¿se persiste al autoguardar?) es [H] y es **la
  pregunta de más riesgo** (sección 5, R1): si el juego guarda el inventario original en el guardado y el programa lo
  escribe a disco en mitad del reto, un fallo en ese momento podría dejar al jugador sin su inventario.
- **Entre jugadores no hay inventario compartido:** cada jugador conserva el suyo (decisión del dueño en
  `COMO-CONTINUAR.md`; el guardado es local del jugador). Lo único que cruza la red es el **botín del suelo** (ver 3.3)
  y las recompensas por flags (`oa_give_item`, `oa_flags.c:299-308`) [L].

### 2.3 ¿Qué tipo de mapa tiene la Prueba de la Espada?

No está comprobado [H; `mapa/08:107` y `:322`]. Se mide con la línea `[online] map:` (M1). Se anotan dos posibilidades
y qué cambia en cada una:

| Si el tipo es… | Entonces |
|---|---|
| Uno propio (p. ej. `AocField/<nombre>`) | `oa_same_map` ya separa a quien está dentro de quien está fuera [L mecanismo]. Sirve para el aislamiento; no hace falta detectar nada más |
| `MainField` (sin nombre) | `oa_same_map` **no distingue**: puppets, enemigos y botín se mezclan con el campo [L mecanismo; H caso]. Habría que detectarlo por otra vía (flag, evento activo o posición) |

### 2.4 ¿Qué pasa si uno sale, muere o se desconecta antes?

- **Hoy:** nada especial [L]. Cada juego aplica sus propias reglas del reto; el compañero, fuera, no se entera.
- **Muerte:** el proyecto ya sabe que un Link muerto (`Dead…`) deja de ser dueño de enemigos (`oa_enemies.c:358-362`) y
  se espera tras volver a levantarse (`oa_frame.c:80-84`) [L]. Qué hace el juego al morir en la Prueba (volver al
  principio de la sala o sacar al jugador del reto) lo decide el juego [H].
- **Desconexión:** en el invitado, fuera de la sesión (`me < 0`), se borran los actores de los jugadores y se sigue
  (`oa_frame.c:32-48`) [L]. El reto del que sigue jugando no se toca [H], y su inventario lo devuelve el juego al
  acabar [H].
- **Salir uno antes (con el otro aún dentro):** con la regla de "entran los dos juntos" hay que decidir qué hace el
  que se queda. Propuesta en 4.4.

### 2.5 ¿Qué pasa si el otro no puede entrar (santuario, escena)?

Hoy no hay mecanismo para esperar al otro ni para forzar su entrada. Casos [L: el código ya detecta estos estados]:

- **En un santuario o interior:** distinto `oa_g_map` (`CDungeon/Dungeon000`, `oa_sample.c:489-492`) [L].
- **En una escena o menú:** `framepace_in_event` / pausa (`ONLINE_F_PAUSED`) / carga (`ONLINE_F_LOADING`,
  `online.h:23`) [L].
- **Muerto, a caballo, en una escena de historia:** el código ya conoce `Dead*` y `horse` [L].

Falta el estado "puede entrar al reto" como una sola consulta; no existe.

## 3. Qué pasa hoy si entra uno y el otro no

1. **Puppets.** El que está fuera deja de ver al que está dentro cuando el mapa es otro (`oa_puppet.c:268`) [L]. Con
   `MainField/` en ambos casos (Eventide), lo ve en su sitio [H].
2. **Enemigos.** Los dueños de enemigos, el reparto del anfitrión y `can_own` exigen el mismo mapa
   (`oa_enemies.c:358-362`, `:488`, `:1117`) [L]. Con mapa propio no se mezclan.
3. **Botín.** `oa_loot.c` **no usa `oa_same_map`** en ninguna parte [L: búsqueda sin resultados]. Lo que cae de un
   enemigo o de un cofre se reparte a todos los demás con sus coordenadas, estén en el mapa que estén
   (`oa_loot.c:5-9`) [L el diseño; H el efecto]. En un reto con inventario quitado, un arma que consiga el A
   **aparecería en el mundo de B** (en unas coordenadas del mapa del reto que no significan nada fuera, o fuera del
   reto con armas de la sala) [H]. Es el punto más peligroso (R2).
4. **Flags.** Cada fotograma el invitado envía los suyos y recibe la unión; el anfitrión aplica la unión completa
   (`oa_flags.c:227-260`) [L]. Con `FinalTrial*` y las puertas del reto dentro de esa unión, un flag que pone el que
   está dentro **se aplica al que está fuera**, y al revés (`oa_flags.c:262-300`) [L mecanismo; H que Eventide/Prueba
   estén en la lista]. En el peor caso el que está fuera "tiene" progreso de un reto que no ha jugado, o el que está
   dentro recibe de golpe un flag de sala que lo salta (R3).
5. **Recompensas por flag.** Algunos flags dan objeto (`IsGet_Obj_*` runas, `FindDungeon_Finish`, `Clear_Dungeon*`,
   `Get_HeartUtuwa_*`, `oa_flags.c:299-308`). Si el reto **vuelve a poner** un flag de runa (p. ej. al devolver el
   inventario) y la unión lo ve como "mío", se podría repetir la entrega [H]. No hay registro de entregas (T415/T416
   son para eso) [L].
6. **Luna de Sangre.** `oa_quests_update` detecta una Luna de Sangre cuando se apagan 64 o más flags de reaparición de
   golpe (`oa_flags.c:226-234`) [L]. Un reto que apague flags en bloque al entrar podría disparar esa heurística [H].

## 4. Diseño propuesto

### 4.1 Principio

**No reimplementar lo que hace el juego.** El juego ya sabe quitar y devolver el inventario del reto. Lo que hay que
hacer es lograr que **los dos jugadores pasen por el mismo camino del juego a la vez**, y **aislar a los dos del
resto del mundo** mientras dura. No se copia, guarda ni restaura ningún inventario desde el programa.

### 4.2 Piezas

| Pieza | Qué hace | Apoyo existente |
|---|---|---|
| **A. Detector de reto** | Una función pura `trial_kind(mapa, evento, flags)` que devuelve `NONE`, `SWORD` o `EVENTIDE`. Se alimenta de `oa_g_map` y, si hace falta, del nombre del evento activo | `oa_sample.c:481-492`; `framepace_in_event` |
| **B. Aviso de entrada** | Cuando uno está a punto de entrar (evento/mapa del reto), envía `TRIAL_ENTER{kind}` al otro por el canal de reglas | cable de F4 (T409) |
| **C. Entrada del compañero** | El otro ve un aviso con **aceptar / rechazar**. Al aceptar, el programa **lanza el mismo disparador del juego** (`oa_start_event`, como el de la pizarra, `oa_flags.c:81-107`) y, para Eventide, lo lleva al punto de entrada con el teletransporte seguro (T407/T418) | `oa_start_event`; T407, T418 |
| **D. Estado "en reto"** | Mientras alguno está en un reto: botín compartido **desactivado en ambos sentidos** (`oa_loot`), unión de flags **sin** `FinalTrial*`/flags de Eventide (se unen al salir con el reto completo), sin soltar objetos marcados (T423), sin teletransporte hacia el que está dentro (T418), sin PvP | `oa_loot_forget` (`oa_loot.c:48-52`); T423, T418, T434 |
| **E. Salida** | Al volver los dos a `MainField/` o terminar el reto, se vuelve a la normalidad. La unión de flags aplica lo que cada uno haya hecho **una sola vez** (los flags de progreso completo) | `oa_quests_update` |

### 4.3 Qué hacer si el compañero no puede entrar

Orden de preguntas del aviso (B→C):

1. El compañero **no responde** a tiempo (`TRIAL_ENTER` sin respuesta en 15 s) → el reto empieza solo para quien
   entra; se avisa a los dos: «El reto es solo de X» [H del tiempo; decisión de producto].
2. El compañero **está en un santuario, en una escena, muerto, a caballo o cargando**: se responde `BUSY` con el
   motivo y quien entra **espera**; puede pulsar Cancelar (vuelve a no entrar) o seguir solo.
3. El compañero **rechaza**: el que quiere entrar decide: entrar solo (el reto es suyo) o cancelar. El rechazo no
   debe dejar al otro sin inventario: el juego solo se lo quita si el otro entra.

Nota: esto **pide una decisión del dueño** que el ADR-025 n.º 7 no cierra («entran los dos» ¿también si uno no
quiere?): ver 7.

### 4.4 Salida, muerte y desconexión

- **Uno sale o muere antes que el otro:** cada juego aplica su regla del reto; el que queda dentro **sigue**, sin
  expulsarlo [H; lo contrario obligaría a tocar el inventario del que se queda]. Al terminar uno, el estado "en reto"
  de ese jugador pasa a `NONE`.
- **Se desconecta uno:** el otro sigue su reto igual; el estado del desconectado se borra con la sesión
  (`oa_frame.c:32-48`, `RT_ON_RESET`) [L mecanismo]. No se guarda nada de la sesión ni punteros del invitado entre
  reinicios (regla de `AGENTS.md`).
- **Guardado:** el programa no guarda ni restaura inventario. Hay que **medir** si el juego escribe el guardado
  durante el reto (M5) y, si lo hace, la copia de seguridad antes de hospedar o unirse (T413) cubre el riesgo.

### 4.5 Pruebas y reglas de código

- La lógica de A y D es pura y se prueba sin juego (como T430/T433).
- Los textos nuevos (aviso, aceptar, rechazar, motivos de `BUSY`) van a `runtime/ui/i18n/*.inc` y se pasa
  `python3 tools/i18n_check.py` (T437).
- Nada de direcciones nuevas sin `g_site` ni patrones en `tools/game_sites.py`.

## 5. Riesgos

| Id | Riesgo | Marca | Cómo se cubre |
|---|---|---|---|
| R1 | Un fallo o cierre a mitad del reto deja al jugador sin su inventario real (si el juego lo guarda fuera de la partida y el guardado se escribió en medio) | H | M5; T413 (copia de seguridad antes de hospedar o unirse); no tocar `fs.c` |
| R2 | El botín compartido pasa armas del reto al mundo del compañero, o al revés | H (sin `oa_same_map` en `oa_loot.c`) | D: botín cerrado mientras alguno está en reto; prueba M4 |
| R3 | La unión de flags salta el reto del otro o le da progreso que no ha jugado | H | D: sacar `FinalTrial*` y los flags de Eventide de la unión mientras haya reto |
| R4 | La heurística de Luna de Sangre (`oa_flags.c:226-234`) salta al entrar | H | M6; si salta, suspenderla mientras haya reto |
| R5 | Entrada forzada con `oa_start_event` en mal momento (escena, caída, caballo) | H | solo desde el estado "puede entrar" de 2.5; si el evento no arranca, `BUSY` |
| R6 | El tipo de mapa de la Prueba coincide con el campo y `oa_same_map` no distingue | H | M1; detector A por otra vía |

## 6. Qué hay que medir en el juego (para el orquestador)

Todas las órdenes son de `tools/swctl.py` (`docs/CONTROL.md`); varias son del modo desarrollador
(`SAVAGEWIND_CONTROL` o `SAVAGEWIND_DEV=1`). Se usan **copias del guardado** (`SAVAGEWIND_SAVE_DIR`); el guardado del
dueño no se toca. Anotar versión (1.9.0), FPS, GPU y ajustes.

**M1 · Tipo de mapa de la Prueba de la Espada y de Eventide.**
1. Con el DLC (`aoc1/`, `aoc2/`) y un guardado que tenga la Espada Maestra, abrir una sola instancia.
2. `swctl state` y `swctl pos` antes de entrar; entrar en la Prueba (carga) y mirar en el registro la línea
   `[online] map: <tipo>/<nombre>` (`oa_sample.c:486`). Repetir al cruzar el límite de Eventide
   (`swctl goto <x> <y> <z>` a la costa de Eventide; la posición exacta sale de `swctl pos` jugando).
3. Anotar el valor literal en cada caso. Responde 2.3 y R6.

**M2 · Disparador y evento.**
1. Con la sesión de arriba, ejecutar `swctl state` cada segundo al entrar (o `swctl --wait 1 state`) y, en el registro,
   buscar `[online] event` / nombres `Demo…` (`hle/framepace.c:96-100`).
2. `swctl flag <nombre>` sobre los flags que se sospechan (`FinalTrial…`, candidatos que salgan de `quests.txt`
   del generador local) antes y después. Anotar cuáles cambian.

**M3 · Qué pasa hoy (dos juegos, A entra y B no).**
1. A hospeda, B se une; se llevan los dos a Eventide (`goto` al mismo punto, dos instancias, ver `docs/ONLINE.md`).
2. A cruza el límite. Registrar en A y en B: `[online] map:`, `swctl pos`, puppets (`swctl actors OnlinePuppet`, el
   nombre exacto sale de `oa_puppet.c`), y si B conserva su inventario.
3. Repetir con la Prueba de la Espada. Responde 3.1, 3.2.

**M4 · Botín.** Con A dentro de la Prueba y B fuera: A mata un enemigo que suelte arma. En B comprobar
(`swctl actors Weapon_`) si aparece el arma en su mundo. Responde R2.

**M5 · Guardado.** En la Prueba, con el guardado en una carpeta de copia: mirar la fecha de modificación de los
ficheros (`fs.c` registra `[save] write ...` en el registro) **durante** el reto, y qué contiene el inventario del
guardado. Cerrar el juego a mitad con `swctl exit` y arrancar: ¿vuelve el inventario? Es la medida de más riesgo.

**M6 · Flags y Luna de Sangre.** `SAVAGEWIND_ONLINE_QUEST_LOG=1` (`oa_flags.c:207`) con A en la Prueba: mirar si
aparecen líneas `quest flag FinalTrial…` aplicadas en B y la línea `a Blood Moon here`.

**M7 · Entrada del compañero.** `swctl event <flujo> [entrada]` en B con el flujo que dé M2 (`docs/CONTROL.md`): ¿arranca
el mismo reto en B sin cargar de nuevo desde la Espada? Si no arranca, el diseño 4.2.C no es viable y la entrada
del compañero tiene que ser el recorrido normal (la mitad de la entrada, a mano).

## 7. Propuesta de tareas

| Id | Tipo | Qué | Depende de |
|---|---|---|---|
| T458 | investigación (orquestador) | Ejecutar M1–M7 y anotar los valores en esta bitácora | — |
| T459 | lógica pura | `trial_state`: detector `NONE/SWORD/EVENTIDE`, consulta "puede entrar" y reglas de D, con tests sin juego | T458 (M1, M2) |
| T460 | protocolo | Mensajes `TRIAL_ENTER / ACCEPT / REJECT / BUSY` en el salto de versión de F4 | T409, T459 |
| T461 | reglas | Aislar durante el reto: botín, unión de `FinalTrial*`/Eventide, soltar marcado, teletransporte y PvP | T459, T423, T418, T434 |
| T462 | reglas | Entrada conjunta: aviso, aceptar/rechazar, arrancar el mismo evento en el compañero | T460, T461, T458 (M7) |
| T463 | ui/i18n | Textos del aviso y de los motivos en 13 idiomas | T462, T437 |
| T464 | pruebas | Escenarios de bot y recorrido de dos juegos: A entra, B acepta / rechaza / ocupado / se desconecta | T462 |

**Viabilidad:** viable en principio, **condicionado a M5 y M7** (guardado seguro y arranque del mismo evento en el
compañero). Si M7 falla, queda la entrada guiada (aviso + teletransporte al punto de entrada + el compañero entra
a mano). Si M5 muestra que el inventario real queda en el guardado en mitad del reto, el diseño necesita una copia
automática antes de entrar (T413) antes de seguir.

## 8. Decisión que falta del dueño

El ADR-025 n.º 7 dice «entran los dos juntos». Falta decidir qué pasa cuando **uno no quiere o no puede**:
(a) el reto empieza solo para quien entra, (b) no empieza hasta que los dos acepten, (c) se pregunta a quien entra.
Recomiendo (c) con tiempo límite, para que nadie pierda el inventario por un aviso que no vio.

## 9. No verificado

Todo lo marcado [H]. En particular: cómo dispara y resuelve el juego ambos retos, el tipo de mapa de cada uno, si el
guardado contiene el inventario durante el reto, si `quests.txt` incluye flags de Eventide, y si `oa_start_event`
puede arrancar el reto en el compañero. No he compilado, ejecutado ni medido nada.
