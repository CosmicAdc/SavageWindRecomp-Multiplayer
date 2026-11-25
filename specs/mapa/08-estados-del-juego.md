# 08 — Estados del juego en el cooperativo: qué hace hoy el código y qué falta

Inventario de los **estados del juego** (cargas, escenas, muerte, menús, viajes, modos, guardado y cortes de red) que
el modo cooperativo trata mal, o cuyo comportamiento no se conoce con certeza. Para cada estado se indica qué hace hoy
el código, qué puede salir mal con dos jugadores, qué tarea o caso de uso lo cubre ya y qué falta. Sirve para las fases
en curso (F2) y para las siguientes.

Convenciones:

- **Base del código:** rama `coop/ESTADOS`, commit `1e6831a`, con `online_actors.c` ya cortado en `runtime/oa_*.c`
  (T207, integrado en `929661f`). Las rutas son relativas a `runtime/` salvo que se diga otra cosa.
- **Referencias:** se citan como `fichero:función` y, entre paréntesis, la línea aproximada (`~l. n`). Si la línea se
  ha movido, se busca por el nombre de la función.
- **Evidencia** (regla 16 de la constitución). Cada afirmación lleva una de estas marcas:
  - **[L]** verificado por lectura del código.
  - **[B]** visto en la bitácora: `bitacora/2026-11-24-F1-prueba-ventana-dueno.md` = **B1**;
    `bitacora/2026-11-25-F1-T120-sesion-ventana.md` = **B2**.
  - **[H]** hipótesis: razonada, no comprobada.
- **Gravedad:** **cuelgue** (el proceso se cierra o se congela), **incorrecto** (el juego sigue, pero hace algo
  equivocado o irreversible) o **cosmético** (solo se ve mal).
- **Cómo se hizo:** solo lectura. Nada se compiló ni se ejecutó. Cuatro lecturas en paralelo (`online.c` y
  servicios; `oa_core`/`oa_frame`/`oa_sample`/`oa_puppet`/`oa_world`; `oa_enemies`/`oa_flags`/`oa_loot`/`oa_rules`/
  `oa_runes`/`oa_control`; `control*.c`, `tools/swctl.py` y `tools/coop/`). Las afirmaciones críticas (huérfanos
  tras un reinicio, mapa vacío, imposición de la hora, causa de N1, orden duplicada) se volvieron a leer a mano.

---

## 0. Lo esencial

1. **Nada del modo en línea tiene `RT_ON_RESET`** (ni `online.c` ni ningún `oa_*.c`) [L]. "Salir al inicio de Savage
   Wind" reinicia el juego **dentro del mismo proceso**, pero la sesión de red sobrevive [L][B1]. Las peticiones de
   actor pendientes (`g_orphan`) guardan direcciones del arranque anterior, y `oa_orphans_update` las lee en el
   arranque nuevo [L]. Es la causa más probable del cierre `0xc0000409` de B1 [H]. Lo cubren T211/T212.
2. **Un mapa desconocido cuenta como "el mismo mapa"** (`oa_sample.c:oa_same_map`) [L]. La pose deja de enviarse a
   los 0,5 s de una pausa o una carga. A partir de ahí, un compañero en un santuario puede aparecer como fantasma en
   el campo [H]. **Sin tarea.**
3. **Los enemigos, el botín y las runas del invitado no miran el mapa** [L]. Un invitado dentro de un santuario sigue
   borrando sus enemigos y creando marionetas con las coordenadas del campo del anfitrión [H]. Las tareas de F5
   (T515/T541) tratan otro problema. **Sin tarea.**
4. **N1 explicado:** todo el daño "en línea" escribe la vida de Link directamente (`ENEMY_LIFE`, 0x848). Eso incluye
   el PvP, los golpes de enemigos ajenos y los golpes con el menú abierto. Así se salta `assists.c:rt_link_life`, que
   es lo único que mira `god_mode` y `damage_taken` [L]. CU-12 solo cubre el PvP.
5. **La hora del anfitrión se impone cada 2 s** si la del invitado difiere en más de 3 minutos de juego [L]. Dormir o
   esperar en el invitado no sirve; si duerme el anfitrión, el invitado salta de hora [L]. Con el anfitrión en el
   título o en una carga, se sigue difundiendo su última hora [L]. **Sin política.**
6. **Revivir:** `rt_online_has_fairy` da un hada siempre que haya sesión y la regla esté activa. No comprueba que
   haya un compañero en pie [L], así que hoy nunca hay fin de partida en sesión, contra R4-23. Lo cubre T431.
7. **Cambiar de modo no se comprueba:** Master Mode no aparece en ningún sitio, y el DLC y el orden de `quests.txt`
   no se comparan al conectar [L]. F3 cubre el DLC y el overlay (R3-14); **Master Mode, no**.
8. **Las esperas del cooperativo se cuentan en fotogramas** (`g_settle` 60/180, `wait=120`, `tick%3`) [L]. Contradice
   la regla de tiempos de `AGENTS.md` [L]. **Sin tarea.**
9. **Herramientas de prueba:**
   - `goto` da por llegado comparando solo x y z, así que una caída cuenta como llegada [L] (N3).
   - La orden `bloodmoon` de `oa_control.c` nunca se registra porque su nombre está duplicado [L].
   - `SAVAGEWIND_TEST_WARP` se reaplica en **cada** carga de la partida [L].
10. Al perder al anfitrión, el invitado sigue solo [B2]. Sus enemigos "rehechos" siguen reales y las marionetas se
    quedan congeladas hasta la siguiente carga [L]. CU-05 y T317-T319 cubren la reconexión, no esta limpieza.

---

## 1. Cómo ve el código los estados (piezas comunes)

Lo que usan todas las tablas siguientes. Todo **[L]** salvo que se diga otra cosa.

