# Bitácora · F5 T540 · Qué pasa hoy en los santuarios a dúo + diseño de la invitación (T465)

- Fecha: 2026-11-25
- Quién: agente Sonnet (`coop/T540`) bajo el orquestador (ADR-006): **solo lee código y escribe**. No compila, no
  ejecuta pruebas ni el juego, no toca código. Sin desensamblado ni descompilaciones.
- Fuentes leídas: ADR-025 (n.º 10-13), `mapa/08` (filas Santuario y Bestia, HE-02, HE-05, puntos 2 y 3, PE-2),
  `mapa/07` §7, `docs/ONLINE.md` ("Players on other maps", l. 402-408; 462-482; 975-1000), `docs/CONTROL.md`,
  `runtime/oa_sample.c`, `oa_puppet.c`, `oa_enemies.c`, `oa_loot.c`, `oa_world.c`, `oa_rules.c`, `oa_flags.c`,
  `oa_frame.c`, `oa_core.c`, `online.c`, `online.h`, `ui/online_ui.mm`, `control_places.[ch]`,
  `control_places_data.inc`, `control_tp.c`, `tools/online_mod.py`, tareas T409, T419, T465, T514, T540, bitácora T451.
- Marcas: **[L]** leído en el código de este checkout · **[H]** hipótesis (no comprobada, o efecto deducido) ·
  **[DOC]** lo dice la documentación del PR, sin comprobar aquí.
- **Alcance honesto:** nada de esto se ha ejecutado. La tabla "visto en el juego" que pide el `verify` de T540 la
  rellena el dueño con la matriz de la sección 6. Este documento es la parte de lectura y el diseño.

## 1. Cómo sabe el programa en qué mapa está cada jugador

| Hecho | Dónde | Marca |
|---|---|---|
| El mapa propio se lee del juego (tipo: `FN_MAP_TYPE`; nombre: `FN_MAP_NAME`) y se guarda en `oa_g_map` como `"<tipo>/<nombre>"`; **el nombre solo se añade si el tipo no es `MainField`**: el campo es siempre `"MainField/"`, un santuario `"CDungeon/Dungeon038"` | `oa_sample.c:481-488` | [L] |
| Cada cambio queda en el registro como `[online] map: <mapa>` | `oa_sample.c:486` | [L] |
| `local_map` se llama desde `oa_sample_pose`, que no muestrea con el juego en pausa | `oa_sample.c:737`, `:878-882` | [L] |
| El mapa viaja en cada STATE (`OnlinePose.map[32]`) solo si la pose tiene menos de 0,5 s; si no, llega `""` | `online.c:822-831`, `:925`; `online.h:93-94` | [L] |
| `oa_same_map`: mismo mapa si el de alguno está vacío o los textos son iguales | `oa_sample.c:490-492` | [L] |
| El mapa recibido solo se valida por "sin terminar → `""`"; **no se comprueba su formato** | `contratos/validacion-red.md:171` | [L] |
| Los santuarios son `CDungeon/DungeonNNN` (ejemplo real: `CDungeon/Dungeon038` en `docs/ONLINE.md:474`); las Bestias, `MainFieldDungeon/Remains*` | `ONLINE.md:405-406`, `:474` | [L doc; H que **todos** sigan ese patrón] |
| `control_places_data.inc` trae 136 santuarios `Dungeon000`…`Dungeon135`, con la clave idéntica al nombre del mapa y el número del juego | `control_places_data.inc` (136 líneas `PLACE_SHRINE`) | [L] |

## 2. Qué pasa hoy (lectura del código)

### 2.1 Entra uno solo (A dentro, B fuera)

