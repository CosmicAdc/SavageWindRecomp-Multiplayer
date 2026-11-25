# Bitácora · `goto` deja a Link fuera del destino o congelado (N3, HE-13)

- Fecha: 2026-11-25
- Quién: subagente Claude Opus (encargo `ENCARGO-GOTO.md`), rama `coop/GOTO`. Sin compilar ni lanzar el juego (ADR-006)
- Fuentes leídas: `runtime/control_tp.c`, `runtime/control_game.c` (`cmd_goto`), `runtime/control_cells.c`,
  `runtime/control_places_data.inc`, `runtime/services/test_warp.c`, `runtime/services/fs.c` (~l. 2058),
  `runtime/oa_rules.c` (`oa_players_collide`), `tools/patches.py` (`_tp_gate`, solo lectura), `tools/coop/recorrido.py`,
  `tools/coop/dos_juegos.sh`, `specs/mapa/08-estados-del-juego.md` (HE-13, PE-8)
- Datos leídos (solo lectura, nada copiado al repositorio): `$SW/pruebas/logs/A.log` y `B.log` (pasada T208,
  2026-11-25 00:42) y `PlayerSavePos` de `$SW/pruebas/guardado-{A,B,base}/save_0/*/game_data.sav`

Niveles (constitución, regla 16): **[L]** leído en el código, **[Log]** visto en un log o un archivo de una partida
ya jugada, **[H]** hipótesis, **[S]** prueba sintética. Nada de esto se ha visto en el juego en esta tarea.

## 0. Lo primero: las coordenadas estaban cambiadas

| Coordenadas | Qué son de verdad | Evidencia |
|---|---|---|
| -1041,6 250,2 1812,1 | Salida del Santuario de la Vida (*Shrine of Resurrection*): el punto `plateau` de `SAVAGEWIND_TEST_WARP` | `test_warp.c:30` [L] |
| -568,66 246,44 1697,44 | **Cima** de la torre 7 (Torre de la Meseta): la posición inicial del juego `Tower07`, sobre la plataforma de arriba | `control_places_data.inc:146` (`ground` = true) [L] |
| -569 193-195 1697-1699 | **El pie de la torre 7**: mismas x y z que la cima, unos 52 m más abajo | x y z dentro de 2 m de la línea anterior [L] |