| Pieza | Dónde | Qué decide |
|---|---|---|
| ¿Hay mundo? | `oa_frame.c:rt_online_frame` (~l. 31-38) | `world = pinfo && !framepace_title_ui() && pos != 0,0,0 && me >= 0` |
| Carga local | `oa_frame.c:rt_online_frame` (~l. 56-70) | Sin mundo o salto de Link > 100 m → `forget_all()` y `g_settle = 60` fotogramas. Excepción: si las marionetas siguen vivas, "a long jump: … kept" |
| El juego borró al compañero | `oa_frame.c:rt_online_frame` (~l. 343-349) | Link remoto no vivo → `forget_all()` y `g_settle = 180` fotogramas |
| `forget_all` | `oa_frame.c:forget_all` (~l. 8) | **No borra actores**: supone que ya los borró el juego. Las peticiones pendientes pasan a `g_orphan` (`oa_core.c:oa_orphan`, ~l. 149) y el resto se pone a cero |
| Cargando (emisor) | `online.c:sample_local` (~l. 780) | Sin `PlayerInfo`, posición 0 o no finita → `ONLINE_F_LOADING`. **Lee memoria del invitado desde el hilo de red** (`pause_state`, `PlayerInfo +0x88`) |
| Cargando (receptor) | `online.c:online_players` (~l. 1434) | Sin instantáneas o más de 3 s sin estado (`STALE_NS`) → LOADING |
| Pose caducada | `online.c:sample_local` (~l. 803) | La pose (mapa, AS, equipo, tinte) solo viaja si tiene menos de 0,5 s |
| Pausa | `hle/framepace.c:framepace_paused` (~l. 240) | `pause_state != 0` o pantalla de título. La pose no se muestrea en pausa salvo con `nopause` (`oa_frame.c`, ~l. 75) |
| Mismo mapa | `oa_sample.c:local_map` (~l. 481), `oa_same_map` (~l. 490) | `"tipo/nombre"` (`MainField/`, `CDungeon/Dungeon000`, `MainFieldDungeon/RemainsWind`…). **Cadena vacía = mismo mapa** |
| Se ve al compañero | `oa_frame.c:rt_online_frame` (~l. 323-336) | `used && !LOADING && same_map && distancia XZ < 400 m`; si no, `oa_drop`, salvo que nuestro juego esté en pausa |
| Evento | `hle/framepace.c:framepace_in_event` (~l. 87) | Lo usan `oa_sample` (marca `ONLINE_E_DEMO` en pedestales y torres), `oa_rules` y `oa_flags` |
| Muerte local | `oa_sample.c` (~l. 740), `oa_frame.c` (~l. 182-189) | AS `Dead*` → no se crea ni se posa nada; al levantarse, 180 fotogramas de espera |
| Detecciones que **existen pero no se usan** en `oa_*` | `hle/framepace.c` | `framepace_loading_ui`, `framepace_dialogue_ui`, `framepace_dlc_ui`, `framepace_map_ui`, `framepace_quick_ui`, `framepace_shop_panes` |
| Reinicio por arranque | `hle.h` (`RT_ON_RESET`), `main.c:teardown` | 38 ganchos en otros ficheros del runtime; **ninguno en un módulo online** |

---

## 2. Tablas por grupo de estados

### 2.1 Arranque, título, carga de partida y reinicio en caliente

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Arranque con sesión | `main.c` (~l. 525) llama a `online.c:online_host`/`online_join` una vez por proceso si existe `SAVAGEWIND_ONLINE` [L]. Hasta que hay mundo se envía `ONLINE_F_LOADING` (`sample_local`) [L] | Ninguno visto: el compañero aparece al llegar el otro al mundo [B2] | — | CU-01 (T103) | — |
| Pantalla de título del juego | `world` exige `!framepace_title_ui()` → `forget_all` [L] | El anfitrión sigue difundiendo su última hora (ver §2.3) [L] | cosmético | — | HE-07 |
| Menú de DLC | Sin código propio; el autocontinuar de `hle/input.c` (~l. 1003-1010) solo pulsa B en el aviso [L]. `online.c:srv_message` (~l. 188) solo compara `magic` y `PROTOCOL` [L] | Uno con DLC y otro sin él: `quests.txt` distinto y el bit k significa otra cosa → se encienden flags equivocados [H] | incorrecto | T314, T325 (R3-14 incluye DLC y overlay) | — |
| Continuar o cargar al arrancar | La carga se ve como "sin mundo" → `forget_all`, `g_settle = 60` [L]. Con `SAVAGEWIND_TEST_WARP`, `services/test_warp.c:test_warp_patch` reescribe la posición en **cada** lectura de `game_data.sav` (`services/fs.c`, ~l. 2058) [L] | Solo en pruebas: toda recarga devuelve a Link al punto de prueba [H] | cosmético | — | HE-12 |
| Cargar otra partida en juego (Sistema → Cargar) | Es una carga. Además, `oa_flags.c:oa_quests_update` (~l. 225-236) ve 64 o más flags de reaparición apagados y lo toma por Luna de Sangre; `had` y `held_off` son estáticas sin reinicio [L] | Falsa Luna de Sangre: deja de reaplicar flags que la unión aún tenía (mapa 07, M7) [L mecanismo, H efecto] | incorrecto | ADR-002 y T411 (la heurística desaparece con la política de flags); riesgo 5 de F4 | Probarlo con el perfil cooperativo (T402) |
| **Salir al inicio de Savage Wind y volver a jugar** | `ui/menu.mm:tab_quit` → `rt_request_reboot(2)`. `main.c:teardown` para los hilos del invitado, ejecuta los `RT_ON_RESET` y libera la memoria del invitado (`platform/windows.c:plat_reset_guest`, `VirtualFree(MEM_DECOMMIT)`) [L]. Ningún módulo online tiene `RT_ON_RESET` [L]. Los hilos de red (`cl_thread`, `srv_thread`) son hilos del anfitrión de máquina: **la sesión sigue viva** [L]. El anfitrión vio "removed Client's Link" y después "again", sin LEAVE [B1]. `oa_g_act` y `g_orphan` conservan *handles* cuya `unit` apunta a memoria del arranque anterior. Al volver al mundo, `oa_core.c:oa_orphans_update` (~l. 155) lee `*unit` y llama a `FN_HANDLE_TAKE` o `FN_HANDLE_RELEASE` [L] | Cierre `0xc0000409` al acercarse otra vez al anfitrión [B1]. Causa más probable: huérfanos o *handles* con `unit` viejo [H]. Alternativa: `sample_local` lee memoria del invitado desde el hilo de red mientras `plat_reset_guest` la libera [H] | **cuelgue** | T211, T212 (criterio S7: salir al inicio + Continuar, 3 veces sin cuelgue) | **HE-01**: decidir qué hace la sesión en un reinicio (Q-EST-1) y que `online.c` deje de leer memoria del invitado fuera del fotograma (regla 6; T218 no lo nombra) |
| Reiniciar por idioma o por mods | `rt_request_reboot(1)` (`ui/menu.mm`, ~l. 1598 y ~l. 2352): el mismo `teardown`, sin pantalla de inicio [L] | Igual que la fila anterior [H] | **cuelgue** | T212 | Incluirlo en la prueba de S7 (PE-1) |
| Salir al escritorio o cerrar | `swctl exit`; `services/am.c:self_controller` (~l. 97) hace `exit(0)` sin `online_leave` [L]. Los demás lo ven como un cierre de TCP | El anfitrión vio "Client left" [B1]; el invitado vio el aviso [B2] | — | T310 | — |