| Qué | Efecto | Marca |
|---|---|---|
| El Link de A **desaparece** del mundo de B: `want` exige `oa_same_map`; si no, `oa_drop` (piezas al pool), salvo con el juego de B en pausa | `oa_puppet.c:268-276` | [L] |
| B **no tiene ninguna invitación ni aviso**: el único aviso del overlay es "X joined/left" | `online.h:183-185` | [L] |
| `can_own`: A deja de poder ser dueño de enemigos del campo de B | `oa_enemies.c:361-364` | [L] |
| Los enemigos del anfitrión que están cerca del compañero no cuentan si ese compañero está en otro mapa (rama del anfitrión) | `oa_enemies.c:488` | [L] |
| Mecanismos, objetos movidos, objetos rotos, empujes, golpes entre jugadores y tiempo meteorológico: filtrados por `oa_same_map` | `oa_world.c:176`, `:406`, `:863`; `oa_rules.c:128`, `:173`; `oa_flags.c:65` | [L] |
| El overlay **sí dibuja la figura y el nombre** de A en B aunque esté en otro mapa; solo lo oculta a más de 3 km (HE-05) | `ui/online_ui.mm:262-266` | [L] |
| Si A está pausado o cargando, la pose se pierde a los 0,5 s y llega `map = ""`, que cuenta como "mismo mapa": puede salir un fantasma de A en el campo de B (punto 2 del mapa 08) | `oa_sample.c:490-492`; `online.c:822` | [L mecanismo; H efecto] |
| **El cliente no filtra por mapa**: `oa_enemies_client` y `oa_enemies_show` no llaman a `oa_same_map`. Un invitado dentro de un santuario sigue borrando sus enemigos dentro de 170 m del *centro* del anfitrión (coordenadas del campo) y creando marionetas del anfitrión | `oa_enemies.c:728-800` (sin `oa_same_map`; `grep` solo lo da en :363, :488, :514, :1117) | [L código; H efecto en el juego] |
| `oa_loot_update` tampoco filtra por mapa: el botín se reparte con coordenadas del mapa de quien lo suelta | `oa_loot.c:138-175` (sin `oa_same_map`) | [L código; H efecto] |

### 2.2 Entran los dos al mismo santuario

| Qué | Efecto | Marca |
|---|---|---|
| Mismo texto de mapa → `want` verdadero → se crea el Link del otro, con `want` también por distancia (< 400 m en horizontal) | `oa_puppet.c:268-272` | [L] |
| Se ven, se envían los mecanismos y los objetos movidos (bolas, cajas) y se aplican en el otro; para lo que mueven los dos manda el de menor ranura | `ONLINE.md:975-983`, `:996-997`; `oa_world.c:406` | [L doc; H que funcione] |
| Los mecanismos de santuario como ascensor ocupado de verdad y las partes de una Bestia **no están probados** | `ONLINE.md:984-986` | [DOC] |
| El estado interno del rompecabezas **no va en flags** ("each entry starts it anew") | `ONLINE.md:996` | [DOC] |
| Los flags `Open_DungeonNNN`, `Enter_DungeonNNN`, `Clear_DungeonNNN` **sí** están en la lista compartida; al llegar uno, el juego del otro entrega el orbe (`Obj_DungeonClearSeal`) | `online_mod.py:881`; `oa_flags.c:304-306`; `ONLINE.md:466-476` | [L] |
| Probado por los autores del PR: a un santuario del plató (Dungeon038) activado y limpiado por el cliente, el anfitrión lo vio abierto y encendido y recibió runa y orbe | `ONLINE.md:473-480` | [DOC] |
| Las coordenadas dentro de un santuario están en el sistema del interior; con los dos en el mismo mapa son comparables | — | [H] |

### 2.3 Salir

| Qué | Efecto | Marca |
|---|---|---|
| Al volver uno a `MainField/` el texto de mapa cambia y `want` vuelve a ser verdadero; el Link del otro se crea de nuevo si está a < 400 m | `oa_puppet.c:268-272`; `oa_sample.c:486` | [L mecanismo; H que reaparezca bien] |
| El orbe llega a los dos por la unión de flags (`Clear_DungeonNNN`) | `oa_flags.c:304-306`; `coop_rewards.c:25` (clase `RW_MILESTONE`) | [L; H que llegue siempre: T411 cambia la unión] |
| Salir uno solo no afecta al otro: cada juego sigue el suyo | — | [H] |

## 3. Lo que se desprende para T514 (lista de cuidado, ordenada)

1. **Cliente dentro de un santuario con el anfitrión fuera** (`oa_enemies_client`/`oa_enemies_show` sin filtro): puede crear marionetas o borrar enemigos en coordenadas que no significan nada en ese mapa. Es HE-02. Probar con el **invitado** dentro y el anfitrión con enemigos cerca; y al revés.
2. **Botín** (`oa_loot_update`) sin filtro de mapa: soltar o recoger algo dentro con el otro fuera (y al revés).
3. **Pausa o carga con `map = ""`** (punto 2 de mapa 08): ¿sale un fantasma del compañero en el campo mientras el otro entra al santuario, durante la pantalla de carga?
4. **Overlay** (HE-05): ¿se ve el nombre y la flecha de A en B mientras A está dentro?
5. Ascensor ocupado de verdad, bolas y cajas, un santuario de giroscopio (`pc_gyro_scene.c`).
6. Reaparición del compañero al salir y que el orbe llegue a los dos.
7. Una vez más con la entrada en ambos mapas: ¿llegan a la vez o hay un desfase?

