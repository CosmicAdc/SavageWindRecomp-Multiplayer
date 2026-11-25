# 04 — Gráficos (NVN HLE → traductor Maxwell → Vulkan)

Mapa del área gráfica de Savage Wind Recomp (rama `test`), verificado contra el código. Todas las referencias
`ruta:línea` son relativas a `SavageWindRecomp/` y corresponden al árbol leído el 2026-11-24 (el directorio de
trabajo no era un repositorio git, así que no hay hash de commit que citar: las líneas pueden moverse).

Convenciones de este documento:

- **Verificado** = leído en el código. **No verificado** / **inferencia** se marca de forma explícita.
- Veredictos de la sección 4: **EXISTE**, **PARCIAL**, **FALTA**.
- Backend Metal (`runtime/nvn/metal.m`, `renderer.m`) existe pero no se mantiene (AGENTS.md:8-9); este mapa cubre
  solo Vulkan.

---

## 1. Propósito y pipeline

El juego (código AArch64 recompilado a C) cree hablar con el driver NVN de la Switch. Savage Wind implementa NVN en el
host: cada `nvn*` es un stub (`NVN(name)`, `runtime/nvn/nvn.h:7`) que **graba** el comando en memoria del host; un
hilo propio **traduce** esas listas a decisiones Vulkan (pases, pipelines, descriptor sets, copias de constantes) y un
tercer hilo **emite** las llamadas `vkCmd*` reales y los `vkQueueSubmit2`. Los shaders del juego son binarios Maxwell
(GM20B) que se traducen a GLSL → SPIR-V en tiempo de ejecución, con cachés en disco. Al presentar, el host añade su
propio posprocesado (escalado, AA, HDR, frame generation) y dibuja encima el overlay ImGui.

```
 HILOS DEL JUEGO (guest, recompilado)                                   HILO PRINCIPAL (host)
 ───────────────────────────────────                                   ─────────────────────
 nvnBootstrapLoader / nvnDeviceGetProcAddress                           ui_run(): SDL events
   hle/nvn_boot.c:43,49  -> hle_lookup_impl(nombre)                       device.c:3702
        │                                                                 menu_handle_event -> cola q_events
        ▼                                                                 (ui/menu.mm:248, :94)
 nvnCommandBuffer*  (cmdbuf.c)                                          crea ventana, VkDevice, swapchain
   graba CmdRec{op,x[7],f[4],extra} en "shadow" del host                  (device.c create_device:611)
   (nvn.h:125-130, enum CMD_* nvn.h:132-150)
   + marcadores solo-host: nvn_ui_scope -> CMD_UI_SCOPE (cmdbuf.c:360)
        │ handle = dirección host del primer registro
        ▼
 nvnQueueSubmitCommands / nvnQueueFenceSync / nvnQueuePresentTexture
   objects.c:291 / :327 / :1226  -> queue_put(QOP_*)  (objects.c:268)
        │  cola FIFO de 4096 ops, como mucho 1 present por delante (QUEUE_AHEAD)
        ▼
 ┌─────────────────────────────────────────────────────────────────────────────────────────┐
 │ HILO "nvn-queue" = T, el traductor (objects.c:226)  — todo bajo gpu_lock (exec.c:179)   │
 │                                                                                         │
 │  QOP_EXEC    -> backend_execute (exec.c:12210) -> exec_list (exec.c:11891)              │
 │                   estado NVN `st` (exec/internal.h) ; begin_pass (exec.c:2107)          │
 │                   draw_ (exec.c:8655):                                                  │
 │                     programa  -> get_program / translate_program (exec.c:3305, :3193)   │
 │                        maxwell_translate (maxwell/translate.c:3040) -> GLSL             │
 │                        vk_glsl_to_spirv (vulkan/spirv.c:198) -> SPIR-V -> módulo        │
 │                        (hilos "translator", "shader-worker", "pso-worker")              │
 │                     texturas  -> vk_tex / vk_view (textures.c:1783)  [subidas, ASTC,    │
 │                                  glifos, hojas de fuente, des-swizzle Tegra]            │
 │                     constantes-> cbuf_info (exec.c:5282)  [copias + parches host]       │
 │                     sets      -> stage_set (exec.c:7508)                                │
 │                     rec_Cmd*  -> paquetes en un anillo (record.c)                       │
 │  QOP_FENCE   -> s->seq = renderer_fence_seq()                                           │
 │  QOP_PRESENT -> backend_present -> present_locked (device.c:3748, :3163)                │
 │                   frame generation (exec.c:11028) ; frame_begin (device.c:2689)         │
 │                   vk_present_draw (exec.c:11636): post_process + present.frag           │
 │                   rec_drain() ; menu_render (ui/menu.mm:2504)  <- ImGui graba directo   │
 │                   vk_frame_end (device.c:2848): submit + present + capturas             │
 └─────────────────────────────────────────────────────────────────────────────────────────┘
        │ paquetes por valor (anillo de 16 MB)                 ▲ mvec_resolve (push constants)
        ▼                                                      │
 HILO "vk-record" = R (record.c:184 run)              HILO "mvec" = W (mvec.c:378 w_run)
   vkCmd*, vkUpdateDescriptorSets, vkQueueSubmit2       copia entradas de vértice del frame,
   señala timeline semaphore = nº de submission         empareja draws con el frame anterior
        │
        ▼
 GPU ──> vkQueuePresentKHR  (recorder, o hilo "present" con frame generation / 2ª cola: device.c:2209)
```

Hilos auxiliares que no están en el camino crítico: `vsync` (hle/vi.c:14, señala el evento vsync cada 16,67 ms),
`pcache-load` y el guardado del pipeline cache (device.c:1414, :1575), ayudante de VRAM (vulkan/vram.c:94), `hc_maker`
(copia del heap, exec.c:6374), generación de contornos de texto (text_redraw.c:1128), hilo de la imagen de arranque
(device.c:3116).

---

## 2. Archivo por archivo

### 2.1 Capa NVN (`runtime/nvn/`)

