# Bitácora · F4 T445 · medir el minimapa: investigación en el código

- Fecha: 2026-11-25
- Quién: agente Opus (`coop/T445`) bajo el orquestador (ADR-006): **solo lee código y escribe**; no compila, no
  ejecuta pruebas ni el juego.
- Ficheros: esta entrada y `tools/coop/minimapa_medir.py` (nuevo, opcional, **sin ejecutar**; solo se comprobó que
  Python lo analiza sin errores de sintaxis con `ast.parse`).
- Objetivo (decisión del dueño; ADR-022 y CU-16 los redacta otro agente): dibujar al compañero como una flecha de su
  color encima del **minimapa** del juego, con una marca en el borde si está fuera de alcance. Hace falta, en cada
  fotograma: centro y radio del minimapa en píxeles, su escala (metros por píxel), si gira con la cámara o va con el
  norte arriba, y cuándo no se ve.
- **Nivel de evidencia (constitución, regla 16): todo lo de abajo es hipótesis** (leído en el código del checkout
  `coop/T445`, base `38b28bd`). Nada se ha visto en el juego. Los números de línea son de ese checkout.

## Recomendación (resumen)

**Leer del layout, y medir a mano solo una vez lo que el código no dice.** El runtime ya engancha, cada fotograma y en
un hilo del juego, el dibujo del layout de AppMap (la pantalla 0x2a, que en juego es el minimapa) y ya sabe distinguir
minimapa de mapa grande. Desde ese gancho se pueden leer la matriz global, el tamaño, la visibilidad y el alfa de los
paneles del minimapa con desplazamientos que el propio runtime ya usa en `pc_cursor.c`. Eso da centro, radio,
visibilidad (incluidos HUD "Pro", menús, eventos) y, si el giro es de un panel, también el giro. Lo que falta saberlo
una vez en el juego: **qué paneles** son el minimapa (sus nombres), **si el giro** es una rotación de panel o una
transformación de textura, y **la escala** mundo → minimapa. Para eso está `tools/coop/minimapa_medir.py`.
El paso de unidades del layout a píxeles de la ventana tiene que repetir la cuenta del renderizador
(`runtime/nvn/hud_scale.h`: forma de la ventana, tamaño de HUD, HUD en los bordes) y la caja de la imagen en la
ventana (`OutScale`), que hoy no llega a la capa ImGui. No hace falta tocar `tools/patches.py` ni
`tools/game_sites.py` (no cambia `GAMELIB_ID`, regla 14).

---

## 1. ¿El runtime ya intercepta el dibujo del layout del minimapa?

**Sí, el del layout entero; el de paneles sueltos también, pero desactivado por defecto. Lo que no hace hoy es
publicar nada del minimapa.**

### 1.1 Los ganchos que existen