## 4. Diseño de T465 (invitación a santuario)

### 4.1 Idea central: sin mensaje nuevo, sin T409

**El compañero ya recibe el mapa del que entra en cada STATE** (`online.c:925`). Así que quien **recibe** la
invitación puede **deducirla** de ese campo: no hace falta un mensaje nuevo, ni subir `PROTOCOL`, ni pasar por T409
(que solo define 12 mensajes de otras funciones; ver `tasks.md` F4, T409 paso 2). [L que el dato llega; H que sea
suficiente: es la propuesta]. Ventajas: sin cambio de cable, sin autoridad nueva (T409 paso 5), sin límite de tasa
nuevo; funciona con cualquier combinación anfitrión/invitado porque el servidor ya reenvía los STATE.

Qué se pierde: A no sabe si B aceptó, rechazó o no contestó. ADR-025 n.º 11-13 no lo pide. Si el dueño quiere que A vea
"B viene", **ahí sí** haría falta un mensaje (TP_REPLY de T409 sirve por tamaño y destino; es una decisión a preguntar,
no a tomar aquí).

### 4.2 Detectar la entrada (en el juego del que recibe, en el hilo del juego)

Una función pura (`runtime/coop_shrine_invite.c/.h`, sin cabeceras del juego ni de red, como T410) recibe cada
fotograma, por cada otro jugador: `used`, `flags` (LOADING), `pose.valid` y el texto de mapa, y devuelve eventos.

- Máquina por jugador: `ultimo_mapa_estable` (el último texto **no vacío**). Un `""` **no** cambia el estado (es una
  carga o una pausa; `online.c:822`).
- Evento `ENTRA(n)`: el texto pasa de `MainField/` (o de cualquier otro) a un texto con **formato exacto**
  `CDungeon/Dungeon` + 3 dígitos, con `n` entre 0 y 135 y presente en la tabla de lugares. Nada más: ni
  Bestias Divinas, ni `AocField`, ni otros. Ningún texto de la red se usa como formato, ni como ruta, ni como
  clave (regla "nunca aceptar nombres sin lista").
- Anti-rebote: se necesita ver el mismo `CDungeon/…` en 2 STATE seguidos (≈ 0,1 s) antes de lanzar el evento, y no se
  repite el mismo `(jugador, n)` durante 10 s (por si la pose salta entre texto y `""`).
- **No** se invita si el que entra ya estaba dentro cuando nos unimos a la sesión (primer STATE visto con mapa
  `CDungeon/…`), salvo que el dueño diga lo contrario: si no, al unirse sale una invitación vieja. Se anota como
  pregunta (sección 7, P1).
- `SAL(n)`: el texto vuelve a `MainField/` o a otro. Si la invitación estaba abierta, se cierra ("ya salió").

### 4.3 Qué santuario es y de dónde salen las coordenadas

- Nombre: el texto `CDungeon/DungeonNNN` ya es el identificador; el número `NNN` es el del juego [L:
  `control_places.h:12`, `control_places_data.inc`].