| Archivo | Responsabilidad | Funciones / puntos clave |
|---|---|---|
| `nvn.h` (164 l.) | Tipos host de los objetos NVN y el formato de comando grabado. Los objetos guest guardan un puntero host en el offset 0 (`HOST`/`SET_HOST`, nvn.h:15-16). | `NvnTexture` :24, `NvnTextureView` :61 (≤16 bytes, assert :67), `NvnProgram` :89, `NvnSync` :101, `NvnCmdBuf` :109, `CmdRec` :125, `enum CMD_*` :132-150, API backend :155-160, `nvn_ui_scope` :164 |
| `renderer.h` | Interfaz común backend. Declara el candado recursivo del renderer. | `gpu_lock/gpu_unlock` :17-18, `renderer_flush` :21 |
| `objects.c` (1403 l.) | Device, queue, sync, pools, buffers, texturas, samplers, programas, ventana. **Contiene la cola de submissions y el hilo traductor.** | comentario del modelo :161-172, `qop_run` :209, `queue_thread` :226, `queue_put` :268, `nvn_queue_idle` :289, `nvnQueueSubmitCommands` :291, `nvnQueueFenceSync` :327, `nvnSyncWait` :351, `nvnQueuePresentTexture` :1226 |
| `cmdbuf.c` (767 l.) | Grabación de comandos. Separa la contabilidad que ve el juego (bytes estimados de NVN real) del almacenamiento host ("shadow"). | cabecera :1-30, `cmdbuf_pool_gone` :194, `host_rec` :327 (registro host sin coste guest), `rec_cost` :340, `nvn_ui_scope` :360, `nvnCommandBufferBind*` :419+ |
| `formats.c`, `tegra.c/.h` | Tabla de formatos NVN (`nvn_format_info` formats.c:56) y layout block-linear de Tegra (`tegra_level` tegra.c:65, `tegra_layer_size` :74). | — |
| `astc.c/.h` | Decodificador ASTC por CPU para GPUs sin soporte. | `astc_decode_block` astc.c:372 |
| `glyphs.c/.h` (1152 l.) | Sustituye los iconos de botón de Switch por los del dispositivo en uso **reescribiendo la textura al subirla**. | `glyphs_apply` (glyphs.h:9), `glyphs_apply_layout` glyphs.c:1144, `glyphs_draw_sheet` (glyphs.h:16), `glyphs_generation` glyphs.c:780 |
| `hw_illust.c/.h` | Redibuja las ilustraciones de la consola (página de Controles) para Deck/Xbox/PS5. | `hw_illust_which` :29, `hw_illust_draw` :1037, `hw_illust_layout` :987 |
| `text_upscale.c/.h`, `text_redraw.c/.h` | "Sharper text": reconoce las hojas de fuente del juego por contenido y las sube ampliadas / redibujadas desde contornos. | `text_sheet_add` text_upscale.c:45, `text_sheet_known` :87, `text_redraw_generation` text_redraw.c:1256 |
| `hud_scale.h` (227 l.) | Matemática pura (inline, testeable) del tamaño de HUD y HUD a los bordes: anclas 3x3, escalado de vistas, ámbitos. | `hud_anchor` :10, `hud_scale_view` :54, `HudScope` / `hud_scope_enter/leave` :206-214 |
| `output_scale.c/.h` (758 l.) | Resolución de salida, upscalers, resolución dinámica, forma de la ventana, **proyección de la cámara del mundo** y jitter temporal. | `OutScale` (output_scale.h:5-11), `output_scale` :381, `output_world_proj` :542, `rt_persp` :553, `rt_world_projection` :616, `rt_frustum` :625, `rt_persp_offset` :697 |
| `shader_archive.c/.h` | Escanea los archivos de shaders del RomFS del usuario para la precompilación. | `shader_code_hash` :21, `shader_archive_scan` :316 |
| `precompile.h` | Interfaz de la precompilación opcional de shaders. | `shader_precompile_run` |
| `maxwell/shader.h` (143 l.) | **Contrato GLSL** entre traductor y renderer (bindings, sets, extensiones). | contrato :4-31, `ShaderKey` :43, `ShaderInfo` :72, `GMEM_BUFFER_INDEX` :101 |
| `maxwell/translate.c` (3293 l.) | Traductor Maxwell → GLSL (Vulkan) / MSL. | `maxwell_translate` :3040, emisión de `Tint` :2324-2327 y su uso :2439-2441, `maxwell_wet_lighting` :2228, `maxwell_translator_id` :2497 |

### 2.2 Backend Vulkan (`runtime/nvn/vulkan/`)

| Archivo | Responsabilidad | Funciones / puntos clave |
|---|---|---|
| `vk.h` (243 l.) | Estado global `VkG g_vk` y **declaración del modelo de sincronización**. | modelo :6-8, `VkG` :33-78, `g_vk_queue_lock` :82, submissions :86-96, memoria transitoria :99-104, `TexBackend` :111-177, destrucción diferida :218-221 |
| `device.c` (3752 l.) | Instancia, dispositivo, swapchain, HDR de salida, presentación, capturas, imagen de arranque, bucle de UI, ganchos de ImGui. | `create_device` :611, `create_swapchain` :1912, `present_thread` :2209, `present_on_recorder` :2364, `frame_begin` :2689, `vk_frame_begin` :2769, `vk_frame_suspend/resume` :2773/:2776, `frame_menu_begin` :2794, `vk_frame_cmd` :2816, `vk_frame_end` :2848, `control_shot_now` :3040, `control_camera_view` :3048, `present_locked` :3163, `backend_reset` :3661, `ui_run` :3702, `backend_present` :3748 |
| `exec.c` (12233 l.) | Intérprete de listas de comandos: pases, draws, clears, copias, compute, pipelines, parches de constantes, posprocesado, frame generation, TAA/upscalers, depuración. | `gpu_lock` :179, `vk_wait_seq` :191, `vk_cmd` :255, `vk_full_barrier` :295, `vk_submit_extra` :631, `renderer_flush` :701, `renderer_wait_seq` :825, `vk_alloc_set` :888, `vk_transient` :970, `renderer_stats` :1258, `vk_exec_init` :1384, `begin_pass` :2107, `blur_edge_source` :2920, `stage_build` :2961, `translate_program` :3193, `hud_patch` :4984, `cbuf_info` :5282, `vk_exec_reset` :6952, `stage_set` :7508, `pick_after_draw` :8408, `one_shot_target` :8589, `draw_` :8655, `debug_draw` :9538, `scene_composite` :10511, `fg_draw` :10869, `fg_project` :10927, `vk_fg_generate` :11028, `mvec_taa` :11371, `vk_present_draw` :11636, `exec_list` :11891, `backend_execute` :12210 |
| `exec/internal.h` | Estado NVN compartido (`RtState`, `Range`, struct `st`) para los módulos que se van extrayendo de exec.c. | — |
| `record.c/.h` (922 l.) | Hilo grabador: anillo de paquetes, `rec_Cmd*`, drenaje. | modelo de estado compartido record.c:10-29, `run` :184, `rec_init` :219, `rec_wait_submitted` :286, `rec_drain` :299, `rec_QueueSubmit2` :421, `rec_custom` :438 |
| `textures.c` (2544 l.) | Imágenes, formatos, subidas, vistas, samplers, escalas de render target, **destrucción diferida**, presupuesto de VRAM. | `vk_collect_dead` :351, `defer` :786, `vk_defer_image/view` :793-794, `idle_expire` :935, `vram_trim` :1036, `backend_texture_destroyed` :1151, `vk_textures_reset` :1158, `upload_texture` :1239, `upload_levels` :1501, sustituciones de glifos/ilustraciones :1606-1616, `vk_tex` :1783 |
| `shaders.c/.h`, `spirv.c` | Cachés de traducción (`cache/xlate`) y SPIR-V (`cache/spirv`), reflexión, shaders internos. | `vk_translate_cached` shaders.c:535, `vk_key_normalize` :274, `vk_reflect` :607, `vk_builtin_shader` :1580, `vk_glsl_to_spirv` spirv.c:198 |
| `precompile.c` | Precompilación de todos los shaders del RomFS y semilla de pipelines (`vk_pipelines.seed`). | `shader_precompile_run` :570, `shader_precompile_export_seed` :506 |
| `vram.c/.h` | Bloques VMA adelantados en un hilo y presupuesto del heap device-local. | `vram_device_local` :221, `VRAM_TRIM_TARGET` 0.85 (vram.h) |
| `mvec.c/.h` (936 l.) | Vectores de movimiento por objeto y **recuperación de la cámara** desde el bloque de constantes de escena. | modelo de hilos mvec.c:20-31, `camera_plausible` :167, `camera_compute` :186, `mvec_enabled` :234, `mvec_reprojection` :885, `mvec_camera_now` :920, `mvec_camera` :932; `MvecCamera` mvec.h:82 |
| `dlss.c`, `fsr3.cpp`, `fsr_api.cpp`, `xess.cpp`, `framegen.cpp` | Integraciones de upscalers temporales y generación de frames (bibliotecas externas opcionales). | `vk_dlss_init` dlss.c:274, `xess_dispatch` xess.cpp:284 |
| `hdr.c`, `hdr_psycho.c` | Salida HDR10: sustituye la fuente del shader de tonemap del juego y registra la imagen float. | `vk_hdr_tonemap_source` hdr.c:306, `vk_hdr_record` :113 |
| `imgui_glue.cpp` (109 l.) | Backend Vulkan de Dear ImGui sobre nuestro dispositivo, texturas RGBA para ImGui. | `vk_imgui_backend_init` :15, `vk_imgui_fonts_changed` :41, `vk_imgui_texture_rgba` :61, `vk_imgui_texture_free` :98 |
| `bloom_scale.h` | Reconocimiento (por hash de shader + forma del target) de los pases de bloom. | probado por `tests/test_bloom_scale.c` |