### 2.2 Pantallas de carga y cambio de mapa

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Pantalla de carga (cualquiera) | Local: §1, "carga local" y "el juego borró al compañero" [L]. Emisor: `ONLINE_F_LOADING` sin `PlayerInfo` [L]. Receptor: LOADING a los 3 s [L]. `framepace_loading_ui` no se usa [L] | `PlayerInfo` puede sobrevivir a Link durante un cambio de escena (comentario de `perfmon.c`), así que el emisor mandaría una posición vieja sin marcar LOADING [H]. Las esperas son de 60/180 **fotogramas**: duran el doble a 30 fps que a 60 [L] | incorrecto | T210 (Links validados al principio), T209 | HE-11 |
| Mapa del compañero desconocido | La pose solo viaja si tiene menos de 0,5 s. Sin `nopause` no se muestrea en pausa, y `nopause` se desactiva en carga (`UI_PAUSE_LOAD`) [L]. Pasados 0,5 s llega `map = ""`, y `oa_same_map` responde "mismo mapa" [L] | Un compañero en un santuario o una Bestia, pausado o cargando, vuelve a ser "de nuestro mapa": aparece un fantasma si sus coordenadas caen a menos de 400 m [H] | incorrecto | — | **HE-06** |
| Santuario (`CDungeon/…`) | Mapa distinto → `oa_drop(c, i, false)` (su Link fuera, piezas al pool), salvo que nuestro juego esté en pausa [L]. `can_own` le impide poseer enemigos del campo [L]: "can't own enemies (loading 1, pose 0, as , same map 1)" [B1]. Los dos dentro del mismo santuario deberían verse (DOC:996-997) [H, sin probar] | `oa_enemies_client` y `oa_enemies_show` **no comprueban el mapa** (`oa_frame.c`, ~l. 316-317) [L]: dentro del santuario, el invitado borra sus enemigos a menos de 170 m del *centro* del anfitrión y crea marionetas en esas coordenadas [H, efecto]. El botín (`oa_loot_update`), Stasis y Cryonis tampoco filtran por mapa [L]. El overlay (`ui/online_ui.mm`, ~l. 263-283) dibuja figura y nombre del compañero aunque esté en otro mapa; solo filtra a más de 3 km [L] | incorrecto | T540, T514 (santuarios juntos, F5); T515/T541 (enemigos en un mapa sin el anfitrión) | **HE-02** (filtro de mapa en el cliente); **HE-05** (overlay) |
| Bestia Divina (`MainFieldDungeon/Remains*`) | Como el santuario [L]. Los mecanismos se eligen por nombre y HashId y se mueven con `FN_BODY_TO` (mapa 07, A2); `has_body` en mecanismos integrado sin verificar en juego (T112 `[~]`) | Cuelgue con mecanismos sin cuerpo (A2) [H]; partes de la Bestia sin probar (mapa 07, §5 n.º 54) | **cuelgue** | T112, T215, T516, T517 | — |
| Prueba de la Espada (DLC) | Nada específico [L]. Su tipo de mapa no está comprobado (¿`AocField`?) [H]. El juego quita el inventario a quien entra; el compañero conserva el suyo [H] | Si su tipo de mapa no se distingue, se mezcla con el campo [H]. Soltar objetos o teletransportarse rompería el reto [H] | incorrecto | — | **HE-16**, Q-EST-6 |
| Isla de Eventide | Es `MainField`: sin trato especial [L]. El reto se lleva con flags y la unión los copia al otro [H] | El compañero puede darle comida y armas por el botín compartido [H] | incorrecto | — | **HE-16**, Q-EST-6 |
| Castillo de Hyrule e interiores | Son `MainField`; nada específico [L] | Densidad de actores (límites de `traverseDist`, mapa 03 §1.4) [H] | cosmético | — | PE-11 |
| Lista de objetos rotos de la sesión | `oa_world.c:world_enforce` (~l. 690) aplica `g_world` por HashId en cualquier mapa; la entrada sí filtra con `same_map` (~l. 863) [L]. El flag de rotura se forma con el tipo de mapa (`break_here`, ~l. 621) [L] | Si dos mapas comparten un HashId, se rompe el objeto equivocado [H] | incorrecto | T540 | Anotarlo en T540 |

### 2.3 Escenas, NPC, tiempo y mundo

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Cinemática o evento | `framepace_in_event` [L]. Solo pedestales y torres marcan `ONLINE_E_DEMO` (`oa_sample.c`, ~l. 627); el receptor bloquea su botón A a 30 m (`oa_frame.c`, ~l. 290-310 → `g_online_block_a`, `hle/input.c:1207`) [L]. En escena `pause_state` es 0, así que el muestreo sigue: el compañero ve a Link en su sitio real con la pose de la escena [L]. Nuestro evento no oculta marionetas [L] | La marioneta del compañero se ve dentro de nuestra escena [H]. `oa_rules.c:oa_players_collide` (~l. 117) sale antes durante un evento y **no refresca** las reglas `revive` y `nopause` del anfitrión [L] | cosmético; incorrecto (reglas) | CU-14a (T518-T523, T542); R4-34 (sin duelos en escenas) | **HE-18** (refrescar las reglas fuera de `oa_players_collide`; encaja en T209) |
| Diálogo con NPC | Sin detección propia: `framepace_dialogue_ui` existe pero no se usa [L]. Los flags de NPC y `LinkTag` van en `quests.txt`, en la unión [L] | Progreso común, como pide ADR-007, decisión 9 [L] | — | CU-06, T410/T411 | — |
| Tiendas | Flags de tienda en la unión [L]; rupias de cada uno (ADR-008, decisión 19) | La compra de uno solo se refleja en la tienda del otro por flags, quizá tras una carga [H] | cosmético | CU-06 | — |
| Cocinar | Es un evento [H]; el plato sale del inventario local [L] | Mientras dura, no se refrescan las reglas (fila 1) | cosmético | — | — |
| Dormir en posada / esperar junto a la hoguera | El invitado aplica la hora del anfitrión con `test_warp_set_hour` si difiere en más de 0,05 h (`oa_frame.c:rt_online_frame`, ~l. 160-172) [L] | Si duerme el invitado, el salto se deshace en unos 2 s [L mecanismo, H efecto]. Si duerme el anfitrión, el invitado salta de hora en mitad de lo que esté haciendo, también en una escena [L] | incorrecto | CU-06 (la hora es del anfitrión) | **HE-07**, Q-EST-2 |
| Hora del día | `online.c:srv_loop` (~l. 334) difunde `g_local_hour` cada 2 s. `g_local_hour` solo se actualiza con mundo, así que con el anfitrión en el título, cargando o muerto se sigue difundiendo la última hora [L]. La hora se para en los eventos del anfitrión [mapa 03 §1.5] | El reloj del invitado vuelve una y otra vez a una hora congelada [H] | cosmético | CU-06 | **HE-07**, Q-EST-3 |
| Clima | `oa_flags.c:oa_weather_follow` (~l. 56): el del primer jugador de la sesión a menos de 500 m; no en escenas (`wm+0x7d6`) ni con `settings.weather` forzado [L] | No siempre es el del anfitrión (CU-06, propuesta) [L] | cosmético | CU-06 | — |
| Luna de Sangre del juego | `oa_flags.c:oa_bloody_moon_follow` (~l. 37) copia el temporizador del anfitrión si difiere en más de 30 [L]. `oa_quests_update` la detecta por 64 flags apagados (`held_off`) [L]. Los actores se rehacen por la vía genérica "the game removed … Link" [L]. **`g_world` no se vacía** con la Luna de Sangre, solo al desconectar (`oa_world.c`, ~l. 726) [L] | La escena solo la ve quien la tiene [H]. Árboles y cajas rotos antes no vuelven en toda la sesión [L] | incorrecto | CU-06 (la Luna de Sangre del anfitrión) | **HE-08**, Q-EST-4 |
| Orden de control `bloodmoon` | `control_game.c` (~l. 497) registra la del juego; `oa_control.c` (~l. 163) registra otra con el mismo nombre y `control.c:control_register` (~l. 172) la rechaza ("duplicate command") [L] | La prueba de "flags de Luna de Sangre" por control no existe [L] | incorrecto (herramienta) | — | **HE-09** |
| Kolog y pizarra | `oa_flags.c:oa_slate_event` (~l. 81) y Kolog: lanza su propia escena (`Demo017_0`) si está a menos de 60 m; nunca con otro evento en curso [L] | Un evento lanzado fuera de contexto puede colgarse (`Demo022_0`, mapa 07 §5 n.º 31) | cuelgue (raro) | T410, T416 | — |