| Gancho | Dónde se instala | Qué recibe | Cuándo actúa hoy |
|---|---|---|---|
| `rt_ui_layout_draw(c, native)` (`runtime/ui_layout.c:143`) | `tools/patches.py:_ui_layout_draw` sobre el sitio `ui_layout_draw` (un *thunk* de cola compartido por MainScreen, **AppMap**, avisos de contexto y diálogos; el parche comprueba que salta a `ui_layout_native` y sus 8 primeras instrucciones) | `c->x[0]` = la pantalla (objeto de pantalla del ScreenMgr), `c->x[1]` = el contexto de dibujo | **Siempre** que el grupo opcional `('ui_layout_draw', 'ui_layout_native', 'ui_layout_pane')` (`tools/game_sites.py:501`) haya casado. Llama a `ui_layout_begin`, que solo marca un ámbito si hay Tamaño de HUD ≠ 100 o HUD en los bordes (`ui_layout.c:105`); el gancho en sí corre igual |
| `rt_ui_layout_pane_draw(c, native)` (`ui_layout.c:67`) | `tools/patches.py:_ui_layout_pane`, sitio `ui_layout_pane` (entrada del dibujo de un panel de nn::ui2d; afirma `ldrb w8, [x0, #0x58]`) | `c->x[0]` = el panel; nombre en `pane+0xb8` | Solo con Tamaño de HUD ≠ 100 o HUD en los bordes, y solo para `Pa_SinJu_0x`, `N_MainAll_00` y `Pa_QuestName_00`. **Ojo:** el mismo sitio (0x7000027d30 en 1.9.0) es también `replace_pane_draw` (`game_sites.py:479, 609`; `patches.py:_replace_pane`) |
| Ámbito en la lista de comandos | `nvn_ui_scope` (`runtime/nvn/cmdbuf.c:360`) escribe un `CMD_UI_SCOPE`; `exec_list` lo apila (`runtime/nvn/vulkan/exec.c:11880`, `HudScope` `hud_scale.h:206`) | El tipo de layout (`UI_LAYOUT_*`, `ui_layout.h:6`) | Mismo filtro que `ui_layout_begin` |
| `hud_patch` (`exec.c:4937`) y `scissor_rect` (`exec.c:2448`) | En el hilo de render, al ejecutar cada *draw* al target final (`RS_OUT`) | El c3 del vertex de cada panel de nn::ui2d: proyección `g[0..15]`, vista del panel `g[16..27]`, rectángulo del panel `g[132..135]` (c3[33]); y el *scissor* (la **máscara del minimapa**, comentario en `exec.c:2455` y `:2513`) | Solo si la ventana no es 16:9 o el Tamaño de HUD ≠ 100 (`exec.c:4996`) |

Cómo se reconoce el minimapa (`ui_layout.c`):

- `ui_layout_begin` (`:101`): la pantalla es AppMap si es `slots[0x2a]` del ScreenMgr (`g_site.screen_mgr`: `+0x28`
  número de pantallas, `+0x30` el vector de punteros; `runtime/game_sites.h:14`).
- `ui_layout_map_mode(screen)` (`:19`): lee la animación en `screen+0x3640`, su recurso en `+0x18` y el fotograma
  final (`u16` en `recurso+8`) y el actual (`float` en `animación+0x20`). Devuelve **0 = minimapa** (fotograma ==
  final; también mientras se abre el inventario), **1 = mapa grande** (fotograma 0, incluidas transiciones), −1 sin
  datos. Solo para `g_game_version == "1.9.0"`.
- El layout dibuja de verdad si `screen[0x100] && !screen[0x102] && screen[0xfe] && screen[0xff] != 2`
  (`ui_layout.c:124`; lo mismo `framepace.c:screen_draws`, `:162`).
- En minimapa, AppMap queda con ámbito `UI_LAYOUT_HUD` (`ui_layout.c:118`), el mismo que MainScreen: el
  renderizador **no** distingue sus *draws* de los del resto del HUD. Por eso leer del lado del renderizador
  (`hud_patch`, `scissor_rect`) obligaría a añadir un tipo de ámbito nuevo y a reconocer el panel por su geometría
  en el hilo de render; no lo recomiendo.

### 1.2 Qué se puede leer cada fotograma desde `rt_ui_layout_draw`

Con `screen == slots[0x2a]` y `ui_layout_map_mode(screen) == 0`, en el propio gancho (hilo del juego que dibuja la
UI: regla 6 de la constitución, "otro gancho que corra en el hilo del juego"), recorriendo el árbol de paneles sin
llamar al juego:

| Dato | Dónde (ya usado en el runtime) |
|---|---|
| Layout de la pantalla y su panel raíz | `screen+0x30` → layout, `+0x18` → raíz (comentario de las tiendas, `runtime/hle/framepace.c:198-203`) [sin verificar que AppMap use la misma clase base] |
| Hijos de un panel | cabeza de lista en `pane+0x20`, enlazada 8 bytes dentro de cada hijo (`pc_cursor.c:pane_named`, `:1385`) |
| Nombre | `pane+0xb8` (`ui_layout.c:74`, `pc_cursor.c:1388`) |
| Visible y alfa | `pane+0x58` bit 0 y `pane+0x5a` alfa global (`pc_cursor.c:1499`) |
| Tamaño | `pane+0x50` ancho, `+0x54` alto (`pc_cursor.c:options_box`, `:490`) |
| Matriz global 3x4 | fila 0 en `pane+0x70,+0x74,+0x78`, traslación x en `+0x7c`; fila 1 en `+0x80,+0x84,+0x88`, traslación y en `+0x8c` (`pc_cursor.c:493-495`) |