### 2.3 HLE y UI relacionadas

| Archivo | Responsabilidad | Puntos clave |
|---|---|---|
| `runtime/hle/nvn_boot.c` (53 l.) | Enlaza cada nombre `nvn*` con su stub host. | `nvn_proc` :34, `nvnBootstrapLoader` :43, `nvnDeviceGetProcAddress` :49 |
| `runtime/hle/vi.c` (86 l.) | `nn::vi` mínimo y temporizador de vsync a 60 Hz. | `vsync_thread` :14, `vi_get_vsync_event` :66, `RT_ON_RESET(vi_reset)` :61 |
| `runtime/ui/menu.mm` (2641 l.) | Overlay ImGui en juego (fuera del área estricta, pero es el punto de entrada de (a)). | `menu_handle_event` :248, fuentes :509-537, `photo_overlay_draw` :1923, `perf_overlay_draw` :1985, `menu_draws` :2487, `menu_render_needed` :2498, `menu_render` :2504 |
| `runtime/ui_layout.c/.h` | Etiqueta con ámbitos host los layouts del HUD nativo del juego. | `ui_pane_group` :55, `ui_layout_begin` :101, `rt_ui_layout_draw` :143 |
| `runtime/hle/freecam.c` | Cámara libre; ve `eye/at/up` de las cámaras del juego. | `rt_lookat` :47 |

---

## 3. Contratos, invariantes y modelo de sincronización

### 3.1 Los tres hilos del renderer y quién posee qué

Fuente primaria: `runtime/nvn/vulkan/record.c:10-29` (tabla "Shared state and how it is protected"), `vk.h:6-8`,
`objects.c:161-172`, `mvec.c:20-31`.

| Recurso | Dueño | Regla |
|---|---|---|
| Estado del renderer (submission abierta, registro de texturas, cachés, struct `st`) | Quien tenga `gpu_lock` (mutex **recursivo**, exec.c:172-180) | `backend_execute` y `backend_present` lo toman enteros (exec.c:12214, device.c:3749). Varios hilos guest pueden someter/presentar; el candado los serializa. |
| Decisiones de traducción | T (`nvn-queue`) | T nunca llama a `vkCmd*` directamente: usa `rec_*` (record.h:1-10). |
| Command buffers, su pool, la cola `g_vk.queue` | R (`vk-record`) | T solo puede grabar en un command buffer o usar la cola **después de `rec_drain()`** (record.c:21-25, record.h:39). |
| Descriptor sets | T los asigna y resetea pools; R los escribe y enlaza | record.c:20-24 |
| Objetos Vulkan nombrados en paquetes | Creados por T antes del paquete; destruidos por T solo cuando la submission que podría usarlos terminó en la GPU | record.c:26-28 |
| Memoria del juego | El guest; T la **lee al traducir**, no al grabar | "The commands read guest memory when they run" (objects.c:168-169). Un paquete del anillo lleva todo por valor: R nunca lee memoria del juego ni estado de T (record.c:7-8). |
| Datos de motion vectors | W (`mvec`) | T solo lee/resetea estado de W tras `mvec_sync()` (mvec.c:28-30). |
| Presentación | R (presents simples), o hilo `present` con 2ª cola / frame generation | `g_vk_queue_lock` protege `g_vk.queue` cuando la comparten (vk.h:80-82, device.c:2231). |
| Ventana, VkDevice, swapchain, eventos SDL | Hilo principal | `vk_wait_ready` (device.c:3651) bloquea a los hilos guest hasta que existen. |

### 3.2 Invariantes

1. **Todas las imágenes viven en `VK_IMAGE_LAYOUT_GENERAL`** y una barrera de memoria completa separa pases, copias
   y dispatches (vk.h:6-7, `vk_full_barrier` exec.c:295). Excepciones locales: la imagen de swapchain
   (`COLOR_ATTACHMENT_OPTIMAL` → `PRESENT_SRC`, device.c:2749, :2859) y los bucles de realimentación.
2. **Submissions numeradas + timeline semaphore**: cada submission señala su número (`vk_submit_extra`
   exec.c:649-650); `vk_pending_seq()` = siguiente número (exec.c:224). Los fences de NVN son ese número
   (`renderer_fence_seq` exec.c:724, `NvnSync.seq` nvn.h:103).
3. **Destrucción diferida**: nada que una submission en vuelo pueda referenciar se destruye en el acto. Se usa
   `vk_defer_image` / `vk_defer_view` (textures.c:793-794), que anotan `vk_pending_seq()` (textures.c:789) y se
   liberan en `vk_collect_dead` cuando el timeline lo alcanza (textures.c:351-360; llamado tras cada submit,
   exec.c:682). AGENTS.md:56-65 lo exige explícitamente.
4. **Memoria transitoria** (`vk_transient`, `vk_transient_copy`, exec.c:970/:1024) vive hasta que termina la
   submission para la que se pidió (vk.h:98). No guardar un `VkSpan` entre submissions.
