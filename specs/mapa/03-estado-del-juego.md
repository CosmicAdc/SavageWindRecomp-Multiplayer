# 03 — Estado del juego: qué lee, escribe y dirige ya el runtime

Mapa del área "estado de juego" de Savage Wind Recomp (rama `test`, repo `SavageWindRecomp/`), hecho solo leyendo el
código. Objetivo aguas abajo: un modo cooperativo online de 2 jugadores en un fork.

Convenciones de este documento:

- Las rutas son relativas a `SavageWindRecomp/`. `archivo:línea` apunta al código leído el 2026-11-24.
- "Sitio" = entrada de `SITES` en `tools/game_sites.py`, disponible en el runtime como `g_site.<nombre>`
  (`runtime/game_sites.h`). Las direcciones `0x70…` son las de la tabla `KNOWN['1.9.0']` del propio repo.
- Los nombres tipo `PlayerInfo`, `world::Manager`, `evt::Manager` son los que usan los comentarios del repo. No he
  comprobado ninguno contra el binario: aquí solo se afirma lo que el código del runtime hace con esos punteros.
- **Verificado** = lo leí en el código. **No verificado / dudoso** se marca explícitamente. No ejecuté el juego ni
  ninguna prueba.

Regla del proyecto que condiciona todo lo que falta (`CONTRIBUTING.md`, "The one hard rule"; `AGENTS.md`, "Code and
data rules"): nada de código, cabeceras ni documentación copiados de proyectos de descompilación. Todo offset o
función nueva tiene que salir del Ghidra propio del usuario (`tools/ghidra/README.md`, proyecto en `../ghidra/`,
consultas con `tools/ghidra/gq.py`) y entrar como patrón en `tools/game_sites.py`.

---

## 0. Lo esencial en diez líneas

1. El runtime conoce **muy poco** del jugador: una copia de la posición (`PlayerInfo +0x88`), la estamina (`+0xa4`),
   un float de vida en `+0xa0`, la posición real escribible (cadena de punteros), dos celdas de daño de caída y la
   cámara orbital. **No conoce rotación, velocidad, animación ni estado de acción de Link.**
2. **No existe ningún código que enumere, cree o destruya actores.** Lo más cercano: obtener el actor del jugador
   (`player_actor`), el actor equipado por categoría (`player_equipment`), y manejar *links* a actores
   (`actor_index_*`, `actor_link_*`) dentro del "quick drop".
3. Dar ítems **existe** y es robusto: llama a la función de recogida del propio juego desde el hilo del juego.
4. Soltar ítems existe **solo** para arma/arco/escudo y solo desde el selector rápido (quick drop, exclusivo 1.9.0).
5. Hora del día: leer, fijar y congelar **existe**. Clima: leer y forzar el tipo actual **existe**.
6. GameData: **no hay API por nombre**. Hay tres mecanismos parciales (parche del `.sav` al cargar, búsqueda en el
   heap del objeto-flag por CRC32, y acceso por *handle* a los dos búferes para `WM_Time`).
7. Detección de pausa, evento/cinemática (con nombre del evento), pantalla de carga, título, diálogo y menús:
   **existe**, solo lectura.
8. La interfaz de control es TCP de texto en `127.0.0.1`, una línea de petición → una línea JSON, ejecutada una vez
   por frame presentado. Sirve de modelo de *registro de comandos y cola por frame*, no de protocolo de red.
9. La entrada inyectada se mezcla con la del jugador 1. El juego solo ve **un** mando (npad id 0). Alimentar la
   entrada de un jugador remoto solo movería al Link local: no hay segundo actor que la reciba.
10. Única versión soportada: **1.9.0** (`VERSIONS` tiene una sola entrada, `tools/game_sites.py:44-46`). Los sitios
    se buscan por patrón, pero casi todos los offsets de estructuras están fijados a mano para 1.9.0.

---

## 1. Inventario de capacidades ya presentes

### 1.1 Jugador (Link)

| Capacidad | Archivo:línea | Mecanismo | Sitios | Notas / robustez |
|---|---|---|---|---|
| Leer posición (x, y, z) | `runtime/control_game.c:102`, `:119-124` | `*(g_site.player_info)` → objeto; floats en `+0x88`, `+0x8c`, `+0x90` | `player_info` (0x7001e13ed0, `ref`) | Es una **copia** que el juego reescribe cada frame (`control_tp.c:3-4`). `0,0,0` se trata como "sin mundo". Lo usan también `framepace.c:125-128`, `perfmon.c:54-72`, `vulkan/exec.c:10884`. |
| Leer posición con guardas de "en juego" | `runtime/perfmon.c:54-72` | Igual, pero rechaza título, carga, evento, película y punteros fuera del heap | `player_info`, `screen_mgr`, `event_mgr` | Es la lectura más defensiva del repo; buen modelo para un hilo de red. Comentario: "PlayerInfo's copy can outlive Link during scene changes". |
| Escribir posición real | `runtime/control_cells.c:79-96` (`player.pos`), `control_tp.c:241-278` | Cadena de punteros desde `PlayerInfo`: `{0x60,0x578,0x80,0x410,0x1c0}` y dos alternativas `{0x10,0xe8,0x750|0x758,0x78,0x1c0}`; se valida que el vec3 final esté a ≤1 m (x, z) de la copia | `player_info` | Cadenas halladas con `mem path` en 1.9.0. Si ninguna valida, cae a búsqueda en memoria (ver teleport). |
| Estamina (leer) | `runtime/control_game.c:182`, `framepace.c:806` | float en `PlayerInfo +0xa4` (1000 = una rueda, `tools/patches.py:1017-1018`) | `player_info` | Solo lectura por esta vía. |
| Estamina (interceptar escritura) | `runtime/assists.c:48`, `tools/patches.py:1020-1024` | Parche en el `str s8,[x19,#0xa4]` del setter; `rt_stamina(old,new)` decide el valor | `stamina_store` (0x7000cc7430) | Grupo opcional. Hilo del juego. |
| Vida de Link (interceptar cada cambio) | `runtime/assists.c:40`, `tools/patches.py:1004-1014` | Parche en los dos `str` del manejo de daño de Link; `rt_link_life(old,new)` en cuartos de corazón | `link_life_store` (0x7000cc4e70), `link_life_tick` (0x7000cc4da4) | Ve **toda** pérdida de vida de Link. Grupo opcional. No expone quién golpeó. |
| Vida (escribir contenedores) | `runtime/rupees.c:113-117`, `:239-252` | Flags `MaxHartValue`/`CurrentHart` en el heap (`+0x18`), `PlayerInfo +0xa0` (float, unidades = cuartos) y, en el hilo del juego, `*(int32*)x1` a la entrada del manejo de daño | `player_info`, `item_tick` | Comando `hearts` (no está en `docs/CONTROL.md`). Semántica de `+0xa0` **dudosa**: el comentario dice "max_hearts" pero se escribe junto con la vida actual. |
| Daño de caída (anular) | `runtime/control_cells.c:98-117` | Dos floats "punto más alto desde que dejó el suelo": `{0x60,0x860}` y `{0x60,0x20d0}` | `player_info` | Indica que `PlayerInfo +0x60` apunta al "objeto jugador" (comentario `:99`). No verificado que sea el mismo puntero que devuelve `player_actor`. |
| Actor del jugador (puntero) | `runtime/quick_drop_action.c:339-343` | `guest_call(g_site.player_actor, x0 = *player_info)` → puntero de actor | `player_actor` (0x7000cc6fd0, `call`), `player_info` | Solo desde el hilo del juego con un `Cpu` válido. Grupo opcional "quick drop". |
| Estado "no puede soltar/equipar" | `runtime/quick_drop_action.c:132-146` | Llamadas a predicados nativos + byte `player +0xcf8 & 4` | `quick_drop_trash_prohibited`, `quick_sort_player_busy`, `quick_sort_event`, `quick_sort_modal`, `player_drop_forbidden` | Son los propios "¿puedo…?" del juego. Útiles como precondición de cualquier acción remota sobre el inventario. |
| ¿Montado? | `runtime/hle/input.c:383-408` | Heurística: la cámara de caballo leyó el stick en el último cuarto de segundo; la moto, por la consulta de su acción | `horse_cam_lr`, `cam_stick`, `act_hold` | Heurística, no estado real. |
| ¿En combate? | `runtime/free_sprint.c` (cabecera), `tools/patches.py:1044-1052` | Los tres controladores de música de batalla reciben un contador; ≥1 = en combate | `battle_info_field`, `battle_info_b`, `battle_info_c` | Es un booleano global, no un estado por enemigo. |
| Mapa/zona actual | `runtime/game_place.c:38-56`, `runtime/blood_moon.c:56-61`, `services/test_warp.c:104-109` | (a) heurística por archivos leídos (`Map/MainField/…`, `Pack/<interior>.pack`, `Title.pack`); (b) `world_mgr +0x7d9 != 0 && +0x6b8 == 1` = campo abierto; (c) sección `A-1…J-8` calculada de x, z | `world_mgr` | No hay lectura directa del nombre de mapa/stage en memoria. |
| Rotación, velocidad, animación, estado de acción de Link | — | — | — | **No existe.** Solo hay yaw/pitch de la **cámara**. |

### 1.2 Teleport

| Capacidad | Archivo:línea | Mecanismo | Sitios | Notas |
|---|---|---|---|---|
| `teleport x y z` | `runtime/control_tp.c:241-278`, `:325-337` | Una escritura de vec3 en la celda `player.pos`; máquina de estados `IDLE→VERIFY→(HOLD)→DONE/FAILED` avanzada una vez por frame presentado (`control_tp_frame`, `:153-218`) | `player_info` | Ruta directa sin búsqueda si la cadena valida. |
| Búsqueda de reserva | `control_tp.c:161-217`, `control_mem.c:552-618` | Escaneo en segundo plano de todo vec3 igual a la posición (~0,3 s), y luego 4 rondas de "empujar 5 cm por grupos y ver cuánto se mueve la copia" hasta aislar la dirección real | — | Requiere a Link quieto. Para otra versión del juego. |
| Salto lejano (≥300 u en xz, o ≥1200 en y) | `control_tp.c:53-86`, `:173-184`; `tools/patches.py:1167-1180` (`_tp_gate`) | El parche envuelve la pregunta del juego "¿sigue cargando el suelo alrededor de Link?" y responde "no" mientras el runtime mantiene a Link en el destino; cuenta la respuesta real y suelta tras 12 updates seguidos "listo" y ≥1,5 s; timeout 120 s | `tp_gate` (0x70010b7a40), `pause_state` | Sin el parche `tp_gate` un salto largo congela el mundo para siempre (razón de pausa 9). Grupo opcional. |
| Aterrizaje sin daño | `control_tp.c:104-112` | Durante 4 s tras llegar, las celdas de caída siguen la altura de Link | `player_info` | |
| API para la UI | `runtime/control.h:90-94`, `control_tp.c:280-322` | `control_tp_link_pos`, `control_tp_request(x,y,z)` (atómico, desde otro hilo), `control_tp_status` | — | **Esta es la entrada natural para "teleport al compañero".** |
| `goto`/`tp` con lugares | `runtime/control_game.c:371-442`, `control_places.c`, `control_places_data.inc` | Tabla estática de santuarios, torres, establos, aldeas, Koroks; añade elevación 1–3 u; pone hora y cámara | — | Sin estado de juego. |
| Arrancar en otro sitio | `runtime/services/test_warp.c:86-143`, `services/fs.c:2058-2063` | Reescribe `PlayerSavePos`, `PlayerSavePosMapType/Name` del `game_data.sav` en memoria al leerlo | — | Solo pruebas (`SAVAGEWIND_TEST_WARP`). Único camino que cambia de **mapa** (p. ej. interior de una Bestia Divina). |

Precondiciones del teleport (verificadas en código y `docs/CONTROL.md:89`): mundo cargado (`PlayerInfo` no nulo),
ningún teleport en curso, destino a más de 5 cm, modo desarrollador (`control_game_frame` solo avanza la máquina con
`control_enabled()`, `control_game.c:36-40`). Documentado como no usar montado, en cinemática ni cargando. **No cambia
de mapa**: teletransportar a alguien que está dentro de un santuario mientras el otro está en el campo no tiene
soporte.

### 1.3 Inventario, ítems, rupias

| Capacidad | Archivo:línea | Mecanismo | Sitios | Notas |
|---|---|---|---|---|
| Lista de ítems del juego | `runtime/items.c:1-8` y siguientes | Lee `Actor/ActorInfo.product.sbyml` y los nombres del RomFS del usuario | — | Solo ítems con tag `CanGetPouch`, sin `Important`, sin platos cocinados ni rupias. |
| Dar ítem | `runtime/items.c:367-384` (`items_request`), `:420-460` (`rt_item_tick`); `tools/patches.py:1089-1097` | Cola protegida por mutex; el hilo del juego la vacía a la entrada del manejo de daño de Link y hace `guest_call(pouch_item_get)` con `x0 = *pouch_mgr`, `x1 = sead::SafeString` construido en la pila invitada (vtable + puntero al nombre), `w2 = valor` (cantidad, o durabilidad×100), `x3 = 0` (sin modificador) | `item_tick` (0x7000cc4c3c), `pouch_item_get` (0x7000d921bc), `pouch_mgr` (0x7001e18570, `got`) | Grupo opcional. Solo corre "en juego" (el manejo de daño no corre en menús/eventos: la petición espera). Hasta 32 peticiones en cola. Guarda y restaura todo el `Cpu`. |
| Resultado de dar | `items.c:352-365` | Cuenta entradas del pouch antes/después (`mgr +0x78` usadas, `+0x90` libres, suma 420) | `pouch_mgr` | Offsets "vistos en 1.9.0"; devuelve -1 si no cuadran. |
| Multiplicador de recogida | `items.c:404-415`, `patches.py:1194-1198` (`_pouch_item_get`) | Parche a la entrada de `pouch_item_get`: reescala `w2` | `pouch_item_get` | **Punto de observación de toda recogida de ítem** (nombre en `x1`, valor en `w2`). |
| Enumerar el inventario | `runtime/quick_drop_action.c:164-207`, `quick_inventory_sort.c:97-99` | Bajo `pouch_lock(mgr+0x28)`: `tabs = mgr +0x44498`; `pouch_page_count(mgr,tab)`, `pouch_page_item(mgr,tab,slot)` → `PouchItem`: `+0x18` tipo, `+0x20` valor, `+0x24` equipado, `+0x25` válido, `+0x30` puntero a nombre | `pouch_lock`, `pouch_unlock`, `pouch_page_count`, `pouch_page_item`, `pouch_item_type` | Grupo opcional "quick sort". Tipos usados: 0 espada, 1 arco, 3 escudo (2 = flechas, `assists.c:34`). Los tipos de armadura **no aparecen** en el código. |
| Ordenar inventario | `runtime/quick_inventory_sort.c` | Llama a `pouch_sort` nativo y reconstruye el selector | `quick_sort_*`, `pouch_sort` | Solo 1.9.0. |
| Soltar arma/arco/escudo no equipado | `quick_drop_action.c:286-327` | `pouch_discard(mgr, tab, slot)` + `pouch_cleanup(mgr)` y refresco del selector | `pouch_discard` (0x7000d9784c), `pouch_cleanup` (0x7000d97f68), `quick_sort_refill/mapping/cursor/labels` | Usa la función de "tirar" del propio juego, que es la que crea el objeto en el suelo. Solo desde el selector rápido abierto. |
| Soltar el equipado | `quick_drop_action.c:372-468`, `:470-524`; `patches.py:66-73` | Transacción asíncrona: snapshot de equipados, `pouch_discard`, `player_schedule_equip(player, nombre_por_defecto, -1)`, y espera a que el actor equipado se suelte (parche alrededor de la llamada virtual `vtable +0x588`) | `player_equipment`, `pouch_default_name`, `player_schedule_equip`, `pouch_equipped_snapshot`, `actor_index_assign/resolve/release`, `actor_link_release/alive`, `quick_drop_actor_call`, `quick_drop_actor_method`, `quick_drop_tick` | Estrictamente `g_game_version == "1.9.0"` (`:69-71`, `:541`). Protege `Weapon_Sword_070` y `_502` y los tags `Important`/`NotDropFromPlayer`. |
| Consultar tags de un actor por nombre | `quick_drop_action.c:147-157` | `actor_info_lookup(info, iter, nombre)` y `actor_info_has_tag(info, iter, crc32_tag)` | `actor_info` (0x7001e39450), `actor_info_lookup`, `actor_info_has_tag` | Acceso a los datos de `ActorInfo` del juego desde el runtime. |
| Reemplazo al recoger con el inventario lleno | `runtime/quick_replace.c` (≈1300 líneas) | Engancha las tres llamadas de recogida, el cofre, el *toast* y el pop-up | grupo `replace_*` (45 sitios), `actor_link_copy/assign`, `pouch_can_receive` | Demuestra que se puede interceptar la interacción "recoger del suelo" y "abrir cofre". Solo 1.9.0. |
| Rupias | `runtime/rupees.c:54-76`, `:85-135` | Hilo propio que escanea el heap: clase s32 hallada por el flag `MaxHartValue`, luego el objeto con hash `CurrentRupee` (crc32 en `+0x1c`), valor en `+0x18` | `main_end` (para validar la vtable) | Sin parche ni sitio propio: robusto entre versiones mientras el layout del objeto-flag (0x28 bytes) no cambie. La primera búsqueda tarda segundos. Escribe **desde un hilo ajeno al juego**. |
| Durabilidad / flechas | `runtime/assists.c:21-56` | Parches en el almacenado de durabilidad y en la resta de flechas | `no_wear_store`, `no_wear_hit`, `no_wear_ret`, `pouch_remove_arrows` | |

### 1.4 Mundo: hora, clima, luna de sangre, distancia de dibujado

| Capacidad | Archivo:línea | Mecanismo | Sitios | Notas |
|---|---|---|---|---|
| Leer hora | `runtime/services/test_warp.c:148-154`, `:291-297` | `*world_mgr`; `+0x5c0` nº de gestores, `+0x5c8` array; `[0]` = gestor de tiempo; hora en `+0xb8` (0..360 = 24 h) | `world_mgr` (0x7001e57580) | Comando `state` y `time`. |
| Fijar hora | `test_warp.c:192-230`, `:257-266` | Escribe el valor en **los dos** búferes del gestor de GameData (flag `WM_Time`, localizado por *handle* en `TimeMgr +0xd4`) y en `+0xb8`; levanta el tope de las 11:00 pre-paravela (`+0x14e`) | `world_mgr`, `main_data`, `main_end` | El puntero al gestor de GameData **no es un sitio**: se decodifica de las instrucciones ADRP/ADD del método `calc` del gestor de tiempo (`:170-188`). Verifica el getter y el hash `WM_Time` antes de escribir. |
| Congelar hora | `test_warp.c:241-254`, `:269-288` | Pone a 0 la velocidad del reloj (`TimeMgr +0xc4`) guardando la original | `world_mgr` | Sin escritura por frame. |
| Leer clima actual | `runtime/weather.c:54-61` | byte `world_mgr +0x7d1` (0 despejado … 8 lluvia con sol) | `world_mgr` | |
| Forzar clima | `runtime/weather.c:17-36` | Cada frame presentado reescribe `+0x7d1` y pone `+0x794 = 4` (temporizador); respeta `+0x7d6` (clima de cinemática) | `world_mgr` | Sin parche. Corre en el hilo que presenta (`vulkan/device.c:3375-3376`). |
| Previsión del clima | `test_warp.c:125-132` | Solo en el `.sav` al cargar (`climateWeather*`) | — | Solo pruebas. |
| Luna de sangre | `runtime/blood_moon.c:63-90` | Pone el bit 12 de `ResourceMgr +0x174` (petición "pánico" de un solo uso); el singleton se halla escaneando el código por patrón en tiempo de ejecución (`:26-49`) | `main_data`, `main_end`, `world_mgr` | Espera a que no haya título, carga, pausa, evento ni overlay. |
| Distancia de actores | `runtime/draw_distance.c`, `patches.py:825-835` | Multiplica la escala de `traverseDist` | `traverse_scale`, `traverse_scale_default` | Documenta límites duros: demasiados actores lejanos → el juego **omite actores en silencio** o se cuelga al cargar. Relevante para añadir actores. |

### 1.5 Estado de pausa, eventos, pantallas

| Capacidad | Archivo:línea | Mecanismo | Sitios | Notas |
|---|---|---|---|---|
| Pausa | `runtime/hle/framepace.c:61`, `:180`, `:193`, `:238` | `u32` en `g_site.pause_state`: 0 jugando, 2 menú de pausa o mapa, 4 menús rápidos | `pause_state` (0x7001e1611c) | `patches.py:1167-1168` habla del "bit 9 de los flags en pause_state + 0" (razón de pausa 9 = cargando terreno): probablemente es una máscara de bits, no un enum. **Dudoso.** |
| Evento activo (cinemática, charla, fanfarria) | `framepace.c:87-92` (`framepace_in_event`) | `*event_mgr`; puntero al evento activo en `+0x1d2b8` | `event_mgr` (0x7001e3a638) | La hora del día se detiene durante un evento. |
| Nombre del evento | `framepace.c:96-108` | `evento +0x68` → cadena (p. ej. `Demo014_0`, `Electric_Relic`) | `event_mgr` | Se registra en el log `[event] <nombre>` una vez por evento (`:142-145`). **Base directa para detectar hitos de historia.** |
| Pantallas de UI | `framepace.c:150-178` | `*screen_mgr`; `+0x28` nº, `+0x30` array por id. Ids usados: 1 título, 0x38 carga/confirmación, 0x55 pantalla de carga, 0x3f DLC, 0x11/0x4a/0x4c diálogo, 0x2a mapa, 0x2f inventario, 0x34 opciones | `screen_mgr` (0x7001e374b8) | `screen +0xfe` estado, `+0xd0` nombre, `+0x30` layout. `SAVAGEWIND_LOG_SCREENS=1` las lista. |
| Película en reproducción | `runtime/hle/movie.c` (`g_movie_playing`) | Contador host de decodificadores | — | |
| Cámara libre / foto | `runtime/hle/freecam.c:63-86`, `patches.py:1160-1164` | Parche en la construcción de la matriz de vista de cada cámara: el runtime sustituye ojo y objetivo | `lookat_update` (0x7000eb2d5c) | También casi detiene el reloj del juego. |
| Cámara (leer/escribir) | `runtime/control_cells.c:119-150`, `control_game.c:194-211`, `vulkan/device.c:3048` | Celdas `camera.eye/target/pitch` desde `PlayerInfo`; o la matriz de vista TAA del renderer | `player_info` | Con la matriz de vista y los parches de proyección (`persp_matrix`) se puede proyectar mundo→pantalla. |

### 1.6 Entrada

| Capacidad | Archivo:línea | Mecanismo | Sitios | Notas |
|---|---|---|---|---|
| Estado del mando hacia el juego | `runtime/hle/input.c:1138-1226` (`fill_state`) | HLE de `nn::hid::GetNpadState(s)`: mezcla teclado + mando + ratón + script | — | `if (id != 0) return;` (`:1145`): **solo el jugador 1**; `GetNpadStyleSet` devuelve 0 para cualquier otro id (`:1259-1264`). |
| Inyección en vivo | `runtime/control_input.c:219-247`, llamada en `input.c:1205` y `:1101` | `press/hold/release/stick`: el hilo que presenta publica botones y ejes en atómicos; `control_input_apply` los suma (OR de botones, suma de ejes o sustitución si "held") | — | Caducidad en frames presentados. Solo en modo desarrollador. |
| Entrada con guion | `input.c:836-850` | `SAVAGEWIND_INPUT`, `SAVAGEWIND_HOST_INPUT` | — | Pruebas. |
| Consultas por acción | `runtime/hle/bindings.c`, `patches.py:1145-1153` | Parche delante de `isHold/isTrig(pad, id)`; 41 acciones | `act_hold`, `act_trig`, `act_masks` | Permite **saber qué acción pregunta el juego** cada frame, no inyectarla en otro actor. |
| Vuelo libre | `runtime/control_fly.c:48-107` | Quita stick/X/B de la entrada y escribe la posición cada frame | `player_info` | Modelo de "mover un cuerpo por escritura de posición por frame" con resincronización si el juego lo mueve >25 u. |

### 1.7 Memoria

| Capacidad | Archivo:línea | Mecanismo | Notas |
|---|---|---|---|
| `mem read/write` | `runtime/control_mem.c:382-533`, `:535-550` | Lectura/escritura con guarda (SEH en Windows, copia por el SO en Linux) | `control_mem_peek/poke` son la API interna segura (poke ≤ 64 bytes). |
| `mem scan/rescan/list` | `control_mem.c:188-285`, `runtime/memscan.c`, `memscan_os.c` | Escáner tipo "Cheat Engine" en un hilo: núcleos de comparación AVX2/escalar, salto de páginas a cero, hasta 100 000 candidatos | Una sola sesión compartida. Es herramienta de **descubrimiento**, no de producción. |
| `mem path` | `control_mem.c:620-727` | BFS de punteros desde un objeto raíz hasta una dirección (profundidad ≤6, ventana ≤65528) | Así se hallaron todas las cadenas de `control_cells.c`. |
| Celdas con nombre | `runtime/control_cells.c`, `control_cells.h` | Receta: raíz (campo de `g_site`) + cadenas + función de validación | Añadir una celda = una entrada. Hoy: 6 celdas, todas con raíz `player_info`. |
| `sites` | `control_cells.c:195-213` | Lista los sitios obligatorios y el puntero guardado en cada uno | Solo `GAME_SITES`, no los opcionales. |

---

## 2. La interfaz de control en detalle

Fuentes: `runtime/control.h`, `runtime/control.c`, `docs/CONTROL.md`, `tools/swctl.py`.

### 2.1 Transporte y activación

- TCP, `AF_INET`, **solo `127.0.0.1`** (`control.c:704-707`). Windows usa `SO_EXCLUSIVEADDRUSE`, el resto
  `SO_REUSEADDR`.
- `SAVAGEWIND_CONTROL=<puerto>`: socket de desarrollador, todos los comandos, activa el modo desarrollador
  (`control.c:751-768`).
- `SAVAGEWIND_COMPANION=<puerto>` o el interruptor "Companion apps" (puerto 7777): socket de **solo lectura**; solo
  ejecuta comandos con `CONTROL_READ` (`control.c:773-780`, `:180-183`).
- `SAVAGEWIND_DEV=1` / `-dev`: sin socket; desbloquea los comandos de desarrollador en la consola del juego (F12).
- Hay **un solo servidor** por proceso; si existe el de desarrollador, el de compañía no se abre.
- Una conexión cuya primera línea parece HTTP se cierra antes de ejecutar nada (`control.c:575-596`): defensa contra
  páginas web que envían a loopback.

### 2.2 Formato

- Petición: una línea UTF-8 terminada en `\n`: comando y argumentos separados por espacios; comillas dobles agrupan,
  `\"` y `\\` dentro de ellas (`tokenize`, `control.c:243-262`).
- Respuesta: una línea JSON. `{"ok":true,"data":{…}}` o `{"ok":false,"error":"…"}` (`reply_json`, `:307-325`).
- Varias peticiones por conexión; respuestas en el mismo orden.
- Límites (`control.c:24`): 4 clientes, línea de 4096 bytes, 64 peticiones pendientes por cliente, respuesta de 1 MB,
  128 comandos registrados.
- Los números se emiten con `%.17g` y punto decimal forzado sea cual sea la configuración regional.

### 2.3 Modelo de hilos (lo más reutilizable)

- Un hilo de socket (`serve`, `control.c:610-694`) con `select` cada 10 ms: solo lee líneas, las encola
  (`enqueue`) y envía respuestas terminadas. Nunca toca el estado del juego.
- `control_frame(frame)` se llama **una vez por frame presentado, en el hilo que presenta**
  (`runtime/nvn/vulkan/device.c:3379-3380`), vacía la cola, ejecuta cada comando y pasa la respuesta a la cola de
  terminadas (`control.c:537-561`).
- Justo antes, en el mismo punto, corren `test_warp_frame`, `weather_frame` y `blood_moon_frame`
  (`device.c:3373-3378`).
- La consola del juego usa una segunda cola con la misma ejecución (`control_console_submit/poll`).
- **Importante:** ese hilo **no es el hilo del juego**. Los comandos leen y escriben memoria invitada mientras los
  hilos del juego corren. Solo `items.c` y el quick drop hacen su trabajo real en el hilo del juego (vía parche).

### 2.4 Registro de comandos

```c
typedef void (*ControlFn)(int argc, char **argv, ControlReply *r);
typedef struct { const char *name, *args, *help; ControlFn fn; unsigned flags; } ControlCmd;
void control_register(const ControlCmd *cmds, int count);
```

Respuestas con `control_field_str/int/num/bool`, `control_fail`, y para la consola `control_text`, `control_copy`
(`control.h:34-45`). Permisos: `CONTROL_PLAYER` (cualquiera), `CONTROL_READ` (socket de compañía), 0 = solo
desarrollador.

### 2.5 Comandos existentes

| Comando | Archivo | Flags | Qué hace |
|---|---|---|---|
| `help`, `ping`, `exit` | `control.c:186-228` | P+R, R, P | |
| `state` | `control_game.c:52-61` | P+R | frame, uptime, fps, hora, `hour_held` |
| `pos` | `control_game.c:173-190` | P+R | x, y, z, estamina, dirección de `PlayerInfo`, línea `swr:1 …` |
| `camera` | `control_game.c:213-222` | P+R | ojo, dirección, yaw, pitch |
| `version`, `report` | `control_game.c:279-334` | P+R, P | |
| `get` / `set` | `control_settings.c:257-260` | P+R / dev | ajustes en memoria |
| `time` | `control_game.c:64-90` | dev | leer / fijar / congelar la hora |
| `bloodmoon` | `control_game.c:94-99` | dev | |
| `goto`, `tp`, `places` | `control_game.c:371-466` | dev | |
| `teleport` | `control_tp.c:325-337` | dev | |
| `look`, `press`, `hold`, `release`, `stick`, `input` | `control_input.c:262-272` | dev | |
| `fly` | `control_fly.c:142-166` | dev | |
| `cell`, `sites` | `control_cells.c:162-221` | dev | |
| `mem …` | `control_mem.c:729-731` | dev | |
| `give` | `items.c:473-514` | dev | |
| `rupees`, `hearts` | `rupees.c:146-214` | dev | `hearts` no está documentado |
| `weather` | `weather.c:39-73` | dev | |
| `shot` | `control_game.c:243-256` | dev | PNG del frame |
| `prof`, `sample`, `frames`, `threads`, `vkprof`, `gpuprof`, `frametrace`, `memstats`, `heaps` | `control_prof.c:502-510` | dev | rendimiento |

### 2.6 Clientes

- `tools/swctl.py`: cliente genérico (`--json`, `--port`, `--host`, `--timeout`, `--wait`); códigos de salida 0/1/2.
- `tools/swprof.py`: lector de capturas del perfilador. **No toca estado de juego.**
- `tools/swq.py`: atribución por hilo sobre una captura de `swprof`. Pese al nombre, **no es una consulta de estado
  de juego**.

### 2.7 ¿Sirve como modelo de protocolo de red?

Sirve como modelo de **arquitectura interna**: registro de manejadores, hilo de E/S que solo encola, ejecución en un
punto fijo del frame, respuestas estructuradas, banderas de permiso. No sirve como protocolo de juego en red:

- Texto + JSON por línea, petición/respuesta estricta: sin mensajes no solicitados ni *streaming* de estado.
- Cadencia de un lote por frame **presentado**, no por actualización del juego (son cosas distintas en este proyecto:
  `AGENTS.md` exige distinguir updates, frames presentados y generados).
- Sin autenticación, sin cifrado, sin versión de protocolo, atado a loopback por diseño de seguridad.
- TCP con `select` de 10 ms: latencia y bloqueo de cabeza de línea inadecuados para posición/animación.

Recomendación para el spec: un módulo de red **nuevo** (UDP o TCP binario propio) que reutilice el patrón "hilo de
E/S → cola → aplicar en un *tick*", pero cuyo *tick* de aplicación esté en el **hilo del juego** (ver §5.1), y que
exponga sus órdenes de depuración como comandos `control_register` normales.

---

## 3. Todos los SITES, agrupados por propósito

Fuente: `tools/game_sites.py:64-448` (lista `SITES`), `:455-507` (`OPTIONAL_GROUPS`), `:509-739` (`KNOWN['1.9.0']`).
Tipos: `code` dirección de la instrucción, `call` destino de un BL/B, `ref` dirección formada por ADRP+ADD/LDR, `got`
puntero guardado en esa dirección, `imm` inmediato de una instrucción, `data`/`end` límites del módulo. "(op)" = en un
grupo opcional (si falta uno, se desactiva el grupo entero).

### 3.1 Raíces de estado de juego (las que importan para multijugador)

| Sitio | Tipo | Significado |
|---|---|---|
| `player_info` | ref | Puntero global al objeto de información del jugador (posición `+0x88`, estamina `+0xa4`, `+0x60` objeto jugador). Marcado "test tools only". |
| `world_mgr` | ref | Puntero global al gestor del mundo: gestores `+0x5c0/+0x5c8`, clima `+0x7d1`, temporizador `+0x794`, clima de demo `+0x7d6`, stage `+0x6b8`/`+0x7d9`. |
| `event_mgr` | ref | Puntero global al gestor de eventos; evento activo en `+0x1d2b8`. |
| `pause_state` | ref | `u32` de pausa (0 / 2 / 4; bit 9 = cargando terreno). |
| `screen_mgr` | ref | Puntero global al gestor de pantallas de UI (`+0x28` nº, `+0x30` array). |
| `actor_info` (op) | ref | Puntero global a los datos de `ActorInfo`. |
| `pouch_mgr` (op) | got | Dónde está el puntero al gestor del inventario (datos del menú de pausa). |
| `main_data`, `main_end` | data, end | Límites de `.data`/`.bss` de `main` (validar punteros a vtables y singletons). |
| `controller_step` | got | Float: paso del contador de "mantener" de los mandos. |
| `dialog_answer` | ref | Respuesta (`u32`) del diálogo de dos botones. |

### 3.2 Jugador: vida, estamina, combate, movimiento

| Sitio | Significado |
|---|---|
| `item_tick` (op) | Entrada del manejo de daño de Link: corre cada frame en juego en el hilo del juego. **El único "tick de juego" genérico que el runtime usa hoy.** |
| `link_life_store`, `link_life_tick` (op) | Los dos almacenados de la vida de Link (cuartos de corazón). |
| `stamina_store` (op) | Almacenado de la estamina en `PlayerInfo +0xa4`. |
| `no_wear_hit`, `no_wear_ret` (op) | Resta de "vida" de cualquier cosa (arma, Link, enemigo) y dirección de retorno que distingue de quién es. |
| `no_wear_store` (op) | Almacenado de la durabilidad del equipado en el pouch. |
| `pouch_remove_arrows` (op) | El arco quitando flechas del pouch. |
| `sprint_cost`, `sprint_press_cost` (op) | Coste de esprintar en la acción Move de Link. |
| `battle_info_field`, `battle_info_b`, `battle_info_c` (op) | Controladores de música de batalla recibiendo el contador de combate. |
| `horse_crash`, `horse_look_ahead` (op) | Choque y anticipación del caballo. |
| `surf_contact`, `surf_boost_remove`, `surf_jump_request` (op) | Surf con escudo. |
| `arrow_fall_gravity` | Gravedad de las flechas. |
| `demo_move_arrive`, `rail_point_radius` (op) | Movimientos de Link dirigidos por demo/raíl (cinemáticas). |
| `tp_gate` (op) | Función "¿sigue cargando el mapa alrededor de Link?". |

### 3.3 Inventario y actores (grupos "quick sort", "quick drop", "replace")

| Sitio | Significado |
|---|---|
| `pouch_lock`, `pouch_unlock` | Mutex recursivo nativo (del inventario y otros). |
| `pouch_page_count`, `pouch_page_item` | Nº de entradas de una página y `PouchItem*` en (página, hueco). |
| `pouch_item_type` | Categoría de equipo a partir de un nombre de actor. |
| `pouch_sort`, `pouch_cleanup`, `pouch_discard` | Ordenar, barrer entradas inválidas, tirar un ítem. |
| `pouch_item_get` | Recogida de ítem (nombre, valor, modificador). |
| `pouch_can_receive` | ¿Cabe este ítem? |
| `pouch_default_name`, `pouch_equipped_snapshot` | Nombre "sin equipar" por tipo; snapshot de equipados. |
| `player_actor` | Función: `PlayerInfo*` → actor del jugador. |
| `player_equipment` | Función: (jugador, categoría 0/1/2) → actor equipado. |
| `player_schedule_equip` | Programa un cambio de equipo del jugador. |
| `player_drop_forbidden` | Predicado de estado del jugador. |
| `actor_info_lookup`, `actor_info_has_tag` | Buscar un actor por nombre en `ActorInfo` y comprobar un tag (crc32). |
| `actor_index_assign`, `actor_index_resolve`, `actor_index_release` | Referencia indexada (retenida) a un actor: crear, resolver a link, soltar. |
| `actor_link_assign`, `actor_link_copy`, `actor_link_release`, `actor_link_alive` | Link ordinario a un actor. |
| `quick_drop_tick`, `quick_drop_actor_call`, `quick_drop_actor_method`, `quick_drop_trash_prohibited` | Tick de la UI, llamada virtual de "soltar" del actor de arma y su método, predicado "prohibido tirar". |
| `quick_sort_enter`, `quick_sort_menu`, `quick_sort_modal`, `quick_sort_event`, `quick_sort_player_busy`, `quick_sort_refill`, `quick_sort_mapping`, `quick_sort_cursor`, `quick_sort_labels` | Ciclo de vida y predicados del selector rápido. |
| `replace_*` (45 sitios) | Reemplazo con inventario lleno: llamadas de recogida (`replace_pickup_call_a/b/c`), objetivo de interacción, cofre (`replace_chest_*`), *toast*, pop-ups, tarjeta, vtables y ranuras de gestores (`replace_card/pouch/interaction_manager_slot`), `replace_weapon_modifier`, `replace_string_assign`. |
| `stale_actor_helper`, `stale_actor_kind`, `stale_actor_target`, `stale_actor_flag`, `stale_actor_flag2` (op) | Evitar cuelgues por un actor muerto en el gestor de objetivos (`actor +0x570` link, `link +0x88` param). |
| `gyro_scene_actor` (op) | Un mecanismo de santuario leyendo su entrada. |

### 3.4 Cámara y proyección

`lookat_update` (op), `cam_stick`, `cam_stick_end`, `horse_cam_lr`, `cam_pitch_chase`, `cam_pitch_surf`,
`cam_pitch_wakeboard`, `cam_pitch_aim`, `cam_pitch_item`, `cam_reverse_x`, `cam_reverse_y`, `persp_matrix`,
`persp_matrix_offset`, `persp_unproject_x`, `persp_unproject_y`, `frustum_lr0/lr1/tb0/tb1`, `proj_update`.

### 3.5 Entrada y acciones

`act_hold`, `act_trig`, `act_masks` (consultas por acción y tabla de 41 máscaras), `pc_button_group`,
`pc_cursor_manager`, `pc_cursor_activate`, `pc_pane_bounds`, `menu_wrap`, `ui_control_layer`, `pc_map_focus`,
`pc_map_focus_target`, `pc_map_focus_now`, `beast_focus_axes`, `beast_focus_linear`, `beast_confirm_axes`,
`beast_confirm_linear`, `beast_clamp_pitch`, `beast_wrap_yaw`, `beast_world`.

### 3.6 UI: opciones, título, diálogo, layout

`opt_two_*` (13 sitios, filas de dos botones), `opt_create_rows`, `opt_row_init_flag_type`, `gdt_flag_type`,
`opt_row_redraw`, `opt_read`, `opt_store`, `opt_table`, `opt_reset`, `opt_msg_call`, `msg_lookup`, `opt_decide`,
`opt_update`, `opt_back_call`, `opt_screen_rows`, `screen_back`, `screen_open_request`, `screen_open`,
`screen_close`, `pause_options_update`, `dialog_texts`, `dialog_back`, `title_update`, `title_button`,
`title_options_state`, `title_msg_file`, `title_version_format`, `ui_layout_draw`, `ui_layout_native`,
`ui_layout_pane`, `quick_menu_capture_alpha`.

Nota: `opt_read`/`opt_store`/`gdt_flag_type` interceptan cómo **la pantalla de Opciones** lee y guarda sus flags, para
filas que son del runtime (`runtime/ui/game_options.mm:2106-2114`). No son una API general de GameData.
`screen_open_request`/`screen_open`/`screen_close` sí permiten abrir y cerrar pantallas nativas por id, y
`msg_lookup` es la búsqueda de textos del juego.

### 3.7 Ritmo de frame, física, sonido

`frame_wait`, `frame_loop_interval_ret`, `vfr_ratio`, `gui_speed`, `phys_solver_steps`, `phys_tick`, `phys_tick_mt`,
`phys_motion_tick`, `phys_motion_tick_c`, `kf_angle`, `snd_frame_floor`.

### 3.8 Distancia de dibujado y vegetación

`traverse_scale`, `traverse_scale_default`, `grass_range`, `grass_fade_blades`, `grass_fade_quads`,
`grass_blade_slots`, `grass_blade_cap_a/b/c`, `grass_blade_cap_c_max`, `grass_blade_lateral_near/far`, `tree_ranges`.

### 3.9 Carga

`yaz0_stream`, `yaz0_stream_ret` (descompresor Yaz0 en *streaming*).

### 3.10 Robustez entre versiones

- `VERSIONS` contiene solo 1.9.0 (build id `cd57b23fa4bbad65`); `runtime/game_versions.h` igual.
- Cada sitio se halla por patrón de instrucciones con campos enmascarados; en una build de `KNOWN` el patrón debe dar
  exactamente la dirección listada, y si deja de hallarla cae a la dirección listada con aviso
  (`tools/game_sites.py:27-33`).
- Los **offsets de estructuras** no pasan por ese mecanismo: están escritos a mano en C y casi todos anotados "1.9.0".
  El quick drop, el quick sort, el reemplazo, el layout de UI y el cursor PC comprueban literalmente
  `strcmp(g_game_version, "1.9.0")`.
- Mecanismos que **no** dependen de direcciones fijas: rupias/corazones (escaneo del heap por vtable+hash),
  luna de sangre (escaneo de código por patrón en ejecución), puntero a GameData para la hora (decodificado de una
  vtable).

---

## 4. Análisis de huecos para el multijugador

Leyenda: **EXISTE** / **PARCIAL** (qué hay) / **FALTA** (qué habría que obtener por ingeniería inversa con Ghidra
sobre la copia del usuario).

| # | Capacidad necesaria | Estado | Qué hay hoy | Qué habría que reversar o construir |
|---|---|---|---|---|
| 1 | Crear un segundo actor tipo jugador | **FALTA** | Nada. Ninguna llamada a creación de actores en todo `runtime/`. Solo se sabe obtener el actor del jugador y manejar links a actores existentes. | La función de petición de creación de actor (nombre, posición, rotación, parámetros, heap) y su ciclo asíncrono; cómo se registra y se destruye; si el actor del jugador puede instanciarse dos veces (probablemente no sin tocar singletons como `PlayerInfo`) o si conviene un NPC/actor de demo con el modelo de Link. Límites de actores y de heap (ver `draw_distance.c`). **Es el riesgo nº 1 del proyecto.** |
| 2 | Leer transformación del jugador local | **PARCIAL** | Posición (copia y real). Yaw/pitch de la cámara. | Rotación (yaw o matriz) y velocidad de Link: offsets en el actor del jugador; con `mem path` desde `player_info` es abordable sin Ghidra para la matriz, pero hay que confirmarlo. |
| 3 | Escribir transformación de un actor remoto | **PARCIAL** | Escritura de posición del jugador por cadena; patrón de "escritura por frame + resincronización" (`control_fly.c`). | Layout base de un actor genérico (matriz mundo, cuerpo físico): escribir solo la posición de un actor con física lo desincroniza del cuerpo rígido; hace falta el setter nativo o desactivar su física. |
| 4 | Leer animación del jugador local | **FALTA** | Nada. | Componente de animación del actor (lista de animaciones activas, nombre/índice, frame, velocidad, mezcla) y el estado de acción (IA/acción actual del jugador). |
| 5 | Reproducir animación en el actor remoto | **FALTA** | Nada. | Función nativa para pedir una animación por nombre y fijar su frame; sincronía con los *triggers* de animación (el propio repo documenta que hay esperas que dependen del frame exacto, `framepace.c:109-118`). |
| 6 | Equipo (arma, arco, escudo) del jugador local | **PARCIAL** | `player_equipment(player, 0/1/2)` → actor; nombre en `actor +0x10`; enumeración del pouch con bandera "equipado" (`+0x24`). | Modificadores del arma (hay un sitio `replace_weapon_modifier`), estado envainado/desenvainado. |
| 7 | Equipo en el actor remoto | **FALTA** | `player_schedule_equip` existe pero actúa sobre **el** jugador. | Cómo adjuntar un actor de arma a un actor anfitrión arbitrario (creación del arma + nodo de mano). |
| 8 | Armadura y modelo | **FALTA** (lectura PARCIAL dudosa) | La enumeración del pouch daría las piezas equipadas si se conocen sus tipos; el código solo usa tipos 0, 1, 2, 3. | Tipos de armadura en `PouchItem`, y cómo el modelo de Link cambia de piezas (carga de modelo por pieza). |
| 9 | Color distinto para el segundo Link | **FALTA** | Nada sobre materiales. El runtime sí controla todo el pipeline gráfico (`runtime/nvn/`), pero no identifica qué *draw* pertenece a qué actor. | Vía juego: parámetros de material/tinte del modelo o del tinte de armadura. Vía renderer: marcar los draws del actor remoto y teñirlos en el traductor de shaders; requiere poder correlacionar draws con actores (no existe). |
| 10 | Eventos de daño | **PARCIAL** | Hilo del juego ve cada cambio de vida de Link (`rt_link_life(old,new)`) y cada resta de vida de cualquier entidad (`rt_life_cost(cost, life, whose)`), clasificada en arma / Link / otro. | La identidad del actor dañado y del atacante en ese punto (registros disponibles en `no_wear_hit`), tipo de daño, y una función para **aplicar** daño a un actor (para PvP y para replicar golpes a enemigos). |
| 11 | Objetos soltados en el suelo | **PARCIAL** | Soltar arma/arco/escudo desde el selector rápido (el juego crea el actor). Interceptar la recogida (`pouch_item_get`, `replace_pickup_call_*`) y el objetivo de interacción. | Soltar/crear un ítem **por nombre en una posición** sin pasar por la UI (lo mismo que #1 para actores de ítem), y materiales/comida (hoy solo tipos 0, 1, 3). Identidad estable de un objeto del suelo entre dos máquinas. |
| 12 | "Enviar ítem al compañero" | **EXISTE** (como dar/quitar) | Emisor: `pouch_discard` + `pouch_cleanup` (solo arma/arco/escudo y con el selector abierto). Receptor: `items_request` (cualquier ítem de la lista, con cantidad). | Quitar del inventario un material/comida sin UI; transmitir durabilidad y modificador (el `give` actual pasa `x3 = 0` y durabilidad nueva). |
| 13 | Teleport al compañero | **EXISTE** (mismo mapa) | `control_tp_request(x,y,z)` con espera de carga de terreno. | Cambio de mapa (campo ↔ santuario ↔ Bestia Divina): hoy solo vía parche del `.sav` al cargar. Hace falta la función de cambio de escena o un "guardar y recargar". Además el teleport hoy exige modo desarrollador. |
| 14 | Estado de enemigos | **FALTA** | Solo el contador global de música de batalla. | Enumeración de actores (lista del sistema de actores), identificación de enemigos, vida, posición, IA; y cómo impedir que cada cliente simule a los suyos (autoridad del anfitrión). |
| 15 | GameData: leer/escribir flags | **PARCIAL** | (a) Parche por CRC32 del nombre sobre el `.sav` al cargarlo (`test_warp.c:57-79`): bool/s32/f32/vec/string. (b) Objeto-flag s32 en el heap por vtable + hash, valor en `+0x18` (`rupees.c`). (c) Gestor de GameData: `+0xbd8 + 8*i` → búfer; `+0x28` nº, `+0x30` array de flags por *handle*; flag "primario" valor `+0x18`, hash `+0x1c`; flag "proxy" valor `+0x10`, primario en `+8` (`test_warp.c:190-216`). | Las funciones nativas get/set por nombre o por handle para cada tipo, la tabla nombre→handle, y la sincronización entre los dos búferes. Sin ellas, escribir "a mano" salta la lógica de publicación del juego. |
| 16 | GameData: sincronizar (detectar cambios) | **FALTA** | Nada observa escrituras de flags. | Enganchar los setters nativos (un parche por tipo) para emitir "flag X cambió", o comparar instantáneas del búfer cada N frames. Filtrar los flags por jugador (posición, inventario, corazones) de los de mundo (santuarios, Koroks, misiones). |
| 17 | Sincronizar hora | **EXISTE** | Leer, fijar, congelar (`test_warp_hour`, `test_warp_set_hour`, `test_warp_hold_hour`). | Nada esencial. Decidir política: el invitado congela su reloj y recibe la hora del anfitrión. El reloj se detiene durante eventos. |
| 18 | Sincronizar clima | **EXISTE** (tipo actual) | Leer `+0x7d1`; forzar por frame. | La previsión por zona climática y los efectos derivados (rayos, temperatura) siguen localmente al tipo forzado; rayos concretos no se sincronizan. |
| 19 | Luna de sangre | **EXISTE** (disparar) | `blood_moon_request()`. | Detectar que el anfitrión la tuvo (su flag en GameData, vía #16). |
| 20 | Detectar cinemáticas y eventos de historia | **EXISTE** (lectura) | `framepace_in_event()`, nombre del evento, pantallas de diálogo/carga/título, `g_movie_playing`. | Decidir política (pausar al otro, ocultar al remoto). Para **forzar** un evento en el otro cliente haría falta la función de lanzar evento por nombre. |
| 21 | Detectar pausa / menús | **EXISTE** | `pause_state`, `screen_mgr`. | En cooperativo el mundo no puede pausarse: habría que impedir la pausa del mundo al abrir menús (el propio `tp_gate` muestra cómo se neutraliza **una** razón de pausa). |
| 22 | Marcadores de mapa | **FALTA** | Se conoce la pantalla del mapa (id 0x2a) y el foco del cursor (`pc_map_focus*`, `beast_world`). | Cómo añadir un icono al mapa/minimapa, o dibujar el propio encima con la transformación mundo→mapa. |
| 23 | HUD (nombre, vida del compañero) | **PARCIAL** | Overlay ImGui propio del runtime (`runtime/ui/`), escalado del HUD nativo (`runtime/ui_layout.c`), matriz de vista (`control_camera_view`) y parches de proyección. | Proyección mundo→pantalla completa en el host (falta la matriz de proyección efectiva ya compuesta) para una etiqueta sobre la cabeza; es trabajo de host, no de reversing. |
| 24 | Entrada del jugador remoto | **PARCIAL** (irrelevante tal cual) | Inyección de botones/sticks en el jugador 1. | Con el diseño "cada uno simula a su Link y replica estado" no hace falta inyectar entrada remota. Un diseño "el anfitrión simula a ambos" exigiría un segundo controlador de jugador en el juego: no existe. |
| 25 | *Tick* en el hilo del juego | **PARCIAL** | `rt_item_tick` (cada frame en juego, con `Cpu` válido para `guest_call`), `quick_drop_action_tick` (tick de UI), `rt_vfr_update` (cada update). | Elegir/crear un punto de enganche que corra **siempre** (también en menús y eventos) y donde sea seguro llamar a funciones del juego. |
| 26 | Guardado compartido "propiedad del anfitrión" | **PARCIAL** | Perfiles de guardado separados; parche del `.sav` en lectura. | Política de qué guarda el invitado; impedir que su autoguardado contamine su partida propia (usar un perfil dedicado). |

### 4.1 Lectura del análisis

- Lo que **ya funciona** cubre la periferia: teleport, dar ítems, hora, clima, detección de estado, memoria.
- Todo lo que hace *visible* a un segundo jugador (#1, #3–#5, #7–#9) **falta por completo** y depende de entender el
  sistema de actores del juego. Ningún archivo del runtime aporta un punto de partida más allá de los links a
  actores del quick drop.
- "Progreso de mundo compartido propiedad del anfitrión" (#14–#16) es el segundo bloque grande: no hay API de
  GameData ni noción de autoridad sobre enemigos.
- Un primer hito realista con lo existente: **"fantasma" sin modelo** — intercambiar posición, hora, clima y nombre
  de evento entre dos instancias, dibujar al compañero como marcador en el overlay del host, teleport al compañero y
  envío de ítems. No requiere crear actores.

---

## 5. Riesgos y preguntas abiertas

### 5.1 Riesgos técnicos

1. **Hilos.** Los comandos de control, el clima y la hora corren en el hilo que presenta; las rupias en un hilo
   propio; todos escriben memoria invitada sin sincronizar con los hilos del juego. Funciona para escalares
   aislados. Para llamar a funciones del juego (crear actores, fijar animaciones, GameData) hay que estar **en el
   hilo del juego** con un `Cpu` válido y usar `guest_call` (`runtime/dispatch.c:174-180`), como hacen `items.c` y
   `quick_drop_action.c` (que además bajan el `sp` 0x100 para tener memoria temporal invitada).
2. **El único tick genérico no corre siempre.** `item_tick` es la entrada del manejo de daño de Link: no se ejecuta
   en menús, eventos ni pantallas de carga.
3. **Reinicio en caliente.** El juego puede reiniciarse dentro del mismo proceso; todo estado por arranque debe
   limpiarse con `RT_ON_RESET` y no se pueden conservar punteros invitados (`AGENTS.md`). Una sesión de red tiene que
   sobrevivir a eso.
4. **Punteros que caducan.** La copia de `PlayerInfo` "puede sobrevivir a Link durante cambios de escena"
   (`perfmon.c:54`). Los actores se reciclan; el propio juego se colgaba con actores muertos (`stale_actor_*`).
   Cualquier referencia a un actor remoto debe ir por link indexado (`actor_index_*`) y revalidarse cada frame.
5. **Presupuesto de actores y heap.** `draw_distance.c` documenta que con demasiados actores el juego omite algunos
   en silencio o se cuelga al cargar. Un segundo Link con equipo y armadura son varios actores y modelos más.
6. **Ritmo de frame variable.** El proyecto corre a 30/60/90/120 fps con una razón VFR; hay esperas del juego que
   cuentan frames. La red debe marcar el tiempo en tiempo de juego, no en frames presentados, y dos clientes a
   distinto fps verán animaciones a distinto paso.
7. **Una sola versión.** Todo offset es de 1.9.0. Ambos jugadores necesitan exactamente 1.9.0 y el mismo estado de
   DLC (los santuarios 120+ y varios ítems dependen de él).
8. **Simulación duplicada.** Cada instancia es un juego completo de un jugador que simula su propio mundo. "Mundo
   propiedad del anfitrión" exige suprimir o sobrescribir en el invitado enemigos, física de objetos, clima, hora y
   flags. No hay nada hecho en esa dirección y es, conceptualmente, lo más caro después de #1.
9. **La pausa detiene el mundo.** Menú, mapa y carga de terreno pausan la simulación local; el compañero seguiría
   moviéndose.
10. **Seguridad.** La interfaz actual es deliberadamente solo-loopback y rechaza HTTP. Un modo en red abre el
    proceso (que tiene `mem write` y `give`) a un par remoto: el canal de red **no debe** exponer el registro de
    comandos de desarrollador, y todo mensaje entrante debe validarse (rangos, nombres de actor contra la lista de
    `items.c`, tamaños).

### 5.2 Riesgos de proyecto / legales

- `CONTRIBUTING.md`: ningún código, cabecera o documentación copiados de proyectos de descompilación; y nada de
  Nintendo en commits. Todos los offsets y firmas que faltan deben derivarse del Ghidra propio y describirse con
  palabras propias. El modelo de Link, sus animaciones y texturas solo pueden venir del RomFS del usuario en
  ejecución; un "Link de otro color" no puede distribuirse como asset.
- `docs/FAQ.md:593-596` responde "Multiplayer? No" (petición abierta #617) y `CONTRIBUTING.md` cierra las peticiones
  que el FAQ ya contesta: esto vive en el fork, no es esperable que upstream lo acepte. Conviene aislarlo en archivos
  nuevos y tras una opción apagada por defecto para poder seguir rebasando sobre `test`.
- Los grupos de sitios son "todo o nada": añadir sitios nuevos como grupo opcional propio evita romper el arranque si
  un patrón falla.

### 5.3 Preguntas abiertas (no resueltas leyendo el código)

1. ¿`PlayerInfo +0x60` es el mismo puntero que devuelve `player_actor(PlayerInfo)`? El código sugiere que es "el
   objeto jugador" pero no los compara.
2. ¿Qué es exactamente `PlayerInfo +0xa0`: vida actual o máxima? `rupees.c` lo escribe con el mismo valor que ambas.
3. ¿`pause_state` es enum o máscara? Se compara con 0, 2 y 4 pero un comentario habla del bit 9.
4. ¿Puede el juego instanciar un segundo actor de jugador, o hay que usar otro tipo de actor con el modelo de Link?
   Determina toda la arquitectura de #1, #5, #7 y #8.
5. ¿Existe un punto del bucle principal que corra en todos los estados (juego, menú, evento, carga) y sea seguro para
   `guest_call`? `rt_vfr_update` corre cada update, pero no se ha comprobado qué estado del juego es coherente ahí.
6. ¿Cuáles son los tipos de `PouchItem` para armadura, materiales, comida y objetos clave? El código solo fija 0
   (espada), 1 (arco), 2 (flechas), 3 (escudo).
7. ¿Qué registros lleva `no_wear_hit` además de coste y vida (actor dañado, atacante)? El parche solo pasa
   `x8`, `x9` y la dirección de retorno.
8. ¿Cómo se cambia de mapa en caliente (entrar a un santuario por código)? Hoy solo se hace reescribiendo el `.sav`.
9. ¿Es fiable `game_place_on_read` (heurística por archivos leídos) para saber si los dos jugadores están en el mismo
   mapa, o hace falta leer el stage del gestor del mundo (`+0x6b8`)? Solo se conoce el valor 1 = campo abierto.
10. ¿Cuánta latencia añade aplicar estado remoto una vez por frame presentado frente a hacerlo por update del juego?

### 5.4 Qué no se verificó

- No se ejecutó el juego ni `make`, ni las pruebas (`make test-control`, `tools/test_game_sites.py`).
- No se abrió el binario ni Ghidra: ninguna dirección u offset de este documento está comprobado contra el juego,
  solo contra el código del runtime que los usa.
- De `runtime/pc_cursor.c` (1898 líneas), `runtime/quick_replace.c`, `runtime/control_prof.c`,
  `runtime/control_settings.c`, `runtime/amiibo.c`, `runtime/shrine_sensor.c`, `runtime/post_update.c` y
  `runtime/draw_distance.c` se leyeron cabeceras y búsquedas dirigidas, no el archivo completo. Conclusiones sobre
  ellos: `amiibo.c` (lector NFC virtual), `shrine_sensor.c` (parchea el audio del sensor en el archivo de sonido) y
  `post_update.c` (actualizador de la app) **no tocan estado de juego en memoria**.
