# 02 — Runtime: el reemplazo del sistema operativo anfitrión (`runtime/`)

Mapa del área `runtime/` de Savage Wind Recomp (rama `test`), orientado al fork de co-op online para 2 jugadores.
Todas las rutas son relativas a `SavageWindRecomp/`. Las referencias `archivo:línea` se comprobaron leyendo el código;
lo que no se pudo comprobar leyendo (solo ejecutando) va marcado como **[sin verificar]**.

Reglas del proyecto que afectan a esta área (AGENTS.md, CONTRIBUTING.md):

- Estado por arranque: se limpia con `RT_ON_RESET` (`runtime/hle.h:40`); no retener punteros del invitado entre reinicios.
- Direcciones del juego: nunca literales nuevas; patrón por versión en `tools/game_sites.py`, aserción de la
  instrucción reemplazada en `tools/patches.py`, y en el runtime `g_site.<nombre>` (`runtime/game_sites.h`).
- Tiempo: distinguir *update del juego*, *frame presentado* y *frame generado*; probar a 30/60/90/120 FPS.
- Cualquier cambio de comportamiento para todos va detrás de una opción, apagada por defecto.
- No tocar los saves ni el `savagewind.ini` del jugador en pruebas (`SAVAGEWIND_SAVE_DIR`, `SAVAGEWIND_SETTINGS`).

---

## 1. Propósito y diagramas

`runtime/` sustituye a Horizon OS: carga los NSO en un espacio de direcciones fijo, emula el kernel (hilos, sincronización,
memoria, SVC), responde a los servicios IPC (`fs`, `am`, `acc`, `hid`, `time`, audio...) y sustituye funciones del SDK
por funciones host (HLE). El código del juego no va en el ejecutable: es una biblioteca aparte (`swgame.dll` / `.so`)
generada en la máquina del jugador y cargada al arrancar (`runtime/gamelib.h:1-3`).

### 1.1 Secuencia de arranque

```
constructor(101) reserve_guest_space            main.c:83    reserva 0x70_0000_0000..0x73_2000_0000 (re-exec si choca)
main()                                          main.c:358
 ├─ plat_init, log, launch_flags_boot           main.c:380-389   (variables SAVAGEWIND_* guardadas -> entorno)
 ├─ control_init                                main.c:393   socket de control si SAVAGEWIND_CONTROL / _COMPANION
 ├─ rt_paths_init  (cwd = raíz de datos)        main.c:430
 ├─ settings_load  (savagewind.ini)             main.c:432
 ├─ comandos CLI / utilidades (salen aquí)      main.c:439-487
 └─ for (;;)                                    main.c:514
     ├─ launcher_run (pantalla de inicio, opcional)          main.c:516
     ├─ save_backup_at_start                                 main.c:522
     ├─ sp_session_begin + sc_lock_take (carpeta de saves)   main.c:524-525
     ├─ boot(root)                                           main.c:170
     │    ├─ gamelib_load   (swgame.dll, una vez por proceso) main.c:180
     │    ├─ loader_load    (NSO -> memoria, relocs, HLE)     main.c:181
     │    ├─ settings_load, kernel_init, dispatch_init        main.c:183-187
     │    ├─ lee main.npdm (prioridad, pila, program id)      main.c:193-204
     │    ├─ hle_register(<rtld_init_sdk|main|fini>)          main.c:213-215
     │    └─ kernel_create_main_thread + kernel_run_main      main.c:216-217  -> pthread nuevo
     ├─ ui_run()   bucle de ventana en el hilo host principal main.c:534 (nvn/vulkan/device.c:3702)
     ├─ teardown() reinicio en caliente                       main.c:148
     └─ sc_lock_drop; vuelve al launcher o arranca otra vez   main.c:536-537
```

El hilo invitado principal ejecuta `nn::init::Start` (`main.c:208`), que llama a los stubs `rtld_init_sdk` (init del
módulo `sdk`) y `rtld_init_main` (init de subsdk9..0 y `main`, `main.c:68-74`) y después a `nnMain`.

### 1.2 Hilos

```
 HILO HOST PRINCIPAL            HILOS INVITADOS (1 pthread por KThread)        HILOS HOST AUXILIARES
 main() -> ui_run()             kernel.c:475 thread_start                      
 ┌──────────────────┐           ┌───────────────────────────────────────┐     ┌────────────────────────────────────┐
 │ SDL eventos       │           │ guest-<handle>: thread_main (k.c:395)  │     │ nvn-queue (nvn/objects.c:226)      │
 │ input_handle_event│           │  core_acquire -> guest_call(entry)     │     │   traduce listas, PRESENTA y corre │
 │ menú/overlay      │           │                                        │     │   los hooks por frame presentado   │
 │ motion_poll (DSU) │           │ principal del juego (prio de npdm, 44  │     │   (device.c:3343-3380)             │
 │ vigila reboot     │           │  por defecto; core 0): bucle de frames,│     │ vsync (hle/vi.c:14)                │
 │ (device.c:3708)   │           │  VFR -> rt_vfr_update (framepace.c:640)│     │ audio renderer / audout            │
 └──────────────────┘           │ render del juego, workers de jobs,     │     │   (services/audio.c:662, 1563)     │
                                 │ SaveMgrThread, audio del juego ...     │     │ flusher de saves (fs.c:1832)       │
                                 │                                        │     │ control socket (control.c:610)     │
                                 │ 4 "cores" = 4 testigos: solo quien     │     │ discord (discord_rpc.c:147)        │
                                 │ tiene el de su core ejecuta código     │     │ rupees (rupees.c), profiler,       │
                                 │ invitado (kernel.c:112-181)            │     │ watch/thread-dump (depuración)     │
                                 └───────────────────────────────────────┘     └────────────────────────────────────┘
```

Puntos clave:

- **El bucle principal del juego NO corre en el hilo host principal.** `kernel_run_main` crea un pthread y el hilo host
  principal "goes on to run the window event loop" (`kernel.c:498-501`).
- **Los hooks "una vez por frame presentado" corren en el hilo que presenta**, que por defecto es `nvn-queue`
  (`SAVAGEWIND_ASYNC_EXEC` activado por omisión, `nvn/objects.c:181-190`), no en el hilo del juego. Con
  `SAVAGEWIND_ASYNC_EXEC=0` corren en el hilo invitado que llama a `nvnQueuePresentTexture` (`nvn/objects.c:1246-1253`),
  y con `SAVAGEWIND_VK_PRESENT_THREAD=1` existe además un hilo de presentación (`nvn/vulkan/device.c:2209`).
- **El gancho en el hilo del juego es `rt_vfr_update`** (`hle/framepace.c:640`), llamado desde código del juego
  parcheado (`tools/patches.py:422-425`).