### 2.4 Muerte, daño y reanimación

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Muerte con la regla `revive` | `oa_rules.c:rt_online_has_fairy` (~l. 104) responde "tiene hada" si la sesión está CONNECTED y la regla activa [L]. **No comprueba** que haya otro jugador ni que esté en pie [L]. B murió dos veces y lo levantó un hada; A, tres [B2] | Un anfitrión solo en su sesión tiene hadas infinitas [L]. Si caen los dos, reviven los dos, contra R4-23 (fin de partida normal) [L] | incorrecto | CU-11, T430/T431 | — |
| Mientras Link está muerto | `oa_g_local_dead` (`oa_sample.c`, ~l. 740): `rt_online_frame` vuelve antes; no se crean ni se posan marionetas, y hay 180 fotogramas de espera tras levantarse [L] | En el invitado, sus enemigos propios siguen vivos mientras el anfitrión los recupera: durante unos segundos hay dos IA [H] | incorrecto | T444 | HE-11, **HE-22** |
| Animación del hada vista por el otro (N2) | La pose no lleva el hada ni sus efectos (`fxev` solo cubre los ELink de Link y del arma) [H] | El compañero ve caer y levantarse, sin hada [B2] | cosmético | CU-11 (B2 la manda a F4) | Pasarla a T431 o a la revisión de animaciones (N4) |
| Fin de partida sin `revive` | Recarga el guardado local; se detecta como carga → `forget_all` [L]. Con `TEST_WARP`, la recarga vuelve al punto de prueba [L] | Reaparece lejos del compañero [H]. Rehacer actores durante el fin de partida colgaba (mapa 07, §5 n.º 47); hoy lo evita `oa_g_local_dead` [L] | incorrecto | CU-11 (R4-23), T431 | — |
| **Modo dios y "daño recibido" (N1)** | `assists.c:rt_link_life` (~l. 40) es lo único que mira `god_mode` y `damage_taken` [L]. Restan la vida **escribiendo directamente** en `player + ENEMY_LIFE` (0x848): `oa_rules.c:oa_pvp_update` (~l. 187-218, golpes y bombas), `oa_enemies.c:oa_strike` (~l. 689-710, enemigos de otro dueño) y `oa_rules.c:oa_own_strikes_in_menu` (~l. 13, golpes con el menú abierto) [L] | Los golpes del compañero y de enemigos ajenos ignoran el modo dios y la ayuda de daño: 8 muertes con `god_mode` en B2 [B2 en PvP; L en enemigos]. "Vida 0 y siguió vivo": la escritura directa no pasa por la comprobación de muerte del juego [H] | incorrecto | CU-12 (T433/T434), **solo PvP** | **HE-10**, Q-EST-7 |
| Daño entre jugadores | El atacante elige blanco y daño; la víctima resta daño menos defensa y bloquea si su AS contiene `Guard` (`oa_pvp_update`) [L]. Bomba ajena a menos de 4 m: 8 menos la defensa [L]. El efecto `OnlineShock` se coloca a 1 m de la víctima, del lado del atacante [L] | N5: el empujón a veces lleva hacia el atacante [B2]. Si `OnlineShock` empuja hacia su origen, el efecto sale al revés [H]. El daño declarado no se valida [L] | incorrecto | CU-12, T433/T434 (R4-26) | N5 ya está en CU-12 (B2) |
| Muerte del compañero | Sin estado propio: su marioneta reproduce el esqueleto de muerte. Los enemigos y la colisión lo excluyen (`oa_enemies.c`, ~l. 362; `oa_rules.c`, ~l. 123-129) [L] | — | — | CU-11 | — |

### 2.5 Viaje rápido, teletransporte y `goto`

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Viaje rápido del juego | Es una carga → `forget_all`, y `g_settle = 180` al ver que el Link remoto ya no vive [L]. Al recibir: si dos instantáneas distan más de 30 m, salta sin interpolar (`online.c:online_players`, ~l. 1462) [L] | — | — | CU-08 (al compañero) | — |
| Salto largo dentro del mapa | Más de 100 m: si las marionetas siguen vivas, "a long jump: … kept"; si no, `forget_all` [L]. Visto 3 veces con las caídas de A [B2] | Si `framepace_title_ui` parpadea un fotograma sin carga real, `forget_all` olvida actores vivos y se crea un duplicado [H] | incorrecto | T210 | — |
| **`goto` / `teleport` (N3)** | `control_tp.c`: `near_pos` compara **solo x y z** (tolerancia 2,0) porque "Link falls after a teleport" [L]. Un salto de 300 u o más pasa por HOLD con el parche `tp_gate`: hacen falta 12 *updates* seguidos con "listo" y 1,5 s [L]. `goto` responde en el acto; "Arrived" sale después en `teleport status` o en la UI [L]. Si un `goto` falla en `tp_begin`, se queda el "done/Arrived" del salto anterior [L]. Tras una caída, `goto` compara x y z con 0,05 y responde "Already at" [L] | Torre 7: destino y ≈ 247,4 (`control_places_data.inc`, ~l. 146, más 1 m) y Link en y ≈ 195 [B2]. O cae a través de la torre, o la torre aún no ha emergido en el guardado de prueba [H]. Congelado tras "Position verification failed" [B2]; causa probable: la puerta de carga vuelve a decir "cargando" después de soltar a Link (`control_tp.c`, comentario ~l. 14-16) [H]. `tools/coop/recorrido.py:esperar_quieto` (~l. 116-135) trata *failed* como *done*, y un Link congelado parece "quieto" [L] | incorrecto (herramienta) | B2 lo manda a "F2/F3 herramientas", sin número de tarea | **HE-13** |
| Teletransporte desde el menú F1 | Pestaña Teleport solo en modo desarrollador (`ui/menu.mm`, ~l. 2250), con el mismo `control_tp_request` [L] | Un jugador con `-dev` puede saltar en plena sesión, con el mismo "Arrived" engañoso [L] | incorrecto | T407 (fuera del modo desarrollador, punto seguro), T418/T419 | — |
| Teletransporte entre mapas | No existe; solo `TEST_WARP` al cargar [L] | — | — | T511-T513 (F5) | — |
| `SAVAGEWIND_TEST_WARP` | Reescribe `PlayerSavePos`, `MapType/MapName` e `IsGet_PlayerStole2` en cada lectura de `game_data.sav` [L]. `tools/coop/dos_juegos.sh:lanzar` exporta `TEST_WARP=plateau` en los dos juegos [L] | En pruebas, cualquier recarga (fin de partida sin `revive`, reinicio) devuelve a la meseta [H]. Con `TEST_TIME`, la hora fija y la del anfitrión se pisan [H] | cosmético (pruebas) | — | **HE-12** |