- La puerta exterior, de `place_by_number(PLACE_SHRINE, n)` (`control_places.h:22`): campos `x,y,z` y `ground`. Sale de
  las posiciones de arranque del juego (`StartPos` de `Static.smubin`, "where the game puts Link when he warps
  there", `CONTROL.md:141`) para 120 santuarios, de medidas reales para 14 y de datos del mapa de la comunidad para 2
  (`CONTROL.md:141-143`). **Las coordenadas se toman siempre de la tabla local, nunca de la red.**
- Caso a vigilar: el 135 (`Ruvo Korbah`) tiene `ground = false` (el marcador flota sobre el suelo, hasta 100 unidades,
  `CONTROL.md:142`); y el 134 y el 133 son de los medidos. Se propone aceptar el 135 igual, con la caída (el viaje
  `goto` ya la maneja: "Link arrives 2 to 3 units above the ground and drops the rest", `CONTROL.md:86`), y ponerlo en
  la prueba (sección 6, G6).
- `[H]` Que la posición de arranque sea **delante de la puerta**, donde se entra andando, y no el centro del santuario o
  el punto de aparición tras salir: no se ha medido. `tools/check_places.py` mide la altura, no la puerta (se anota
  en la prueba).

### 4.4 Descubierto al entrar andando

- Lo que pasa **hoy**: los flags `Open_DungeonNNN`, `Enter_DungeonNNN` y `Clear_DungeonNNN` ya van en la lista
  compartida (`online_mod.py:881`) y los autores probaron que activar un santuario en un juego lo deja abierto y
  encendido en el otro [DOC, `ONLINE.md:473-476`]. Es decir, **los santuarios que el compañero activó ya llegan por
  flags**; el aviso solo tiene que llevarle a su puerta.
- Lo que **no sé**: si al entrar por el ascensor, el juego pone solo el estado de "descubierto" en el mapa del que
  entra, o si hace falta tocar el terminal exterior (un santuario no activado tiene el ascensor cerrado: `ONLINE.md`
  l. 982-984: "closed in the test save, didn't run by itself") [H]. Dos casos que el diseño **no** debe suponer
  resueltos:
  1. **El santuario del que entra A estaba ya activado en A pero no en B**: el flag `Open_DungeonNNN` de A llegará a B por
     la unión, así que B llega a una puerta que ya funciona. Si T411 cambia la unión (el anfitrión deja de aplicar los
     flags del invitado), el flag de un invitado **no** llega al anfitrión: B (anfitrión) llegaría a una puerta cerrada.
     Hay que probarlo (G3, G4).
  2. **Santuarios ocultos, bajo tierra o de misión**: B puede llegar a una puerta que el juego aún no tiene abierta.
     El dueño dijo "no fuerces la entrada" (T465, *no hagas*); la invitación debe dejar esto en el aviso, no
     resolverlo.
- Conclusión: "queda descubierto en su partida" (ADR-025 n.º 12) lo hace el juego al entrar, **si la puerta funciona**.
  Qué hace falta para que funcione lo decide la prueba G3/G4, no esta lectura. Si no basta, la tarea siguiente sería
  poner el flag `Open_DungeonNNN` desde la lista `quests.txt` antes del viaje (T410/T411 mandan sobre esa puerta;
  decisión del dueño a pedir).

### 4.5 El aviso (qué dice y cuándo)

- Texto propuesto (dentro de `tr("…")` para `i18n_check.py`): "%s entered %s. Join?" con el nombre del jugador y el
  nombre del santuario en inglés de la tabla (`Place.name`, ej. "Ne'ez Yohma Shrine"). Aceptar / Rechazar,
  ≈ 30 s (ADR-025 n.º 13). Reutilizar el sistema de avisos de T428 (`online_notice_push`), que solo guarda
  **identificador y nombre** (T428 paso 1); el aviso con botones y cuenta atrás es un trozo nuevo de interfaz
  (`ui/coop_hud.mm` de F4, T428/T425), no existe en `online_ui.mm` hoy [L: `online.h:183-185`, solo "joined/left"].
- Atajos: `Aceptar`/`Rechazar` con los botones del juego y también con el panel de la pestaña (porque el juego está
  corriendo y el menú del juego no debe abrirse): se resuelve en la tarea de interfaz.
- Si se abre otra invitación mientras hay una abierta: la nueva sustituye a la vieja si es del mismo jugador; si es de
  otro, se pone en cola de 1.

### 4.6 Estados en los que **no** se puede aceptar (y qué se muestra)

La función pura recibe un `CoopShrineCtx` con estos datos y devuelve `CAN_JOIN` o el motivo:

| Estado de B | Fuente actual | Resultado |
|---|---|---|
| Pausado o con menú que pausa | `framepace_paused()` | aviso se queda abierto; aceptar queda deshabilitado hasta que salga (el tiempo corre) |
| En una escena o evento | `framepace_in_event` (`hle/framepace.c:96-100`, [L] por T451) | igual; **no se interrumpe** (como el viaje de T419 paso 5, ADR-011 n.º 37) |
| Cargando | `ONLINE_F_LOADING` / `UI_PAUSE_LOAD` | igual |
| Muerto o en pantalla de fin | `oa_g_local_dead` (`oa_sample.c:739`) | rechazo con motivo; no se abre |
| Ya dentro de otro santuario o en otro mapa que no es `MainField/` | `oa_g_map` | rechazo con motivo ("estás en otro mapa"): `goto` no cambia de mapa (lo dice la tarea T540 paso 2; `control_tp.c` no tiene guardia de mapa [L]) y un salto desde dentro no sirve |
| Ya en **el mismo** santuario | `oa_g_map == pose.map` | no se invita (ya está dentro) |
| En un reto de T451 (Eventide, Prueba de la Espada) | estado de T451 | rechazo con motivo; el aislamiento de T451 manda |
| A caballo, planeando, nadando, escalando, en el aire | `coop_tp_can` de T418/T419 | igual que el viaje "ir" de T419: se pospone o se rechaza con el mismo criterio (`T419` paso 2, 8) |
| El que entró ya salió (`SAL`) | máquina de 4.2 | el aviso se cierra: "X ya salió" |
| El jugador que invita ya no está en la sesión | `used` | se cierra |
| Ajuste apagado | 4.7 | no se abre |
| Un santuario de la lista con `ground = false` (el 135) | tabla | se permite, ver 4.3 |

### 4.7 Opción de ajustes

- `OnlineSettings` (`online.h:148-158`) gana `bool shrine_invite` (por defecto **encendido**, ADR-025 n.º 13), con
  lectura y escritura junto a `show_markers`/`pvp` en `online.c:1602` y `:1630`, y un `opt_toggle` en `online_ui.mm`
  junto a "Show players" (l. 105). Texto: "Shrine invitations" / "Ask me to join when another player enters a shrine."
  (en `tr()` y en `runtime/ui/i18n/*.inc`). Es de este jugador (no del anfitrión): no entra en las reglas de sesión.
- **No** se manda al compañero: apagarlo no se nota fuera.

### 4.8 Aceptar: el viaje

- Se usa el viaje de T419: `control_tp_allow_coop(true)` + `control_tp_request(x, y, z)` con las coordenadas de la
  tabla local, solo en el hilo del juego, solo si no hay escena ni muerte (T419 pasos 1-7). Eso es de F4 y **depende de
  T419**, no de esto.
- Hay que fijar si el viaje se hace **a la puerta** o a "tantos metros de la puerta" y qué pasa cuando el mapa del
  campo no está cargado a esa distancia: `control_tp` espera a que el suelo cargue con su `HOLD`
  (`control_tp.c:14-20`, tiempo máximo en `HOLD_TIMEOUT_NS`, :180). Si falla, no se reintenta: el aviso lo dice.
- Tras llegar, el jugador **entra por su cuenta** (ADR-025 n.º 11). Nada se carga a mano, no se fuerza el ascensor.
- Si B tarda más que A: A ya puede haber terminado. Nada especial: "X ya salió" cierra el aviso y la entrada sigue
  siendo una entrada normal sin A. Si A sigue dentro, B se ve con A cuando entran (2.2).

### 4.9 Lo que esto **no** resuelve (para no prometerlo)

- Los fallos de 2.1 (`oa_enemies_*` y `oa_loot_*` sin filtro): HE-02 (carril de F2/F4). T465 **depende de HE-02**
  (como dice `tasks.md`), porque la invitación convierte en normal tener a uno dentro y al otro fuera.
- Bestias Divinas: el mismo mecanismo se podría aplicar a `MainFieldDungeon/Remains*`, pero el dueño pidió **santuarios**;
  no se incluye.
- Que A sepa que B aceptó (4.1).

## 5. Tareas propuestas

(Los números son sugerencias para el orquestador; no se han reservado.)

- **T465a [lógica] Invitación a santuario: máquina de estados pura** · modelo medio
  files: `runtime/coop_shrine_invite.c`, `runtime/coop_shrine_invite.h`, `tests/test_coop_shrine_invite.c`
  verify: `clang -Wall -Iruntime tests/test_coop_shrine_invite.c runtime/coop_shrine_invite.c -o build/test_coop_shrine_invite && build/test_coop_shrine_invite` sale con 0. Casos: `MainField/` → `CDungeon/Dungeon038` (ENTRA(38) tras 2 STATE); `""` entre medias no cambia; texto malo (`CDungeon/../x`, `CDungeon/Dungeon999`, sin NUL, `MainFieldDungeon/RemainsWind`, `CDungeon/Dungeon5`) → ninguno; mismo `(jugador, n)` dos veces en 10 s → una; salir cierra; 30 s sin respuesta → caduca; cada estado de la tabla 4.6 → su motivo; ajuste apagado → nada; primer STATE ya dentro → nada.
  no hagas: no incluyas cabeceras del juego ni de red; no uses memoria dinámica; no uses el texto de la red como formato ni como ruta.

- **T465b [ajustes+i18n] `shrine_invite` en ajustes** · modelo bajo
  files: `runtime/online.h` (solo el campo), `runtime/online.c` (leer/escribir), `runtime/ui/online_ui.mm` (un `opt_toggle`), `runtime/ui/i18n/*.inc`
  verify: `python3 tools/i18n_check.py` sale con 0 **y** `make runtime-objs` (lo ejecuta el orquestador, ADR-006)
  depende de: T409 (dueño de `online.h`, `tasks.md` F4 l. 33)
  no hagas: no cambies ningún otro ajuste ni el formato del fichero salvo la línea nueva.

- **T465c [juego] Cablear detección + aviso + viaje** · modelo alto
  files: `runtime/oa_frame.c` o el módulo de reglas de `oa_*` (una llamada por fotograma), `runtime/ui/coop_hud.mm` (aviso con aceptar/rechazar y cuenta atrás), `runtime/control_tp.c` (solo lo que deje T419)
  verify: recorrido manual G1-G8 de la sección 6 (lo ejecuta el orquestador)
  depende de: T465a, T465b, T419 (viaje), T428 (avisos), HE-02 (filtro de mapa en el cliente), T409
  no hagas: no cargues el mapa del santuario; no fuerces la entrada; no muevas nunca al Link de otro jugador; no leas coordenadas de la red.

- **T465d [mundo, opcional, solo si G3/G4 fallan] Puerta abierta antes del viaje** · modelo alto
  Si B llega a una puerta cerrada, poner `Open_DungeonNNN` en B por la vía de flags antes del viaje. Cambia la autoridad
  de flags (T410/T411): requiere decisión del dueño. No se escribe hasta ver el resultado de G3/G4.

- **T465e [decisión] ¿A debe saber que B viene?** · para el dueño
  Si sí, un mensaje con destino (TP_REPLY de T409 sirve) → pasa por T409. Si no, queda como está.

## 6. Pruebas en el juego para el orquestador

Sin ventana, con `tools/coop/dos_juegos.sh` (A = anfitrión, control 7777; B = invitado, control 7778; logs en
`$SW/pruebas/logs/A.log` y `B.log`; ver `mapa/08` §5). Siempre sobre **copias** de guardado (`restaurar` copia
`guardado-base`; no se toca el guardado del dueño). Desde la raíz del repositorio, en MSYS2 UCRT64:

```
PY=tools/venv/Scripts/python.exe
bash tools/coop/dos_juegos.sh restaurar && bash tools/coop/dos_juegos.sh arrancar
$PY tools/swctl.py --port 7777 --wait 180 state ; $PY tools/swctl.py --port 7778 --wait 180 state
```

Cierre: `bash tools/coop/dos_juegos.sh parar`.

### 6.1 Antes de T465: T540 en sí (lo que pide el `verify` de T540, el dueño o el orquestador)

| # | Qué | Órdenes | Qué anotar |
|---|---|---|---|
| T540-0 | Elegir un santuario sin completar cercano, de un solo mecanismo | `$PY tools/swctl.py --port 7778 places shrine` y luego `goto shrine <n>` en 7777 y en 7778 (el exterior; `goto` no cambia de mapa) | número y nombre; el guardado debe estar ya junto al santuario |
| T540-1 | Fase 1: entra solo A | en 7777: `stick l 0 1 60`, `press a 8` hasta entrar (entrar exige pisar el ascensor [H]). Antes y después, en 7778: `actors OnlinePlayer`; `$PY tools/swctl.py --port 7778 state` | B.log: `[online] map: CDungeon/DungeonNNN` (en A.log). ¿A desaparece de B? ¿La lista y el overlay siguen mostrando a A (HE-05)? |
| T540-2 | Fase 2: entra B | lo mismo en 7778 (mueve con `stick`/`press`) | cuánto tardan en verse; `actors OnlinePlayer` en los dos; `[online] map:` en ambos |
| T540-3 | Fase 3: jugar | `SAVAGEWIND_ONLINE_MOVER_LOG=1` delante de `dos_juegos.sh arrancar`; mover un mecanismo/objeto cada uno | `[online] ... taken/sent` en los logs; `actors OnlineEnemy`/`Enemy_` por si el cliente crea marionetas dentro (HE-02) |
| T540-4 | Salir | los dos salen; hablan con el monje; salir | orbe en los dos; ¿se ven en el campo? ¿fantasma? |
| T540-5 | Repetir 2 y 3 | — | estabilidad |

### 6.2 T465 (cuando existan T465a-c)

| # | Qué | Órdenes | Esperado |
|---|---|---|---|
| G1 | Aviso: entra A, B en el campo | como T540-1; `$PY tools/swctl.py --port 7778 state` y `report` | en B.log la línea de la invitación con el número; el aviso se ve en B; A no ve nada |
| G2 | Aceptar | en 7778 la orden de aceptar (la que defina T465c en `docs/CONTROL.md`) | B llega a la puerta (`$PY tools/swctl.py --port 7778 pos` a < 3 m de las coordenadas de la tabla), sin daño; `stick l 0 1 60` + `press a 8` y entra; en los dos `actors OnlinePlayer` y `[online] map:` |
| G3 | **Santuario no activado en B** (la pregunta clave 4.4) | guardado de B sin `Open_DungeonNNN`; A lo activa y entra | ¿la puerta de B ya funciona? Anotar con `flags`/`actors DgnObj_Entrance` si hace falta |
| G4 | Lo mismo con B anfitrión y A invitado (T411 cambia la unión) | roles al revés | idem |
| G5 | Rechazar y 30 s sin contestar | no contestar 31 s | el aviso se cierra, B no se mueve; A no nota nada |
| G6 | El 135 (`ground = false`) y uno de los 14 medidos | `goto shrine 135` luego la invitación real | llega y cae sin daño |
| G7 | Estados: B en pausa (`press plus 6`), en una escena, muerto (`DIE_TEST`), dentro de otro santuario, en un reto (T451) | uno por uno | cada uno con el motivo de 4.6; no se mueve nunca |
| G8 | Ajuste apagado | poner `shrine_invite` a 0 en `ajustes-B.ini` | no sale |
| G9 | Cortes: `bash tools/coop/dos_juegos.sh matar A` con el aviso abierto en B | — | el aviso se cierra; sin cuelgue en B |
| G10 | 10 invitaciones seguidas (entrar y salir 10 veces con A) | bucle de `press`/`stick` | máximo uno abierto; sin acumular; sin fugas (`state` fps estable) |

Cada prueba: anotar versión, FPS, GPU, ajustes y resultado; mantener separado lo [L], lo [H] y lo visto.

## 7. Preguntas para el dueño (no se deciden aquí)

- **P1.** ¿Se invita también si el compañero **ya estaba dentro** cuando nos unimos a la sesión? (Propuesta: no.)
- **P2.** ¿A debe saber que B viene o que rechazó? (Propuesta: no; si sí, mensaje con destino por T409.)
- **P3.** Si B llega a una puerta cerrada (G3/G4), ¿se abre sola o se queda cerrada? (Cambiar la autoridad de flags.)
- **P4.** ¿Y el Santuario de la Resurrección y otros interiores con nombre distinto? El diseño solo cubre `CDungeon/DungeonNNN`; el resto da "ningún evento". El nombre real de ese mapa no está comprobado [H].

## 8. Lo que no se comprobó

- Nada se ejecutó: ni compilación, ni pruebas, ni el juego (ADR-006, encargo).
- No se vio el mapa de ningún santuario real: el patrón `CDungeon/DungeonNNN` sale de un ejemplo de la documentación
  (`ONLINE.md:474`) y de la coincidencia con las claves de la tabla; el resto es [H].
- No se abrió `online_mod/quests.txt` (generado): solo el filtro del generador (`online_mod.py:881`).
- No se midió si la posición de arranque de cada santuario está delante de la puerta.
- No se conoce el efecto real de `oa_enemies_client` y `oa_loot_update` dentro de un santuario: solo que no filtran.
- No se verificó si el juego deja un santuario "descubierto" al entrar andando.