5. **Solapamiento CPU/GPU** (`SAVAGEWIND_GPU_OVERLAP`, exec.c:727-736): la espera de un fence se toma con un frame de
   retraso; como mucho dos frames de trabajo de GPU en cola. Las lecturas de render targets llegan un frame tarde,
   salvo las "one-off".
6. **Shaders asíncronos**: un draw cuyo programa o pipeline aún se construye **se omite** si su target se dibujó el
   frame anterior, y **espera** si es un target de una sola vez (exec.c:8577-8588, `one_shot_target` :8589). Un
   draw nuevo puede faltar durante algunos frames.
7. **Reinicio in situ**: el juego puede reiniciarse sin cerrar el proceso. `backend_reset` (device.c:3661) drena y
   llama a `vk_textures_reset` / `vk_exec_reset`. Todo estado por arranque debe registrarse con `RT_ON_RESET`
   (AGENTS.md:49-50; ejemplos: cmdbuf.c:224, vi.c:61, glyphs.c:630). **No retener punteros guest entre arranques.**
8. **Contabilidad de memoria de comandos**: el juego dimensiona su memoria para los comandos de NVN real; un registro
   host adicional debe usar `host_rec` (cmdbuf.c:327), que no consume memoria guest ni dispara el callback de
   falta de memoria (cmdbuf.c:367).
9. **Contrato GLSL fijo** (maxwell/shader.h:4-31): set 0 vértice, set 1 fragmento; cbuf k → binding k; gmem 17;
   rscale 18; **tint 19**; texturas 32+i. Cambiar el traductor invalida cachés: el id del traductor forma parte de
   las claves (`maxwell_translator_id` translate.c:2497, `vk_shader_version` shaders.c:24).
10. **ImGui graba directamente en el command buffer del frame**, saltándose el anillo: por eso `rec_drain()` va
    siempre antes (device.c:3299, ui/menu.mm:2525) y `menu_render_needed()` y `menu_render()` deben coincidir en
    qué se dibuja (ui/menu.mm:2484-2486: ya hubo un bug por olvidar un elemento ahí).

### 3.3 Lo que una función nueva NO debe hacer

- Llamar a `vkCmd*`, `vkUpdateDescriptorSets` o `vkQueueSubmit*` desde T sin pasar por `rec_*` o sin `rec_drain()`.
- Destruir imágenes/vistas con `vkDestroy*` directo mientras pueda haber submissions en vuelo (usar `vk_defer_*`; la
  alternativa aceptada en código raro y lento es `rec_drain()` + `vkDeviceWaitIdle`, imgui_glue.cpp:98-106).
- Tocar estado del renderer desde un hilo propio (p. ej. un hilo de red) sin `gpu_lock`; mejor aún, no tocarlo:
  publicar datos en una estructura propia con su mutex/atómicos y que T los lea.
- Bloquear T: es el hilo que limita los fps (record.c:3-6, mvec.c:23-25). Nada de E/S de red ni esperas ahí.
- Leer memoria del juego desde R, o asumir que la memoria guest que T leyó sigue igual después.
- Añadir un elemento al overlay sin añadirlo también a `menu_draws()`.
- Cambiar el comportamiento para todos los jugadores sin una opción desactivada por defecto (AGENTS.md:143-146), o
  mezclar pasos de tiempo de juego / frames presentados / frames generados (AGENTS.md:52-54).
- Añadir texto visible sin actualizar las tablas de traducción y pasar `python3 tools/i18n_check.py`
  (AGENTS.md:67-68, :100).

---

## 4. Respuestas para el modo cooperativo

### (a) Overlays dibujados por el host — **EXISTE**

**Mecanismo.** Un único contexto Dear ImGui dibuja en la imagen de la ventana, después de la imagen del juego, a
resolución nativa de la ventana:

- Punto de inserción en el frame: `present_locked` dibuja el juego con `vk_present_draw` (device.c:3288) y después,
  si `menu_render_needed()` (device.c:3297), hace `rec_drain()` y `menu_render(cb, w, h)` (device.c:3299-3301).
- `menu_render` (ui/menu.mm:2504) prepara el frame ImGui, escala fuentes y estilo a la altura de la ventana
  (`scale = th/1080 * tamaño de texto`, ui/menu.mm:2543-2546) y emite con
  `ImGui_ImplVulkan_RenderDrawData` dentro del dynamic rendering ya abierto.
- **Ya hay overlays que se dibujan con el menú cerrado**, que son el patrón exacto a copiar: overlay de rendimiento
  F10 (`perf_overlay_draw`, ui/menu.mm:1985), registro de acciones (`act_overlay_draw` :2373), tira de teclas de la
  cámara libre y aviso de foto con miniatura (`photo_overlay_draw` :1923, usa `GetForegroundDrawList`), pistas del
  inventario rápido (:2458), recuadro del puntero (:2569-2576).
- Texturas propias para ImGui: `vk_imgui_texture_rgba` (imgui_glue.cpp:61) crea una imagen RGBA8 y devuelve un
  `ImTextureID`; el pool tiene 512 descriptores (imgui_glue.cpp:24). Sirve para iconos de jugador o marcadores.
- Fuentes: FiraSans empaquetada, con respaldo a fuentes del sistema (ui/menu.mm:509-537); hay fuentes de iconos de
  botón que siguen al dispositivo (`glyphs_font_generation`, glyphs.h:30). No se usan fuentes de Nintendo.
- Entrada: el hilo principal encola eventos SDL (`menu_handle_event` ui/menu.mm:248 → `q_events` :94) y T los
  consume en `drain_events` durante `menu_render`. Un lobby con campos de texto puede reutilizar esto y el teclado en
  pantalla (`osk_draw`, ui/menu.mm:2619).
- HDR: con imagen float del juego el overlay va a la imagen de 8 bits y se compone después del PQ
  (`frame_menu_begin` device.c:2794). Con frame generation el overlay se dibuja en **ambos** frames del par
  (device.c:3261-3262).

**Lo que hay que saber para etiquetas de nombre, marcadores, HUD de conexión y lobby:**

| Necesidad | Estado | Nota |
|---|---|---|
| HUD de estado/conexión fijo en pantalla | EXISTE (copiar `perf_overlay_draw`) | Añadir la condición a `menu_draws()` (ui/menu.mm:2487). |
| Lobby / UI con ventanas, botones, texto | EXISTE (ImGui completo; ya hay pestañas, OSK, consola) | El menú principal vive en `tabs_body` (ui/menu.mm:2608). |
| Etiqueta de nombre sobre el compañero | PARCIAL | El dibujo existe; falta la proyección mundo→pantalla (ver (c)). |
| Marcadores en el **minimapa / mapa del juego** | FALTA como mecanismo | El mapa es un layout nativo del juego. El host solo sabe **reconocerlo y moverlo**: `ui_layout_begin` identifica AppMap (slot 0x2a, ui_layout.c:117-120) y `hud_patch` reescala paneles (exec.c:4984). No hay API para saber qué coordenadas de mundo cubre el minimapa. Dibujar un icono encima con ImGui exigiría leer el estado del mapa del lado del juego (fuera de esta área). |
| Que el overlay respete "Tamaño de HUD" / HUD a los bordes | PARCIAL | Esa lógica (hud_scale.h) opera sobre matrices del HUD nativo en T, no sobre ImGui. La matemática es inline y reutilizable (`hud_anchor` :10), pero ImGui usa su propio `scale`. |
| Dibujar **debajo** del HUD del juego (p. ej. etiqueta ocultada por los corazones) | FALTA | ImGui va siempre encima de todo. Existe el punto "justo después del composite de escena y antes del primer draw del HUD" (`scene_composite` exec.c:10511; lo usa frame generation para la copia sin HUD, `fg_copy_hudless` :10846), pero ahí no hay ningún camino de dibujo host de primitivas. |