Unidades: las del layout, **1280x720 con el origen en el centro y la y hacia arriba** (`pc_cursor_math.h:22-28`,
`pc_cursor_to_layout`; las cajas de `pc_cursor.c:994` se comprueban contra ±640/±360). Son las coordenadas **antes**
de que el renderizador aplique Tamaño de HUD y HUD en los bordes (eso pasa después, en la GPU: §3).

Con eso: **centro** = traslación del panel del disco; **radio** = semiancho del panel (`|m00|·w/2 + |m01|·h/2`);
**giro** = `atan2(m10, m00)` si el contenido gira rotando un panel; **visible** = el layout se dibujó este fotograma
en modo 0, el panel tiene el bit 0 y alfa > 0. El alfa sirve además para atenuar la flecha cuando el HUD se funde.

### 1.3 Lo que el código no dice (hay que verlo una vez en el juego)

1. **Los nombres de los paneles del minimapa.** No están en el repositorio y no se pueden sacar de proyectos de
   descompilación (regla 2). Salen de nuestra propia copia con `minimapa_medir.py paneles` (lee el árbol con
   `mem read`, sin escribir nada). El runtime ya usa nombres de panel encontrados así (`Pa_SinJu_00`…).
2. **Si el giro es del panel o de la textura.** Si el minimapa gira rotando un panel, su matriz lo dice. Si gira con
   una transformación de coordenadas de textura (SRT del material), la matriz no cambia y el giro hay que tomarlo de
   la cámara. `docs/SAVAGE-WIND.md:1998` habla de "the minimap turning", lo que apunta a que **gira** [sin
   verificar si con la cámara o con Link].
3. **La escala mundo → minimapa.** Para el **mapa grande** el runtime ya la conoce: `screen+0x3628` → objeto mapa,
   `+0x8d8` → datos `d`; `d+0x870/+0x874` traslación, `d+0x878` zoom, `d+0x880/+0x884` centro
   (`pc_cursor.c:982`, `:1212`) y `mundo = (punto − traslación) / zoom` (`pc_cursor_math.h:30`,
   `pc_cursor_map_world`). **Hipótesis:** en modo minimapa el mismo objeto lleva el zoom y la traslación del
   minimapa. Si se confirma, la escala se lee cada fotograma; si no, es una constante medida. Esas lecturas de
   `pc_cursor.c` usan direcciones fijas de 1.9.0 (`0x7001ce8298` como vtable de AppMap, `0x7000f6f800` para buscar
   paneles): no se copian al código nuevo sin pasar por `g_site` (regla 13).
4. **Si Link está en el centro exacto** y si el norte del minimapa es −z (hipótesis: x este, z sur, como el mapa).

## 2. Cómo dibuja hoy la capa online y qué se reutiliza

- `online_overlay_draw` (`runtime/ui/online_ui.mm:181`) se llama desde `menu_render` (`runtime/ui/menu.mm:2594`) en
  el hilo que presenta, en la lista de fondo de ImGui (debajo del menú, **encima del HUD del juego**).
  `io.DisplaySize` es el tamaño de la *swapchain* en píxeles (`menu.mm:2548`); `scale` = alto / 1080 × tamaño de
  texto (`menu.mm:2556-2557`).
- Mundo → pantalla: `project()` (`online_ui.mm:135`) multiplica por la matriz vista-proyección `vp` que publica el
  renderizador (`exec.c:11004-11005`, en `vk_fg_generate`, con `online_wants_camera`; `online_camera_set/get`,
  `runtime/online.c:1532/1540`, bajo cerrojo) y descarta una cámara de más de 0,5 s (`online_ui.mm:256`).
