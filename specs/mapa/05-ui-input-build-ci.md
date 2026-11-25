# Mapa 05 — UI, entrada, build/release, CI/gobernanza y tests

Repositorio: `SavageWindRecomp`, rama `test`, commit `2ae4b39` ("Beta 1 fixes ..."). Todas las rutas son relativas a
la raíz del repo. Las referencias `archivo:línea` se comprobaron contra ese commit; se desplazan con cualquier cambio.

Cómo leer este documento:

- **Verificado** = leído en el código o ejecutado (`tools/i18n_check.py` se ejecutó: 1245 textos, 0 faltantes en los
  13 idiomas).
- **No verificado** = no se pudo comprobar en esta máquina. Aquí no hay MSYS2, `make`, `clang` ni Go instalados, así
  que **no se compiló nada ni se ejecutó ningún `make test-*`**. Los comandos de build y test salen de leer el
  `Makefile`, los docs y los workflows.

Objetivo aguas abajo: modo cooperativo online de 2 jugadores en un fork (lobby, ajustes, traducciones, módulo de red
en C dentro de `runtime/`, servidor relay en Go en `server/`).

---

## 0. Lo esencial en 12 puntos

1. **Un ajuste nuevo toca 4-6 archivos calientes**: `runtime/settings.h` (campo), `runtime/settings.c` (default,
   carga, guardado), opcionalmente `runtime/control_settings.c` (tabla CLI), `runtime/ui/menu.mm` (overlay),
   `runtime/ui/game_options.mm` (Options del juego) y los **13** `runtime/ui/i18n/*.inc`.
2. **Las filas del menú Options del juego se añaden en C++**, en `build_items()` de `runtime/ui/game_options.mm:367`.
   **No hace falta tocar `tools/patches.py`** para añadir una fila; los hooks ya existen.
3. **Tocar `tools/patches.py`, `tools/game_sites.py`, `tools/gen.py`, `runtime/cpu.h`, `runtime/simd*.h`,
   `runtime/gamelib.h` o `runtime/hle/hle_symbols.txt` cambia `GAMELIB_ID`** (`Makefile:160-164`) y obliga a
   regenerar y recompilar todo el código del juego.
4. **Un `.c` nuevo en `runtime/` entra solo en el build** (comodín `runtime/*.c`, `Makefile:62`). Un subdirectorio
   nuevo (`runtime/net/`) **no** entra: hay que añadirlo a `RT_SRC` y `RT_HDRS`.
5. **`ws2_32` ya está enlazado** en Windows (`Makefile:141`) y ya hay código de sockets multiplataforma
   (`runtime/dsu.c` UDP, `runtime/control.c` TCP local). En Linux/macOS los sockets van en libc.
6. **El código nuevo debe compilar en Windows, Linux y macOS**: los tres tienen job de build en CI y el comodín mete
   cada `.c` en los tres.
7. **Traducciones**: la clave es el texto en inglés. Cada texto nuevo necesita una línea `T("English", "...")` en los
   13 `.inc`; si falta en alguno, `tools/i18n_check.py` sale con 1 y el job `i18n` falla.
8. **CI no ejecuta tests en Windows**: `windows.yml` solo compila. La mayoría de los `make test-*` no corren en
   ningún workflow.
9. **Ningún check de PR necesita secretos**. Los únicos secretos son los webhooks de `activity-feed.yml`, que además
   está limitado al repo original.
10. **`checks.yml` y `.gitignore` tienen patrones que pueden morder a un `server/` en Go**: directorios llamados
    `input`, `save`, `keys`, `dump`, `generated*`, `extracted`, `build`; archivos llamados exactamente `main` o `sdk`.
11. **El actualizador apunta al repo original** (`REPO "Revivedx/SavageWindRecomp"` y clave pública embebida). Un
    binario del fork ofrecería "actualizarse" a la release oficial, sin coop.