**Capturas:** los overlays ImGui **no** salen en `SAVAGEWIND_SHOT_AT` ni en el `shot` del endpoint de control (guardan
la imagen del juego, device.c:3484-3497, :3040); sí salen en `SAVAGEWIND_WINDOW_SHOT(_AT)` (device.c:3304-3320). Las
pruebas visuales de overlays cooperativos deben usar las capturas de ventana.

**Glifos y texto nítido** (glyphs.c, text_upscale.c, text_redraw.c, hw_illust.c) **no son un sistema de overlay**:
son sustituciones del contenido de texturas del propio juego en el momento de subirlas (textures.c:1606-1616,
:1519-1527). Sirven de precedente para (b), no para dibujar cosas nuevas.

### (b) Alterar el sombreado de un draw concreto — **PARCIAL**

Hay cuatro mecanismos reales, todos ad hoc; **ninguno identifica hoy "los draws del modelo X"**.

1. **Tinte por draw: cableado de extremo a extremo pero inactivo.** El traductor emite en todo fragment shader cuyo
   target 0 es float y escribe RGB un bloque `uniform Tint { vec4 ltint[2]; }` en el binding 19
   (translate.c:2324-2327) y, al final del shader, mezcla `c0.rgb` hacia `ltint[0].rgb * pow(luma, ltint[1].w) +
   ltint[1].rgb * luma` con peso `ltint[0].w` (translate.c:2439-2441). El renderer lo enlaza en cada draw
   (`stage_set`, exec.c:7603) pero el valor es siempre cero: `float tint[8] = {0};  // ... no game-specific highlight
   sets one yet` (exec.c:9201), pasado a `stage_set_reuse` (exec.c:9349). Con `ltint[0].w == 0` el shader no cambia
   nada. **Es exactamente el gancho que hace falta**: basta decidir, por draw, un tinte distinto de cero.
   Reservas (verificado / no verificado):
   - Solo afecta al **target de color 0**. BOTW usa sombreado diferido y la documentación del proyecto dice que el
     albedo del G-buffer es el target **1** (docs/SAVAGE-WIND.md:1605-1606). Qué contiene el target 0 del pase de
     G-buffer **no lo he verificado**; teñirlo puede no dar un cambio de color limpio. Habría que comprobarlo con
     `SAVAGEWIND_DEBUG_PICK` / `SAVAGEWIND_DEBUG_FRAME`, o ampliar el traductor para teñir otro target (eso cambia el
     id del traductor e invalida las cachés de shaders de todos los usuarios).
   - El contenido idéntico de tinte se comparte entre draws (`copy_dedupe(DEDUPE_TINT…)`, exec.c:7603), así que el
     coste es despreciable.
2. **Sobrescritura de constantes por draw: existe como patrón, sin API general.** `cbuf_info` (exec.c:5282) es el
   embudo por el que pasa cada constant buffer de cada draw; ahí se encadenan parches que devuelven una copia
   modificada: `fxaa_patch` (:5333, por hash de shader), `sdw_patch` (:5335), `up_patch` / `up_patch_frag`
   (:5336-5337), `hud_patch` (:5338, por contenido de matriz + ámbito). Añadir "si este draw es del jugador 2,
   devuelve el bloque de material con otro color" sigue el mismo molde. Requiere conocer el layout del bloque de
   material del shader de Link (**no investigado**).
3. **Identificación y parcheo de shaders por hash: existe.** Cada draw calcula `g_cur_fs_hash` (512 bytes del
   binario del fragment shader, exec.c:8689-8715). Se usa para omitir draws (`SAVAGEWIND_SKIP_FSHASH`, :8694;
   profundidad de campo :8731; bloom :8718-8730) y para elegir parches. Sustitución del **código fuente** traducido:
   `blur_edge_source` reescribe el GLSL de un shader reconocido antes de compilarlo (exec.c:2920, aplicado en
   `stage_build` :2969-2970) y `vk_hdr_tonemap_source` sustituye el tonemap (hdr.c:306). El hash identifica el
   **shader**, no el objeto: Link y un segundo Link comparten shader.
4. **Sustitución de texturas al subirlas: existe para casos concretos.** `upload_levels` reconoce por
   tamaño/contenido y reescribe glifos, ilustraciones y hojas de fuente (textures.c:1501-1527, :1606-1616). No hay
   un sistema general de "packs de texturas". Las imágenes se comparten por dirección de memoria y forma
   (`key_addr`, vk.h:151-153): si los dos jugadores usan la misma textura en memoria, sustituirla cambia a los dos.

**Lo que falta de verdad: saber qué draws son del segundo jugador.** Opciones, de más a menos prometedora:

- *Ámbito host en la lista de comandos.* Precedente directo: `nvn_ui_scope` inserta un registro solo-host
  `CMD_UI_SCOPE` (cmdbuf.c:360) alrededor de la función nativa de dibujo de un layout (ui_layout.c:55-65), y
  `exec_list` mantiene una pila de ámbitos (exec.c:11910-11913, `HudScope` hud_scale.h:206). Un `CMD_TINT_SCOPE`
  análogo alrededor del dibujo del actor fijaría el tinte. **Riesgo no resuelto:** los modelos 3D de BOTW se dibujan
  con listas de comandos cacheadas por modelo (cmdbuf.c:12-17 describe el "calc resource") y, probablemente, colas de
  render ordenadas por material/pase; que exista una función guest "dibuja este actor" que envuelva sus draws en
  todos los pases (G-buffer, sombras, profundidad) **no está verificado** y depende del lado del juego.
- *Por los datos del draw.* `mvec.c` ya construye por draw una identidad (`pkey` shaders, `skey` streams de vértices,
  `iaddr`, mvec.h:26-36) y una huella con las constantes propias del objeto; el comentario confirma el layout de los
  shaders de escena: bloque de cámara en el slot 3 y **matriz de mundo 3x4 del objeto en el slot 4**
  (mvec.c:93-95). Comparar la traslación de esa matriz (o la dirección del buffer de huesos) con la posición conocida
  del actor del jugador 2 distinguiría sus draws. Los modelos con skinning (Link) probablemente no usan una única
  matriz de mundo: **no verificado**.