- Se reutiliza tal cual:
  - los colores por ranura `k_colors` y `with_alpha`; `label()` (`:173`) para nombre y distancia con fondo oscuro;
  - el triángulo de la flecha de rumbo de `figure()` (`:163-170`) y el de la flecha de borde (`:289-302`): para el
    minimapa, el mismo triángulo orientado con `p.yaw` menos el giro del minimapa, y la marca de borde **recortando
    a un círculo** (radio − margen) en vez de al rectángulo de la pantalla;
  - las condiciones de dibujo: `in_play` (`:186`), `show_markers`, `ONLINE_F_LOADING`, "otro mapa" (> 3000 m,
    `:266`; mejor `pose.map` cuando exista, CU-10);
  - los datos del compañero de `online_players()` (`pos`, `yaw`, `flags`) y `online_local_id()`.
- No se reutiliza la proyección: el minimapa es 2D. La cuenta es `d = (x_comp − x_link, z_comp − z_link)` en metros →
  girar por el giro del minimapa → × píxeles por metro → + centro; si `|d| > radio − margen`, marca de borde.
- Dos defectos de hoy que afectan a esto:
  1. **La capa usa toda la ventana como si fuera la imagen del juego.** Con barras (Llenar pantallas anchas apagado
     en 21:9, o `SAVAGEWIND_WIDE_TEST`) la imagen va en una caja `vx, vy, vw, vh` (`OutScale`,
     `runtime/nvn/output_scale.h:5-11`, calculada en `device.c:3150-3158`) y los marcadores del mundo quedan
     desplazados. Esa caja **no se publica** hoy para ImGui: hace falta un *getter* nuevo (lo mismo para el
     minimapa).
  2. **La posición de Link se lee desde el hilo de la UI** (`online_ui.mm:258-259`, `player_info+0x88`): ya lo
     señala CU-10 y va contra la regla 6. La flecha del minimapa no debe añadir lecturas así: el gancho del layout
     publica una instantánea y la UI solo la copia (patrón de `online_camera_set/get`).

## 3. Ultrawide y Tamaño de HUD

El HUD del juego es un layout 16:9 dibujado con una cámara de 40° en el target final. El runtime lo recoloca en la
GPU (`hud_patch`, `scissor_rect`), así que **las unidades del layout no cambian** con la ventana: cambia el paso a
píxeles. La cuenta para un punto del minimapa `(lx, ly)` (layout, y arriba) y su radio `rl`:

1. **Caja de la imagen** (`output_scale.c:output_scale`, `:380-450`): `forma = (W/H) / (16/9)` (o
   `SAVAGEWIND_WIDE_TEST`). Con Llenar pantallas anchas (`wide_view`) y `|forma − 1| > 0,005`, la imagen ocupa la
   ventana (`vw = W, vh = H`); si no, `forma = 1` y va una caja 16:9 con barras. Con `WIDE_TEST`, caja de esa forma
   dentro de la ventana.
2. **Banda 16:9**: `borde_x = 1 / max(forma, 1)`, `borde_y = min(forma, 1)` (`exec.c:5000`);
   `nx = lx/640 · borde_x`, `ny = ly/360 · borde_y`.
3. **Tamaño de HUD** `s = hud_size/100` (`hud_scale_view`, `hud_scale.h:54`): ancla de la zona 3x3 del panel
   (`hud_anchor`, `:10`; el minimapa, abajo a la derecha: `(borde_x, −borde_y)`), `n' = a + (n − a)·s`, radio × s.
   `hud_size = 0` **oculta** el HUD (`settings.h:281`): hay que ocultar la flecha.
4. **HUD en los bordes** (`hud_edges`, solo con forma ≠ 1): en ventanas más anchas `nx += (ax/borde_x)(1 − borde_x)`
   (`hud_edge_dx`, `hud_scale.h:177`), en más altas `ny += (ay/borde_y)(1 − forma)` (`hud_edge_dy`, `:138`). El
   texto de la opción solo habla de ventanas más altas (`menu.mm:841-842`), pero el código lo hace también en las
   más anchas.