### 2.6 Movimiento

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Caballo | Si el AS contiene `Horse` o `Motorcycle`, busca `GameRomHorse*` a menos de 3 m y envía matriz, AS, silla y riendas (`oa_sample.c`, ~l. 760-822). El receptor crea `OnlineHorse_*` (`oa_frame.c`, ~l. 627-648) [L]. Montar de verdad está sin probar (mapa 07, §5 n.º 57) | Si el caballo queda a más de 3 m, la marioneta monta sin caballo [H]. La colisión no se aplica a caballo [L] | cosmético | — | **HE-23** |
| Moto de Hyrule (DLC 2) | Necesita `GameROMMotorcycle` en `gear.txt` del overlay [H] | Sin esa entrada no se ve [H] | cosmético | — | HE-23 |
| Paravela | Solo si `as[0]` empieza por `Para` y no es `ParaEquipOff` (`oa_sample.c`, ~l. 512) [L]. Sonido continuo descartado (`continuous_sound`, ~l. 151) [L] | — | cosmético | — | — |
| Nadar, escalar, escudo-surf, caer | Sin trato especial: todo va por el esqueleto completo (`oa_sample_skeleton`, ~l. 846; `oa_puppet.c:oa_pose_link`, ~l. 124) [L]. El escudo tiene un alcance de 1,5 m para seguir pegado al hueso al surfear (~l. 591) [L]. `FallFlap` y `ReactionSlide` se descartan [L] | Caminar y saltar se ven "creíbles" [B2]; lo demás, sin probar | cosmético | N4 (revisión de animaciones, spec nueva) | — |
| Colisión | `oa_players_collide` empuja solo al Link local, nunca a caballo, muerto ni en evento [L] | "Ya no le atravieso" [B2] | — | CU-02; T524/T525 | — |

### 2.7 Runas, objetos, botín, cámara y amiibo

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Magnesis | `oa_runes.c:oa_grab_update` (~l. 213): elige el objeto por **nombre**, el más cercano (primero el botín del otro a 30 m, luego el mundo a 15 m con cuerpo) y lo mueve con `FN_BODY_TO` cada fotograma [L]. Sin arbitraje si los dos sujetan el mismo [L] | Con mundos distintos mueve otro objeto, o los dos lo empujan [H]. `oa_core.c:oa_drop` (~l. 256) y `forget_all` ponen a cero `oa_g_act` **sin apagar** el brillo, el haz y el imán. Ocurre al alejarse a más de 400 m, cargar, cambiar de mapa o salir de la sesión [L] | incorrecto | T113 (Link vivo), T217 (`has_body`) | **HE-14** |
| Stasis | `oa_stop_update` (~l. 71) [L]. Sobre un enemigo no encuentra nada: la marioneta tiene otro nombre [H] | El efecto y las cadenas no se apagan al soltar, igual que en Magnesis [L] | cosmético | T113 | HE-14 |
| Cryonis | `oa_ice_update` (~l. 130) crea pilares `OnlineIce` [L] | Tras `oa_drop`/`forget_all` los pilares se pierden sin borrar [L]; `delete_piece` sobre una petición en curso (mapa 07, M2) | incorrecto | — | HE-14 |
| Bombas | `OnlineBoom` (`oa_puppet.c`); daño PvP de bomba en `oa_pvp_update` [L] | Doble daño si `OnlineBoom` también hace daño de verdad [H] | incorrecto | CU-12 | PE-6 |
| Botín | Lista en el anfitrión, 64 entradas (`oa_loot.c:oa_loot_update`, ~l. 138) [L]. Las armas puestas por el mapa usan PICK: se borra la del mismo nombre a menos de 2 m (`oa_ground_update`, ~l. 444) [L]. `g_ground` y `g_drop_seen` no se reinician [L]. Botín en los dos sentidos [B2] | Si los dos recogen a la vez, se lo quedan los dos [H]. Tras recargar, un `g_ground` viejo puede enviar un PICK falso [H] | incorrecto | CU-09, T423 | **HE-15** |
| Cámara de la pizarra y fotos | Sin detección [L]; la cámara no es pausa [H]. La cámara libre del runtime casi para el reloj (mapa 03 §1.5) | En las fotos sale la marioneta [H] | cosmético | — | — |
| amiibo | `services/nfp.c` es un lector local con `nfp_reset` [L]; nada en el modo online [L] | Lo que suelta el amiibo puede entrar en el botín compartido si queda quieto sin HashId [H]. Link Lobo solo existe en su juego [H] | cosmético | — | Q-EST-8 |

### 2.8 Pausa, menús, mapa grande, inventario y foco

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Menú de pausa, mapa y menús rápidos con `nopause` | `oa_frame.c:rt_online_frame` (~l. 76-154): con compañeros y CONNECTED, cada fotograma limpia los bits de pausa (BaseProcMgr `0x7001e937f8 + 0x168`, física, escena); nunca con `UI_PAUSE_LOAD` [L]. `ONLINE_F_PAUSED` → "(paused)" en el overlay [L] | Direcciones fijas de 1.9.0 (puerta de versión, T111) [L] | — | Q-02-1 (ADR-011: el compañero se ve en menús) | — |
| A o X en la bolsa | `held_stop` vuelve a parar el mundo hasta cerrar el menú (~l. 103-150) [L] | En esa pausa la pose deja de muestrearse. A los 0,5 s el compañero recibe una pose inválida: equipo bajo el mundo, caballo fuera, mapa "", tinte -1 [L]. Ese -1 rehace la armadura, y una pieza hecha por segunda vez no se dibuja (comentario de `oa_core.c`, ~l. 171): la armadura podría quedar invisible [H] | incorrecto | — | **HE-17** |
| Menús con la regla `nopause` apagada | El juego para el mundo; los golpes de los enemigos propios se aplican con la heurística de menú (`oa_own_strikes_in_menu`) [L] | Igual que la fila anterior, durante todo el menú | incorrecto | — | HE-17 |
| Nuestro juego en pausa | Mientras `framepace_paused()`, no se hace `oa_drop` (~l. 331) [L] | Un compañero que cambió de mapa o se fue sigue visible y congelado hasta cerrar el menú [L] | cosmético | — | — |
| Overlay con menús | Figuras y flechas solo en juego: ni título, ni menú, ni pausa (`ui/online_ui.mm`) [L] | — | — | CU-16 (no dibujar con menús) | — |
| Foco de ventana | `services/am.c` responde siempre "con foco". `hle/input.c:window_focus` (~l. 513) suelta la entrada al perder el foco; el juego no se pausa [L] | Con dos juegos en un PC, el de atrás sigue corriendo sin entrada, que es lo que se busca [L] | — | — | — |

### 2.9 Modos, contenido y guardado

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Master Mode | No hay código ni comparación en ningún sitio (búsqueda sin resultados) [L] | Mezclarlo con el modo normal: flags de otra ranura de guardado, enemigos de otro rango [H] | incorrecto | — (R3-14 no lo nombra) | **HE-16**, Q-EST-5 |
| DLC instalado | `services/misc.c:aoc_u` (~l. 45) y `services/fs.c:open_aoc_storage` (~l. 1763), locales [L]; no se compara [L] | `quests.txt` distinto → bits cruzados [H] | incorrecto | T314, T325 (R3-14) | — |
| Guardado y autoguardado durante la sesión | Cada juego guarda su partida con el autoguardado normal; ningún código online toca guardados [L]. La unión de flags se escribe en la GameData local (`oa_gamedata.c`, `oa_flags.c`) [L] | El progreso del otro queda grabado para siempre en la partida propia (mapa 07, M7a) [L] | incorrecto (irreversible) | T401, T410/T411, T413, T414, T402 | — |
| Dos juegos con la misma carpeta | `sc_lock_take` (`main.c`, ~l. 590) avisa, pero no lo impide [L] | Se pisan los guardados [L] | incorrecto | T101 (carpetas separadas en pruebas) | — |
| Perfil cooperativo (F4) | No existe [L]. La carpeta de guardado se fija una vez por arranque (`services/save_profiles.c:sp_active_save_dir`, ~l. 214) [L] | — | — | T402, T412, T414 (R4-05) | — |