---

## 2. Archivo por archivo

### 2.1 Núcleo

| Archivo | Responsabilidad | Funciones / símbolos clave |
|---|---|---|
| `runtime/main.c` | Entrada, CLI, arranque y reinicio en caliente | `reserve_guest_space` :83 · `rt_request_reboot` :106 · `reboot_test_frame` (`SAVAGEWIND_REBOOT_AT`) :115 · tabla de resets `.rst$a/.rst$z` :127-136 · `teardown` :148 · `boot` :170 · `main` :358 · bucle juego/launcher :514-538 |
| `runtime/rt.h` | Declaraciones internas, log y mapa de memoria | `rt_log`/`LOG` :19-29 · `FATAL` :30 · `GUEST_LO/HI` :35-36 · `MAIN_BASE`, `HLE_BASE`, `TLS_BASE`, `HLE_HEAP`, `ALIAS_BASE`, `HEAP_BASE`, `HEAP_MAX` :39-50 · `Module` :53 · `HostFn`, `hle_register`, `hle_override` :72-77 · API kernel :83-90 · reboot :98-102 · `guest_call`, `guest_sym`, `hle_alloc` :108-110 |
| `runtime/cpu.h` | Estado de CPU invitada y acceso a memoria | `struct Cpu` (x[32], sp, pc, nzcv, tpidr, `thread`, v[32]) :7-21 · `GuestFn` :30 · `LD8..LD64` / `ST8..ST64` :64-71 · monitor exclusivo por CAS :74-97 |
| `runtime/hle.h` | Registro HLE y estado por arranque | `ENV(name)` (getenv cacheado por sitio) :6 · `HLE_FUNC(sym, fn)` :22-26 · `RT_ON_RESET(fn)` :40-44 · `MAKE_RESULT` :47 |
| `runtime/kernel.h` | Tipos del kernel | `STACK_BASE/SIZE` :8-9 · `KObject` :14 · `struct KThread` :22-46 · `g_klock`, `klock`, `kunlock` :62-64 · `handle_get`, `kernel_new_handle`, `kernel_signal` :67-69 · `kernel_create_event_pair` :72 · `kernel_thread_off_cores` :74 · `kernel_sleep_until_ns` :75 · `kernel_host_work_begin/end` :78-79 · `kernel_preempt` :87 · `kernel_heap_size` :88 |
| `runtime/kernel.c` | Mini-kernel Horizon | tabla de handles (4096) :38-78 · `rt_ticks` (19,2 MHz) :93 · "cores" :112-181 (`core_acquire` :164, `core_release` :183) · `kernel_thread_off_cores` :196 · `kernel_sleep_until_ns` :205 · `kernel_host_work_begin/end` :218-223 · `kernel_preempt` :243 · mapa de regiones :298-361 · `rt_unwind_point` :391 · `thread_main` :395 · `thread_new` :446 · `thread_start` (pila 256 MB) :475 · `kernel_create_main_thread` :490 · `kernel_run_main` :498 · mutex con herencia de prioridad :507-597 · `rt_svc` :869 · `svc_impl` (SetHeapSize :928, CreateThread :980, SendSyncRequest :1118...) :925 · `kernel_stop_threads` :1196 · `kernel_reset` :1236 · `kernel_init` (watch, thread dump) :1412 |
| `runtime/ksync.c/.h` | Mutex y condvars tipo futex (WaitOnAddress / futex; pthread como respaldo) | `KMutex`, `KCond` (`ksync.h:18-26`) · `kmutex_lock` `ksync.c:109` · `kcond_wait` :202 · `SAVAGEWIND_KSYNC`, `_KLOCK_SPIN`, `_KSYNC_SPIN` `ksync.c:63-70` |
| `runtime/loader.c` | Carga de NSO, relocs, símbolos | `load_nso` (comprueba build id contra la biblioteca) :49 · `loader_find_symbol` :130 · `loader_symbol_at` :141 · `resolve` (liga imports a HLE) :160-170 · `relocate` :193 · `loader_load` :230 |
| `runtime/dispatch.c` | Dirección invitada -> función; registro HLE; crash | `hle_lookup_impl` (`SAVAGEWIND_HLE_SKIP`) :41 · `hle_register` (stub en `HLE_BASE + 16*i`) :53 · `hle_override` :62 · `guest_sym` :80 · `hle_alloc` :99 · `dispatch_init` :111 · `dispatch_lookup` :121 · `dispatch_slow` :140 · `rt_dispatch` :166 · `guest_call` :174 · `rt_fatal_trace` :224 · watchpoint :279-401 |
| `runtime/gamelib.c/.h` | Carga de `swgame.*` y sus tablas | `GAMELIB_ABI 4`, `GameLibModule`, `GameLibSite`, `GameLibTables` (`gamelib.h:27-56`) · `gamelib_load` (una vez por proceso) `gamelib.c:185` · `gamelib_sync` (una vez por frame de juego) :228 · `SAVAGEWIND_GAME_LIB` :148 |
| `runtime/gamebuild.c` | Genera y compila el código del juego desde la pantalla de inicio (gen.py + compilador) | `find_tools` (`SAVAGEWIND_GEN`, `SAVAGEWIND_CC`) :128 · caché de objetos :342-456 · `gamebuild_run` :478 |
| `runtime/game_sites.h` | Lista de direcciones del juego por nombre | `GAME_SITES` :8-33 (`pause_state`, `event_mgr`, `player_info`, `world_mgr`, `screen_mgr`...) · `GAME_SITES_OPTIONAL` :36-98 · `g_site`, `g_game_version` :111-113 |
| `runtime/ipc.c/.h` | HIPC/CMIF para servicios HLE | `IpcReq`/`IpcResp` (`ipc.h:9-34`) · `IN`/`OUT`/`resp_size` :57-59 · `in_buf`/`out_buf` :62-71 · macro `SERVICE(nombre, fn)` :85-89 · `stub_handler` (servicio desconocido -> "ok" con log) `ipc.c:20` · `service_lookup` :35 · `ipc_handle_request` :117 · `g_domain_lock` :50 |
| `runtime/paths.c/.h` | Raíz de datos y recursos | `choose_root` (`SAVAGEWIND_DATA_ROOT`, modo dev, carpeta del .exe, XDG) `paths.c:83` · `rt_paths_init` :122 · `rt_data_root` :143 · `rt_resource` :145 |
| `runtime/settings.c/.h` | `savagewind.ini` | `struct Settings` (termina en `settings.h:244`) · `g_settings` `settings.c:24` · `settings_file` (`SAVAGEWIND_SETTINGS`) :19 · `settings_load` :348 · `settings_save_to` :771 |
| `runtime/framepace.h`, `runtime/hle/framepace.c` | Ritmo de frames y reloj del juego (ksys::VFR) | `framepace_target` :240 · `framepace_period_ns` :301 · `rt_frame_wait` :309 · `framepace_presented` :348 · `rt_vfr_update` :640 · `framepace_game_frame` :554 · `framepace_shown_frame` :638 · `framepace_script_frame` :752 · predicados de UI (`framepace_paused`, `_title_ui`, `_loading_ui`, `_in_event`) :92-238 |