5. **Píxeles**: `X = vx + (nx + 1)/2 · vw`, `Y = vy + (1 − ny)/2 · vh`; radio `rl/640 · borde_x · s · vw/2`.

Como AppMap en minimapa tiene ámbito HUD propio, `hud_owned()` es cierto (`exec.c:1442`): el Tamaño de HUD se aplica
también en eventos (`hud_scale_now`, `:1453`) y el modo de bordes es siempre 1 (`hud_edges_now`, `:1460`). Las
excepciones de `hud_patch` (barra del menú rápido, avisos de contexto, tiendas) están en el centro de la pantalla, no
en la esquina del minimapa. Escala de render, *upscaler* y Campo de visión no mueven el HUD (`output_scale.c:415-424`:
solo la escena 3D cambia de resolución; el FOV solo toca la cámara del mundo).

Ejemplos (punto `(lx, ly)` del layout; `1,5 px` por unidad cuando la altura útil es 1080):

| Ventana y ajustes | Centro en píxeles | Nota |
|---|---|---|
| 1920x1080 | `(960 + 1,5·lx, 540 − 1,5·ly)` | referencia |
| 2560x1080, Llenar sí, bordes no | `(1280 + 1,5·lx, 540 − 1,5·ly)` | el HUD queda en la banda 16:9 central (320 px de margen a cada lado) |
| 2560x1080, Llenar sí, **bordes sí** | `(1600 + 1,5·lx, …)` | el minimapa se va 320 px a la derecha, al borde |
| 2560x1080, Llenar **no** | `(1280 + 1,5·lx, …)` | barras de 320 px; mismo sitio que sin bordes |
| 1920x1200 (16:10), bordes sí | `(…, 600 − 1,5·ly + 60)` | la parte de abajo baja 60 px al borde |
| 1920x1080 con `WIDE_TEST=1.3333` | imagen de 2560x1080 en una caja de 1920x810 en `y = 135`: ventana = 0,75 × imagen + (0, 135) | sirve para probar 21:9 en este portátil 16:9 |

`tools/coop/minimapa_medir.py prediccion` hace esta cuenta (y la inversa con `--inversa`) para comparar lo predicho
con lo medido. La caja de `pointer_box` de `menu.mm:2585` (`pc_cursor_layout_scale`) es para los menús de pausa
(1280x720 ajustado con barras), **no** vale para el HUD con bordes.

Con generación de fotogramas, el HUD de los dos fotogramas es el del fotograma real más nuevo y su giro avanza al
ritmo real (`docs/SAVAGE-WIND.md:1998`); la capa ImGui se dibuja en los dos (`docs/SAVAGE-WIND.md`, mapa 04
`device.c:3261-3262`). La flecha seguirá el ritmo de la instantánea del layout: aceptable.

## 4. Procedimiento de medición (lo ejecuta el orquestador)

`SAVAGEWIND_WINDOW_SHOT(_AT)` (`device.c:3220-3237`) solo se programa por número de fotograma al arrancar y guarda la
**ventana** (con barras y ImGui). Para medir el minimapa es mejor la orden de control **`shot`**
(`control_game.c:250`, `device.c:2955`): se pide en cualquier momento y guarda la **imagen del juego** (con el HUD,
sin la capa ImGui ni las barras). La captura de ventana queda para comprobar al final, a ojo, que la flecha nueva cae
sobre el minimapa. Las órdenes `teleport`, `look`, `pos`, `camera`, `mem read`, `sites`, `time`, `set` y `get` son de
`docs/CONTROL.md` (necesitan `SAVAGEWIND_CONTROL`).

**Preparación** (un solo juego; copias de partida y ajustes, como en `AGENTS.md`):

```
SAVAGEWIND_SAVE_DIR=<copia> SAVAGEWIND_SETTINGS=<copia.ini> SAVAGEWIND_NO_LAUNCHER=1
SAVAGEWIND_CONTROL=7777 SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"   (sin SAVAGEWIND_HIDDEN, por si acaso)
```