### 2.10 Enemigos (transversal)

| Estado | Qué hace hoy | Riesgo con dos jugadores | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Radio de los enemigos del anfitrión | El anfitrión registra enemigos a menos de 150 m de **cualquier** jugador (`oa_enemies.c:oa_enemies_host`, ~l. 366-506). El invitado borra los suyos a menos de 170 m del **centro del anfitrión** (`oa_enemies_show`, ~l. 778-788) [L] | Con el invitado a unos 200 m del anfitrión, un enemigo junto a él existe dos veces: su copia local y la del anfitrión [L mecanismo, H frecuencia] | incorrecto | — | **HE-03** |
| Más de 32 enemigos | `g_reg` tiene 64 entradas, pero solo viajan 32 (`ONLINE_ENEMIES`, ~l. 629) [L] | Si una entrada que no viaja pasa al invitado, el anfitrión borra su actor y el invitado nunca lo recibe: desaparece para todos [L] | incorrecto | — | **HE-04** |
| Golpe durante un cambio de dueño | `own_hits` (~l. 295) no encuentra el enemigo (`own_find`) y descarta el golpe [L]. `OWNS = 64` lleno: `own_add` devuelve NULL en silencio [L] | Golpes perdidos [L] | cosmético | T444 | Anotarlo en T444 |
| Enemigo que pasa al jugador cercano | ADR-012 (a) [B2] | — | — | CU-15, T443/T444 | ADR-012 (b) y (c) sin probar [B2] |

### 2.11 Red: cortes en cada estado

Comportamiento común [L]:

- **El invitado se va.** `online.c:srv_drop` (~l. 174) difunde `M_LEAVE`. En el anfitrión, `oa_drop(…, true)`, salvo
  que esté en pausa. `gone_owner` devuelve al anfitrión, o deja sin dueño, los enemigos que eran del invitado
  (`oa_enemies.c`, ~l. 586-610).
- **El anfitrión cae.** El cliente pasa a `ONLINE_FAILED` ("Disconnected by the server" o "Lost the connection" a los
  10 s, `cl_thread`, ~l. 1229). En el mundo, con `me < 0`: `oa_drop` de todos y "the host's enemies stay as they
  were" (`oa_frame.c`, ~l. 42-55). Las colas no se vacían hasta Leave, Host o Join (`stop_all`, ~l. 1353).
- **No hay reconexión.** Visto en B2: B siguió vivo 18 s, `OnlinePlayer` pasó de 1 a 0 y apareció el aviso.

| Estado del que se corta | Si se va el invitado (lo que ve el anfitrión) | Si cae el anfitrión (lo que ve el invitado) | Gravedad | Cubierto por | Falta |
|---|---|---|---|---|---|
| Jugando normal | LEAVE, se borra su Link [B1][B2] | Aviso y sigue solo [B2] | — | CU-05, T317-T319 | — |
| Con el menú abierto | El Link del invitado se queda congelado hasta cerrar el menú (`oa_frame.c`, ~l. 330-335) [L] | La rama `me < 0` borra los actores aunque esté en pausa [L] | cosmético | — | — |
| Cargando o en otro mapa | Los hilos de red no dependen del hilo del juego: el PING sigue durante la carga [L]. Una carga de más de 10 s no corta la sesión [H] | Igual [H] | — | T317-T319 | PE-9 |
| Muerto | El bucle de `oa_drop` va antes del `return` por muerte: se aplica [L] | Igual [L] | — | — | — |
| Con enemigos del invitado | Vuelven al anfitrión (`gone_owner`) [L]; un golpe en vuelo se pierde [L] | Sus enemigos "rehechos" **siguen reales** con su IA y las marionetas `OnlineEnemy_*` se quedan **congeladas** hasta la siguiente carga (`stop_all` pone `g.en` a cero y `oa_enemies_client` vuelve en el acto) [L] | incorrecto | T319 (durante la gracia) | **HE-20** |
| Con Magnesis o Stasis activos | El objeto se suelta: `pl.used = false` se procesa antes de `oa_drop` [L]; el efecto visual se queda (HE-14) [L] | Igual | cosmético | — | HE-14 |
| En el título o en un reinicio en caliente | El reinicio no corta la sesión: el anfitrión ve "removed Client's Link" y LOADING, sin LEAVE [B1][L] | El anfitrión en el título sigue difundiendo su hora [L] | cuelgue (§2.1) | T212 | HE-01, Q-EST-1 |
| Cuando ya no hay sesión | — | La línea "Online: <estado>" del overlay sigue hasta pulsar Leave (`online_overlay_wanted`) [L] | cosmético | T329 (reconexión y motivos en pantalla) | — |

---

## 3. Huecos: estados sin tarea

Propuestas, no decisiones: ninguna tarea existente se cambia aquí. La fase propuesta respeta el reparto de ficheros
de `fases/README.md` §4.