12. **Postura del proyecto**: multijugador = "No" (FAQ, issue #617); los issues que lo piden se cierran. Los PR no se
    fusionan por GitHub: el mantenedor los aplica a mano en `test`.

---

## 1. UI

Hay **tres superficies de UI** distintas. Conviene decidir pronto en cuál vive el lobby:

| Superficie | Quién dibuja | Archivo | Cuándo se ve |
|---|---|---|---|
| Pantalla de inicio / "Savage Wind Configuration" | ImGui | `runtime/ui/launcher.mm` | Antes de arrancar el juego (`--config`, o setup inicial) |
| Overlay en juego (F1 / Esc / Back+Start) | ImGui | `runtime/ui/menu.mm` | Sobre el juego; comparte pestañas con Settings de la pantalla de inicio |
| Páginas propias dentro de Options del juego | El propio juego (layout, fuente y sonidos nativos) | `runtime/ui/game_options.mm` + hooks de `tools/patches.py` | Pausa > System > Options, y Options del título |

### 1.1 Archivos de `runtime/ui/`

| Ruta | Líneas | Qué es |
|---|---|---|
| `runtime/ui/menu.h` | 33 | API pública del overlay: `menu_handle_event`, `menu_is_open`, `menu_render`, `menu_render_needed` (`menu.h:17-28`) |
| `runtime/ui/menu.mm` | 2641 | Overlay. Eventos y teclas (`menu_handle_event`, `:248`; F1 en `:301`, F10 `:310`, F11 `:315`, consola en `:285`). Páginas: `tab_colour :676`, `tab_display :735`, `tab_audio :1355`, `cheats_options :1392`, `tab_amiibo :1510`, `tab_language :1566`, `tab_quit :1594`, `tab_cheats :1675`, `tab_teleport :1746`, `tab_about :1843` (es la pestaña **Controls**, pese al nombre). Barra de pestañas: `tabs_body :2223`. Entrada compartida con el launcher: `menu_tabs_draw :2360`. Overlays: foto `:1923`, rendimiento `:1985`, teclado `:2088`. Al cerrar el overlay se guarda: `set_open` → `settings_save()` (`:122`) |
| `runtime/ui/option_rows.h` / `.mm` | 61 / 356 | Widgets de página de ajustes en dos paneles (fila a la izquierda, descripción a la derecha). API en `option_rows.h:11-60`: `opt_begin`, `opt_section`, `opt_toggle`, `opt_choice`, `opt_list`, `opt_dropdown`, `opt_slider`, `opt_int`, `opt_button`, `opt_radio`, `opt_action`, `opt_text` (campo de texto, con teclado en pantalla para mando), `opt_disable_next`, `opt_indent`, `opt_end`. **Traducen solos**: etiqueta y descripción pasan por `tr()` dentro (`option_rows.mm:51,98,109`) |
| `runtime/ui/launcher.mm` | 2675 | Pantalla de inicio. Versión del producto: `#define SAVAGEWIND_VERSION "0.6.0-alpha.6"` (`:114`). Páginas `enum Page { MAIN, SETTINGS, SETUP }` (`:1219`). La página SETTINGS llama a `menu_tabs_draw(..., false, prelaunch_tab)` (`:2072`). `prelaunch_tab` (`:1456`) es la pestaña "Prelaunch". Bucle principal `launcher_run` (`:1733`). También setup de archivos, DLC, precompilación de shaders y CLI (`launcher_setup_cli :985`) |
| `runtime/ui/game_options.mm` | 2481 | Páginas propias en el Options del juego. Cabecera explicativa `:1-15`. `enum Page` (`:100`), `enum Action` (`:102`), `struct Item` (`:105-120`), constructores `toggle :255`, `choice :262`, `button :278`. Lista de filas: `build_items()` (`:367-1429`). Instalación en la tabla del juego: `install()` (`:1431`). Hooks llamados desde el código recompilado: `rt_opt_rows :1977`, `rt_opt_read :2120`, `rt_opt_store :2136`, `rt_opt_reset :2175`, `rt_opt_msg :2189`, `rt_opt_decide :2237`, `rt_opt_tick :2299`, `rt_opt_back :2330`, `rt_opt_title_tick :2420`, `rt_opt_pause_tick :2433` |
| `runtime/ui/controls_ui.mm` | 296 | Editor de asignaciones de teclas/botones (`controls_bindings_draw`), usado por la pestaña Controls |
| `runtime/ui/binding_row_layout.h` | 28 | Geometría de las filas de dos botones de Key bindings |
| `runtime/ui/mods_tab.h` / `.mm` | 6 / 355 | Pestaña Mods. API de 2 funciones (`mods_tab.h:5-6`). **Mejor ejemplo de pestaña en archivo propio** |
| `runtime/ui/saves_tab.h` / `.mm` | 7 / 927 | Pestaña Saves (solo en la pantalla de inicio) |
| `runtime/ui/launch_flags_ui.h` / `.mm` | 13 / 194 | Sección "Launch options" de la pantalla de inicio (variables `SAVAGEWIND_*` guardadas en `launch-flags.txt`) |
| `runtime/ui/i18n.h` / `.mm` | 39 / 674 | Traducción: `tr()`, `tr_upper()`, `ui_lang()`, RTL para hebreo, fuentes de reserva. Tablas incluidas en `i18n.mm:32-68`. Packs de idioma del jugador: `read_pack` (`:127`) |
| `runtime/ui/i18n/*.inc` | ~1263 c/u | 13 tablas: `de es fr he it ja ko nl pt ru th zh-Hans zh-Hant` |
| `runtime/ui/osk.h` / `.mm` | 27 / 187 | Teclado en pantalla para mando (lo usa `opt_text`) |
| `runtime/ui/console.h` / `.mm` | 15 / 154 | Consola en juego (`` ` `` o F12). En inglés; excluida del check de i18n |
| `runtime/ui/splash.h` / `.mm` | 10 / 516 | Fondo de la pantalla de inicio |
| `runtime/ui/gyro_drag.h` | 75 | Arrastre de ratón para el giroscopio virtual |
| `runtime/ui/file_size_cache.*`, `quick_hint_font.*`, `quick_inventory_layout.h` | — | Auxiliares (tamaños de archivos, fuente de pistas del inventario rápido) |

Nota: los `.mm` son **C++ normal** salvo bloques `__APPLE__`; en Windows y Linux se compilan con `-x c++`
(`Makefile:127,148`).

### 1.2 Barra de pestañas del overlay (`tabs_body`, `runtime/ui/menu.mm:2223-2358`)

- Array local `Tab tabs[12]` (`:2226`) con nombre, id y habilitada.
- Nombres por id: `names[] = {"Video","Audio","Language","Saves","amiibo","","","","Controls","Quit","Cheats","Teleport","Mods"}` (`:2230-2231`). Los ids 5-7 están vacíos pero **reservados** (hooks de test).
- Orden visible: `order[] = {8, 0, 1, 2, 4, 3, 12, 10, 11, 9}` (`:2233`).
- Filtros (`:2234-2240`): Quit solo en juego; Saves solo en la pantalla de inicio; Cheats y Teleport solo en juego y con modo desarrollador (`control_enabled()`).
- Despacho por id: `switch (t.id)` (`:2324-2346`). Mods es el `case 12` (`:2335`).
- `SAVAGEWIND_MENU_TAB=<id>` abre una pestaña concreta (`:102`), útil para pruebas automáticas.
- Inconsistencia menor: `mods_tab.h:1` y `mods_tab.mm:1` dicen "tab 11", pero el id real es **12**.

### 1.3 Ajustes (`runtime/settings.h`, `runtime/settings.c`)

| Qué | Dónde |
|---|---|
| Struct `Settings` | `runtime/settings.h:91-244` |
| Valores por defecto (inicializador de `g_settings`) | `runtime/settings.c:~27` |
| Archivo: `savagewind.ini` o `SAVAGEWIND_SETTINGS` | `settings_file`, `runtime/settings.c:19` |
| Carga (cadena de `else if (!strcmp(key, ...))`) | `settings_load`, `runtime/settings.c:348` |
| Guardado (un `fprintf` por clave) | `settings_save_to`, `runtime/settings.c:771` |
| Versión de formato | `SETTINGS_VERSION 4`, `runtime/settings.c:126`; historia en `:114-124` |
| Tabla para `savagewind settings set` y el endpoint de control | `runtime/control_settings.c:27-...` (macros `B`, `I`, `F`) |

Límite a tener en cuenta: la carga lee cada línea con `char line[256]` y `val[128]` (`settings.c:359-365`), así que
**un valor de más de 127 caracteres se trunca**. Importa si se guarda una URL de relay o un token largo.

Campos de texto existentes que sirven de modelo: `display_name[64]` (`settings.h:97`), `motion_dsu_axes[32]`
(`:150`), `save_dir[512]` (`:240`; carga en `settings.c:617`).

### 1.4 Options del juego y `tools/patches.py`

Mecanismo (verificado en `game_options.mm:1-15` y `:1431-1476`):

1. El juego construye una fila por entrada de una tabla estática cada vez que abre Options (`g_site.opt_table`).
2. La primera vez, `install()` apunta esa tabla a una copia con las 14 entradas del juego más una por cada `Item` de
   `build_items()`.
3. Cada página (`enum Page`) tiene su orden de filas en `g_order[P_COUNT]`.
4. Textos: cada fila usa ids de mensaje `MSG_BASE + i*256 + parte`; `rt_opt_msg` (`:2189`) devuelve `tr(it->title)` /
   `tr(it->desc)` (`:2204-2206`).
5. Valor y acción: `rt_opt_read`, `rt_opt_store`, `rt_opt_decide` llaman a los `get` / `set` del `Item`.

Lado del generador: `tools/patches.py:190-386` define los parches (`_opt_rows :223`, `_opt_read :240`,
`_opt_store :248`, `_opt_reset :256`, `_opt_msg :263`, `_opt_decide :272`, `_opt_tick :285`, `_opt_back :336`,
`_title_tick :345`, `_pause_option_state :378`) y los registra en `PATCHES` (`:1216-1235`).

Investigación previa: `docs/research/option-row-second-button.md` (dos controles nativos por fila, análisis
estático) y `docs/research/two-button-row-spike.md` (prototipo en juego). `docs/research/colour-vision.md` es de
otro tema.

Regla del proyecto relevante: **los trucos no van en los menús del juego**, solo en el overlay y la consola
(`docs/FAQ.md:630-635`). Una función no original como el coop encaja mejor en overlay o pantalla de inicio; una fila
en Options del juego es posible pero es una decisión de producto.

### 1.5 `tools/i18n_check.py`

- Fuentes escaneadas (`:21-25`): todos los `runtime/ui/*.mm` **excepto `console.mm`**, más `runtime/language.c`,
  `runtime/hle/bindings.c`, `runtime/launch_flags.c`. También los nombres de presets de `runtime/color_grade.c`
  (`:120-124`).
- **Un `.c` nuevo fuera de esa lista no se escanea.** Si el módulo de red devuelve textos para el jugador, o se
  traducen en el punto de uso (`tr()` en un `.mm`) o se añade el archivo a `SOURCES`.
- Heurística: recoge literales de cadena que "parecen texto" (`looks_like_text`, `:78`). Se saltan líneas con `LOG(`,
  `fprintf(`, `printf(`, `getenv(`, `SAVAGEWIND_`, `//`, etc. (`SKIP_LINE`, `:26-28`) y las que contienen el
  comentario `i18n-skip` (`:116`).
- Resultado: **faltante → exit 1**; "unused" y "left in English" solo se listan (`:176-177`).
- `--missing <lang>` imprime las líneas `T("...", "")` que faltan, listas para pegar (`:154-159`).

---

## 2. Entrada

| Ruta | Líneas | Qué hace | Puntos de entrada |
|---|---|---|---|
| `runtime/hle/input.c` | 1298 | SDL (teclado, ratón, mando) → estado `nn::hid` Npad que lee el juego | `input_handle_event :672`, `input_frame :1025`, `input_buttons_now :1093`, `fill_state :1138`, HLE `GetNpadState` `:1227-1231`, `input_reset :282`, foco `:513-525`, entrada guionizada de test `host_test_frame :911` |
| `runtime/hle/bindings.c` | ~1000 | Tablas de asignación, captura "pulsa una tecla", acciones por juego, líneas de `savagewind.ini` | `bind_capture_event :343`, `bind_settings_load :408`, `bind_settings_save :449`, `rt_act_query :906` |
| `runtime/input_bind.h` | 160 | Enum `BIND_*` (`:16-28`), dispositivos (`:31,43`), tablas `g_bind_pad/key` (`:56-59`) | — |
| `runtime/hle/motion.c` | 1267 | Sensores de movimiento → `SixAxisSensorState`. Fuentes: giroscopio SDL, mando "gemelo" de Steam Input, DSU, IMU del Deck, giroscopio virtual | `motion_sensor_event :615`, `motion_poll :812`, `motion_keyboard :913`, HLE `:1223-1244` |
| `runtime/hle/vibration.c` | 181 | Vibración HD del SDK → dos motores de SDL, con hilo propio | `vibration_send :24`, `rumble_worker :82`, `vibration_update :134` |
| `runtime/dsu.c` / `dsu.h` | 280 | **Cliente UDP** del protocolo Cemuhook DSU. Winsock y BSD con `#ifdef _WIN32` (`:9-22`), `WSAStartup` en `:183` | `dsu_init :141`, `dsu_poll :243`, `dsu_close :168` |
| `runtime/deck_imu.c` / `.h` | 126 | Lee el IMU del Steam Deck por hidraw (solo Linux) | `deck_imu_poll :84` |
| `runtime/pc_gyro.h` | 34 | Paso e interpolación del giroscopio virtual (solo header) | `pc_gyro_step`, `pc_gyro_follow` |
| `runtime/pc_gyro_scene.c` / `.h` | 77 / 20 | Detecta si el jugador controla un aparato de santuario (sitio opcional `gyro_scene_actor`, solo 1.9.0) | `pc_gyro_scene_tick :40`, `rt_gyro_scene_actor :65` |
| `runtime/pc_bow.h` | 36 | Máquina de estados "apuntar con botón derecho, disparar con izquierdo" (solo header) | `pc_bow_aim`, `pc_bow_fire`, `pc_bow_buttons` |
| `runtime/pc_cursor.c`, `pc_cursor_math.h` | — | Puntero de ratón en menús, inventario y mapa | — |
| `docs/controls.md` | 35 | Controles por defecto y cámara de ratón |
| `docs/PC_MOUSE_CONTROLS.md` | 66 | Puntero de PC: comportamiento y validación |
| `docs/VIRTUAL_GYRO.md` | 62 | Giroscopio virtual; `make test-gyro` |
| `docs/ACTIONS.md` | 187 | Asignaciones por acción del juego |

Hechos útiles para el coop:

- **Solo existe un jugador en la capa de entrada**: se abren todos los mandos y se lee "el último usado"
  (`docs/STATUS.md`, fila Controllers; `input_last_device`, `input.c:616`). No hay noción de jugador 2.
- **El overlay bloquea la entrada del juego** mientras está abierto (`capturing()`, `menu.mm:201`; `input_reset()` al
  abrir y cerrar, `:115`).
- **Estado por arranque**: el juego puede reiniciarse dentro del mismo proceso. Todo estado que dependa de una sesión
  debe limpiarse con `RT_ON_RESET(fn) { ... }` (`runtime/hle.h:28-40`; ejemplos en `input.c:997`, `motion.c:937`,
  `vibration.c:69`). Aplica a cualquier puntero a memoria del juego que guarde el módulo de red.
- **Entrada inyectable para pruebas**: `SAVAGEWIND_INPUT="900:A:8,..."` (`AGENTS.md`, sección Build and verify).

Código de red que ya existe y sirve de patrón:

| Archivo | Transporte | Notas |
|---|---|---|
| `runtime/dsu.c` | UDP cliente, no bloqueante | El más parecido a un cliente de juego; tiene test (`tools/test_dsu.sh`) |
| `runtime/control.c` | TCP servidor en `127.0.0.1`, hilo propio | Abstracción `ControlSocket` Winsock/BSD (`:24-39`), `WSAStartup` en `:702`. "Solo el hilo del socket toca sockets; los handlers corren en el límite de frame" (`:1`) |
| `runtime/discord_rpc.c` | Pipe local con tramas JSON | Ejemplo de función opcional, apagada por defecto |
| `runtime/lib_fetch.c`, `runtime/platform/windows_update.c` | HTTPS con WinHTTP (solo Windows) | Descargas con SHA-256 |
| `runtime/update_appimage.c` | `curl` del sistema (Linux) | — |

Matiz de diseño declarado en el código: `runtime/post_update.h:41` dice que la comprobación de actualizaciones se
delega a SW Update, "no network code in the game itself". El coop sería el primer código que conecta el juego a
Internet; hoy todo lo que abre el proceso del juego es local (`127.0.0.1`) o un servidor DSU configurado.

---

## 3. Build y release

### 3.1 `Makefile` (960 líneas)

| Tema | Líneas | Detalle |
|---|---|---|
| Plataforma | `5-13` | `windows` (MINGW/MSYS), `linux`, `macos` |
| Compiladores | `45-46` | `clang` / `clang++` siempre |
| Flags del código generado | `48`, `56` | `GENFLAGS := -O1 -fno-strict-aliasing ...`; `GENFLAGS_ZIG` para el build en la máquina del jugador |
| Fuentes generadas | `60-61` | `generated/*.c` y `generated/*/*.c` → `build/obj/` |
| **Fuentes del runtime** | `62-64` | `RT_SRC`: `runtime/*.c`, `runtime/hle/*.c`, `runtime/services/*.c`, `runtime/nvn/*.c`, `runtime/nvn/maxwell/*.c`, `runtime/platform/$(PLATFORM).c` |
| **Cabeceras** | `65-68` | `RT_HDRS`: comodines de `.h`. **Todo `.o` del runtime depende de todas**: tocar un `.h` recompila el runtime entero |
| UI | `69-70` | `UI_SRC := runtime/ui/*.mm` |
| ImGui | `71-72` | Objetos de `third_party/imgui` |
| Enlace Windows | `140-142` | `-lSDL3 -lavformat -lavcodec -lavutil -lswscale -lvulkan-1 -lwinmm -lole32 -lshell32 -lcomdlg32 -luuid -lgdi32 -ladvapi32 -lshaderc_shared -lpdh` **`-lws2_32`** `-lwinhttp -l:libz.a -lpthread` |
| Exportación de símbolos | `151-152` | El `.exe` exporta todo (`--export-all-symbols`) y `swgame.dll` enlaza contra `savagewind.dll.a` |
| Enlace Linux | `122-123` | `-lSDL3 ... -lvulkan $(SHADERC) -lz -lpthread -ldl -lm` |
| **`GAMELIB_ID`** | `160-164` | SHA-256 (16 hex) de `tools/gen.py lifter.py analyze.py nso.py passes.py patches.py simd_c.py regs.py flags.py layout.py game_sites.py`, `tools/gamelib_entry.c`, `runtime/hle/hle_symbols.txt`, `runtime/cpu.h simd.h simd_fp.h simd_softfma.h gamelib.h`, más `GENFLAGS` y `GENFLAGS_ZIG` |
| ID compatible | `169-176` | `GAMELIB_ID_COMPAT` para CPUs sin AVX2/FMA (`-march=x86-64-v2`) |
| IDs de shaders | `185-193` | `TRANSLATOR_ID`, `SPIRV_ID`: invalidan cachés de shaders |
| `SW_SOURCE` | `200` | Commit y árbol en la primera línea del log; "modified" si cambian `runtime`, `tools` o `Makefile` |
| Flags del runtime | `204-205` | `RTFLAGS := -O2 -g -fno-strict-aliasing -Wall -Wno-unused-function -Iruntime -Iruntime/nvn ...`; `CXXFLAGS := -O2 -g -std=c++17 ...` |
| PGO / LTO | `216-240` | `PGO=gen|use`, `LTO=thin`; perfil en `tools/pgo/runtime.profdata` |
| Objetivo por defecto | `271` | `all`: exe, exe de configuración y, si existe `generated/`, la librería del juego |
| Regla C | `300-302` | `$(B)/rt/%.o: runtime/%.c $(RT_HDRS)` |
| Regla UI | `335-338` | `$(B)/rt/ui/%.o: runtime/ui/%.mm` |
| Enlace del exe | `324-327` | — |
| `make gen` | `356-357` | `gen.py all ../extracted/exefs ../generated ...` con el Python de `tools/venv` (`:353`) |
| `make runtime-objs` | `370` | Compila runtime, backend y UI **sin enlazar ni código del juego**: chequeo rápido |
| `make dist` | `434-473` | Zip de Windows; ver 3.4 |
| `make gen-helper` | `491-501` | Generador congelado con PyInstaller |
| `make appimage(s)` | `506-510` | AppImage en contenedor podman |
| `make deps`, `app`, `dmg` | `404-414` | macOS |
| Tests | `514-960` | Ver sección 5 |

Variables que se pueden pasar: `B` (directorio de build), `DEPS`, `RELEASE=1`, `PGO`, `LTO`, `GENARCH`,
`RECOMP_LOCALS`, `GENARGS`, `ARCH`/`SYSROOT` (Linux), `RENDERER` (macOS).

### 3.2 Documentos y scripts de build

| Ruta | Contenido |
|---|---|
| `docs/BUILD-windows.md` | Setup, build, empaquetado, firma, diferencias con macOS, tests de shaders |
| `tools/setup_windows.ps1` | Instala MSYS2 con winget, paquetes UCRT64 (`:15-20`: make, clang, lld, sdl3, ffmpeg, vulkan-headers, vulkan-loader, shaderc, spirv-tools, pkgconf, gdb, vulkan-memory-allocator, vulkan-validation-layers, nasm), Python 3.12 de python.org, crea `tools/venv` con `lz4 capstone unicorn`, y fija `core.autocrlf false` / `core.eol lf` |
| `tools/build_deps_windows.sh` | `make deps-windows`: FFmpeg 9.0.2 **solo LGPL** en `build/deps-win` (con SHA-256 fijado); falla si detecta GPL (`:35`) |
| `docs/linux.md` | AppImage, datos en `~/.local/share/savagewind`, DSU en Steam Deck, build desde fuente (`:130-166`), AppImages (`:168`) |
| `tools/linux/distro_build.sh` | Compila la app en contenedores de 5 distros (lo usa `linux-distros.yml`) |
| `tools/linux/appimage.sh`, `package.sh`, `Containerfile` | AppImage; `package.sh:38-59` copia una lista **explícita** de archivos |
| `docs/macos.md`, `tools/build_deps_macos.sh`, `bundle_macos.sh`, `make_dmg.sh` | macOS (Vulkan sobre MoltenVK); no mantenido como objetivo, pero **sí compilado en CI** |
| `tools/fetch_zig.sh`, `tools/zig_slim.sh` | Compilador que la pantalla de inicio usa en la máquina del jugador |
| `tools/pgo/README.md` | Cómo regenerar el perfil PGO |

### 3.3 `third_party/` y licencias

| Carpeta | Qué | Licencia | Cómo se usa |
|---|---|---|---|
| `third_party/imgui` | Dear ImGui (con una modificación RTL: `GImTextReorderLine`, ver `i18n.h:22`) | MIT | Compilado en el exe |
| `third_party/FidelityFX` | AMD FidelityFX SDK 1.1.4 (dos arreglos de alineación) | MIT | Compilado en el exe |
| `third_party/smaa` | SMAA | MIT | Shader incrustado como cadena |
| `third_party/stb` | `stb_image.h`, `stb_image_write.h` | Dominio público / MIT | Header-only |
| `third_party/vma` | Vulkan Memory Allocator | MIT | Header-only |
| `third_party/xess` | Solo cabeceras de Intel XeSS | MIT (cabeceras); el DLL no se distribuye | Carga en tiempo de ejecución |
| `third_party/fonts` | Fira Sans, Noto Sans Thai, Noto Sans Hebrew | SIL OFL 1.1 | Se copian a la release con su `OFL.txt` |
| `third_party/luts` | 3 LUTs de color | CC0 1.0 | `colour-luts/` en la release |
| `third_party/totk-pc-recomp` | Solo el `LICENSE` | MIT | Atribución de pases del generador |

No vendorizados, enlazados o empaquetados (`docs/third-party.md`): SDL 3 (zlib), FFmpeg (LGPL 2.1+, compilado propio),
shaderc (Apache 2.0), libiconv (LGPL), runtime de GCC (GPL 3 con excepción), winpthreads, Zig 0.17.0 (MIT) y, dentro
de `swgen/`, PyInstaller, Python, Capstone (BSD), **Unicorn (GPL 2.0)** y python-lz4 (BSD). DLSS y XeSS se descargan
en la máquina del jugador, nunca se distribuyen.

### 3.4 Sistema de actualización

| Ruta | Papel |
|---|---|
| `runtime/update_core.c` / `.h` | Lógica pura: versiones (`uc_version_cmp`), elegir release de la lista de GitHub (`uc_pick_release`), manifiesto `sw-manifest.txt`, rutas seguras |
| `runtime/update_crypto.c` / `.h` | SHA-256 y verificación ECDSA P-256 sin librerías (para AppImage) |
| `runtime/update_appimage.c` / `.h` | Linux: descarga con `curl`, verifica, reemplaza el AppImage. `REPO` en `:26` |
| `runtime/post_update.c` / `.h` | Primer arranque tras actualizar: vaciar cachés de shaders, reconstruir código, rollback |
| `runtime/platform/windows_update.c` | `SW Update.exe`: programa aparte, estático (WinHTTP + bcrypt). `REPO` en `:46`, `FEED_URL` en `:47` |
| `runtime/platform/sw_update_key.h` | Clave pública ECDSA de las releases. **La privada no está en el repo ni en CI** |
| `tools/make_update_manifest.py` | Escribe `sw-manifest.txt` (hash y tamaño de cada archivo); lo llama `make dist` (`Makefile:470`) |
| `tools/sign_update.py` | Firma el zip con `openssl` → `<zip>.sig`. Una release sin `.sig` no se ofrece |

`make dist` (`Makefile:434-473`), en orden: exige `make deps-windows`, `make gen-helper` y `tools/fetch_zig.sh`;
compila en `build/win-release` con `RELEASE=1 PGO=use LTO=thin`; comprueba que el exe no lleve código del juego
(`:441`); copia exe, exe de configuración, `SW Update.exe`, zig recortado, `swgen/`, fuentes, licencias, LUTs,
`README.md`, `LICENSE`, `recomplauncher.json`; copia los DLL que reporta `ldd` desde `ucrt64` o `deps-win` (`:466`);
falla si aparece un códec GPL (`:468`); escribe el manifiesto; hace el zip.

Consecuencia para dependencias nuevas: una **librería del sistema** (`ws2_32`) no añade nada al paquete. Un **DLL
nuevo de MSYS2** se copiaría solo por `ldd`, pero habría que añadir su licencia a `docs/third-party.md` y revisar
que no sea GPL.

---

## 4. CI y gobernanza

### 4.1 Workflows (`.github/workflows/`)

| Workflow | Disparo | Qué comprueba | Secretos / permisos |
|---|---|---|---|
| `checks.yml` | PR a `test` o `main` | (a) **Sin archivos de Nintendo**: nombres de archivos añadidos contra un patrón (`:30-31`). (b) **Sin rutas personales** en líneas añadidas, excluyendo `third_party` (`:41-43`) | `contents: read` |
| `tests.yml` | push a `test`/`main`, PR, manual | Seis jobs en Ubuntu, cada uno filtrado por rutas en PR (`:38-66`): `i18n`, `layout`, `astc`, `simd`, `translate`, `feedback_alias` | `contents: read`, `pull-requests: read` |
| `windows.yml` | push, PR, manual | **Solo build** en `windows-latest` con MSYS2 UCRT64: `make -j$(nproc)` y comprueba `build/savagewind.exe` y `savagewind.dll.a` (`:41`). Sin código del juego, sin tests | `contents: read` |
| `macos.yml` | push, PR, manual | Build en `macos-15` (Vulkan/MoltenVK) y `test_simd.py --simd asm` | `contents: read` |
| `linux-distros.yml` | push a `test`/`main`, manual (**no en PR**) | Build en 5 contenedores: debian:13, ubuntu:26.04, fedora:44, archlinux, opensuse tumbleweed | `contents: read` |
| `quick_inventory_drop.yml`, `_replace.yml`, `_sort.yml` | PR con rutas concretas (**incluye `Makefile`, `tools/patches.py`, `tools/gen.py`, `tools/game_sites.py`**), push, manual | `make test-quick-*` en Ubuntu y tests Python de codegen | `contents: read` |
| `macos-dmg.yml` | release publicada, push a `test` que toque el empaquetado macOS, manual | `.dmg` y subida a la release | `contents: write` con `github.token` |
| `activity-feed.yml` | issues etiquetados, PR (`pull_request_target`), releases | Publica en un webhook de chat | Secretos `FEED_WEBHOOK_BUGS/ENHANCEMENTS/PRS/ANNOUNCEMENTS`. **Limitado a `github.repository == 'Revivedx/SavageWindRecomp'`** (`:21`) |
| `triage.yml` | issues abiertos/editados, comentarios | Pone `needs-info` a reportes sin log | `issues: write` |
| `stale.yml` | cron cada hora | Cierra issues con `needs-info` tras 24 h | `issues: write` |
| `crash-save.yml` | issue etiquetado `crash` | Pide el save | `issues: write` |
| `retarget.yml` | PR de fork abierto contra `main` | Cambia la base a `test` | `pull-requests: write` |
| `.github/dependabot.yml` | semanal | Un PR agrupado de actualizaciones de Actions, contra `test` | — |

Filtros de `tests.yml` (`:38-66`):

| Job | Se activa en PR si cambia |
|---|---|
| `i18n` | `runtime/ui/**`, `runtime/language.c`, `runtime/hle/bindings.c`, `tools/i18n_check.py`, el propio workflow |
| `layout` | `tools/*.py` |
| `astc` | `runtime/nvn/astc.c`, `tools/test_astc*` |
| `simd` | `runtime/simd*.h`, `runtime/cpu.h`, `tools/lifter.py`, `tools/simd_c.py`, `tools/test_simd*` |
| `translate` | `runtime/nvn/maxwell/**`, `tools/test_translate*` |
| `feedback_alias` | `runtime/nvn/vulkan/exec.c`, `tools/test_feedback_alias.py` |

En push a `test`/`main` y en ejecuciones manuales corren los seis.

### 4.2 El check "no game files" (`checks.yml:30-31`)

Patrón sobre **nombres de archivos añadidos, copiados o renombrados**, sin distinguir mayúsculas:

- Directorios en cualquier nivel: `extracted/`, `generated*/`, `input/`, `dump/`, `keys/`, `save/`.
- Archivos llamados exactamente: `main`, `main.npdm`, `rtld`, `sdk`, `subsdkN`.
- Extensiones: `.keys .xci .xcz .nsp .nsz .nca .nso .nro .bfttf .bfotf .sav .sarc .ssarc .pack .bfres .sbfres .bntx .sbactorpack .msbt .byml .sbyml .bfstm .bwav .bars .bfevfl`.

El segundo paso falla si una línea añadida contiene `/home/<usuario>/`, `/Users/<usuario>/` o la carpeta de usuarios de Windows en la unidad del sistema.

Trampas para el fork:

- Un paquete Go en `server/internal/input/`, `server/keys/`, `server/save/` o código en `server/generated/` **falla el
  check**. Además `.gitignore` ignora `input/`, `generated/`, `save/`, `keys/`, `dump/`, `build/` y `venv/` en
  cualquier nivel (son patrones sin `/` inicial en `.gitignore`): git **no los añadiría y no avisaría**.
- Un binario Go compilado como `server/main` (sin extensión) falla. `main.go` no.
- Documentos de specs con rutas absolutas de la carpeta de usuario de Windows fallan el paso de rutas personales si se
  meten en el repo. Este mapa usa rutas relativas por eso.

### 4.3 Plantillas y modelo de ramas

| Elemento | Contenido |
|---|---|
| `.github/pull_request_template.md` | Qué cambia, cómo se probó, checklist: basado en `test`, compilado y probado en juego, sin archivos del juego |
| `.github/ISSUE_TEMPLATE/bug_report.yml` | Formulario con log obligatorio; la versión "latest" está escrita a mano (`:35`) |
| `.github/ISSUE_TEMPLATE/feature_request.yml` | Avisa: las peticiones ya respondidas en el FAQ, "such as ... multiplayer", se cierran (`:8-9`) |
| `.github/ISSUE_TEMPLATE/config.yml` | Sin issues en blanco; enlaces al FAQ |
| `governance/audit/audit.jsonl` | Registro de auditoría (no analizado) |

Modelo de ramas (`CONTRIBUTING.md`, "Which branch to work on"; `AGENTS.md`, "Commits and PRs"):

- `test`: desarrollo. Los PR van contra `test`.
- `main`: solo se mueve en releases, mediante PR desde `test`.
- **Los PR no se fusionan con GitHub.** El mantenedor aplica los elegidos en una rama local, los prueba y publica en
  `test` **un commit limpio** con `Co-authored-by` por autor; luego comenta y cierra cada PR.
- Un PR = un cambio. Lo que cambie el comportamiento para todos por beneficio de una configuración va **detrás de una
  opción, apagada por defecto**.
- Antes de resolver conflictos: `git log origin/test --grep='keyword'`; no reaplicar un arreglo ya integrado.

Consecuencia para el fork: como upstream publica commits "aplastados", **los hashes del fork nunca coincidirán** con
los de upstream. Sincronizar es hacer rebase o merge de `upstream/test` sobre las ramas del fork, y los archivos
calientes de la sección 7 serán los puntos de conflicto.

### 4.4 Qué correría en un fork

- En un fork recién creado GitHub deja **Actions desactivado** hasta que el dueño lo habilita, y los workflows con
  `schedule` quedan apagados. **No verificado aquí**: es comportamiento general de GitHub, no del repo.
- Con Actions habilitado: `checks`, `tests`, `windows`, `macos` y los `quick_inventory_*` corren en PR internos del
  fork contra sus ramas `test` o `main`, y en push a esas ramas. Ninguno necesita secretos.
- `activity-feed.yml` no hace nada fuera del repo original.
- `triage.yml`, `crash-save.yml` y `stale.yml` actuarían sobre los issues del fork si se habilitan (y `stale` cierra
  issues). Conviene desactivarlos en el fork.
- `macos-dmg.yml` intentaría adjuntar un `.dmg` a cada release publicada en el fork.
- Coste: `macos-15` y `windows-latest` consumen minutos con multiplicador en repos privados; en un fork público son
  gratis.

### 4.5 ¿Go en el mismo repo choca con algo?

Verificado leyendo workflows, Makefile y scripts de empaquetado:

- **Build**: el Makefile solo mira `runtime/`, `tools/`, `third_party/`, `generated/`. Un `server/` es invisible.
- **Empaquetado**: `make dist` y `tools/linux/package.sh` copian listas explícitas; `server/` no entra en ninguna
  release. `swgen.spec` solo congela `tools/`.
- **CI**: ningún filtro de rutas incluye `server/`. No habría checks de Go hasta añadir un workflow propio
  (`go vet`, `go test`), que es un archivo nuevo y aislado.
- **`SW_SOURCE`**: el "modified" del log solo mira `runtime tools Makefile` (`Makefile:200`).
- **`.gitattributes`**: `* text=auto eol=lf` aplica también a Go (compatible con `gofmt`).
- **Dependabot**: solo está configurado para `github-actions`; no tocaría `go.mod`.
- **Únicos choques reales**: los patrones de `checks.yml` y `.gitignore` del punto 4.2.

---

## 5. Tests

Convenciones verificadas: no hay framework; cada test es un programa con `assert` que sale con 0. Muchos **incluyen
el `.c` de producción** (`#include "../runtime/foo.c"`) y usan `-ffunction-sections` + `--gc-sections` para descartar
lo no usado. Fixtures siempre sintéticos (regla dura: nada de Nintendo). Viven en `tests/` (los más nuevos) y en
`tools/test_*` (la mayoría).

Comando en Windows: todos se lanzan desde una **shell MSYS2 UCRT64** en la raíz del repo, con
`export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`.

**Aviso (no verificado)**: varias recetas llaman a `python3` literalmente (`Makefile:546,619,682,702,821,867,870,883,887`).
`tools/setup_windows.ps1` instala el Python de python.org y crea `tools/venv`, pero no instala un `python3` de MSYS2.
Si `python3` no existe en el PATH, esos pasos fallan; habría que instalar `pacman -S python` o lanzar el script a
mano con `tools/venv/Scripts/python.exe`. En esta máquina, en Git Bash, `python3` no existía y `py -3` sí.

### 5.1 Objetivos `make test-*`

| Objetivo | Makefile | Cubre | Windows | En CI |
|---|---|---|---|---|
| `make test-bind-capture` | `524` | Captura de asignación con el botón de confirmar mantenido (#175) | Sí | No |
| `make test-blood-moon` | `533` | Petición de luna de sangre sobre código sintético | Sí | No |
| `make test-actions` | `544` | Asignaciones por acción, menús, teclas compartidas, ini; más `tools/test_act_site.py` | Sí (usa `python3`) | No |
| `make test-pc-controls` | `570` | Política de cursor, foco, puntero en Options, persistencia de `pc_cursor` | Sí | No |
| `make test-output-scale` | `585` | Escalado de presentación | Sí | No |
| `make test-window-size` | `594` | Lista de tamaños de ventana | Sí | No |
| `make test-vibration` | `603` | HID → SDL rumble | Sí | No |
| `make test-prof` | `617` | Grabador del profiler; `test_prof_lifecycle` es **solo POSIX** (`:612`) | Parcial | No |
| `make test-input-focus` | `631` | Cambios de foco con mando virtual SDL | Sí | No |
| `make test-gyro` | `670` | Giroscopio virtual: teclas, ajustes, sensores, DSU, arrastre, activación; incluye `test-bind-capture` y `tools/test_gyro_site.py` | Sí (usa `python3`; enlaza `-lws2_32`) | No |
| `make test-swkbd` | `690` | Teclado software: codificación del nombre (#271) | Sí | No |
| `make test-save-convert` | `701` | Importador Wii U/Cemu contra la herramienta Python | Sí (usa `python3`) | No |
| `make test-save-profiles` | `712` | Perfiles de guardado en carpetas falsas | Sí | No |
| `make test-mods` | `721` | Listado, orden y conflictos de mods | Sí | No |
| `make test-game-file-size` | `730` | Comprobación de tamaños | Sí | No |
| `make test-file-size-cache` | `737` | Caché de tamaños sin bloquear | Sí | No |
| `make test-dlc-check` | `744` | Validador de DLC | Sí | No |
| `make test-control` | `755` | Capa de comandos del endpoint de control: tres ejecuciones (normal, `dev`, `companion`) | Sí (enlaza `-lws2_32`) | No |
| `make test-places` | `767` | Lugares con nombre de `goto` | Sí | No |
| `make test-console` | `777` | Líneas, historial y autocompletado de la consola | Sí | No |
| `make test-hour` | `786` | Comando de reloj | Sí | No |
| `make test-textures` / `bench-textures` | `801` / `805` | Formato BC5 | Sí (no macOS) | No |
| `make test-two-button-rows` | `819` | Hooks reales de Options con filas sintéticas; más `tools/test_two_button_sites.py` | Sí (usa `python3`) | No |
| `make test-bloom-scale` | `829` | Escalado de bloom | Sí | No |
| `make test-audio-mix` | `838` | 5.1, downmix y binaural | Sí | No |
| `make test-movie-stop` | `847` | Carreras de parada del decodificador de vídeo (necesita FFmpeg) | Sí | No |
| `make test-update-core` | `857` | Lógica del actualizador: versiones, elección de release, manifiesto | Sí | No |
| `make test-sw-update` | `866` | SW Update contra un servidor local, todos los fallos | **Solo Windows** (usa `python3`) | No |
| `make test-update-appimage` | `869` | AppImage actualizándose | Solo Linux | No |
| `make test-steam-shortcuts` | `872` | `shortcuts.vdf` de Steam | Sí | No |
| `make test-cli` | `882` | Cada comando `savagewind <cmd>`; **depende de `all`** (build completo) | Sí (usa `python3`) | No |
| `make test-launch-flags` | `885` | Opciones de lanzamiento; `gen_launch_flags.py --check` falla si la tabla está desfasada | Sí (usa `python3`) | No |
| `make test-ui-layout` | `948` | Layout de UI del juego | Sí | No |
| `make test-quick-inventory-sort` | `901` | Ordenación del inventario rápido | **Solo Linux** (`:892`) | `quick_inventory_sort.yml` |
| `make test-quick-inventory-font ROMFS=...` | `914` | Fuente real del juego; **necesita tu RomFS** | Solo Linux | No |
| `make test-quick-drop-native`, `test-quick-drop` | `934`, `938` | Soltar desde el selector rápido | Solo Linux (`:921`) | `quick_inventory_drop.yml` |
| `make test-quick-replace` | `956` | Reemplazo de equipo | Solo Linux (`:951`) | `quick_inventory_replace.yml` |

Binarios con regla pero sin objetivo `test-*`: `$(B)/test_game_place` (`:558`; necesita
`runtime/game_place_table.inc`, que se genera de tus archivos del juego), `$(B)/test_discord_rpc` (`:562`),
`build/test_romfs` (`:517`), `build/test_shaders` (`:514`, Metal).

### 5.2 Tests sin objetivo en el Makefile

| Archivo | Cubre | Cómo se lanza (según su cabecera o docs) |
|---|---|---|
| `tools/i18n_check.py` | Traducciones completas | `tools/venv/Scripts/python.exe tools/i18n_check.py` — CI: `tests.yml` job `i18n` |
| `tools/test_layout.py` | `layout.py`, `gen.py`, `sw_probe.py` con NSO sintéticos | `tools/venv/Scripts/python.exe tools/test_layout.py` — CI: job `layout` |
| `tools/test_simd.py` | Bajada FP/SIMD contra Unicorn | `tools/venv/Scripts/python.exe tools/test_simd.py --jobs 8` — CI: job `simd` y `macos.yml` |
| `tools/test_game_sites.py` | Sitios y versiones del juego | `tools/venv/Scripts/python.exe tools/test_game_sites.py` — CI: los tres `quick_inventory_*` |
| `tools/test_astc.sh` | Decodificador ASTC contra astcenc | `bash tools/test_astc.sh` — CI: job `astc` |
| `tools/test_translate.sh` | Fuzz del traductor de shaders | `sh tools/test_translate.sh 20000 8 glsl` — CI: job `translate` |
| `tools/test_feedback_alias.py` | Alias de framebuffer | `python tools/test_feedback_alias.py` — CI: job `feedback_alias` |
| `tools/test_dsu.sh` | Cliente DSU e IMU del Deck (cabecera: Linux o MSYS2 UCRT64) | `tools/test_dsu.sh` |
| `tests/test_cursor_math.c` | Matemática del puntero | `clang tests/test_cursor_math.c -o build/test_cursor_math && build/test_cursor_math` (`docs/PC_MOUSE_CONTROLS.md`) |
| `tests/test_gpu_settings.c`, `test_hud_scale.c`, `test_hud_button_layout.c`, `test_shrine_sensor.c` | Ajustes de GPU, escala del HUD, disposición de botones, sensor de santuarios | Sin receta en el Makefile; compilar a mano. **Comando exacto no verificado** |
| `tools/test_locals.py`, `test_lift.py` | Pases del generador contra Unicorn (`test_lift.py` necesita un NSO propio) | A mano |
| `tools/test_quick_*_codegen.py`, `test_ui_layout_codegen.py`, `test_vk_stage_reuse.py` | Parches con AArch64 sintético | CI en los `quick_inventory_*`; el resto a mano |
| `tools/test_swctl.py`, `test_swbench.py`, `test_swprof.py`, `test_botw_save_convert.py` | Herramientas Python con servidor simulado | `python -m unittest tools.test_swprof`; resto a mano |
| `tools/test_saves_tab.cpp` | Pestaña Saves sin ventana (ImGui real, GPU simulada) | Sin receta; **buen modelo para probar un lobby ImGui** |
| `tools/test_vk_*.c`, `test_vk_pipelines.sh` | Vulkan con GPU real | A mano; necesitan GPU |
| `tools/test_softfma.c`, `test_surf_takeoff.c`, `test_menu_arrows.c`, `test_perfmon_*.c` | Varios | Comando en la cabecera de cada archivo |

### 5.3 Prueba en juego automatizada

No hay test de integración en CI (necesitaría archivos del juego). Para una pasada sin atender en Windows
(`AGENTS.md`, "Build and verify"): apuntar `SAVAGEWIND_SAVE_DIR` y `SAVAGEWIND_SETTINGS` a copias, y fijar
`SAVAGEWIND_NO_LAUNCHER=1`, `SAVAGEWIND_HIDDEN=1`, `SAVAGEWIND_TEST_WARP=plateau`,
`SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"`. Se confirma que llegó al mundo mirando los contadores de dibujo en
las líneas `[vk] t=` del log. En Linux: `tools/linux/test_run.sh`.

---

## 6. Recetas

### 6.1 Cómo añadir una opción

Ejemplo real más simple: **"Mute in background"** (`mute_unfocused`), un booleano.

| Paso | Archivo:línea del ejemplo | Qué escribir |
|---|---|---|
| 1. Campo | `runtime/settings.h:113` | `bool mute_unfocused; // comentario` dentro de `Settings` |
| 2. Default | `runtime/settings.c:29` | `.mute_unfocused = false,` |
| 3. Carga | `runtime/settings.c:391` | `else if (!strcmp(key, "mute_unfocused")) g_settings.mute_unfocused = atoi(val) != 0;` |
| 4. Guardado | `runtime/settings.c:801` | `fprintf(f, "mute_unfocused = %d\n", g_settings.mute_unfocused);` |
| 5. CLI / control (opcional) | `runtime/control_settings.c:35` | `B(mute_unfocused)` en la tabla |
| 6. Overlay ImGui | `runtime/ui/menu.mm:1375-1376` | `opt_toggle("Mute in background", &g_settings.mute_unfocused, false, "descripción");` |
| 7. Options del juego (opcional) | `runtime/ui/game_options.mm:1115-1118` | `v.push_back(toggle(P_AUDIO, "título", "descripción", getter, setter, 0));` |
| 8. Uso | `runtime/services/audio.c:629` | Leer `g_settings.mute_unfocused` |
| 9. Traducción | `runtime/ui/i18n/es.inc:219` y los otros 12 | Ver 6.3 |

Notas:

- El overlay guarda al cerrarse (`menu.mm:122`); llama a `settings_save()` solo si necesitas persistir de inmediato.
- Opción solo en la pantalla de inicio: modelo "Companion apps" en `runtime/ui/launcher.mm:2378-2385`, que además
  arranca un socket al activarse.
- Texto (código de sala, IP): modelo `opt_text` en `runtime/ui/launcher.mm:2308` y `runtime/ui/menu.mm:1772`. Para
  el ini, modelo `save_dir` (`settings.h:240`, `settings.c:617`). Recuerda el límite de 127 caracteres.
- Entero con nombres: `opt_choice` (`menu.mm:1367-1370`) y macro `I(...)` en `control_settings.c`.
- Si el ajuste cambia semántica de archivos existentes, sube `SETTINGS_VERSION` y documenta la migración como en
  `settings.c:114-124`. Un campo nuevo con default no lo necesita.
- Si añades un test, hay precedentes que cargan y guardan un ini temporal: `tests/test_pc_settings.c`,
  `tools/test_gyro_settings.c`.

### 6.2 Cómo añadir una pestaña al menú

Ejemplo real: **Mods** (`runtime/ui/mods_tab.h`, `mods_tab.mm`).

1. Crea `runtime/ui/coop_tab.h` y `runtime/ui/coop_tab.mm`. El comodín `runtime/ui/*.mm` (`Makefile:69`) lo compila
   sin tocar el Makefile. Estructura mínima, como `mods_tab.mm:182-207`:
   `opt_begin("##coop", ImGui::GetFontSize() / 34); opt_section("..."); ... opt_end();`
2. En `runtime/ui/menu.mm`:
   - `#include "coop_tab.h"` junto a `:21-22`.
   - Añade el nombre al final de `names[]` (`:2230`). El siguiente id libre es **13**. No reutilices 5-7.
   - Añade el id a `order[]` (`:2233`) en la posición deseada.
   - Revisa el tamaño de `Tab tabs[12]` (`:2226`). Hoy se usan como mucho 9 (los filtros quitan Quit, Saves o
     Cheats/Teleport según el contexto) y el máximo teórico sin filtros es 11. Una pestaña más cabe, pero súbelo
     si añades más de una para no depender de los filtros.
   - Añade condiciones en el bucle de filtros (`:2234-2240`) si la pestaña es solo de juego o solo de inicio.
   - Añade `case 13: coop_tab_draw(in_game); break;` en el `switch` (`:2324-2346`).
3. Traduce el nombre de la pestaña y todos los textos (6.3). El título pasa por `tr(t.name)` (`:2271`).
4. Prueba rápida: `SAVAGEWIND_MENU_TAB=13` abre esa pestaña al iniciar.

Alternativas:

- **Página en Options del juego**: añade un valor a `enum Page` antes de `P_COUNT` (`game_options.mm:100`), un botón
  en la página principal como `:370-380` (`button(P_MAIN, "Co-op", "...", "Open", A_PAGE, P_COOP)`) y filas con
  `toggle` / `choice` / `button`. Limitaciones verificadas: solo existen los tipos `KIND_TOGGLE`, `KIND_CHOICE` y
  `KIND_BUTTON` (`:98`); **no hay campo de texto nativo**. Un código de sala o una IP no se pueden teclear ahí sin
  trabajo de ingeniería inversa adicional.
- **Página en la pantalla de inicio**: `enum Page { MAIN, SETTINGS, SETUP }` (`launcher.mm:1219`) y el bucle de
  `launcher_run` (`:1733`). Es el archivo más enredado; mejor reutilizar la pestaña del punto 2, que aparece tanto
  en Settings de la pantalla de inicio como en el overlay.

### 6.3 Cómo añadir texto traducible

1. Escribe el texto **en inglés** en el código. La cadena inglesa es la clave.
   - En filas `opt_*` basta el literal: se traduce dentro.
   - En ImGui directo: `ImGui::TextUnformatted(tr("Texto"))`.
   - Con formato: `snprintf(buf, sizeof buf, tr("Room %s"), code)`. La traducción debe conservar los mismos `%` en
     el mismo orden.
2. Lista lo que falta: `tools/venv/Scripts/python.exe tools/i18n_check.py --missing es` imprime líneas
   `T("...", "")  // archivo:línea`.
3. Pega cada línea en **los 13** archivos `runtime/ui/i18n/<lang>.inc` con su traducción. `T("X", "")` deja el
   inglés (válido para nombres propios y unidades; si parece frase, el check lo lista como "left in English" sin
   fallar).
4. Ejecuta `tools/venv/Scripts/python.exe tools/i18n_check.py`. Debe decir `0 missing` en todos y salir con 0.
5. Para que el check ignore un literal que no es texto de jugador: comentario `i18n-skip` en esa línea, o que la
   línea sea un `LOG(` / `fprintf(`.
6. Recompila: `i18n.o` depende de todos los `.inc` (`Makefile:335`).

Cuidados:

- El texto debe estar en un `runtime/ui/*.mm` (no `console.mm`), `runtime/language.c`, `runtime/hle/bindings.c` o
  `runtime/launch_flags.c`. En otro archivo, el check no lo ve y `tr()` devolvería el inglés sin avisar.
- Hebreo (`he`) es de derecha a izquierda y tailandés/CJK usan fuentes de reserva: revisa que un código de sala o una
  IP mezclados con texto se vean bien.
- Los 13 `.inc` son el **mayor punto de conflicto** entre agentes en paralelo (sección 7).
- Los jugadores pueden corregir traducciones sin recompilar con `lang/<idioma>.txt` (`docs/menu-language-packs.md`).

### 6.4 Cómo añadir un archivo `.c` al build

- **En `runtime/` directamente** (por ejemplo `runtime/coop_net.c` y `runtime/coop_net.h`): nada que tocar. Lo
  recogen `RT_SRC` (`Makefile:62`) y `RT_HDRS` (`:65`), y se compila con la regla de `:300-302`.
- **En un subdirectorio nuevo** (`runtime/net/`): añade `$(wildcard runtime/net/*.c)` a `RT_SRC` (`:62-64`) y
  `$(wildcard runtime/net/*.h)` a `RT_HDRS` (`:65-68`). El `mkdir -p` de la regla crea `build/rt/net/`.
- **Librería de sockets**: Windows ya enlaza `-lws2_32` (`:141`). Linux y macOS no necesitan nada. Llama a
  `WSAStartup` una vez, como `dsu.c:183` o `control.c:702`.
- **Otra librería**: añádela a `LDFLAGS` en **los tres bloques** de plataforma (`:82`, `:122`, `:140`), a
  `tools/setup_windows.ps1`, a `.github/workflows/windows.yml:35-39`, a `macos.yml:28`, a
  `tools/linux/distro_build.sh` (`deps_for`) y a `docs/third-party.md`. Es mucho más caro que usar sockets del
  sistema; un cliente WebSocket o TLS escrito a mano o con una librería header-only evita todo eso.
- **Reglas de compilación a respetar**:
  - `-Wall` activo y código en estilo C11/GNU con `clang`.
  - Debe compilar en Windows, Linux y macOS: usa `#ifdef _WIN32` como `control.c:2-11`.
  - En Windows se define `WIN32_LEAN_AND_MEAN` y `NOMINMAX` (`Makefile:134`): incluye `<winsock2.h>` explícitamente.
  - El exe exporta todos sus símbolos (`:151`): usa `static` para lo interno y un prefijo (`coop_`) para lo público.
  - Estado por arranque: `RT_ON_RESET` (`runtime/hle.h:40`).
- **Variables `SAVAGEWIND_*` nuevas**: si el código lee una con `getenv`, `runtime/launch_flags_table.c` queda
  desfasado y `make test-launch-flags` falla. Regenera con `python tools/gen_launch_flags.py` y, si procede, edita
  `tools/launch_flags_curated.txt`.
- **Test unitario**: copia el patrón de `test-control` (`Makefile:750-760`) o `test-update-core` (`:857-864`):
  regla del binario, objetivo `test-coop` y `.PHONY`. Cada bloque nuevo al final del Makefile minimiza conflictos.
- **No toques** los archivos de `GAMELIB_SRCS` salvo que haga falta un hook en el código del juego (sección 0,
  punto 3).

### 6.5 Cómo compilar y probar en Windows 11 desde cero

Requisitos:

- Windows 10/11 x64, GPU con driver Vulkan 1.3, CPU con SSE4.2 (AVX2 recomendado).
- `winget` y Git para Windows.
- **Para jugar**: tu propia copia de BOTW 1.9.0 extraída (`exefs/` y `romfs/`). El repo no la incluye ni explica
  cómo obtenerla. **Sin ella se puede compilar la app y correr todos los tests unitarios**, pero no entrar al juego.
- Disco, RAM y tiempo: el README pide a los jugadores ~20 GB libres y 8 GB de RAM (16 GB con gráfica integrada), y
  cifra el setup en ~15 min con 12 hilos. `AGENTS.md` dice `make -j8` y, si el sistema mata el proceso por memoria,
  `make -j4`. **No hay cifras publicadas para el build desde fuente y no se midieron aquí**; cuenta con varios GB
  más para MSYS2, `build/` y `generated/`.

Pasos:

1. Clona el fork y cambia a `test`:
   `git clone <url-del-fork> && cd SavageWindRecomp && git checkout test`
2. Setup único, en PowerShell desde la raíz (`docs/BUILD-windows.md:9`):
   `powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1`
   Instala MSYS2 en `C:\msys64`, los paquetes UCRT64, Python 3.12 y `tools\venv`, y pone el clon en LF.
3. Abre una shell **MSYS2 UCRT64** (o Git Bash) en la raíz y pon MSYS2 primero en el PATH:
   `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`
4. Comprobación rápida sin juego: `make runtime-objs` (compila runtime, backend y UI sin enlazar).
5. Build de la app sin juego: `make -j8` → `build/savagewind.exe` y `build/savagewind.dll.a`. Es exactamente lo
   que hace el job de CI `windows.yml`.
6. Con archivos del juego: cópialos a `extracted/` (`extracted/exefs`, `extracted/romfs`), luego `make gen` (una
   vez) y `make -j8` → además `build/swgame.dll`.
   Alternativa sin `make gen`: `make gen-helper` y `tools/fetch_zig.sh`, y que la pantalla de inicio construya el
   código del juego como para un jugador.
7. Ejecuta desde una shell con `C:\msys64\ucrt64\bin` en el PATH (DLL de SDL3, FFmpeg, Vulkan, shaderc):
   `build/savagewind.exe` o `build/savagewind.exe --config`.
8. Tests sin juego (elige los relevantes):
   - `tools/venv/Scripts/python.exe tools/i18n_check.py`
   - `tools/venv/Scripts/python.exe tools/test_game_sites.py`
   - `tools/venv/Scripts/python.exe tools/test_layout.py`
   - `make test-control test-update-core test-mods test-console test-pc-controls test-gyro test-launch-flags`
   - `tools/test_dsu.sh`
9. Solo documentación: `git diff --check` y no compilar (`AGENTS.md`).
10. Build obsoleto: compara el `.id` junto a `swgame.dll` con
    `make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id`. Si difiere, `make gen` y recompila;
    no edites el `.id`.

Problemas conocidos:

- Finales de línea CRLF rompen los scripts bash; `setup_windows.ps1:32-33` lo corrige en el clon. **Un worktree
  nuevo hereda la configuración del repo**, pero conviene comprobarlo.
- `python3` puede no existir en MSYS2 (ver aviso de la sección 5).
- No lances ventanas de test bajo el `timeout` de MSYS: bloquea la ventana (`docs/BUILD-windows.md:81`).
- Worktrees: `build/`, `generated/`, `extracted/` y `tools/venv/` están en `.gitignore`, así que **cada worktree
  nace sin ellos**. Hay que compilar por worktree (`make B=<dir>`), y para jugar hacen falta `extracted/` y
  `generated/` por worktree o enlazados. `GEN ?= generated` es redefinible (`Makefile:18`), pero **compartir un
  `generated/` entre worktrees solo vale si su `GAMELIB_ID` coincide**. No verificado en la práctica.

---

## 7. Puntos calientes y zonas aisladas

Frecuencia medida en los últimos 150 commits de `test` (`git log --name-only -n 150`).

### 7.1 Puntos calientes (riesgo alto de conflicto entre agentes y con upstream)

| Archivo | Cambios / 150 | Por qué choca | Mitigación |
|---|---|---|---|
| `runtime/ui/i18n/*.inc` (13) | 34 c/u (`he`: 27) | Cada texto nuevo añade una línea en los 13; todos añaden al final o por bloques | **Un solo agente "dueño de i18n"** integra al final; los demás entregan la lista de textos. O bloque propio al final de cada archivo, con comentario delimitador |
| `runtime/settings.c` | 33 | Tres cadenas largas (default, carga, guardado) | Agrupar todos los ajustes del coop en un bloque contiguo, definido de una vez por la spec |
| `runtime/settings.h` | 29 | Un struct único; además es cabecera global: recompila todo | Igual: un bloque, una vez |
| `runtime/ui/menu.mm` | 31 | 2641 líneas; `tabs_body` es el embudo de toda pestaña | Tocar solo `names`, `order`, `tabs[]`, el `switch` y un `#include`; todo lo demás en `coop_tab.mm` |
| `runtime/ui/game_options.mm` | 29 | `build_items()` es una lista de 1000 líneas | Evitarlo si el lobby vive en el overlay |
| `runtime/ui/launcher.mm` | 27 | 2675 líneas, estado local abundante, contiene la versión | Evitarlo; reutilizar pestañas de `menu.mm` |
| `Makefile` | 29 | Reglas de test al final, listas de librerías; dispara los 3 workflows `quick_inventory_*` | Usar comodines existentes; un único bloque de test del coop |
| `tools/patches.py`, `tools/game_sites.py` | 21, 20 | Cambian `GAMELIB_ID`; activan 4 workflows | Un solo agente; agrupar cambios |
| `runtime/hle/input.c` | 21 | Núcleo de entrada, con tests que lo incluyen entero | Añadir funciones pequeñas con nombre propio; no reordenar |
| `runtime/control_settings.c` | 17 | Tabla densa en pocas líneas | Añadir entradas en línea propia al final |
| `runtime/launch_flags_table.c` | 16 | **Generado**: cada variable `SAVAGEWIND_*` nueva lo reescribe | Regenerar al integrar, nunca fusionar a mano |
| `runtime/main.c`, `runtime/platform/windows.c`, `linux.c` | 14, 17, 14 | Arranque y bucle de ventana | Un punto de enganche único (`coop_init()`, `coop_frame()`) |
| `README.md`, `CHANGELOG.md`, `docs/STATUS.md`, `docs/FAQ.md`, `docs/SAVAGE-WIND.md` | 31, 16, —, —, 22 | Todos los PR los tocan | Documentar el coop en archivos nuevos (`docs/coop.md`) |
| `runtime/nvn/vulkan/exec.c`, `device.c` | 58, 34 | Los más cambiados del repo | Fuera del alcance del coop; no tocar |

Todas las cabeceras de `runtime/*.h` son dependencia de todos los objetos (`RT_HDRS`): cambiar una fuerza una
recompilación completa del runtime, lo que encarece cada iteración de cada agente.

### 7.2 Zonas bien aisladas (seguras para trabajo en paralelo)

| Zona | Por qué es segura |
|---|---|
| `runtime/coop_*.c` / `.h` nuevos | Entran por comodín; nadie más los toca |
| `runtime/ui/coop_tab.mm` / `.h` nuevos | Entran por comodín; el check de i18n los escanea solo |
| `server/` (Go) | Invisible para Makefile, empaquetado y CI actual. Cuidado con los nombres de 4.2 |
| `tests/test_coop_*.c`, `tools/test_coop_*` nuevos | Archivos nuevos; solo la regla del Makefile es compartida |
| `.github/workflows/coop.yml`, `server.yml` nuevos | Archivos nuevos |
| `docs/coop*.md`, specs | Archivos nuevos |
| `runtime/dsu.c`, `runtime/control.c`, `runtime/discord_rpc.c` | Solo lectura como referencia |
| `third_party/` | No tocar; excluido del check de rutas personales |

### 7.3 Reparto sugerido para agentes en paralelo

- Agente A: protocolo y cliente de red (`runtime/coop_net.*`, tests unitarios con servidor simulado).
- Agente B: servidor relay (`server/`, workflow Go).
- Agente C: UI de lobby (`runtime/ui/coop_tab.*`) contra una interfaz `coop_*.h` **congelada por la spec antes de
  empezar**, para no editar cabeceras compartidas a mitad de trabajo.
- Agente integrador: las pocas líneas en `settings.h`, `settings.c`, `control_settings.c`, `menu.mm`, `Makefile`,
  `main.c` y los 13 `.inc`, en un solo commit al final.

---

## 8. Licencias, postura del proyecto y obligaciones del fork

### 8.1 Licencia

- Código propio: **MIT** (`LICENSE`), con dos líneas de copyright: "Revivedx and the Savage Wind Recomp
  contributors" y "Peiikai (LAS Recomp)". Obligación: **conservar el aviso de copyright y el texto de la licencia**
  en toda copia o parte sustancial. Se puede añadir una línea de copyright propia; no quitar las existentes.
- La licencia **no cubre** el juego de Nintendo, ni el código C generado a partir de él, ni `third_party/`
  (`LICENSE:25-27`).
- `third_party/` conserva sus licencias: no borrar `LICENSE.txt` / `OFL.txt`.
- Si el fork **distribuye binarios**: replicar lo que hace `make dist` (licencias de fuentes, FidelityFX, README y
  LICENSE dentro del paquete), mantener FFmpeg como build LGPL en DLL separados (`docs/third-party.md`, "What the
  Windows release bundles") y actualizar `docs/third-party.md` con cualquier dependencia nueva. `swgen/` incluye
  Unicorn (GPL 2.0), con enlace a su fuente.
- Un binario con el juego compilado dentro (`swgame.dll`, `generated/`) **no se puede compartir**: contiene código
  de Nintendo (`README.md`, "Respect copyright"). `make dist` lo comprueba (`Makefile:441`).
- Servidor en Go: elegir su licencia es libre; MIT mantiene la coherencia. Sus dependencias Go tendrán las suyas.
- Esto es una lectura técnica de los archivos del repo, **no asesoría legal**.

### 8.2 Regla dura del proyecto

Nunca commitear ni adjuntar archivos, claves, firmware, assets, fuentes, saves, código generado ni salida de
descompilación; no copiar de proyectos de descompilación; fixtures sintéticos (`AGENTS.md`, "Code and data rules";
`CONTRIBUTING.md`, "The one hard rule"). Para el coop esto afecta a:

- Capturas de red, volcados de memoria o saves usados como fixture: no al repo.
- Nombres de símbolos o estructuras tomados de proyectos de descompilación: prohibido. El conocimiento del juego
  debe salir de la copia propia.
- Sincronizar estado entre dos jugadores **no debe transmitir assets ni código del juego** por el relay.

### 8.3 Postura sobre multijugador

- `docs/FAQ.md:593-596`: "**Multiplayer?** No. It's an open request (#617), but nothing like it is in the game."
- `CONTRIBUTING.md` ("Read the FAQ before opening an issue") y `.github/ISSUE_TEMPLATE/feature_request.yml:8-9`:
  las peticiones ya respondidas, multijugador incluido, **se cierran de inmediato**.
- No hay ningún otro rastro de multijugador en el código (búsqueda de `multiplayer`, `co-op`, `617` en todo el
  repo: solo esas tres menciones).
- Lectura: no está prohibido, pero **no está en el plan** y el mantenedor es una sola persona que prueba cada PR a
  mano. Un PR grande de coop tiene pocas probabilidades de entrar tal cual. Si se quiere proponer algo upstream,
  tendría que ser pequeño, de un solo cambio, detrás de una opción apagada por defecto. El contenido del issue #617
  **no se consultó** (sin acceso a GitHub en esta sesión).

### 8.4 Postura sobre mods

- Desde Alpha 7 hay carga de mods desde `mods/` y pestaña Mods (`docs/FAQ.md:532-549`, `README.md` sección Mods).
- "Mods are other people's work": no se revisan, prueban ni soportan; un bug causado por un mod se cierra; las
  releases nunca incluyen mods; los mods no deben compartirse con archivos del juego dentro.
- Los trucos se quedan en el overlay y la consola, no en los menús del juego (`docs/FAQ.md:630-635`).
- Relevancia: dos jugadores con mods distintos tendrán datos de juego distintos. El coop necesitará decidir si
  compara listas de mods al conectar.

### 8.5 Otras posturas que condicionan el diseño

- **Sin dinero**: sin donaciones ni versiones de pago (`docs/FAQ.md:619-623`). Un relay con coste de hosting es
  asunto del fork.
- **Sin canales de comunidad**: todo por GitHub (`CONTRIBUTING.md`, "Moderation").
- **Proyecto hecho casi entero con IA** y lo declara (`README.md`); contribuciones con IA son bienvenidas si
  funcionan y están probadas.
- **Solo la versión 1.9.0** del juego. Las direcciones del juego van por `g_site` (`runtime/game_sites.h`) con
  patrones en `tools/game_sites.py`; nunca direcciones fijas.

---

## 9. Riesgos y preguntas abiertas

| # | Riesgo o pregunta | Estado |
|---|---|---|
| 1 | **El actualizador apunta a upstream** (`runtime/platform/windows_update.c:46`, `runtime/update_appimage.c:26`, `runtime/platform/sw_update_key.h`). Un binario del fork con `sw-manifest.txt` ofrecería la release oficial. Opciones: cambiar `REPO` y la clave, o desactivar la comprobación. En un build desde fuente no hace nada (`post_update.h:2`) | Verificado en código; decisión pendiente |
| 2 | **`recomplauncher.json`** también nombra el repo original (`"repo": "Revivedx/SavageWindRecomp"`) | Verificado |
| 3 | **Divergencia con upstream**: `test` recibe commits aplastados que tocan a la vez `settings.*`, `menu.mm`, `game_options.mm` y los 13 `.inc`. Cada sincronización del fork chocará ahí | Verificado por historial; ritmo: 233 commits en la rama |
| 4 | **Sin tests en Windows en CI** y casi ningún `make test-*` en CI. El fork tendrá que montar su propio workflow si quiere red de seguridad | Verificado |
| 5 | **`python3` en MSYS2** para las recetas de test | No verificado |
| 6 | **Tiempo, disco y RAM del build desde fuente** | No medido |
| 7 | **Entrada de un solo jugador**: no hay jugador 2 en `runtime/hle/input.c`. Cómo se representa al compañero es trabajo del núcleo del juego, fuera de esta área | Verificado que no existe |
| 8 | **Lobby en Options del juego**: sin campo de texto nativo. Lobby realista: overlay ImGui o pantalla de inicio | Verificado (`game_options.mm:98`) |
| 9 | **Primer código que sale a Internet desde el juego**: sin TLS ni cliente HTTP multiplataforma en el runtime (WinHTTP solo Windows; `curl` externo en Linux). Un relay con WebSocket seguro exige traer TLS (dependencia nueva en 3 plataformas) o usar UDP/TCP propio con cifrado propio | Verificado que no hay TLS multiplataforma |
| 10 | **Hilos**: el overlay se dibuja en el hilo de render, los eventos llegan en el principal y los hooks de Options corren en el hilo del juego (`menu.h:16-28`, `game_options.mm:15`). El módulo de red necesita su hilo y colas, como `control.c` | Verificado el modelo; diseño pendiente |
| 11 | **Reinicio en caliente**: "Quit to Start Screen" reinicia el juego en el mismo proceso. ¿La conexión sobrevive o se cierra? Requiere `RT_ON_RESET` | Pregunta abierta |
| 12 | **Tasa de frames variable** (30-360 fps, generación de frames): la sincronización no puede asumir un tick fijo (`AGENTS.md`: distinguir updates del juego, frames presentados y generados) | Pregunta abierta |
| 13 | **Mods y versión de `GAMELIB_ID` distintos entre jugadores**: ¿se comprueban al conectar? | Pregunta abierta |
| 14 | **Privacidad**: `display_name` y la IP del jugador pasarían por el relay. El proyecto hoy no envía nada | Pregunta abierta |
| 15 | **Límite de 127 caracteres** por valor en `savagewind.ini` | Verificado (`settings.c:359-365`) |
| 16 | **Patrones de `checks.yml` y `.gitignore`** contra nombres de carpetas habituales en Go | Verificado |
| 17 | **Workflows de issues en el fork** (`triage`, `stale`, `crash-save`) actuarían sobre issues propios | Verificado en YAML; comportamiento de Actions en forks no verificado |
| 18 | **macOS**: no mantenido, pero el job `macos.yml` compila cada PR. Código de red que no compile en macOS rompe el CI | Verificado |
| 19 | **Comentario desfasado** "tab 11" para Mods (`mods_tab.h:1`); el id real es 12 | Verificado, menor |
| 20 | **`governance/audit/audit.jsonl`** y el contenido del issue #617 | No analizados |