En el mundo: `swctl.py goto` a un sitio **llano y abierto, sin agua ni acantilados en 40 m** (p. ej. la pradera de la
Meseta; anotar la línea `place` de `pos`), Link quieto, HUD normal del juego (no "Pro"). Ajustes de partida:
Llenar pantallas anchas sí, Tamaño de HUD 100, HUD en los bordes no.

**Paso A · encontrar los paneles** (sin mover nada):

```
python tools/coop/minimapa_medir.py --puerto 7777 paneles --salida pan_base.json
python tools/coop/minimapa_medir.py --puerto 7777 paneles --todos --salida pan_todos.json
```

Comprobar que `estado` dice que AppMap dibuja (`fe` ≠ 0, `ff` ≠ 2, `100` ≠ 0, `102` = 0) y que la raíz se llama
`RootPane`; si no, el eslabón `+0x30/+0x18` no es el de AppMap y hay que buscarlo con `mem path` desde la pantalla.
Candidatos: paneles visibles con centro abajo a la derecha y semiancho parecido en x e y (un disco).

**Paso B · 16:9** (`medir` hace solo: congela la hora, captura en la posición inicial, con la cámara +90°, tras mover
a Link +40 m en x y en z, al volver y con `hud_size 0`; restaura el Tamaño de HUD y descongela la hora):

```
python tools/coop/minimapa_medir.py --puerto 7777 medir --juego <carpeta del juego> --etiqueta 16x9
```

Salen `minimapa_16x9.json` y `minimapa_16x9_marcado.png` en la carpeta del juego. Leer:
- `circulo`: centro y radio en píxeles de la imagen (de la mancha del HUD, **aproximado**). Mirar el PNG marcado; si
  el círculo magenta no coincide con el borde del minimapa, medirlo a ojo sobre `minimapa_16x9_base.png` y repetir
  con `--centro CX CY --radio R` (o comprobar con `marcar`).
- `giro`: `giro_horario` ≈ ±90 y `error` claramente menor que `error_sin_giro` → **gira con la cámara**; ≈ 0 →
  norte arriba (o gira con Link: repetir girando a Link en vez de la cámara).
- `paneles_cambian_girada`: si algún panel cambia su `giro` en ±90°, el giro se lee de la matriz de ese panel.
- `ejes_px_por_metro`: hacia dónde va en pantalla +x y +z del mundo y cuántos píxeles por metro (la escala es
  `1 / px_por_metro` m/px). Si `x` y `z` no son perpendiculares y de igual módulo, algo falló (Link cayó, la cámara
  se movió).
- `paneles_cambian_mas_x` / `_mas_z`: si un panel se desplaza, su traslación lleva la posición del mundo (escala en
  unidades del layout por metro, que no depende de la ventana).
- `desplazamiento_vuelta` ≈ (0, 0): si no, la posición o la cámara no volvieron igual.
Pasar el centro y el radio al layout: `prediccion CX CY R --ventana W H --inversa` (W, H = tamaño de la imagen si
llena la ventana).

**Paso C · 21:9**: reiniciar con `SAVAGEWIND_WIDE_TEST=1.3333` (imagen de 2560x1080 en caja dentro de la ventana
16:9), repetir el paso B con `--etiqueta 21x9`. Comparar con `prediccion LX LY RL --ventana 2560 1080`: debe
coincidir el centro ±2 px y el radio ±1 px. Si hay monitor 21:9 de verdad, repetirlo también en ventana de ese
tamaño (ahí imagen = ventana).

**Paso D · HUD**: en 21:9, `set hud_size 80` y `set hud_edges on` (por control o en Opciones), `medir --etiqueta
21x9_hud80_bordes`, y comparar con `prediccion … --hud 80 --bordes`. Repetir en 16:9 con `hud_size 130`.