| ID | Estado | Qué falta | Gravedad | Fase propuesta | Tarea más cercana |
|---|---|---|---|---|---|
| HE-01 | Reinicio en caliente con sesión | (a) Decidir si la sesión sobrevive al reinicio (Q-EST-1). (b) `online.c:sample_local` lee memoria del invitado desde el hilo de red (regla 6), y puede hacerlo mientras `plat_reset_guest` la libera. Hace falta que el hilo del juego deposite la muestra y que el hilo de red solo la copie. (c) Que `online.c` registre su propio `RT_ON_RESET` (T212 se limita a `oa_*`) | cuelgue | F2 (carril red, junto a T218) | T212, T218 |
| HE-02 | Invitado en otro mapa | Filtrar por `oa_same_map` en `oa_enemies_client`/`oa_enemies_show`, botín, Stasis y Cryonis del cliente; no borrar enemigos locales con el centro de otro mapa | incorrecto | F2 (carriles T216 y T217) o F4 | T216, T217; T515 es otra cosa |
| HE-03 | Radio 150/170 m | Que el invitado suprima según la lista que recibe, no según el centro del anfitrión | incorrecto | F4 | T444 |
| HE-04 | Más de 32 enemigos | Nunca dar a un jugador un enemigo que no viaja, o enviar todos los que tienen dueño | incorrecto | F4 (si cambia el cable, en T409) | T444 |
| HE-05 | Overlay con el compañero en otro mapa | No dibujar la figura ni el nombre si el mapa difiere; mostrar el nombre del mapa (R4-35) | cosmético | F4 | T425 |
| HE-06 | Pose caducada = mapa vacío | Recordar el último mapa conocido de cada jugador, o enviar el mapa siempre, aunque la pose esté caducada | incorrecto | F2 (carril OA tras T210); si cambia el cable, F3 T308 | T210 |
| HE-07 | Hora: dormir, esperar, escenas, anfitrión sin mundo | Política de quién mueve la hora (Q-EST-2, Q-EST-3); no difundir `g_local_hour` sin mundo | incorrecto | F4 (CU-06) | T410 (CU-06) |
| HE-08 | Luna de Sangre | Vaciar `g_world` y el botín de la sesión cuando la tiene el anfitrión; decidir qué ve el invitado (Q-EST-4) | incorrecto | F4 (CU-06) | T411 |
| HE-09 | Orden `bloodmoon` duplicada | Cambiar el nombre de la orden de `oa_control.c` (por ejemplo `bloodmoonflags`) y documentarla | incorrecto (herramienta) | F2 (herramientas) | — |
| HE-10 | Daño en línea sin modo dios ni ayuda de daño | Hacer pasar `oa_strike`, `oa_own_strikes_in_menu` y `oa_pvp_update` por la misma regla que `rt_link_life` (Q-EST-7) | incorrecto | F4 | T434 (solo PvP) |
| HE-11 | Esperas en fotogramas | `g_settle` 60/180, `wait = 120` de `oa_piece_update`, `frames < 60` de efectos y bombas, `tick % 3` de enemigos, `DIE_TEST × 30`: pasarlas a tiempo y comparar a 30/60/120 fps | incorrecto | F2 (carril OA, tras T209) | T209 |
| HE-12 | `TEST_WARP` en cada carga | Opción en `dos_juegos.sh`, o variable de prueba, para que el punto de prueba solo se aplique en la primera carga | cosmético (pruebas) | F2 (herramientas) | T102 (ya cerrada) |
| HE-13 | `goto`/`teleport` (N3) | Comprobar también y (o el suelo) antes de "Arrived"; no conservar "Arrived" de un salto anterior; que `recorrido.py` distinga *failed*; investigar el congelado tras soltar a Link | incorrecto (herramienta) | F2 (herramientas) o F3 T334, como dice B2 | T105, T334 |
| HE-14 | Efectos de runa al soltar | `oa_drop` y `forget_all` deben apagar LOOK, Stasis y efectos, y borrar los pilares `OnlineIce` | incorrecto | F2 (carril runas) | T217, T113 |
| HE-15 | Botín recogido por los dos | Arbitraje en el anfitrión: el primero que pide la recogida se la queda | incorrecto | F4 | T423 (CU-09) |
| HE-16 | Eventide, Prueba de la Espada, Master Mode | Comparar Master Mode al conectar; reglas para los retos que quitan el inventario (Q-EST-5, Q-EST-6) | incorrecto | F3 (versión) y F4 (reglas) | T314, T325 |
| HE-17 | Pose inválida mientras el mundo está parado | Enviar la última pose válida marcada "en pausa" en vez de dejarla caducar; no tratar el tinte -1 como un cambio | incorrecto | F2 (carril OA) | T210 |
| HE-18 | Reglas sin refrescar en eventos | Sacar el refresco de `revive` y `nopause` de `oa_players_collide` | incorrecto | F2 | T209 |
| HE-20 | Fin de sesión con enemigos del anfitrión | Al perder al anfitrión: borrar las marionetas y decidir qué pasa con los enemigos rehechos; durante la gracia, congelarlos (R3-23) | incorrecto | F3 | T319 |
| HE-22 | Doble IA mientras Link está muerto | Seguir llamando a `oa_enemies_client` con Link muerto, o soltar sus enemigos | incorrecto | F4 | T444 |
| HE-23 | Caballo lejos, moto | Ampliar la búsqueda del caballo; comprobar `gear.txt` con DLC 2 | cosmético | F4/F5 (revisión de animaciones, N4) | — |

(No hay HE-19 ni HE-21: lo que iban a cubrir ya lo cubren T431 y HE-07.)

---

## 4. Preguntas para el dueño

Cada una lleva un valor por defecto recomendado. Hasta que el dueño conteste, se construye con ese valor y se anota.

| # | Pregunta | Valor por defecto recomendado | Bloquea |
|---|---|---|---|
| Q-EST-1 | Si un jugador usa "Salir al inicio" (o reinicia por idioma o mods) con la sesión abierta, ¿sigue en la sesión al volver a jugar? | **No.** El reinicio cierra la sesión limpiamente (LEAVE) y empieza como un proceso recién arrancado (regla 8). Cuando exista la reconexión de F3, el invitado vuelve a unirse solo | HE-01, T212 |
| Q-EST-2 | ¿Quién puede mover la hora durmiendo en una posada o esperando junto a la hoguera? | **Solo el anfitrión**, y la hora salta para los dos. Si lo intenta el invitado, ve el aviso "La hora la lleva el anfitrión" y no se le deshace a medias | HE-07 |
| Q-EST-3 | Mientras el anfitrión está en una escena, en una carga o en el título, ¿el reloj del invitado se para con el suyo? | **No.** El invitado sigue con su reloj y solo se iguala cuando el anfitrión vuelve al mundo | HE-07 |
| Q-EST-4 | Cuando el anfitrión tiene la Luna de Sangre, ¿qué pasa en el juego del invitado? | Sin escena: un aviso, sus enemigos y objetos de mundo se reinician como los del anfitrión, y la lista de objetos rotos de la sesión se vacía | HE-08 |
| Q-EST-5 | ¿Se permite jugar juntos con uno en Master Mode y otro en modo normal? | **No.** Se rechaza al conectar con un motivo claro, como el DLC distinto (R3-15) | HE-16, T314/T325 |
| Q-EST-6 | Isla de Eventide y Prueba de la Espada: ¿el compañero puede ayudar a quien está en el reto? | El reto es de quien entra. Mientras dura, el compañero no puede soltarle objetos marcados ni teletransportarse a su lado, y el botín compartido no le llega | HE-16 |
| Q-EST-7 | ¿El modo dios y la ayuda "daño recibido" se aplican también al daño en línea (enemigos de otro dueño y PvP)? | **Sí**, a todo el daño que recibe tu Link, venga de donde venga | HE-10 |
| Q-EST-8 | ¿Se permiten los amiibo durante una sesión? | **Sí**, en local. Lo que suelten se comparte como cualquier botín del suelo; Link Lobo solo lo ve quien lo invoca | — |

---

## 5. Pruebas en juego que el orquestador puede hacer sin el dueño

Todas sin ventana, con `tools/coop/dos_juegos.sh` (puertos de control 7777 = A anfitrión, 7778 = B invitado; logs en
`$SW/pruebas/logs/A.log` y `B.log`). Se lanzan desde la raíz del repositorio, en MSYS2 UCRT64, con:

```
PY=tools/venv/Scripts/python.exe
bash tools/coop/dos_juegos.sh restaurar && bash tools/coop/dos_juegos.sh arrancar
$PY tools/swctl.py --port 7777 --wait 180 state ; $PY tools/swctl.py --port 7778 --wait 180 state
```

Las variables de entorno puestas delante de `dos_juegos.sh` llegan al juego, porque el guion exporta las suyas sin
borrar las demás [L]; que todas funcionen en un juego sin ventana es [H]. Cada prueba termina con
`bash tools/coop/dos_juegos.sh parar`. Ninguna toca guardados del dueño: `restaurar` copia `guardado-base`.