### 2.2 Servicios (`runtime/services/`)

| Archivo | Responsabilidad | Clave |
|---|---|---|
| `fs.c` | `fsp-srv`: RomFS (imagen construida de `extracted/romfs` + overlay de mods + parches de recursos), DLC y **sistema de archivos host para saves** | `fs_reset` :84 · `fs_game_file` (lee un archivo del RomFS del juego) :1613 · `storage` (IStorage) :1659 · `fs_aoc_present` :1755 · `host_path` :1787 · `fs_retry` (reintentos 30 s en Windows) :1804 · `flush_later` + hilo `flusher` :1832-1861 · `file` (IFile) :1881 · `directory` :1945 · `filesystem` (IFileSystem) :2009 · `open_host_fs` :2094 · `SERVICE("fsp-srv")` :2108 |
| `archive.c/.h` | Yaz0 y SARC (lectura) | `yaz0_unpack` :7 · `sarc_find` :32 · `sarc_node` :58 |
| `am.c` | `appletOE`, `apm`, `lm` (foco, modo de operación, logger) | `am_reset` :18 · `SERVICE("appletOE")` :217 |
| `acc.c` | `acc:u0`: **un único usuario local** | `g_user_id` :7 · apodo "Player" :23 · `GetUserCount = 1` :46 |
| `audio.c` | audren / audout, renderizador en proceso | hilos `renderer_tick` :662, `audout_thread` :1563 · `kernel_thread_off_cores` :1296, :1374 · `hle_override` del procesador de comandos :1448 |
| `time.c` | `time:u/a/s` y reloj estándar | `SERVICE` :126-128 · `HLE_FUNC` GetCurrentTime :134 |
| `hid_set.c` | `hid`, `hid:sys`, `set`, `set:sys` (idioma) | :32, :83, :108, :128 |
| `nfp.c` | amiibo | `nfp_user` :76 · `SERVICE("nfp:user")` :234 |
| `misc.c` | `pctl*`, `aoc:u` | :33-48 |
| `save_profiles.c/.h` | Perfiles de guardado (carpetas completas) | `sp_validate_name` :102 · `sp_save_dir` :119 · `sp_active_save_dir` :214 · `sp_session_begin` :230 · `sp_session_save_dir` :237 · `sp_set_active` :389 · `sp_copy` :428 · `sp_delete` :512 |
| `save_convert.c/.h` | Import/export Wii U <-> Switch, copias y **cerrojo de carpeta** | API en `save_convert.h`: `sc_backup` :120 · `sc_lock_take` / `sc_lock_drop` :132-133 · `sc_list_slots` :144 |
| `test_warp.c` | (solo pruebas) reescribe `game_data.sav` al leerlo | formato del save :16-17 · `test_warp_active` :81 · `test_warp_patch` :86 · `test_warp_frame` :301 |

No existe ningún servicio de red del invitado (`bsd:u`, `nifm`, `sfdnsres`, `ldn`): búsqueda sin resultados en
`runtime/`. Un servicio desconocido cae en `stub_handler` (`ipc.c:20-25`).

### 2.3 Plataforma (`runtime/platform/`) y `platform.h`

| Archivo | Clave |
|---|---|
| `platform.h` | contrato: `plat_reserve_guest` :20 · `plat_commit` :23 · `plat_committed_range` :29 · `plat_reset_guest` :31 · `plat_protect` :42 · `plat_now_ns` :53 · `plat_sleep_until_ns` :54 · `plat_thread_elevate` :57 · `plat_spawn` :91 · `plat_file_flush` :97 |
| `windows.c` | reserva con `MEM_WRITE_WATCH` :151 · **commit perezoso** por excepción (`lazy_commit` :85, `vectored_handler` :111) · `plat_now_ns` :272 · `plat_thread_elevate` :369 · log a archivo sin consola :381 · `plat_init` :472 · `plat_cpu_placement` :499 · `pread`/`pwrite` :692-693 |
| `linux.c` | ventana entera mapeada (`plat_commit` vacío :49) · `plat_reset_guest` :44 · log + eco :150-211 · `crash_handler` :335 |
| `macos.c` | conservado, sin mantenimiento (AGENTS.md) |
| `windows_update.c`, `windows_config.c` | ejecutables aparte (`sw-update.exe`, `savagewind-config.exe`), no forman parte del runtime del juego |

### 2.4 Makefile (partes del runtime)

| Qué | Dónde |
|---|---|
| Fuentes del runtime (`runtime/*.c`, `hle/`, `services/`, `nvn/`, `platform/$(PLATFORM).c`) — **comodines: un `.c` nuevo en esas carpetas entra solo** | `Makefile:62-64` |
| Flags del runtime (`-O2 -g -fno-strict-aliasing -Wall`) | `Makefile:204` |
| Enlace Windows: pila 256 MB, `-lws2_32 -lwinhttp` ya enlazados | `Makefile:140-142` |
| El .exe exporta todos sus símbolos y genera `savagewind.dll.a`; `swgame.dll` se enlaza contra ella | `Makefile:151-152` |
| Linux: `-rdynamic` (la biblioteca resuelve `rt_*` contra el ejecutable) | `Makefile:122`, :129 |
| `GAMELIB_ID` = hash de `tools/*.py` del generador + flags | `Makefile:160-164` |
| `make gen` pasa `--hle runtime/hle/hle_symbols.txt` | `Makefile:356-357` |
| `make runtime-objs` (compilar sin enlazar) | `Makefile:370` |

---

## 3. Contratos e invariantes

### 3.1 Memoria del invitado

- **Dirección invitada == dirección host** (`rt.h:32`, `cpu.h:53`). No hay traducción ni base que sumar: un puntero del
  juego se desreferencia tal cual (`LD32(a)`, `*(float *)(uintptr_t)a`).
- Ventana fija `GUEST_LO = 0x7000000000` .. `GUEST_HI = 0x7320000000` (`rt.h:35-36`): módulos desde `MAIN_BASE`, stubs HLE
  en `HLE_BASE`, TLS en `TLS_BASE` (0x200 por hilo, `kernel.c:463-465`), `HLE_HEAP` (memoria visible al invitado que
  posee el host, `hle_alloc`, nunca se libera), pilas en `STACK_BASE` (`kernel.h:8`), heap del juego en `HEAP_BASE`
  hasta 3,25 GB (`rt.h:49-50`).