- *Recursos distintos.* Si el segundo jugador se instancia como un actor con modelo/texturas propios (otro archivo de
  recursos, vía el sistema de mods de RomFS, fuera de esta área), sus shaders/texturas/streams tendrían direcciones
  distintas y la identificación sería trivial, o ni siquiera haría falta teñir en el renderer.

### (c) Proyección mundo → pantalla — **PARCIAL**

Los datos existen en tres sitios; **no hay una función pública `world_to_screen`** ni una cámara publicada de forma
segura entre hilos para la UI.

1. **Bloque de contexto de escena (la fuente más completa).** Documentado en exec.c:10783-10787: los draws del mundo
   leen un constant buffer `c3` que contiene, como floats, la vista (0-11, 3x4 por filas), **vista-proyección
   (12-27)**, proyección (28-43, estilo OpenGL), la matriz propia de la cámara (44-55) y near/far (56, 57). La cámara
   del mundo es la de proyección en perspectiva con far > 10000. `fg_draw` ya la captura por frame
   (exec.c:10869-10890, `fg.cand[i].m`), junto con **la posición de Link** leída de `PlayerInfo + 0x88`
   (exec.c:10884-10887, `g_site.player_info`). Solo se rellena si hay frame generation, DLSS o XeSS (exec.c:10870).
2. **`mvec_camera()`** (mvec.c:932, struct `MvecCamera` mvec.h:82: `view[12]`, `proj[16]`, `vp[16]`, `inv_vp[16]`,
   `valid`), validada contra la proyección real de la cámara (`camera_plausible` mvec.c:167). Limitaciones
   verificadas: solo con `mvec_enabled()` (TAA, FSR 3/4, DLSS, XeSS o `SAVAGEWIND_MVEC=1`; mvec.c:234-244); da la
   cámara del frame **anterior** (mvec.c:933); pertenece a T. Ya se expone hacia fuera en un caso:
   `control_camera_view` (device.c:3048), que usa el comando `camera` del endpoint de control (docs/CONTROL.md:68).
3. **Del lado del juego, sin depender del renderer.** `rt_persp` (output_scale.c:553) intercepta la construcción de
   proyecciones del juego y publica la de la cámara del mundo: `rt_world_projection` → tan(fovy/2), aspecto, near,
   far (output_scale.c:616; atómicos) y `output_world_proj` (output_scale.c:542; con mutex y número de serie).
   `rt_lookat` (hle/freecam.c:47) ve `eye`/`at`/`up` de las cámaras de vista en cada actualización. Con eye/at/up +
   tan/aspecto se puede construir la vista-proyección **en el hilo del juego**, siempre disponible, sin jitter.
   (freecam.c no publica hoy esos valores para otros consumidores: habría que añadirlo.)

Ya existe el cálculo de proyección de un punto: `fg_project` (exec.c:10927, estática): punto de mundo × VP
row-major → uv + profundidad, con rechazo de puntos detrás de la cámara. Se usa para la caja de Link
(exec.c:10990-11006).

**Trampas al pasar de NDC a píxeles de ventana** (todas verificadas):

- La imagen del juego no ocupa necesariamente toda la ventana: `OutScale` da el rectángulo `vx, vy, vw, vh` y un
  recorte uv `crop[4]` (output_scale.h:9-10; aplicado en device.c:3244-3255, `g_shot_crop`).
- La vista se ensancha en ventanas no 16:9 (`g_view_shape`, `rt_persp` output_scale.c:587-590); la proyección que
  hay que usar es la ya ensanchada (la que publica `rt_persp`).
- Con AA/upscaler temporal la proyección lleva **jitter subpíxel** (`rt_persp_offset` output_scale.c:697): hay que
  restarlo (`mvec_camera_now` devuelve el jitter, mvec.c:920-928) o la etiqueta temblará.
- Sentido de Y y rango de profundidad dependen de `nvn_window_origin()` / `nvn_depth_mode()` (exec.c:10969-10978).
- Latencia: con el solapamiento CPU/GPU y la cola de un present por delante, la cámara que T tradujo va uno o dos
  frames por detrás del estado actual del juego. Para que la etiqueta no "nade", la cámara y la posición del
  compañero deben capturarse **en el mismo punto del mismo frame**; el patrón de `fg_draw` (capturar la posición de
  Link en el momento en que T ve el bloque de cámara) es el correcto.
- Oclusión: no hay consulta de profundidad disponible para la UI. La profundidad de escena existe en GPU; leerla en
  CPU costaría una lectura diferida un frame (invariante 5). Lo razonable es no ocluir, o atenuar por distancia.

### (d) Modelo de hilos y sincronización — **EXISTE (documentado en el propio código)**

Descrito en la sección 3. Resumen operativo para una función nueva:

- Dónde corre cada cosa: el juego graba (hilos guest) → `nvn-queue` traduce y presenta bajo `gpu_lock` → `vk-record`
  emite → hilo `present` opcional. El overlay ImGui corre en `nvn-queue`, dentro de `present_locked`.
- Grabación frente a ejecución: los comandos se graban sin tocar Vulkan; **la memoria del juego se lee al traducir**
  (objects.c:168-169). El juego puede ir un present por delante (`g_q_ahead`, objects.c:176, :261).
- Destrucción diferida: `vk_defer_image` / `vk_defer_view` + `vk_collect_dead` (textures.c:351, :786-794).
- Esperas: `vk_wait_seq` (exec.c:191), `renderer_flush` (exec.c:701), `rec_drain` (record.c:299). Todas son caras
  en T.
- Datos de red hacia el renderer: publicar una instantánea (posición/nombre/estado del compañero) con mutex o
  atómicos propios; leerla en `menu_render`. Es lo que hacen `output_world_proj` (output_scale.c:536-548) y los
  overlays existentes con sus fuentes de datos.

### (e) Presupuesto de rendimiento de dibujar un personaje más — **PARCIAL (hay cifras del proyecto, no una medición de este caso)**

Cifras verificadas en el código y la documentación:

- Mundo abierto: ~5000 draws por frame (exec.c:132-133); recorridos de prueba 3500-4900
  (docs/SAVAGE-WIND.md:1081); cascadas de sombra del sol ~650-750 draws (docs:1152).
- Coste de traducción en T: **2,4 µs por draw** tras las optimizaciones (docs:456), ~10 ms por frame en el mundo
  (objects.c:163).
- Motion vectors: ~1,2 ms por frame para ~2969 draws, por eso se movieron a su propio hilo (mvec.c:23-25,
  docs:1862).
- T es el hilo que limita los fps cuando la GPU no lo hace (record.c:3-6).

Estimación (**inferencia mía, no medida**): un personaje adicional como Link son del orden de decenas de draws por
pase, repetidos en profundidad, G-buffer y hasta tres cascadas de sombra; digamos 50-200 draws, 1-4 % más de draws,
≈0,1-0,5 ms de T por frame. En GPU es despreciable frente a la escena. El coste de **simular** al segundo actor
(física, animación, IA) está del lado del juego y fuera de esta área.

Costes que sí pueden notarse:

- **Primera aparición**: si el personaje trae shaders/pipelines no vistos, sus draws se omiten hasta que compilan
  (invariante 6) o provocan un tirón; la precompilación (`precompile.c`) y la semilla de pipelines solo cubren lo que
  ya está en el RomFS y en la lista de pipelines.
- **VRAM**: texturas nuevas cuentan contra el presupuesto; por encima del 85 % se desalojan imágenes
  (`VRAM_TRIM_TARGET`, `vram_trim` textures.c:1036). En tarjetas de 2 GB ya hay incidencias (device.c:3540-3552).
- **Overlay ImGui**: unos pocos textos y rectángulos son microsegundos, pero `menu_render_needed()` verdadero fuerza
  un `rec_drain()` por frame (device.c:3299), es decir, T espera a que R alcance. El overlay F10 ya paga ese coste;
  **no he medido cuánto es**. Con frame generation se paga dos veces por par.
- **Tinte**: gratis (el binding ya existe y se deduplica).
- Cualquier cambio de rendimiento exige números antes/después en el PR (AGENTS.md:143-145).

---

## 5. Pruebas y conmutadores de depuración

### 5.1 Pruebas del área

| Prueba | Qué cubre | Cómo se ejecuta |
|---|---|---|
| `tools/test_translate.c` + `.sh` | Fuzz del traductor Maxwell: programas sintetizados desde los patrones de opcode de translate.c; cada GLSL debe compilar (shaderc, Vulkan 1.3) y validar (SPIRV-Tools). Con `REF=<commit>` compara además el MSL byte a byte. | `sh tools/test_translate.sh 20000 <jobs> glsl` (CI: .github/workflows/tests.yml:129) |
| `tools/test_astc.c`, `test_astc_ref.cpp`, `test_astc.sh` | Diferencial de `astc.c` contra astcenc de ARM (clonado en `build/`). | `bash tools/test_astc.sh` (CI: tests.yml:104) |
| `tests/test_output_scale.c` | `output_scale.c` | `make test-output-scale` (Makefile:585) |
| `tests/test_textures_format.c` | Formatos de textures.c, con y sin `SAVAGEWIND_BC5_UNORM` | `make test-textures` (Makefile:801), `make bench-textures` |
| `tests/test_bloom_scale.c` | Reconocimiento de targets de bloom (`bloom_scale.h`) | `make test-bloom-scale` (Makefile:829) |
| `tests/test_ui_layout.c` | `ui_layout.c` + `cmdbuf.c` + `hud_scale.h` (ámbitos host) | `make test-ui-layout` (Makefile:948) |
| `tests/test_hud_scale.c`, `test_hud_button_layout.c`, `test_gpu_settings.c`, `test_window_size.c` | Matemática de HUD, layout de botones, ajustes de GPU, tamaño de ventana | `test-window-size` existe (Makefile:594). **No encontré un objetivo `make` con nombre para `test_hud_scale` ni `test_gpu_settings`** con mi búsqueda; puede que cuelguen de otro objetivo o solo de CI: no verificado. |
| `tools/test_vk_gpl_dynamic.c`, `test_vk_heap_range.c`, `test_vk_pipelines.c/.sh`, `test_vk_stage_reuse.py`, `test_feedback_alias.py`, `test_shaders.c` | Piezas del backend Vulkan (GPL, rango de heap, semilla de pipelines, reutilización de stages) | No leídas en detalle. |

No existe una prueba automática de imagen (golden frames). La verificación de render es **en juego**: CONTRIBUTING.md:102-104
pide probar en el juego, mirar el resultado e indicar GPU y driver.

### 5.2 Ejecución desatendida

AGENTS.md:111-115: apuntar `SAVAGEWIND_SAVE_DIR` y `SAVAGEWIND_SETTINGS` a copias, `SAVAGEWIND_NO_LAUNCHER=1`,
`SAVAGEWIND_HIDDEN=1`, `SAVAGEWIND_TEST_WARP=plateau`, `SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"`. Se confirma que
se llegó al mundo por el número de draws en las líneas `[vk] t=… frame … fps … (N draws, …)` (device.c:3536-3538,
formato en `renderer_stats` exec.c:1266-1267). En Linux: `tools/linux/test_run.sh`. Fin de la corrida:
`SAVAGEWIND_EXIT_AT=<frame>` (device.c:3359-3371).

Nota de Git Bash en Windows (docs/SAVAGE-WIND.md:1634-1636): usar `timeout 60 env VAR=1 ./build/savagewind.exe`, no
`VAR=1 timeout …`.

### 5.3 Conmutadores `SAVAGEWIND_*`

Hay **338 variables distintas** solo en `runtime/nvn/` y `hle/nvn_boot.c` (conteo con grep). Se documentan donde se
definen: `grep 'getenv("SAVAGEWIND_'` (CONTRIBUTING.md:103-104). Las más útiles para este trabajo:

| Variable | Efecto | Ref. |
|---|---|---|
| `SAVAGEWIND_SHOT_AT=<f>,<f>…` | Imagen **del juego** en esos frames → `build/frames/frame_N.png` (sin overlay) | device.c:3484 |
| `SAVAGEWIND_WINDOW_SHOT=<f>` / `_WINDOW_SHOT_AT=<f>,…` | La **ventana** tal como se presenta (barras y overlay incluidos) → `build/window.png` / `build/frames/window_N.png` | device.c:3304-3320 |
| `SAVAGEWIND_SCREENSHOT_EVERY=<n>`, `_SCREENSHOT_FROM`, `_EXIT_AFTER_SHOT` | Series de capturas | device.c:3420, :3515-3524 |
| F12 | Graba una de cada dos imágenes del juego en `build/frames/rec_N.png` | device.c:3500-3514 |
| `SAVAGEWIND_DEBUG_FRAME=<f>` (o F9 en modo desarrollador), `_DEBUG_FRAMES=<n>` | "Frame de depuración": vuelca cada pase a `build/pass_NNN.png` y loguea cada draw (`[draw]`: shaders, targets, estado) | device.c:3421-3429, exec.c:2084-2091, `debug_draw` exec.c:9538 |
| `SAVAGEWIND_DEBUG_PASS_FROM/TO`, `_DEBUG_MAIN_ONLY` | Limitan el volcado de pases | exec.c:2087-2091 |
| `SAVAGEWIND_DEBUG_PICK=<x>,<y>[;…]`, `_DEBUG_PICK_RT=<i>` | Lista qué draws cambiaron esos píxeles (`[pick]`), con hash de shader. **La herramienta para encontrar los draws de un modelo.** | exec.c:8400-8477 |
| `SAVAGEWIND_LOG_FSHASH`, `_SKIP_FSHASH=<hash>,…` | Loguea / omite draws por hash de fragment shader | exec.c:8689-8714 |
| `SAVAGEWIND_LOG_FS_CB=<hash>:<k>:<primero>:<n>` | Sigue floats de un constant buffer de un shader frame a frame | exec.c:8756-8777 |
| `SAVAGEWIND_DEBUG_TEXDUMP=<fs>` | Guarda las texturas que muestrea un shader | exec.c:9634 |
| `SAVAGEWIND_SAVE_MSL=1` | Guarda el GLSL traducido en `build/glsl/<dir>_<hash>_{vs,fs}.glsl` | exec.c:2881-2889 |
| `SAVAGEWIND_DBG_FS=<addr>:<pc>:<reg>` | Saca un registro del shader como color | exec.c:3200-3216 |
| `SAVAGEWIND_SHADER_DUMP` | Vuelca binarios de shaders que fallan al traducir | exec.c:3156 |
| `SAVAGEWIND_VK_PROF=1/2`, `_GPUPROF_AT=<f>` | Tiempos de CPU del renderer por categoría / tiempo de GPU por pase | exec.c:88-90, device.c:3431-3447 |
| `SAVAGEWIND_FRAME_LOG=1/2`, `_TIMELINE=<f>:<n>` | Tiempos de frame; quién espera a quién dentro del frame | device.c:3340-3345, objects.c:143-158, :193-208 |
| `SAVAGEWIND_VK_RECORDER=0`, `_ASYNC_EXEC=0`, `_GPU_OVERLAP=0`, `_MVEC_THREAD=0` | Apagan cada nivel de hilos (A/B para descartar carreras) | record.c:234, objects.c:184, exec.c:735, mvec.c:31 |
| `SAVAGEWIND_MVEC=1`, `_MVEC_VIEW=<px>`, `_MVEC_LOG=<n>` | Fuerza motion vectors (y con ellos `mvec_camera`), los visualiza | mvec.c:17-18, :234, mvec.h:100 |
| `SAVAGEWIND_LOG_PROJ`, `_LOG_CAMERAS=<f>` | Loguea proyecciones y cámaras del juego | output_scale.c:575-588, hle/freecam.c:37-41 |
| `SAVAGEWIND_MENU_AT=<f>` | Abre el menú overlay en ese frame (pruebas del overlay) | device.c:3294-3296 |
| `SAVAGEWIND_NO_HUD_FIX`, `_NO_SCALE`, `_SCALE=<x>`, `_WIDE_TEST=<forma>` | A/B de parches de HUD y escalado | exec.c:4986, textures.c:235, output_scale.c:389-391 |
| `SAVAGEWIND_NVN_LOG`, `_NVN_REAL`, `_LOG_OPS`, `_LOG_CMD_COUNTS` | Trazas de la capa NVN | nvn_boot.c:7-13, :36; exec.c:11906, :1260 |
| `SAVAGEWIND_CONTROL` | Endpoint de control: `shot`, `camera`, `goto`… (docs/CONTROL.md) | device.c:3411 |

`docs/SAVAGE-WIND.md` (en español) explica muchos de ellos con su historia en "Cambios respecto de LASRecomp"
(líneas 93-1646; depuración gráfica hacia 1590-1640) y tiene secciones propias de heap/memoria sparse de GPU (1647),
DLSS (1837), FSR 3 (1855), frame generation (1870) y colour grading (2007). `docs/motion-vectors-spike.md` explica el
diseño de mvec.c. No leí esas secciones enteras: solo las partes citadas.

---

## 6. Riesgos y preguntas abiertas

**Riesgos**

1. **Tamaño y acoplamiento de `exec.c`** (12 233 líneas, estado estático por todas partes). Cualquier gancho
   cooperativo ahí debe ser mínimo y condicionado; el proyecto está empezando a partirlo (`exec/internal.h`), así
   que un fork divergirá rápido si toca mucho. Preferir ganchos en `ui/menu.mm` y en el lado del juego.
2. **Constantes dependientes de la versión.** Hashes de shader, offsets de `PlayerInfo`, slots de pantallas del HUD:
   todo está fijado a 1.9.0 (`strcmp(g_game_version, "1.9.0")`, ui_layout.c:71, :105). AGENTS.md:42-44 exige
   declarar los sitios en `tools/game_sites.py` y usar `g_site`.
3. **Identificar los draws del jugador 2** es el problema difícil de (b), no el tinte. Sin una función guest que
   envuelva el dibujo del actor, el ámbito host no sirve; la alternativa por datos necesita ingeniería inversa del
   bloque de huesos/matriz de Link.
4. **Target 0 ≠ albedo** en el G-buffer (según docs:1605-1606): el tinte existente puede no dar el efecto deseado
   en materiales diferidos. Cambiar el traductor invalida cachés y precompilaciones de todos los usuarios.
5. **Latencia y jitter de la cámara** para etiquetas: usar una cámara de otro frame o con jitter produce etiquetas
   que tiemblan o van retrasadas, sobre todo con frame generation (el overlay se dibuja idéntico en el frame
   generado y en el real).
6. **Shaders asíncronos**: un modelo nuevo puede no aparecer los primeros frames.
7. **`rec_drain()` por frame** si el HUD cooperativo está siempre visible: coste sin medir en T.
8. **Reinicio in situ**: todo estado cooperativo del renderer/overlay necesita `RT_ON_RESET`; el contexto ImGui
   sobrevive al reinicio pero el juego no (ui/menu.mm:1901-1909).
9. **Reglas del proyecto**: nada de recursos de Nintendo (ni fuentes ni iconos del juego en el overlay);
   CONTRIBUTING.md:64 lista "multiplayer" entre las peticiones ya respondidas en la FAQ, así que esto vive en un
   fork y no se espera que entre aguas arriba.
10. **Metal**: `menu.h` y `ui_layout.c` tienen ramas `SW_METAL` que devuelven sin hacer nada; un fork puede ignorar
    macOS pero no debe romper su compilación sin querer.

**Preguntas abiertas** (no resueltas leyendo esta área)

- ¿Qué contiene el target de color 0 del pase de G-buffer de los personajes? (Comprobar con
  `SAVAGEWIND_DEBUG_FRAME` + `SAVAGEWIND_DEBUG_PICK_RT`.)
- ¿Cómo dibuja BOTW un actor con skinning: lista cacheada por modelo llamada con `CMD_CALL`, con las matrices de
  huesos en qué slot de constant buffer? ¿Hay una función guest por actor y por pase que se pueda envolver?
- ¿Se puede instanciar un segundo actor jugador con recursos propios? (Área de actores/juego.) Eso decide si (b) se
  resuelve en el renderer o con recursos.
- ¿Qué expone el juego sobre el minimapa (centro, escala, rotación) para colocar marcadores? El host hoy solo
  reconoce el layout.
- ¿Coste real de `rec_drain()` por frame con overlay permanente, con y sin frame generation, en GPU integrada?
- ¿Conviene publicar la cámara desde el hilo del juego (`rt_lookat` + `rt_persp`) o desde T (bloque `c3`)? La
  primera es independiente de los ajustes de vídeo; la segunda está garantizada coherente con lo dibujado. No he
  verificado que `rt_lookat` vea exactamente la cámara que acaba en el bloque `c3` del mismo frame.
- Objetivos `make` de `tests/test_hud_scale.c` y `tests/test_gpu_settings.c`: no localizados.