| # | Qué comprueba | Órdenes | Qué mirar |
|---|---|---|---|
| PE-1 | Cierre al recargar (T212, HE-01) | `bash tools/coop/dos_juegos.sh matar B`; `SAVAGEWIND_REBOOT_AT=6000:game bash tools/coop/dos_juegos.sh relanzar B` (reinicio en caliente a los 6000 fotogramas presentados, `main.c:reboot_test_frame`). Variante con pantalla de inicio: `SAVAGEWIND_REBOOT_AT=6000:title SAVAGEWIND_LAUNCHER_AUTOPLAY=1 …`. Tras el reinicio: `$PY tools/swctl.py --port 7778 --wait 120 press a 8` (si el guion de entrada no vuelve a pulsar Continuar [H]), y luego `goto tower 7` en 7777 y en 7778 | B.log: `[reboot] game torn down`. A.log: `removed Client's Link` y `again`. ¿Sigue vivo el proceso de B (`tasklist \| grep savagewind`)? Repetir 3 veces (S7 de F2) |
| PE-2 | Entrar en un santuario (HE-02, HE-06) | `$PY tools/swctl.py --port 7778 places shrine`; `goto shrine <n>` (el exterior); acercarse a la entrada con `stick l 0 1 60` y `press a 8` (entrar exige pisar el ascensor [H]). Antes y después: `actors Enemy_` y `actors OnlineEnemy` en 7778; `actors OnlinePlayer` en 7777 | B.log: `[online] map: CDungeon/…`. A.log: `can't own enemies`. ¿B borra enemigos o crea marionetas dentro del santuario? ¿A deja de ver a B? |
| PE-3 | Pausa y bolsa (HE-17) | `$PY tools/swctl.py --port 7778 press plus 6`; a los 2 s, en 7777, `actors OnlinePlayer` y `actors OnlineArmor`; `press a 8` en 7778 (A en la bolsa); 2 s; repetir `actors` en 7777; `press b 6` | B.log: `A or X in the pouch`, `our pose: not sampled (paused)`. ¿A pierde la armadura de B o la rehace? |
| PE-4 | Hora (HE-07) | `$PY tools/swctl.py --port 7778 time set 22` → a los 5 s, `state` en 7778. `--port 7777 time set 6` → `state` en 7778. `--port 7777 time freeze` → `state` en 7778 dos veces, con 60 s de separación | B.log: `the host's time`. ¿El invitado vuelve a la hora del anfitrión? ¿Se congela con él? |
| PE-5 | Luna de Sangre (HE-08) | `$PY tools/swctl.py --port 7777 bloodmoon` (la del juego: espera a que no haya menú, carga ni escena) | A.log y B.log: `[event]`, `the game removed`, `a load`; `actors OnlinePlayer` en los dos; ¿se rehacen los objetos rotos? |
| PE-6 | N1 con enemigos y PvP (HE-10) | `$PY tools/swctl.py --port 7777 set god_mode true`; llevar a B al campamento y a A unos 15 m detrás (`teleport x y z` en cada puerto; B más cerca para que el enemigo sea suyo). PvP: arrancar con `SAVAGEWIND_ONLINE_PVP=1 bash tools/coop/dos_juegos.sh arrancar` y en 7778 `press y 4` mirando a A | A.log: `hit us: N (life …)` con `god_mode` activo = N1 confirmado para enemigos; `boom … go off` y su daño |
| PE-7 | Muerte y hada (T431, HE-22) | `bash tools/coop/dos_juegos.sh matar B`; `SAVAGEWIND_ONLINE_DIE_TEST=40 bash tools/coop/dos_juegos.sh relanzar B` (muere hacia los 40 s; la cuenta va en fotogramas × 30 [L]). Los dos a la vez: `parar` y luego `SAVAGEWIND_ONLINE_DIE_TEST=40 bash tools/coop/dos_juegos.sh arrancar` | B.log: `die test`, `Link died: revived by a fairy`. Con los dos muertos, ¿reviven los dos? (hoy sí [L]; R4-23 pide fin de partida) |
| PE-8 | N3 `goto tower 7` (HE-13) | `$PY tools/swctl.py --port 7777 goto tower 7`; repetir `teleport status` y `pos` cada segundo hasta *done* o *failed*; `actors Tower` | ¿y ≈ 247 o ≈ 195? ¿Existe la torre? Si se congela: `stick l 0 1 30` y `pos` |
| PE-9 | Cortes en cada estado (§2.11) | Para cada estado de B (normal; `press plus 6`; recién llegado a un santuario de PE-2; con `DIE_TEST`): `bash tools/coop/dos_juegos.sh matar B` y en 7777 `actors OnlinePlayer`, `actors Enemy_`. Al revés: `matar A` y en 7778 `state`, `actors OnlineEnemy`, `actors Enemy_` | A.log: `Client left`, enemigos recuperados. B: ¿marionetas congeladas? ¿enemigos rehechos vivos? (HE-20) |
| PE-10 | Radio 150/170 m (HE-03) | A en el campamento; B a unos 160 m de A, junto a enemigos (`teleport` con coordenadas del campamento + 160 en x); `actors Enemy_` y `actors OnlineEnemy` en 7778 | ¿Dos copias del mismo enemigo en B (real y marioneta en el mismo sitio)? |
| PE-11 | Salto largo y Castillo (§2.5, §2.2) | `$PY tools/swctl.py --port 7777 goto castle` (si `places` lo tiene) o `teleport` a 1000 m; volver | A.log o B.log: `a long jump: … kept` frente a `a load`; fps de `state` |
| PE-12 | Clima (§2.3) | `$PY tools/swctl.py --port 7777 weather rain`; a los 5 s, `weather` en 7778 con B a menos de 500 m y después a más de 500 m | ¿B sigue el clima de A solo de cerca? |
| PE-13 | Orden duplicada (HE-09) | `$PY tools/swctl.py --port 7777 help \| grep -i bloodmoon`; `grep -i duplicate $SW/pruebas/logs/A.log` | Una sola `bloodmoon`, la del juego |

Lo que **no** se puede probar sin el dueño o sin otro guardado: montar a caballo (el guardado de prueba no tiene
caballo), runas Magnesis y Stasis (aún no las tiene, B2), Prueba de la Espada y Master Mode, cargar otra partida desde
el menú del juego (no hay orden de control) y todo lo que se ve solo en el overlay (no sale en `shot`).

---

## 6. Qué no se verificó

- Nada se compiló ni se ejecutó. Las afirmaciones [L] son lecturas de `coop/ESTADOS @ 1e6831a`; los números de línea
  son aproximados.
- Las causas del cierre `0xc0000409` (huérfanos frente a la lectura del hilo de red durante el reinicio) son
  hipótesis: no hay volcado ni log del proceso que se cerró (B1).
- No se comprobó en el juego: el tipo de mapa de la Prueba de la Espada, cómo el juego nombra los flags de rotura
  fuera del campo, si `OnlineShock` empuja hacia su origen, ni si un *handle* con `unit` viejo llega a coincidir con 2,
  3 o 5 en memoria nueva.
- Las órdenes de §5 están escritas a partir de `tools/swctl.py`, `tools/coop/dos_juegos.sh` y los registros de
  órdenes de `control*.c`. Que `SAVAGEWIND_REBOOT_AT` y `SAVAGEWIND_LAUNCHER_AUTOPLAY` funcionen sin ventana, y que
  `SAVAGEWIND_INPUT` vuelva a pulsar Continuar tras un reinicio, es [H].
- `specs/fases/F2-cimientos/tasks.md` aún marca T207 como pendiente aunque `929661f` lo integró; este documento no
  toca `tasks.md`.