- **No hay un validador central de punteros.** Cada módulo tiene el suyo; copiar el patrón:
  - rango de la ventana: `sort_range` (`quick_inventory_sort.c:27`), `drop_range` (`quick_drop_action.c:79`), `in_guest`/`ptr` (`menu_wrap.c:18-19`);
  - heap vivo: `HEAP_BASE .. HEAP_BASE + kernel_heap_size()` (`hle/framepace.c:100-101`, :617-618; `items.c:388-395`);
  - lectura/escritura a prueba de fallos desde cualquier hilo: `control_mem_peek` / `control_mem_poke` (`control_mem.c:535`, :539);
  - recorrer solo memoria respaldada desde un hilo propio: `plat_committed_range` (`rupees.c:54`).
- Windows: tocar una página reservada pero no respaldada dentro de la ventana **la respalda** (`windows.c:85-104`); no
  falla, pero un hilo host que barre memoria infla el commit. Linux: toda la ventana está mapeada.
- El reinicio pone a cero toda la ventana (`plat_reset_guest`, `main.c:163`).

### 3.2 Hilos y planificación

- Un `pthread` por hilo invitado, con `Cpu` propio dentro del `KThread` (`kernel.h:22-25`); `t_cur` es thread-local
  (`kernel.c:35`). `kernel_current_cpu()` devuelve `NULL` en un hilo host (`kernel.c:504`).
- **Cores como testigos** (`kernel.c:112-119`): dos hilos del mismo core nunca ejecutan código invitado a la vez; gana la
  prioridad numérica más baja, luego orden de llegada. El juego depende de ello (heap de sead sin cerrojo entre hilos
  del mismo core). `SAVAGEWIND_CORES=0` lo desactiva; `SAVAGEWIND_ONE_CORE=1` serializa todo.
- Puntos de cesión del core: cada SVC (`rt_svc`, `kernel.c:869-879`), cada llamada indirecta (`rt_dispatch`,
  `dispatch.c:166-168`), esperas del kernel (`kwait`, `kernel.c:277-295`). El código generado no tiene otros puntos.
- **Código host largo en un hilo invitado debe soltar el core**: `kernel_host_work_begin/end` (`kernel.c:218-223`) o
  `kernel_sleep_until_ns` (`kernel.c:205`). Si no, bloquea a los demás hilos de ese core.
- Orden de cerrojos: `g_klock` y después el `m` de un core; `core_acquire` nunca corre con `g_klock` tomado
  (`kernel.c:281`).
- Herencia de prioridad en mutex del invitado (`kernel.c:527-573`); `SAVAGEWIND_NO_PRIO_INHERIT=1` la apaga.
- Pila: 256 MB por hilo (recursión del invitado sobre la pila host): `pthread_attr_setstacksize` fuera de Windows
  (`kernel.c:479`) y reserva por defecto del ejecutable en Windows (`Makefile:140`), que **también se aplica a cualquier
  hilo host nuevo** (reservada, no comprometida).
- Observación: `thread_main` eleva el hilo solo si `t->id == 1` (`kernel.c:398`), pero los ids empiezan en `0x50`
  (`kernel.c:364`, :1252); leído así, esa rama no se ejecuta nunca. **[sin verificar en ejecución]**

### 3.3 Invitado -> host

Tres caminos, todos corren **en el hilo invitado que llama**:

1. **Import ligado a HLE**: `resolve` (`loader.c:160-170`) liga un símbolo *no definido* de un módulo a `HLE_FUNC` si
   existe ("host implementation wins over the guest SDK's"); si nadie lo define, a un stub que registra
   `UNIMPLEMENTED` y devuelve 0 (`dispatch.c:72-78`). Solo sirve para símbolos importados entre módulos (lo normal:
   `main` -> `nn::*` de `sdk`).
2. **Llamada directa desde código parcheado**: `tools/patches.py` sustituye una instrucción por C que hace
   `extern ...; rt_algo(c, ...)`. La biblioteca resuelve `rt_algo` contra el ejecutable (`Makefile:151-152`, :122), así
   que la función host debe tener **enlace externo y firma C estable**. Un cambio en `tools/patches.py` cambia
   `GAMELIB_ID` y obliga a regenerar.
3. **`hle_symbols.txt`**: funciones internas de un módulo que no es `main` sustituidas por `hle_<nombre>`
   (`tools/gen.py:470-473`, :820-825); hoy solo `longjmp` (`runtime/hle/setjmp.c:39`).

`hle_override(addr, fn, name)` (`dispatch.c:62`) solo intercepta lo que pasa por `rt_dispatch`: llamadas indirectas y
entre módulos. Un `bl` directo dentro del mismo módulo se genera como llamada C directa a `f_<addr>`
(`tools/gen.py:458-460`) y **no ve el override**.

Convención de llamada HLE: argumentos en `c->x[0..7]` / `c->v[0..7]`, resultado en `c->x[0]`.

### 3.4 Host -> invitado

- `guest_call(Cpu *c, uint64_t addr)` (`dispatch.c:174-180`): pone `x30 = 0`, `pc = addr`, despacha y restaura `x30`.
  **No guarda nada más**: quien llama preserva el `Cpu`.
- Solo desde un hilo invitado (hace falta un `Cpu` con `thread`, `tpidrro` y pila válidos). No hay mecanismo para
  llamar al invitado desde un hilo host; el patrón existente es encolar y ejecutar en un gancho del hilo del juego (§4.4).
- Las funciones HLE no deben mantener un cerrojo host mientras llaman al invitado, hacen SVC o esperan: el reinicio
  desenrolla con `longjmp` y lo dejaría tomado (`kernel.c:367-372`).

### 3.5 Reinicio en caliente

`teardown` (`main.c:148-168`): parar el profiler -> `kernel_stop_threads` (cada hilo invitado hace `longjmp` en su
siguiente punto seguro: SVC, llamada HLE o espera) -> vaciar la cola NVN -> `backend_reset` -> **todos los
`RT_ON_RESET`** -> `kernel_reset` -> `plat_reset_guest` -> `g_nmodules = 0`.

- Los `RT_ON_RESET` corren con los hilos invitados ya parados y antes de borrar la memoria (`hle.h:28-31`).
- No se reinician: ajustes, ventana, dispositivo GPU, valores de `ENV()`, la biblioteca del juego (`gamelib_load`
  devuelve el primer resultado, `gamelib.h:66`), los **hilos host** (siguen vivos) y cualquier `static` sin hook.
  Ejemplo: los acumuladores `static` de `rt_vfr_update` (`hle/framepace.c:663-671`) no tienen hook.