**Paso E · cuándo no se ve** (a mano, con `paneles` y `shot` en cada caso; anotar `estado`, visibilidad y alfa del
panel del minimapa y `ui_layout_map_mode` implícito por la animación): mapa grande abierto, inventario y menú de
pausa, menú rápido (cruceta), diálogo con un PNJ, una escena de historia, dentro de un santuario, HUD "Pro" en las
opciones del juego, cámara/catalejo de la tableta, carga. Lo que haga falta es que en todos ellos o el layout no se
dibuje en modo 0, o el panel no sea visible, o `in_play` sea falso.

**Qué apuntar** (en la bitácora del orquestador): versión (1.9.0), FPS, GPU y controlador, ventana y ajustes de cada
corrida, el JSON y los PNG marcados (fuera de git: son capturas del juego, regla 1), y el resultado observado.

## Riesgos

- **Nombres de panel y desplazamientos de 1.9.0**: todo va detrás de `strcmp(g_game_version, "1.9.0")` y del grupo
  opcional de sitios; si no casa, la flecha no se dibuja (nada de suponer constantes). El recorrido del árbol tiene
  que validar cada puntero con `ui_range` y limitar profundidad y número de paneles, como `pane_named`.
- **Giro por textura**: si el minimapa gira por SRT de textura, la matriz no sirve y hay que tomar el giro de la
  cámara: `control_camera_view` (`device.c:2962`) solo existe con AA temporal (`mvec_enabled`), y de `vp` hay que
  sacar el rumbo a mano. Leer el SRT del material del panel sería más ingeniería inversa.
- **Escala variable**: santuarios, Bestias Divinas o el DLC usan mapas propios (`pc_cursor.c:725` menciona
  AppMapDungeon). Con el compañero en otro mapa no hay flecha (CU-10), pero dos jugadores en el mismo santuario
  podrían ver una escala distinta. Si `d+0x878` resulta ser el zoom del minimapa, se resuelve leyéndolo.
- **Hilos**: el gancho del layout corre en un hilo del juego y la capa ImGui en el de presentación. La instantánea
  se publica sin bloquear al juego (cerrojo corto o secuencia atómica) y se limpia con `RT_ON_RESET` (regla 8); no
  se guardan punteros de panel entre reinicios. Una instantánea vieja (más de ~100 ms o de N presentaciones) =
  oculto.
- **No añadir un tercer consumidor al sitio de panel** (`ui_layout_pane` y `replace_pane_draw` comparten dirección):
  recorrer el árbol una vez por dibujo de AppMap desde `rt_ui_layout_draw` y cachear los punteros del panel por raíz.
- **Dibujar encima**: ImGui va siempre encima del HUD (mapa 04); la flecha tapará iconos del minimapa. Mantenerla
  pequeña y con contorno.
- **Barras / `WIDE_TEST`**: hasta que ImGui conozca `OutScale`, la flecha (y los marcadores del mundo de hoy) caen mal
  con barras.
- **Medición**: `mem read` por control lee mientras el juego escribe (sin sincronizar); vale para medir, no para el
  código. Teletransportar 40 m puede dejar a Link en una pendiente o en agua: elegir bien el sitio.
- **Metal**: `rt_ui_layout_pane_draw` y `ui_layout_begin` no hacen nada en `__APPLE__`; Metal no se mantiene.

## Qué NO se verificó

- Nada de esto se ha visto en el juego ni compilado. Los desplazamientos de §1.2 están tomados de otros usos del
  runtime; que AppMap tenga su layout en `+0x30` y su raíz en `+0x18` es una suposición.
- No sé si el minimapa gira con la cámara, con Link o va con el norte arriba, ni su escala, ni los nombres de sus
  paneles.
- `tools/coop/minimapa_medir.py` **no se ha ejecutado**: ni el decodificador de PNG (solo RGB/RGBA de 8 bits) contra
  las capturas reales de `shot`, ni las búsquedas de giro y desplazamiento, ni el formato exacto de las respuestas de
  `get`. La detección del círculo por la mancha del HUD es aproximada y puede incluir los medidores de al lado.
- Que `set hud_size 0` oculte el HUD en el acto (lo dice `settings.h:281`; no lo he visto).