El encargo dice al revés: que -1041 es la torre 7 y que -569 es la salida del santuario y el sitio de la partida
guardada. La bitácora `2026-11-24-F1-prueba-ventana-dueno.md:10` ya anotaba "`goto tower 7` (`x=-1041.60 y=250.19
z=1812.10`)". Con los datos correctos, los síntomas se leen de otra forma:

- Al cargar, los dos juegos aparecen donde manda el *warp*: `A.log:371` y `B.log:361` dicen
  `[place] swr:1 x=-1041.60 y=250.20 z=1812.10` justo después de `[warp] Link starts at -1041.6, 250.2, 1812.1` [Log].
  Las seis ranuras de `guardado-A` (y de `guardado-base`) están entre x -1083 y -1042, junto al santuario, con
  `IsGet_PlayerStole2` = 0: todavía sin paravela, al principio del juego [Log].
- Un `pos` de -569,7 193,5 1698,9 **no puede ser el de recién cargado** con esas partidas: es Link en el pie de la
  torre 7 **después** de un `goto tower 7` [H, muy probable].
- `A.log:506`: tras `goto tower 7` desde el punto de carga, Link está en `x=-568.96 y=195.47 z=1697.96`. Es decir,
  llegó en x y z (a 0,6 m del destino) y **52 m más abajo** que el destino, y=247,44 [Log].
- `B.log:1356-1357`: el autoguardado de B escribió `x=-568.49 y=177.69 z=1697.16`, **18 m por debajo** del suelo donde
  estaba A [Log]. Es el `PlayerSavePos` actual de `guardado-B/save_0/0` (00:41, mapa E-6) [Log]. La línea `[warp]`
  aparece también en ese autoguardado porque `fs.c` parchea cualquier apertura de `game_data.sav` [L].

## 1. Causa de cada síntoma

### Síntoma 2 · "done / Arrived" pero `pos` sigue en -569 195 1697

**Causa: el `goto` sí funcionó.** -569/1697 es el destino en x y z. Link cae desde la cima de la torre hasta el pie
porque a esa altura no hay nada. Además, "Arrived" no mira la altura.

- `control_tp.c:95-97` `near_pos` compara solo x y z ("not y: Link falls after a teleport"). `finish(DONE, "Arrived")`
  sale en cuanto x y z están a menos de `ttol` (2,0 en un salto lejano; l. 204-207 y 177) [L].
- Un destino de torre es la **cima** (`control_places_data.inc:146`, más 1 de `place_lift` en `control_game.c:369`) [L].
- En el guardado de prueba no hay paravela y la partida está junto al santuario del principio. Lo más probable es que
  **la torre de la Meseta todavía no haya emergido**: en el juego sube cuando Link activa su pedestal. Así que en
  y ≈ 247 no hay plataforma [H]. Otra posibilidad, que acaba igual: la torre existe pero su colisión no está cargada
  cuando termina la espera de `settling`, porque esa espera solo mira el terreno (`rt_tp_gate_end`, l. 80-85) [H].
- Debajo hay dos niveles a la misma x y z: A quedó en y 195,5 y B en y 177,7. B atravesó la superficie y quedó
  **dentro** de la estructura [Log; la interpretación es H].

### Síntoma 1 · `teleport status` = `failed: Position verification failed` y Link congelado

- "Position verification failed" solo sale del estado `TEST` (`control_tp.c:214`). Es decir, cuando el camino directo
  (`path_addr`, la celda `player.pos`) no sirvió o no se verificó, la búsqueda en memoria escribió el destino en el
  último candidato y `PlayerInfo +0x88` no lo siguió en 3 fotogramas y 120 ms (`WAIT_FRAMES`, `WAIT_NS`) [L].
- **No hay un bloqueo de posición que se quede puesto al fallar** [L]:
  - `finish(FAILED)` llama a `hold_stop()`, que pone `hold_on` = 0 y `hold_cell` = 0 (l. 86, 115).
  - `land_follow` y `then_look` solo se activan con `DONE` (l. 114, 118).
  - Lo único que escribe `player.pos` en cada fotograma es: el *hold* (solo en `HOLD` o con `hold_on`), el modo
    vuelo (`control_fly.c`, solo si está encendido) y el empujón entre jugadores (`oa_rules.c:oa_players_collide`,
    como mucho 0,5 por fotograma y solo a menos de 0,7 m de la marioneta del otro).
  - Un detalle menor: `hold_on` sigue a 1 mientras dura la búsqueda que viene después de un `VERIFY` fallido; se apaga
    al terminar.
- Hay dos explicaciones del congelado, las dos **[H]**:
  - **H1, Link atascado en la geometría del pie de la torre.** Encaja con y = 193,5, unos 2 m por debajo de la
    superficie donde estaba A (195,5), y con B dentro, a 177,7. Atascado, el *stick* no lo mueve y la gravedad no
    cambia su y. Un salto corto (de -569,7/1698,9 a la torre 7 hay 1,8 m en x z; sin *hold*, con `ttol` = 0,45)
    tendría que vencer a un juego que lo devuelve a su sitio en cada *update*. Una sola escritura pierde: falla
    `VERIFY`, después la búsqueda y después `TEST`, y sale "Position verification failed".
  - **H2, el mundo se queda en pausa para siempre** (pausa por mapa sin cargar, la del comentario `control_tp.c:14-20`;
    es la hipótesis del mapa 08). Si el `goto` que falló era un salto **lejano** que fue por la búsqueda, el destino se
    escribe con el *hold* puesto. Pero si `PlayerInfo +0x88` tarda más de 120 ms en seguirlo (el primer *update* tras
    un salto de 485 m puede tardar), el código devuelve a Link al origen (l. 213), da el salto por fallido y quita el
    *hold* (l. 115). El juego puede quedarse entonces esperando un mapa que solo carga su propio *update*: el
    congelado documentado de los saltos lejanos. Encaja con "ni la gravedad cambia su y" y con que el reloj se pare.
- Para distinguirlas (§3, prueba B): con H1 el reloj (`state`) avanza y `camera` responde; con H2 la hora se para y
  `camera` dice "No world loaded".

### Síntoma 3 · `teleport -1041.6 250.19 1812.1` lo desbloquea y después `stick` funciona

Desde el pie de la torre son unos 485 m, así que es un salto lejano (`HOLD_DIST` = 300): `want_hold` y `hold_on` = 1
**antes** de escribir (l. 257-258). `hold_write` escribe el destino en cada *update* y en cada fotograma hasta que el
suelo esté listo 12 veces seguidas (l. 72-85, 173-177) [L]. Con las dos hipótesis eso lo arregla: con H1 la posición
se fuerza en cada *update* durante segundos y Link sale de la geometría; con H2 la puerta `tp_gate` vuelve a responder
"listo" y el mundo sigue. Además el destino es suelo firme (el punto de carga). No hace falta ningún arreglo en el
*runtime* para esto [H].

### Interacción entre el *warp* de arranque y la partida

El *warp* reescribe `PlayerSavePos`, el mapa y `IsGet_PlayerStole2` en **cada** apertura de `game_data.sav`, también
en los autoguardados (`fs.c:2058-2063`) [L]. La partida **no** coloca a Link después del *warp*: al cargar aparece
justo en el punto del *warp* (A.log:371, B.log:361) [Log]. Ojo con un efecto lateral: el autoguardado guarda la
posición real en la partida de prueba. `guardado-B/save_0/0` tiene ahora a Link **dentro del pie de la torre**
(-568,5 177,7 1697,2) [Log]. `pruebas/abrir_invitado.cmd` **no** pone `SAVAGEWIND_TEST_WARP`, así que el dueño
cargaría ahí a B, probablemente atascado [H]. Antes de la próxima sesión con ventana hace falta o `dos_juegos.sh
restaurar` (borra `guardado-A` y `guardado-B` y los rehace desde `guardado-base`), o añadir
`set SAVAGEWIND_TEST_WARP=plateau` a ese `.cmd`. Lo decide el dueño: aquí no se ha tocado ningún guardado.

## 2. Arreglo: `tools/coop/recorrido.py` (uso indebido; el *runtime* no se toca)

`goto tower 7` no es un punto de encuentro válido con estas partidas. Cambios:

1. `ENCUENTRO = (-1041.6, 250.2, 1812.1)`, el punto de carga del *warp* (suelo firme visto en los dos logs), en lugar
   de `goto tower 7` en los pasos 2, 5 y 10 y en `enemigos()`. Desde ahí hay unos 235 m al campamento, más de los
   60 m que pide `enemigos()`. El título del paso 2 pasa a "goto al punto de encuentro en ambos (CU-02)".
2. `esperar_quieto(..., tp=True)`: un `teleport status` = `failed` cuenta como Falla, con su mensaje. Antes un fallo
   contaba como "quieto" y Link congelado parecía "llegado" (HE-13).
3. `Ctx.mover()` lanza el `goto` o `teleport` y no mira el estado cuando `goto` responde `already_there`: entonces no
   empieza ningún salto y el estado guardado es el del salto anterior. `ir()` lo usa.
4. `Ctx.llegar(xs, destino)` hace `goto` a coordenadas, espera y comprueba la posición final, **también y**:
   como mucho 3 m en x z y 10 m en y. Así un destino sin suelo debajo ya no pasa por "Arrived".

`git diff --check` limpio y `py_compile` correcto. **[S]**: prueba sintética con respuestas de `swctl` falsas
(fuera del repo, en el *scratchpad*), con 5 casos y 5 PASA:

- llega;
- cae 54,7 m desde la cima, que da Falla;
- `failed`, que da Falla con "Position verification failed";
- `already_there` con un `failed` viejo, que es correcto;
- 485 m fuera en x z, que da Falla.

**No escrito (propuesto para F2 "herramientas" / T334, en el *runtime*):**

- (a) que `teleport status` informe de la caída: la y del destino y la y final;
- (b) con H2 confirmada, en un salto lejano esperar más (por ejemplo 2 s) antes de dar `VERIFY`/`TEST` por fallido,
  y no devolver a Link al origen ni quitar el *hold* mientras el mapa del destino se carga;
- (c) una nota en `docs/CONTROL.md` ("Named places"): el destino de una torre es su cima y solo existe cuando la torre
  ha emergido.

Ninguno es pequeño y seguro sin antes ver el juego.

## 3. Cómo comprobarlo (órdenes para el orquestador)

`PY=tools/venv/Scripts/python.exe`, dos juegos con `bash tools/coop/dos_juegos.sh arrancar`, partidas de
`guardado-base`.

**A. Síntoma 2 (la torre no está).** Solo en 7777:

```
$PY tools/swctl.py --port 7777 pos                    # esperado: ~ -1041.6 250.2 1812.1 (punto del warp)
$PY tools/swctl.py --port 7777 places tower 7         # destino -568.66 246.44 1697.44
$PY tools/swctl.py --port 7777 goto tower 7
$PY tools/swctl.py --port 7777 teleport status        # repetir cada segundo hasta done o failed
$PY tools/swctl.py --port 7777 pos                    # repetir 3 veces, cada 3 s: ¿y ~ 247 o ~ 195 / 178?
$PY tools/swctl.py --port 7777 shot goto-torre7.png   # mirar si hay torre o solo el pie
```

Esperado si la hipótesis es cierta: `done`, `Arrived`, y entre 178 y 196, y en la captura no hay torre.

**B. Síntoma 1 (congelado), solo si Link queda con y < 196 y no se mueve:**

```
$PY tools/swctl.py --port 7777 state                  # dos veces con 5 s de diferencia: ¿avanzan frame y la hora?
$PY tools/swctl.py --port 7777 camera                 # ¿responde o dice "No world loaded"?
$PY tools/swctl.py --port 7777 stick l 0 1 60
$PY tools/swctl.py --port 7777 pos                    # ¿cambió?
$PY tools/swctl.py --port 7777 cell player.pos        # ¿coincide con pos?
$PY tools/swctl.py --port 7777 teleport -568.66 247.44 1697.44
$PY tools/swctl.py --port 7777 teleport status        # repetir: ¿failed "Position verification failed"?
$PY tools/swctl.py --port 7777 cell player.pos        # justo después: ¿vuelve a la y vieja? (el juego lo devuelve: H1)
$PY tools/swctl.py --port 7777 teleport -1041.6 250.2 1812.1   # tiene que liberarlo (síntoma 3)
```

Lectura: si la hora avanza y `camera` responde, es H1 (atascado). Si la hora se para y `camera` falla, es H2 (mundo
en pausa).

**C. El arreglo de `recorrido.py`:**

```
$PY tools/coop/recorrido.py --pasos 2
$PY tools/coop/recorrido.py --pasos 1-10 --enemigos
```

Esperado: el paso 2 ya no falla por la posición. Si falla, el mensaje dice si fue `failed` o cuántos metros quedó por
debajo. `pos` de A y B queda a unos 3 m de -1041,6 250,2 1812,1. Que A vea `OnlinePlayer` en el paso 2 es otro asunto
(línea base, hallazgo 2): este arreglo no lo toca.

## NO verificado

- Nada de esto se ha visto en el juego. No se ha compilado ni ejecutado el juego (prohibido por el encargo). Lo único
  ejecutado: `py_compile`, `git diff --check` y la prueba sintética.
- Que la torre de la Meseta no haya emergido en `guardado-base` [H]: la confirma o la descarta la prueba A.
- Qué congela a Link, H1 o H2 [H]: prueba B. Tampoco sé por qué en ese caso falló el camino directo (`player.pos`).
- El `pos` -569,7 193,5 1698,9 "tras cargar" del encargo no encaja con los logs ni con las partidas de A. Supongo que
  se tomó después de un `goto tower 7` [H].
- Que `ENCUENTRO` sirva para los pasos 5 y 10 con salto lejano: el *hold* lleva ahí a Link (es el desbloqueo del
  síntoma 3), pero `recorrido.py` modificado no se ha ejecutado.
- `guardado-B` tiene a Link dentro del pie de la torre; no se ha tocado.