- Los objetos del kernel se filtran a propósito (`kernel.c:1234-1235`).
- El contador de frames presentados sigue entre reinicios (`main.c:113`).

### 3.6 Tiempo y frames

| Concepto | Qué es | Dónde |
|---|---|---|
| Update del juego | una llamada a `ksys::VFR::updateInterval` -> `rt_vfr_update`; **dos por frame en pausa** | `hle/framepace.c:15-16`, :640, :678-679 |
| Frame presentado | un `nvnQueuePresentTexture`; `present_pace` espera su turno en una rejilla fija | `nvn/objects.c:1205-1224` |
| Frame generado | frame interpolado; el juego dibuja a la mitad de la tasa | `hle/framepace.c:297-305` |
| Paso del juego | ratio = segundos x 30 escrito en VFR; cambia como mucho 3 % por frame, tope 1/20 s, devuelve lo "debido" | `hle/framepace.c:685-701` |
| Intervalo (1/2/3) | conmuta ramas 60/30/20 fps del juego con histéresis | `hle/framepace.c:721-733` |
| Reloj de scripts | `SAVAGEWIND_SCRIPT_CLOCK=1`: 30 avos de segundo en vez de frames presentados | `hle/framepace.c:746-777` |

El paso del juego **no es fijo ni reproducible**: depende del tiempo real medido. Para pruebas existen
`SAVAGEWIND_DEBUG_FIXED_STEP=<fps>` (`hle/framepace.c:703-711`) y `SAVAGEWIND_MAX_INTERVAL` (`nvn/objects.c:1153-1166`).

### 3.7 Guardados

```
<raíz de datos>/                         (Windows: carpeta del .exe; Linux: ~/.local/share/savagewind; SAVAGEWIND_DATA_ROOT)
  savagewind.ini                         ajustes (o SAVAGEWIND_SETTINGS)
  save/                                  perfil "Default"
    save_0/{0..5, tracker, option.sav}   lo que monta el juego: carpeta save_<tipo>  (fs.c:2114-2118)
    sdcard/                              fsp-srv cmd 18                              (fs.c:2112)
  save.lock                              cerrojo mientras corre el juego             (save_convert.h:126-133)
  save-backups/{auto-<fecha>, save-<fecha>}                                          (save_backup.c:1-6)
  save-profiles/<nombre>/save            los demás perfiles                          (save_profiles.h:5-10)
  save_profile.txt                       nombre del perfil activo
```

- La carpeta se decide **una vez por arranque**: `sp_session_begin` (`main.c:524`) y `open_host_fs` usa
  `sp_session_save_dir()` (`fs.c:2096`). Prioridad: `SAVAGEWIND_SAVE_DIR` > perfil activo > `save_dir` del ini > `save`
  (`save_profiles.c:214-226`).
- El juego escribe sus archivos por IPC (`IFileSystem`/`IFile`); el runtime no interpreta el contenido salvo en
  `test_warp.c` (solo con variables de prueba) y en el conversor de saves.
- `Flush` va a un hilo aparte (`fs.c:1821-1861`); errores `EACCES` en Windows se reintentan 30 s (`fs.c:1798-1819`).
- Copia automática al arrancar, 5 conservadas, opción `save_backups` (`save_backup.c:55`, :21); **no** se hace con
  `SAVAGEWIND_SAVE_DIR`.
- Formato de `game_data.sav`: cabecera de 0xC bytes y entradas de 8 bytes (crc32 del nombre de la flag, valor de 32
  bits) (`services/test_warp.c:16-17`).

### 3.8 Ajustes, log y variables de entorno

- `g_settings` es una estructura global sin cerrojo; la escribe la UI y la leen todos los hilos.
- Log: `LOG(...)` = una sola escritura a `stderr` por línea, máximo 8192 bytes (`rt.h:14-29`); prefijo de subsistema
  entre corchetes por convención (`[fs]`, `[kernel]`, `[control]`). `FATAL` imprime trazas host e invitada y aborta
  (`dispatch.c:224-263`). En release de Windows el log va a `savagewind.log` (`platform/windows.c:381`).
- Variables `SAVAGEWIND_*`: `getenv` puntual o `ENV()` para rutas calientes (`hle.h:4-6`); se documentan en el
  comentario junto a su uso. `runtime/launch_flags_table.c` es **generado** por `tools/gen_launch_flags.py` y
  `make test-launch-flags` falla si queda desfasado (`launch_flags_table.c:1-2`): una variable nueva obliga a regenerarlo.

Variables más útiles para esta área:

| Variable | Efecto | Dónde |
|---|---|---|
| `SAVAGEWIND_DATA_ROOT`, `SAVAGEWIND_SAVE_DIR`, `SAVAGEWIND_SETTINGS` | raíz de datos / saves / ini de prueba | `paths.c:84`, `save_profiles.c:215`, `settings.c:20` |
| `SAVAGEWIND_NO_LAUNCHER=1`, `SAVAGEWIND_HIDDEN=1` | directo al juego, ventana oculta | `main.c:395`, :493 |
| `SAVAGEWIND_INPUT="frame:BOTON:frames,..."` | entrada guionizada; ignora teclado y mando reales | `hle/input.c:836-839`, :584-598, :1044 |
| `SAVAGEWIND_TEST_WARP / _TEST_TIME / _TEST_WEATHER / _TEST_FLAGS` | parchea el save al leerlo | `services/test_warp.c:5-14` |
| `SAVAGEWIND_EXIT_AT`, `SAVAGEWIND_REBOOT_AT`, `SAVAGEWIND_SCREENSHOT_EVERY` | fin, reinicio y capturas por frame | `nvn/vulkan/device.c:3353`, `main.c:112`, `device.c:3381` |
| `SAVAGEWIND_FPS`, `_FRAMEPACE=0`, `_DEBUG_FIXED_STEP`, `_SCRIPT_CLOCK`, `_LOG_FRAMEPACE` | ritmo y reloj | `hle/framepace.c:33-41`, :703, :746 |
| `SAVAGEWIND_CORES=0`, `_ONE_CORE`, `_NO_PREEMPT`, `_NO_PRIO_INHERIT` | planificador | `kernel.c:119`, :166, :136, :531 |
| `SAVAGEWIND_ASYNC_EXEC=0` | listas y presentación en el hilo que llama | `nvn/objects.c:171` |
| `SAVAGEWIND_CONTROL=<puerto>`, `_COMPANION=<puerto>`, `_DEV=1` | socket de control / solo lectura / comandos de desarrollador | `control.c:751-758` |
| `SAVAGEWIND_TRACE_SVC`, `_PROF_SVC`, `_THREAD_DUMP_AT`, `_WATCH`, `_HANG_WATCH`, `_LOAD_WATCH` | depuración de kernel e hilos | `kernel.c:915`, :882, :1261, :1414; `device.c:3733`; `framepace.c:323` |
| `SAVAGEWIND_HLE_SKIP=<texto>` | desactiva HLE cuyos nombres lo contienen | `dispatch.c:42` |

Ejecución desatendida: Linux `tools/linux/test_run.sh <carpeta de save> [segundos]`; Windows, las variables de
AGENTS.md (`SAVAGEWIND_SAVE_DIR` y `SAVAGEWIND_SETTINGS` a copias, `SAVAGEWIND_NO_LAUNCHER=1`, `SAVAGEWIND_HIDDEN=1`,
`SAVAGEWIND_TEST_WARP=plateau`, `SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"`), comprobando los recuentos de dibujo en
las líneas `[vk] t=` del log.

---

## 4. Recetas

### 4.1 Cómo registrar una función HLE

**(a) Sustituir un import del SDK** — basta un `HLE_FUNC` en cualquier `.c` de `runtime/` (el Makefile lo recoge):

```c
#include "../hle.h"
HLE_FUNC("_ZN2nn2vi12SetLayerCropEPNS0_5LayerEiiii", vi_set_layer_crop) { c->x[0] = 0; }   // runtime/hle/vi.c:52
```

Más ejemplos: `hle/input.c:1227` (lee argumentos de `c->x[]`, escribe en memoria invitada), `services/time.c:134`.
El nombre es el símbolo mangled tal como lo importa el módulo. No requiere regenerar el código del juego (se liga al
relocalizar, `loader.c:164-165`). No sirve para funciones internas de `main`.

**(b) Engancharse dentro de `main`** — cuatro piezas (ejemplo completo: "Menus wrap around"):

1. patrón del sitio en `tools/game_sites.py:253` y dirección 1.9.0 en :598;
2. parche que afirma la instrucción y emite C con `extern` (`tools/patches.py:1135-1141`), dado de alta en `PATCHES` (:1297);
3. función host con enlace externo: `rt_menu_wrap` (`runtime/menu_wrap.c:41`);
4. si el runtime necesita la dirección: entrada en `GAME_SITES` / `GAME_SITES_OPTIONAL` (`game_sites.h:37`).

Después: `make gen` y `make` (cambia `GAMELIB_ID`).

**(c) Servicio IPC**: `SERVICE("nombre", fn) { switch (req->cmd) ... }` (`services/acc.c:43`, `services/fs.c:2108`).

**(d) Stub con dirección invitada creado en ejecución**: `hle_register("<nombre>", fn)` devuelve la dirección falsa
(`main.c:213-215`, `services/audio.c:1394`).

**(e) Envolver una función alcanzable por despacho**: `dispatch_lookup(a)` para guardar la original y
`hle_override(a, envoltura, "nombre")` (`hle/setjmp.c:85-87`).

Estado propio del HLE que apunte al invitado: añadir `RT_ON_RESET(mi_reset) { ... }` (`services/time.c:106`,
`quick_inventory_sort.c:51`).

### 4.2 Cómo llamar a una función del juego desde el host

Siempre en un hilo invitado y preservando el `Cpu`. Tres variantes en uso:

```c
// 1) Copia del Cpu (lo más limpio): runtime/quick_inventory_sort.c:30-35
static uint64_t sort_call(Cpu *source, uint64_t entry, uint64_t a, uint64_t b, uint64_t d) {
    Cpu call = *source;
    call.x[0] = a; call.x[1] = b; call.x[2] = d; call.x[3] = 0;
    guest_call(&call, entry);
    return call.x[0];
}
```

- 2) Guardar y restaurar el mismo `Cpu`: `menu_wrap.c:21-28`.
- 3) Sin `Cpu *` a mano: `kernel_current_cpu()` + guardar/restaurar (`ui/game_options.mm:1534-1543`); devuelve `NULL`
  fuera de un hilo invitado, comprobarlo.

Reglas observadas en el código:

- La dirección sale de `g_site.<nombre>` (interna de `main`) o de `guest_sym("<mangled>")` (exportada, fatal si falta,
  `dispatch.c:80-94`; ejemplo `hle/movie.c:165`).
- Pila: si el invitado necesita espacio o datos temporales, bajar `sp` en la copia y validar el rango
  (`quick_drop_action.c:95-101`, `quick_inventory_sort.c:109-112`). Los datos que el invitado vaya a leer deben estar en
  memoria invitada (pila invitada o `hle_alloc`), no en la pila host (`items.c:435-440`).
- Argumentos float en `c->v[0]` (`pc_cursor.c:558`).
- Desde un sitio de parche el `Cpu` está al día: el generador guarda los registros sucios antes y los recarga todos
  después (`docs/RECOMP-LOCALS.md`, apartado *Patch sites*).
- No mantener un cerrojo host durante `guest_call` (§3.4): `rt_item_tick` copia la cola con el cerrojo, lo suelta y
  entonces llama (`items.c:424-429`, :447).

### 4.3 Cómo añadir una opción de configuración

Ejemplo real: `menu_wrap`.

| Paso | Dónde |
|---|---|
| 1. Campo en `struct Settings` con comentario | `runtime/settings.h:128` |
| 2. Valor por defecto si no es 0/false | inicializador `g_settings`, `runtime/settings.c:24` |
| 3. Lectura: rama `else if (!strcmp(key, "menu_wrap"))` en `settings_load` | `runtime/settings.c:455` |
| 4. Escritura: `fprintf(f, "menu_wrap = %d\n", ...)` en `settings_save_to` | `runtime/settings.c:833` |
| 5. Comando `settings` del socket/CLI: `B(menu_wrap)` en la tabla | `runtime/control_settings.c:38` |
| 6. Fila en el menú del juego (`toggle(...)` con getter y setter) | `runtime/ui/game_options.mm:1145-1148` |
| 7. Traducciones del texto visible, una por idioma | `runtime/ui/i18n/es.inc:241` (y demás `.inc`) |
| 8. Comprobar textos | `python3 tools/i18n_check.py` |
| 9. Uso: leer `g_settings.menu_wrap` | `runtime/menu_wrap.c:42` |

Opción que solo se cambia en la pantalla de inicio y arranca algo: `companion_apps` (`settings.h:162`,
`settings.c:498`, :861, `ui/launcher.mm:2378-2383`, `main.c:434`). Texto libre: `save_dir` (`settings.h:240`,
`settings.c:943`).

### 4.4 Cómo ejecutar código host una vez por frame en el hilo principal

"Hilo principal" tiene tres significados aquí; elegir según lo que haga el código:

| Gancho | Hilo | Frecuencia | Úsalo para |
|---|---|---|---|
| `rt_vfr_update` (`hle/framepace.c:640`) | hilo invitado del juego (según `framepace.h:38`) **[el hilo exacto no se verificó en ejecución]** | cada update; 2 por frame en pausa | leer/escribir estado del juego y `guest_call` de forma coherente con la simulación |
| `rt_item_tick` (`items.c:420`) | hilo invitado del juego | cada frame **solo "en juego"** (entrada del manejo de daño de Link) | acciones sobre Link/inventario; ya tiene `Cpu *c` y patrón de cola |
| bloque de `device.c:3343-3380` | el que presenta (por defecto `nvn-queue`, host) | una por frame presentado | scripts, capturas, comandos; **no** es el hilo del juego |
| `ui_run` (`device.c:3707-3744`) | hilo host principal | ~cada 16 ms | eventos SDL, ventana |

Para el hilo del juego, añadir una llamada en `rt_vfr_update` junto a las existentes:

```c
// runtime/hle/framepace.c:653-654
extern void freecam_frame(void);  // (hle/freecam.c)
freecam_frame();
```

(`gamelib_sync` :642 e `input_cam_frame` :657-660 siguen el mismo patrón.) Si debe ser exactamente una vez por frame
presentado, filtrar con el contador como hace el propio archivo (`renderer_present_count`, `framepace.c:644-647`).

Patrón completo "hilo host pide, hilo del juego ejecuta" (el que necesita la red):

```
productor (cualquier hilo)                 consumidor (hilo del juego)
items_request  items.c:367                 rt_item_tick  items.c:420
  lock; g_req[g_nreq++] = ...; unlock        if (!atomic_load(&g_pending)) return;      // barato si no hay nada
  atomic_store(&g_pending, n)                lock; copiar cola; vaciar; unlock
                                             guardar Cpu; guest_call(...); restaurar Cpu
resultado: items_last_result items.c:352     lock; g_result = ...; unlock
reset:     RT_ON_RESET(items_reset) items.c:337
```

Para un gancho por frame presentado, añadir un `extern` + llamada en `nvn/vulkan/device.c:3371-3380` (y en
`nvn/metal.m:1063-1072` para mantener la paridad), como `control_frame` o `weather_frame`.

---

## 5. Código de red existente y qué se puede reutilizar

| Componente | Qué hace | Estructura | Reutilizable para co-op |
|---|---|---|---|
| `runtime/control.c` + `control.h` | Servidor **TCP en 127.0.0.1**, protocolo de líneas -> JSON, hasta 4 clientes | Un hilo `serve` con `select` (timeout 10 ms) que solo lee y encola; los comandos corren en `control_frame`, en el hilo que presenta (`control.h:5-6`). Abstracción Winsock/BSD en :25-40; `listen_on` :699-738; colas `requests`/`finished` con `queue_lock` :282-285, :543-559; generación por conexión contra respuestas tardías :623 | **La mejor plantilla**: tipos `ControlSocket`, `socket_again`, `nonblocking`, `WSAStartup`, hilo separado, colas de ida y vuelta. Ojo: escucha solo en loopback y ejecuta en el hilo presentador |
| `runtime/dsu.c` + `dsu.h` | Cliente **UDP** Cemuhook (giroscopio) | Sin hilos ni bloqueo: socket no bloqueante sondeado desde el bucle de ventana (`motion_poll`, `hle/motion.c:812`, :861); `dsu_open` :183-213; `receive` :232-242; trabajo acotado por sondeo (128 datagramas) :254; reintento cada 1 s; helpers de paquete independientes del socket y testeados | Plantilla para UDP no bloqueante, CRC, expiración por tiempo y pruebas sin red |
| `runtime/discord_rpc.c` | Rich Presence por **named pipe** local (solo Windows) | Hilo propio que reconecta; `discord_rpc_set` nunca bloquea (cerrojo + copia) :205-232; construcción de mensajes separada para tests (`DISCORD_RPC_TEST`) | Patrón "API no bloqueante + hilo que habla" |
| `runtime/lib_fetch.c`, `dlss_fetch.c`, `xess_fetch.c` | Descarga HTTPS de DLL con **WinHTTP** | Solo Windows | No (HTTP cliente) |
| `runtime/update_core.c`, `update_crypto.c`, `update_appimage.c`, `post_update.c`, `platform/windows_update.c` | Actualizador: GitHub + verificación de firma; en Linux lanza el `curl` del sistema | Proceso aparte / subproceso | No para tiempo real; `update_crypto.c` sí si se quiere verificar firmas |
| `runtime/control_mem.c` | Lectura/escritura protegida de memoria invitada y escaneos en hilos de trabajo | `control_mem_peek`/`poke` :535-552 | Sí, para leer estado desde un hilo host sin arriesgar un fallo |

Enlace: `-lws2_32` y `-lwinhttp` ya están en `LDFLAGS` de Windows (`Makefile:141`); en Linux los sockets BSD no
necesitan nada. No hay capa común de sockets: `control.c` y `dsu.c` duplican cada uno su `#ifdef _WIN32`. No hay
TLS, NAT traversal, descubrimiento, ni serialización binaria general.

---

## 6. Riesgos, trampas y preguntas abiertas para multijugador

### 6.1 Seguridad entre hilos

1. **El gancho por frame presentado no es el hilo del juego.** `control_frame`, `weather_frame`, `blood_moon_frame`
   corren en `nvn-queue` mientras los hilos invitados siguen ejecutando; lo que escriben en memoria invitada compite
   con el juego. Aplicar estado de red ahí daría lecturas a medio actualizar. Usar `rt_vfr_update` o un parche propio.
2. **Código host en un hilo invitado conserva el core.** Un `recv` bloqueante o un cerrojo disputado dentro del gancho
   del juego detiene a todos los hilos de ese core. El intercambio debe ser no bloqueante (cola + `atomic`), como
   `items.c:422`.
3. **Cerrojos host y desenrollado.** Si un hilo invitado sostiene un `pthread_mutex` propio y llega a un punto de
   desenrollado (`guest_call` que haga SVC, llamada HLE, espera), un reinicio lo deja tomado para siempre
   (`kernel.c:367-372`). Copiar bajo cerrojo, soltar y luego llamar.
4. **Barreras de memoria**: los hilos invitados usan atómicos reales (`cpu.h:74-97`); un hilo host que lea estructuras
   del juego no tiene sección crítica posible contra ellos, salvo el mutex nativo del inventario (`pouch_lock` /
   `pouch_unlock`, `game_sites.h:38-39`), usable solo desde un hilo invitado.
5. **`g_settings` sin cerrojo** y `gamelib`/`g_site` de solo lectura tras el arranque: correcto para leer, no para
   publicar estado de red que cambie.
6. **Windows y páginas no respaldadas**: un hilo de red que lea direcciones invitadas obsoletas las compromete en vez
   de fallar (`windows.c:85-104`): errores silenciosos y memoria comprometida de más.
7. `kernel_current_cpu()` es `NULL` en hilos host: cualquier helper compartido debe comprobarlo.

### 6.2 Determinismo

8. **El paso de simulación es variable y local** (`framepace.c:685-701`): depende del tiempo real, de la tasa elegida y
   del historial; en pausa hay dos updates por frame y el reloj se detiene; en eventos puede forzarse 30 fps
   (`framepace.c:117-120`, :255). Dos máquinas no avanzan el mismo tiempo de juego por frame: un modelo lockstep no es
   viable sin tocar esto; lo natural es replicar estado con marcas de tiempo.
9. **Orden entre hilos no reproducible**: los cores dan exclusión por core, no un orden global; la apropiación ocurre en
   llamadas indirectas y SVC, que dependen del tiempo real (`kernel.c:238-247`).
10. `rt_ticks` y `time:*` usan el reloj host (`kernel.c:93-100`, `services/time.c:11-12`).
11. Hay correcciones dependientes de la tasa repartidas en `framepace.c` (pasos del solver :414, gravedad de flechas
    :395, tick de física :485): el comportamiento físico ya difiere entre 30 y 120 fps en la misma máquina.

### 6.3 Reinicio y ciclo de vida

12. **Los hilos host sobreviven al reinicio** ("volver a la pantalla de inicio" es el mismo proceso). Un hilo de red
    debe tolerar que la memoria invitada se borre: nada de punteros invitados retenidos, `RT_ON_RESET` que invalide
    colas y un contador de generación (patrón `control.c:623`, `dsu.h` `generation`).
13. Entre `teardown` y el siguiente `boot` no hay hilos invitados ni memoria válida, pero el hilo presentador y el de
    ventana siguen: los ganchos deben comprobar que el juego está arriba (`kernel_heap_size() != 0`, como `rupees.c:228`).
14. La carpeta de saves y su cerrojo se toman por arranque (`main.c:524-525`) y se sueltan tras `teardown` (:536):
    cambiar de perfil exige pasar por ahí.
15. `exit(0)` directo al cerrar la ventana (`device.c:3715-3718`) y `_exit` en `SAVAGEWIND_EXIT_AT` (:3362): no hay
    secuencia de apagado ordenado donde despedirse del otro jugador.
16. `static` sin hook dentro de funciones por frame (§3.5): revisar los propios.

### 6.4 Guardados del segundo jugador

17. **Un solo usuario**: `acc:u0` devuelve un uid fijo y `GetUserCount = 1` (`services/acc.c:7`, :46); el juego abre
    `save_<tipo>` sin distinguir usuario (`fs.c:2114-2118`). No hay noción de "datos del invitado" en el runtime.
18. Opciones visibles en el código, sin elegir ninguna: (a) un perfil aparte por sesión usando `save_profiles`
    (`sp_copy`, `sp_set_active`, cerrojo incluido); (b) un archivo propio del fork fuera de `save_0`, escrito por el
    host; (c) interceptar en `filesystem`/`file` (`fs.c:2009`, :1881), como hace `test_warp` con `game_data.sav`
    (`fs.c:2058-2068`). El formato de `game_data.sav` (§3.7) permite leer flags por hash.
19. `sc_lock_take` impide dos instancias sobre la misma carpeta (`main.c:525-526`, solo avisa): probar co-op en una
    máquina requiere `SAVAGEWIND_SAVE_DIR` o `SAVAGEWIND_DATA_ROOT` distintos por instancia.
20. La copia automática se salta con `SAVAGEWIND_SAVE_DIR` (`save_backup.c:57`): una partida co-op con carpeta
    alternativa no tendría copias salvo que se añadan.

### 6.5 Otras trampas

21. `hle_override` no intercepta llamadas directas dentro de `main` (§3.3): para funciones del juego hace falta parche.
22. Cualquier cambio en `tools/patches.py` / `game_sites.py` cambia `GAMELIB_ID`: los jugadores regeneran el código
    (minutos) y dos builds con parches distintos no son intercambiables.
23. Los sitios se validan solo para versiones conocidas (objetivo 1.9.0); las opciones existentes comprueban
    `!strcmp(g_game_version, "1.9.0")` (`quick_inventory_sort.c:37`). Ambos jugadores necesitan la misma versión de
    juego y, previsiblemente, el mismo build del runtime: hoy no hay intercambio de versión.
24. Una variable `SAVAGEWIND_*` nueva requiere regenerar `launch_flags_table.c` (§3.8).
25. Texto visible nuevo requiere todas las tablas de `runtime/ui/i18n/` y pasar `tools/i18n_check.py`.
26. La entrada del juego se compone en `fill_state` (`hle/input.c:1198-1226`) con un solo mando lógico;
    `control_input_apply` (`control_input.c:235`) es el punto existente para inyectar botones y ejes.
27. El socket de control ejecuta comandos que escriben memoria; hoy solo escucha en loopback (`control.c:707`). Un
    canal de red hacia fuera no debe reutilizar ese registro de comandos.
28. El FAQ del proyecto responde "Multiplayer? No" (`docs/FAQ.md:593`, issue #617): esto vive en el fork.

### 6.6 Preguntas abiertas

- ¿Qué hilo invitado llama a `VFR::updateInterval` y con qué frecuencia durante cargas, eventos y pantalla de título?
  Se resuelve con `SAVAGEWIND_LOG_FRAMEPACE=1` y registrando `kernel_current_thread_handle()` dentro de `rt_vfr_update`.
- ¿Existe un punto del frame en que los workers de jobs estén parados (para aplicar estado de actores sin carreras)?
  No hay nada en `runtime/` que lo indique; depende del mapa del lado del juego.
- ¿Cómo se representa al segundo jugador (actor, modelo, animación)? Fuera de esta área; aquí solo existen
  `player_info`, `player_actor`, `player_equipment` como sitios (`game_sites.h:30`, :65, :55).
- ¿El hilo de red debe ser único con `select` (modelo `control.c`) o UDP sondeado (modelo `dsu.c`)? Ambos encajan;
  sondear desde el gancho del juego evita un hilo, pero ata la latencia a la tasa de frames.
- ¿Qué ocurre con la sesión en pausa (el reloj del juego se detiene, hay dos updates por frame) y durante las cargas
  (`framepace_loading_ui`)?
- ¿Se necesita ejecutar sin ventana para pruebas de dos instancias? `SAVAGEWIND_HIDDEN=1` + dos raíces de datos parece
  suficiente, **[sin verificar]**.
