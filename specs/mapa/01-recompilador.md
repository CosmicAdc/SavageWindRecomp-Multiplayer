# Mapa 01 — El recompilador / generador (`tools/`)

Repositorio: `SavageWindRecomp` (rama `test`). Mapa de solo lectura, hecho leyendo el código; cada afirmación lleva
`ruta:línea`. Donde algo no se pudo comprobar se dice. Rutas relativas a la raíz del repo.

Estado del checkout al hacer el mapa: **no hay** `extracted/`, `generated/`, `build/` ni `tools/venv/`, así que no se
pudo ejecutar `make gen` ni las pruebas que necesitan `capstone` / `unicorn` / `lz4` / `clang`. Sí se ejecutaron con el
Python del sistema (sin escribir nada en el repo) `test_act_site.py`, `test_gyro_site.py` y `test_two_button_sites.py`:
las tres pasan. `test_game_sites.py` se detuvo por falta de `capstone` (lo importa `analyze.py`), no por un fallo.

Avisos previos importantes:

- **Solo existe la 1.9.0 en las tablas.** `VERSIONS` tiene una entrada (`tools/game_sites.py:46-48`),
  `KNOWN` solo `'1.9.0'` (`tools/game_sites.py:517-518`), `runtime/game_versions.h:13-14` igual, y
  `tools/test_game_sites.py:176` comprueba que "1.8.2 is no longer known". Las menciones a 1.8.2 que quedan son
  comentarios, `layout.GAME_SUBSDKS` (`tools/layout.py:40-43`) y la documentación.
- **`docs/SAVAGE-WIND.md` está desfasado en esta parte**: habla de "38 sitios", de `KNOWN['1.8.2']` y de
  `GAMELIB_ABI 3` (`docs/SAVAGE-WIND.md:1705`, `:1749`, `:1769`). El código real: 227 sitios, 131 parches,
  `GAMELIB_ABI 4` (`runtime/gamelib.h:27`). La mecánica que describe sigue siendo correcta.

---

## 1. Propósito y flujo de datos

El generador traduce estáticamente los módulos AArch64 del ExeFS del jugador (`main`, `subsdk0`, `sdk`) a C, una
función C `f_<dirección>(Cpu *c)` por función del juego. Ese C se compila en una biblioteca aparte (`swgame.dll` /
`.so` / `.dylib`) que el runtime carga al arrancar. Nada del juego se distribuye: el C se genera en la máquina del
jugador (`make gen` en un checkout, o el ayudante congelado `swgen` desde la pantalla de inicio).

Durante la generación del módulo `main`, instrucciones sueltas en "sitios" localizados por patrón se sustituyen por C
escrito a mano ("parches") que llama al runtime. Los mismos sitios viajan dentro de la biblioteca para que el runtime
lea direcciones del juego por nombre (`g_site`).

```
 extracted/exefs/{main,subsdk0,sdk}            (NSO0, segmentos LZ4; rtld no se usa)
        │
        │  nso.py        Nso: descomprime, imagen, MOD0, .dynamic, símbolos, relocs (RELA, JMPREL, RELR)
        ▼
 layout.py   compute()/checked(): módulos uno tras otro desde 0x70_0000_0000, redondeo a 4 KiB
        │                                        │
        │                                        ├──► generated/layout.txt  (nombre base tamaño build_id)
        │   game_sites.py  for_module()/resolve()│
        │   SITES + patrones ─► {nombre: dirección} ─► generated/layout.c
        │        │                                     (game_modules[], game_version[], game_sites[])
        │        ▼
        │   patches.py  table(sites) ─► {dirección: fn(gen, a) -> C}
        ▼        │
 gen.py  Module ─┤  boundaries(): eh_frame + símbolos + relocs RELATIVE
        │        │  discover(): + destinos de BL, ADR, ADRP+ADD; parte funciones hasta punto fijo
        │        │  jump_table(): emula con Unicorn las tablas de `br`
        │        │  lifter.py (+ simd_c.py): 1 instrucción -> Insn(kind, C)
        │        │  _emit_body(): etiquetas, parches, llamadas, rt_dispatch
        │        └─ --locals: regs.py (registros en locales) + flags.py (NZCV en `Flags F`)
        ▼
 generated/<módulo>/<módulo>_NNN.c   (trozos de ≤1500 funciones / ~1,5 MB)
 generated/<módulo>/<módulo>_table.c (<módulo>_fn_addrs[], _fn_ptrs[], _fn_count)
        │
        │  Makefile: clang $(GENFLAGS) (-O1 -fno-strict-aliasing ... -Iruntime)  + tools/gamelib_entry.c
        ▼
 build/swgame.{dll,so,dylib}  +  swgame.<ext>.id (GAMELIB_ID)
        │   exporta solo swgame_tables(); importa rt_* / hle_* del ejecutable
        ▼
 runtime/gamelib.c  gamelib_load(): comprueba ABI + id, take_sites() llena g_site
 runtime/loader.c   carga cada NSO donde dice la biblioteca (build id debe coincidir)
 runtime/dispatch.c dispatch_init(): tablas dirección -> GuestFn; rt_dispatch(), guest_call()
```

Dos caminos de generación equivalentes: `make gen` (Makefile:356-357) y la pantalla de inicio
(`runtime/gamebuild.c`, que corre `swgen` = `gen.py` congelado con PyInstaller, `tools/swgen.spec`, con los mismos
argumentos `GAMELIB_GENARGS`, Makefile:288-291).

---

## 2. Fichero por fichero

### 2.1 Núcleo del generador

| Fichero | Responsabilidad | Funciones / clases clave | Entradas → salidas |
|---|---|---|---|
| `tools/nso.py` (138 l.) | Cargador de NSO. | `header()` `:11` (solo cabecera: offsets, bss, build id); `class Nso` `:21` (descomprime LZ4, arma `image`); `_parse_mod0` `:50` (MOD0, `.dynamic`, `syms`, `relocs`); `DT_RELR` `:86-99`; `_nsyms` `:101` (DT_HASH o DT_GNU_HASH); `imports()/exports()` `:123-127`. | fichero NSO → `Nso.image`, `.text/.rodata/.data` (offset, tamaño), `.syms` (nombre, info, shndx, valor, tamaño), `.relocs` (offset, tipo, símbolo, addend), `.build_id` |
| `tools/layout.py` (152 l.) | Único lugar que decide las direcciones invitadas de los módulos. | `MODULES_LO/HI` `:21-22`; `LOAD_ORDER` `:24`; `image_size` `:31`; `GAME_SUBSDKS` `:40`; `foreign` `:46`; `compute` `:55`; `problems` `:70`; `checked` `:80`; `text`/`read` `:89/:94`; `c_source` `:104` (escribe `layout.c` con módulos, versión y sitios); `write` `:132`. | carpeta exefs (+ `(versión, sitios)`) → `layout.txt`, `layout.c` |
| `tools/analyze.py` (167 l.) | Descubrimiento de funciones por `.eh_frame` y censo. | `eh_frame_functions` `:28`; `eh_frame_extents` `:60` (rangos [inicio, fin) de cada FDE); `discover` `:111` (CLI de censo, no es el de gen.py); `census` `:142`. | `Nso` → listas de inicios / rangos |
| `tools/gen.py` (958 l.) | El recompilador: descubre funciones, levanta, aplica parches, escribe C. | `class Module` `:85`; `setjmp_targets` `:121`; `lift` `:142` (instrucción no soportada → `rt_udf`); `boundaries` `:153`; `jump_table` `:190` (+ `_emulate_table` `:291`); `scan` `:412`; `emit_function` `:470`; `_flow` `:489`; `_flag_plan` `:517`; `_emit_body` `:543` (parches en `:608-623`); `single_with_locals` `:712`; `discover` `:741`; `main_sites` `:805`; `generate` `:812` (sitios y parches solo si `name == 'main'`, `:816-818`); `main` `:896`. | NSO + layout + sitios → `<mód>_NNN.c`, `<mód>_table.c`; con `layout`/`all` también `layout.txt`/`layout.c` |
| `tools/lifter.py` (737 l.) | AArch64 → C, una instrucción cada vez. | Offsets del `Cpu` `:11-15`; `class Insn` `:138` (kinds: `op asm b bcond bl blr br ret trap`); `class Lifter` `:170`; `lift` `:180`; `dp_imm` `:198`; `branch_sys` `:276` (SVC → `rt_svc`, BRK → `rt_brk`); `system` `:317`; `ldst*` `:362-484`; `dp_reg` `:522`; `fp_simd` `:636`; `emit_asm_batch` `:687` (FP/SIMD como asm inline en hosts arm64). | palabra + pc → `Insn` |
| `tools/simd_c.py` (1156 l.) | Bajada portable a C de FP / AdvSIMD / crypto / CRC32 (`--simd c`, el defecto en x86-64). | `class SimdC` `:124`; `dp` `:129`; familias `three_same` `:327`, `misc2` `:506`, `shift_imm` `:731`, `fp_*` `:876-970`, `ldst*` `:998-1087`, `crc` `:1150`. Llama a los helpers `static inline` de `runtime/simd.h`. | palabra → C sobre `c->v[]` |
| `tools/regs.py` (136 l.) | Pase `--locals`: los N registros más usados de cada función en locales C. | Marcadores `SPILL/RELOAD/RELOAD_CALL/OPAQUE/END` `:40`; `CALLEE_SAVED` `:41`; `LOCALS = 6` `:44`; `ESCAPE` `:36`; `class Plan` `:77` (`decl` `:84`, `resolve` `:88`); `plan` `:109`. | textos C por instrucción + flujo → declaraciones y texto reescrito |
| `tools/flags.py` (159 l.) | Pase `--locals`: NZCV en un local `Flags F`. | `effect` `:42`; `class Plan` `:56` (`publish` `:74`, `reload` `:79`, `rewrite` `:83`); `plan` `:105`. | efectos por instrucción + flujo → reescritura de lecturas/escrituras de flags |
| `tools/game_sites.py` (1062 l.) | Sitios del `main` por patrón y por versión; CLI `find / pattern / common`. | `VERSIONS` `:46`; `Site` `:50`; `SITES` `:64-448`; `OPTIONAL_GROUPS` `:455-515`; `KNOWN` `:517-740`; `SiteError` `:743`; `supported_version` `:747`; `adrp_page` `:765`; `lo12` `:769`; `imm_value` `:781`; `mask` `:792`; `make_pattern` `:810`; `compile_pattern` `:831`; `class Main` `:847` (`find` `:856`, `target` `:870`, `locate` `:901`, `pattern_for` `:926`); `common_pattern` `:946`; `resolve` `:971`; `for_module` `:1011`; `main` (CLI) `:1025`. | `Nso` de `main` → `(versión, {nombre: dirección})` |
| `tools/patches.py` (1321 l.) | C escrito a mano que reemplaza una instrucción en cada sitio. | `_word` `:18`; `_fn` `:186`; `_sub_sp` `:198`; un `_xxx(gen, a)` por parche; `PATCHES` `:1183-1315`; `table` `:1318`. | `{nombre: dirección}` → `{dirección: fn(gen, a) -> str de C}` |
| `tools/gamelib_entry.c` (22 l.) | Única función exportada de la biblioteca. | `swgame_tables()` `:18-22`. | — |
| `tools/sw_probe.py` (217 l.) | Sondeo sin escribir C: layout, versión, funciones, instrucciones no soportadas, estimación de C, NVN. | `probe_module` `:86`; `main` `:146`. | exefs → informe (stdout / `--json`) |
| `tools/sym.py` (62 l.) | Simbolizar direcciones (`sym.py <addr>... [--dis N]`). **Desfasado**: bases fijas `main` 0x7000000000 y `sdk` 0x7004000000 (`:10`) y ruta `../extracted/exefs`; el layout real pone `sdk` detrás de `subsdk0`. No fiarse para `sdk`. | `lookup` `:26`. | direcciones → símbolo + desensamblado |
| `tools/swgen.spec` | PyInstaller: `gen.py` como ayudante `swgen` (`make gen-helper`, Makefile:491-496). | — | — |
| `tools/passes.py`, `tools/transients.py` | No son del generador (resumen de un log de frame, parpadeos). `passes.py` está sin embargo en `GAMELIB_SRCS` (Makefile:160): tocarlo cambia `GAMELIB_ID`. | — | — |

### 2.2 Ghidra (`tools/ghidra/`)

| Fichero | Qué hace |
|---|---|
| `README.md` | Montaje: Ghidra 12.1.2 + Switch Loader, JDK 21+, proyecto **fuera** del repo en `../ghidra/` (`botw_1.9.0`), análisis headless una vez (`README.md:13-20`). Regla de trabajo: toda afirmación cita dirección y evidencia (`:50-55`). |
| `gq.py` (91 l.) | Lanza `analyzeHeadless` en solo lectura con `SWQuery.java`. Comandos: `summary`, `decompile`, `xrefs`, `callees`, `disasm`, `strings`, `symbols`, `functions` (`gq.py:5-12`). Convierte direcciones de runtime de `main` (0x70…) a la base de Ghidra 0x71… (`to_ghidra` `:45`, `GHIDRA_SHIFT` `:36`). Configuración por entorno o `tools/ghidra/gq.local.json` (git-ignorado): `GHIDRA_INSTALL_DIR`, `JAVA_HOME`, `SW_GHIDRA_PROJECT`, `SW_GHIDRA_NAME`, `SW_GHIDRA_PROGRAM` (`:19-24`). |
| `SWQuery.java` (253 l.) | El script: `decompile` `:109`, `xrefs` `:127`, `callees` `:144`, `disasm` `:157`, `data` `:174` (existe en el script aunque el docstring de `gq.py` no lo lista), `strings` `:192`, `symbols` `:218`, `functions` `:232`, `summary` `:242`. |

Cada llamada tarda ~30 s (arranca Ghidra): pasar varias direcciones a una sola invocación. La salida va a stdout y
**no debe entrar en el repo** (deriva del código del juego).

### 2.3 Lado runtime que consume lo generado (contexto mínimo)

| Fichero | Papel |
|---|---|
| `runtime/cpu.h` | `struct Cpu` y helpers que usa el C generado (`:7-28`, `LD*/ST*` `:64-71`, flags `:100-130`, `Flags` `:173`, `rt_fsub_s` `:306`). |
| `runtime/gamelib.h` | Contrato de la biblioteca: `GAMELIB_ABI 4` `:27`, `GameLibModule` `:31`, `GameLibSite` `:42`, `GameLibTables` `:47`, `GAMELIB_ENTRY "swgame_tables"` `:60`. |
| `runtime/gamelib.c` | `take_sites` `:80-108`, `id_matches` `:123`, `open_lib` `:166-183`, `gamelib_load` `:185-219`. |
| `runtime/game_sites.h` | X-macros `GAME_SITES` (obligatorios, `:8-33`) y `GAME_SITES_OPTIONAL` (`:36-98`); `struct GameSites`; `extern GameSites g_site` `:111`; `g_game_version` `:113`. |
| `runtime/game_versions.h` | `GAME_VERSIONS(X)` `:13-14`: una fila, 1.9.0. |
| `runtime/dispatch.c` | `dispatch_init` `:111`, `dispatch_lookup` `:121`, `rt_dispatch` `:166`, `guest_call` `:174`, `hle_override` `:62`. |
| `runtime/rt.h` | Mapa de direcciones (`MAIN_BASE`, `MODULES_END`, `HLE_BASE`… `:35-50`), `guest_call`, `guest_sym`, `hle_alloc` `:108-110`, `kernel_current_cpu` `:90`. |
| `runtime/hle.h` | `HLE_FUNC` `:22`, `RT_ON_RESET` `:40`. |
| `runtime/hle/hle_symbols.txt` | Símbolos del `sdk`/`subsdk` sustituidos por `hle_<nombre>`: hoy solo `longjmp`. |

---

## 3. Contratos e invariantes

### 3.1 `struct Cpu`

`runtime/cpu.h:7-28`, con `_Static_assert` de los offsets; `tools/lifter.py:11-15` los repite (comentario "must match
runtime/cpu.h").

| Campo | Offset | Nota |
|---|---|---|
| `x[32]` | 0 | x0–x30; `x[31]` sin uso |
| `sp` | 256 | aparte de `x[]` |
| `pc` | 264 | solo destino del despacho indirecto, no un PC vivo |
| `nzcv` | 272 | flags en bits 31..28 |
| `tpidr`, `tpidrro` | 280, 288 | |
| `excl_addr`, `excl_val` | 296, 304 | monitor exclusivo emulado con CAS |
| `thread` | 312 | objeto de hilo del runtime |
| `fpcr`, `fpsr` | 320, 328 | solo con `--simd c` |
| `v[32]` | 512 | `__uint128_t` |

- **Direcciones invitadas = direcciones del host**: `LD8/16/32/64(a)` y `ST*(a, v)` desreferencian directamente
  (`runtime/cpu.h:64-71`). Leer un objeto del juego desde C del host es un cast de puntero.
- Asignar un `uint32_t` a `c->v[n]` pone a cero el resto del registro, como `ldr s<n>` (así lo usan los parches:
  `tools/patches.py:437`, `:1034`).
- Cambiar `cpu.h`, `simd*.h` o `gamelib.h` cambia `GAMELIB_ID` (Makefile:160-164).

### 3.2 Layout de módulos

- Orden `main`, `subsdk0..9`, `sdk`; cada uno ocupa su imagen redondeada a 4 KiB; el primero en `0x7000000000`; nada
  pasa de `0x7080000000` (`tools/layout.py:21-24`, `:55-78`; `runtime/rt.h:39-40`). Ambos límites están duplicados a
  mano en Python y en C.
- Un `subsdk` que no sea del juego en una versión conocida (un mod) se deja fuera (`tools/layout.py:40-52`).
- `layout.txt` ata cada módulo a su build id; `gen.py <módulo>` se niega si el NSO no es el del layout
  (`tools/gen.py:946-951`). `runtime/loader.c:55-57` se niega a cargar un NSO con otro build id.
- Las direcciones están horneadas en el C: mover un módulo obliga a regenerar todo.

### 3.3 Biblioteca del juego y `GAMELIB_ID`

- `GAMELIB_ID` = primeros 16 hex de SHA-256 de: `gen.py lifter.py analyze.py nso.py passes.py patches.py simd_c.py
  regs.py flags.py gamelib_entry.c hle_symbols.txt cpu.h simd.h simd_fp.h simd_softfma.h gamelib.h layout.py
  game_sites.py` + `GENFLAGS` + `GENFLAGS_ZIG` + `gen --locals` (Makefile:160-164). Hay un segundo id,
  `GAMELIB_ID_COMPAT`, para CPUs x86-64 sin AVX2/FMA (Makefile:169-174).
- El id se graba en `gamelib_entry.o` (Makefile:278-280) y en el fichero `<biblioteca>.id` (Makefile:330-333). El
  runtime compara el fichero antes de cargar (`runtime/gamelib.c:123-126`, `:196`) y otra vez `abi` + `id` tras cargar
  (`:178`). Si no coincide: "was built for another version" y la pantalla de inicio reconstruye.
- **`runtime/game_sites.h` no está en `GAMELIB_SRCS`**, pero `tools/game_sites.py` y `tools/patches.py` sí: añadir un
  sitio o un parche siempre cambia el id, y todos los jugadores recompilan el juego (orden de 16–19 min de clang
  medidos en `docs/SAVAGE-WIND.md:2142`, `:2169`).
- La biblioteca exporta solo `swgame_tables` (`-fvisibility=hidden`, Makefile:48) y resuelve `rt_*` / `hle_*` contra
  el ejecutable: en Windows por la biblioteca de importación `build/savagewind.dll.a` que el `.exe` genera con
  `--export-all-symbols` (Makefile:151-152); en Linux por `-rdynamic` (Makefile:122, :129). Consecuencia: una función
  `rt_*` nueva llamada desde un parche tiene que existir (no `static`) en el ejecutable con el que se enlaza.
- `runtime/*.c` entra solo por comodín (`RT_SRC`, Makefile:62-64): un `.c` nuevo en `runtime/` se compila sin tocar
  el Makefile.

### 3.4 Convención de llamadas del C generado

- Toda función del juego es `void f_<addr>(Cpu *c)` (`tools/gen.py:58`, `runtime/cpu.h:30`). Argumentos y resultado
  van en el `Cpu` según AAPCS64 (x0–x7, v0–v7, x30 = dirección de retorno).
- `bl` a código del mismo módulo: `c->x[30] = <pc+4>; f_<destino>(c);` — llamada C directa, **sin pasar por
  `rt_dispatch`** (`tools/gen.py:653-655`). `bl` fuera del módulo, `blr`, `br`: `c->pc = …; rt_dispatch(c)`
  (`:656-672`). `b` a otra función: `musttail return f_<destino>(c)` (`:597`). `ret`: `return;` (`:674`).
- `rt_dispatch` (`runtime/dispatch.c:166-171`) busca `c->pc` en una caché por hilo y luego en las tablas
  (`dispatch_lookup` `:121`); primero mira el rango HLE y las sustituciones de `hle_override` (`:141-152`). Una
  dirección sin función es `FATAL("no function at …")` (`:154-157`).
- Como los `bl` internos son llamadas C directas, **`hle_override` solo intercepta llamadas indirectas**; para
  interceptar siempre una función de `main` hay que parchear su primera instrucción (lo dice el comentario de
  `tools/patches.py:53-54`).
- Tablas de salto: se emulan con Unicorn (`tools/gen.py:190-380`) y se emiten como `switch`; un fallo cae en una copia
  fría `f_<addr>_r` con etiqueta en cada instrucción (`:470-487`, `:546-550`, `:703-707`).
- `setjmp`: la llamada lleva un `setjmp` del host (`tools/gen.py:65-70`); `longjmp` es HLE.
- Instrucción que el lifter no traduce → `rt_udf(c, pc)` (trampa en ejecución, no error de generación)
  (`tools/gen.py:145-149`).
- Solo `main` recibe sitios y parches; `--hle` se pasa a todos los módulos menos `main` (`tools/gen.py:816-818`,
  `:940-944`).

### 3.5 Pases `--locals` (por defecto, `RECOMP_LOCALS=1`, Makefile:52-53)

Documentado en `docs/RECOMP-LOCALS.md` y en los docstrings de `tools/regs.py:1-27` y `tools/flags.py:1-23`.

- Cada función copia sus 6 registros más usados (`regs.LOCALS`, `tools/regs.py:44`) a locales `xN` / `sp_` y vuelca
  los modificados antes de una llamada, SVC, trampa, **sitio de parche**, retorno o salida de cola; recarga todos
  tras un SVC o un parche, y solo x0–x18 y x30 tras una llamada (confía en que el llamado preserva x19–x29 y SP,
  `tools/regs.py:13-18`, `:41`).
- NZCV vive en `Flags F`; solo se escribe `c->nzcv` al salir hacia otra función por algo que no sea llamada/retorno y
  antes de un parche (`tools/flags.py:105-114`).
- No se aplican en funciones con el fallback de todas las etiquetas ni con asm FP/SIMD (`tools/gen.py:556-557`).
- **Invariante para código del host**: si el runtime escribe x19–x29 o SP del `Cpu` de un hilo que está ejecutando
  código generado y vuelve normalmente, la función llamante seguirá usando sus locales viejos. El código existente
  guarda y restaura el `Cpu` entero alrededor de una llamada al juego (`docs/RECOMP-LOCALS.md:33-38`).
- Cada salto hacia atrás lleva `__asm__ volatile("" ::: "memory")` para que clang no saque de un bucle de espera la
  carga que sondea memoria (`tools/gen.py:586-592`).
- Los volcados de fallo leen registros de otros hilos del `Cpu`, que puede ir atrasado respecto a los locales
  (`docs/RECOMP-LOCALS.md:48-49`).

### 3.6 Sitios

`Site(name, kind, pattern, at)` (`tools/game_sites.py:50`). Hoy: 227 sitios — 187 `code`, 18 `ref`, 14 `call`,
4 `got`, 2 `imm`, 1 `data`, 1 `end` (contado importando el módulo).

**Patrón**: palabras de instrucción en hex separadas por espacio, cada una `valor` o `valor/máscara`
(`parse_pattern` `:823`). Se compila a una regex de bytes sobre `.text` y solo cuentan coincidencias alineadas a 4
(`compile_pattern` `:831`, `Main.find` `:856-868`). `mask()` (`:792-807`) deja fuera lo que cambia cuando el código
se mueve:

| Instrucción | Máscara que se conserva |
|---|---|
| `b`, `bl` | `fc000000` (solo el opcode) |
| `b.cond`, `cbz`, `cbnz`, `ldr` literal | `ff00001f` |
| `tbz`, `tbnz` | `fff8001f` |
| `adr`, `adrp` | `9f00001f` |
| `add`/`ldr`/`str` cuya base viene de un `adrp` anterior del patrón | `ffc003ff` (se quitan los 12 bits bajos) |
| resto | completa (incluidos offsets de struct: **un campo que se mueve rompe el patrón**) |

**`at`** = índice (en palabras desde el inicio del patrón) de la instrucción que define el sitio. **Tipos**
(`Main.target`, `:870-899`):

| kind | Valor del sitio | Ejemplo |
|---|---|---|
| `code` | la dirección de esa instrucción | `Site('frame_wait', 'code', …, 0)` `:138` |
| `call` | el destino del `b`/`bl` que hay ahí | `Site('msg_lookup', 'call', …, 0)` `:97` |
| `ref` | la dirección que un `add`/`ldr`/`str` forma con su `adrp` (busca el `adrp` hasta 32 palabras arriba); sobre el `adrp` mismo, su página | `Site('player_info', 'ref', …, 1)` `:216` |
| `got` | el puntero guardado en esa dirección `ref`, más la base del módulo | `Site('pouch_mgr', 'got', …, 2)` `:394` |
| `imm` | el inmediato de la instrucción (offset de `ldr`/`str`, `add`/`sub`/`cmp`, `movz`): un offset de campo que una versión movió | `Site('opt_screen_rows', 'imm', …, 0)` `:116` |
| `data` / `end` | sin patrón: inicio de `.data` / fin de la imagen | `:446-447` |

Varios sitios pueden compartir patrón con distinto `at` (`BLADE_LATERAL`, `TRAVERSE`, `PAD_QUERY`, `:52-61`,
`:268-269`, `:295-297`), y el mismo patrón puede dar un sitio `code` y otro `call` (`opt_msg_call` / `msg_lookup`,
`:95-98`).

**Resolución** (`resolve`, `:971-1008`):

1. El patrón debe encontrar exactamente un lugar (`locate` `:901-912`). Un sitio puede llevar una lista
   `[(patrón, at), …]`; gana el primero con una sola coincidencia (hoy ningún sitio usa lista).
2. Con tabla `KNOWN` para la versión: si el patrón encuentra otra dirección que la tabla → `SiteError` (se para la
   generación, `:988-989`). Si el patrón encuentra 0 o varios → se usa la de la tabla con aviso (`:995-997`).
3. Sin tabla y sin coincidencia única: si el sitio está en `OPTIONAL` se omite; si no, error (`:990-994`).
4. Si falta **uno** de un grupo de `OPTIONAL_GROUPS`, se quita **todo el grupo** y con él sus parches (`:1000-1007`;
   `patches.table` solo mapea sitios presentes, `tools/patches.py:1318-1321`).
5. Los `OPT_TWO_SITES` son estrictos: nunca usan la dirección de `KNOWN` como recurso (`:980-986`).

**Versiones** (`for_module`, `:1011-1022`): un `main` cuyo build id no está en `VERSIONS` como soportado se rechaza,
salvo `--any-version` (`make gen GENARGS=--any-version`), que exige que todos los patrones encuentren un solo lugar.
`KNOWN['1.9.0']` lista 219 de los 227 (los que faltan son opcionales; `tools/test_game_sites.py:96-100` lo permite).

**Cómo llegan al runtime**: `gen.py layout|all` resuelve los sitios (`tools/gen.py:932`) y `layout.c_source` los
escribe ordenados por nombre en `game_sites[]` junto a `game_version[]` (`tools/layout.py:114-117`).
`gamelib_entry.c` los expone en `GameLibTables`. `take_sites` (`runtime/gamelib.c:80-108`) copia por nombre a
`g_site`: un sitio de `GAME_SITES` que falte impide cargar ("the game code has no address for …"); uno de
`GAME_SITES_OPTIONAL` queda a 0 y el código debe comprobarlo (`g_site.player_info ? … : 0`,
`runtime/control_game.c:102`). Los sitios que solo usan los parches no necesitan estar en `game_sites.h`: el parche
hornea la dirección en el C (`gen.sites[...]`).

Observación (no comprobada en ejecución): algunos nombres están a la vez en `GAME_SITES` (obligatorio en runtime,
`runtime/game_sites.h:10-21`: `opt_two_redraw`, `beast_*`…) y en un grupo opcional del generador
(`tools/game_sites.py:477`, `:489-490`). Si ese grupo se omitiera, la biblioteca no cargaría. Las pruebas solo
verifican que los opcionales del runtime pertenezcan a grupos opcionales (`tools/test_game_sites.py:102-104`), no lo
contrario. Para sitios nuevos de un fork: si el grupo es opcional en Python, declararlos en `GAME_SITES_OPTIONAL`.

### 3.7 Parches: qué C puede contener

Un parche es `def _nombre(gen, a) -> str` registrado en `PATCHES` por nombre de sitio. `gen` es el `Module` (da
`gen.nso.image`, `gen.base`, `gen.sites`, `gen.lift(a)`), `a` la dirección. El texto se emite tal cual en el cuerpo
de la función C, en el lugar de **una sola instrucción** (`tools/gen.py:608-623`). Reglas verificadas:

1. **Aserción de la palabra**: cada parche comprueba la instrucción que espera con `_word(gen, a)` y `assert`
   (`tools/patches.py:18-19`; regla de AGENTS.md:42-44). Si no coincide, la generación falla.
2. **Reproducir el efecto de la instrucción sustituida** en el camino normal. Ejemplos: `sub sp` →
   `c->sp -= 0x60;` (`:61-63`); `stp x29, x30, [sp, #-0x30]!` → `uint64_t ea = c->sp - 0x30; ST64(ea, c->x[29]);
   ST64(ea + 8, c->x[30]); c->sp = ea;` (`:415-419`); `ldr w8, [x19, #0x78]` → `c->x[8] = LD32(c->x[19] + 0x78);`
   (`:422-425`); `adrp` → la página calculada con `adrp_page` (`:248-253`).
3. **Llamar al runtime**: declarar la función con `extern` dentro del propio texto (los `.c` generados solo incluyen
   `cpu.h`) y llamarla con `c` o con valores sueltos: `{ extern void rt_vfr_update(uint64_t);
   rt_vfr_update(c->x[19]); … }` (`:424`), `rt_menu_wrap(c, c->x[20], c->x[19], (uint32_t)c->x[22])` (`:1140-1141`).
4. **Llamar a una función del juego**: `extern void f_<addr>(Cpu *); c->x[30] = <a+4>; f_<addr>(c);`, con el nombre
   de `_fn(gen, 'sitio')` (`:186-187`; uso en `:230-237`, `:1118-1124`). Llamada indirecta: `c->pc = …;
   c->x[30] = …; rt_dispatch(c);` (`:66-73`, `:202-211`). La dirección tiene que ser un inicio de función que
   `gen.py` haya descubierto, o el enlace falla por símbolo sin definir.
5. **Pasar la función nativa al runtime** como puntero para que la envuelva: `rt_ui_layout_draw(c, f_<addr>)`
   (`:22-34`), `_replace_call` (`:79-84`).
6. **Salir de la función**: `return;` — correcto en la primera instrucción (antes de montar el marco) o en un `ret`
   (`:417`, `:142-144`). Patrón "puerta": `if (rt_xxx(c)) return; <instrucción original>` (`_replace_gate`
   `:110-112`; con resultado: `c->x[0] = r; return;` `:1150`).
7. **Saltar a una etiqueta**: `goto L_<hex en minúsculas>;` exactamente con esa forma, porque `gen.py` lo reconoce
   por regex para el flujo y para recargar locales (`tools/gen.py:502`, `:615`). El destino debe ser ya una etiqueta
   de la **misma función C**: `scan()` solo crea etiquetas a partir de saltos reales (`tools/gen.py:412-440`) y no
   mira los parches. Por eso los parches reutilizan el destino de un salto existente (`_stale_actor_cbz` `:462-470`,
   `_phys_tick` `:600-605`, `_surf_jump_request` `:522-527`). Un destino sin etiqueta es un error de compilación.
8. **Variables entre parches de la misma función**: declaradas fuera de llaves en un parche y usadas en otro
   (`qr_query` en `_replace_query` / `_replace_query_return`, `:136-144`; `qr_chest_ai` `:147-157`). Los dos sitios
   deben caer en la misma función C.
9. **Usar `c->sp`, nunca `sp_`**: el local `sp_` solo existe si la función tiene SP entre sus locales
   (`tools/patches.py:1137-1139`, #909; sin locales `gen.py` reescribe `sp_` → `c->sp`, `tools/gen.py:618-620`).
10. El parche corre "sobre el `Cpu`": con `--locals` se vuelca antes, se publican los flags, el texto queda opaco y
    se recarga todo después y antes de cada `goto` (`tools/gen.py:611-616`). Puede leer y escribir cualquier registro
    y `c->nzcv`.
11. **La función Python debe ser determinista y sin efectos**: se invoca varias veces por sitio (`_flow` `:501`,
    `_emit_body` `:610`, y dos cuerpos cuando hay tablas de salto).
12. FP: preservar la semántica (`rt_fsub_s` en vez de una resta C, `:450-454`; uniones `uint32_t`/`float` para
    reinterpretar bits, `:436-437`).
13. Para enganchar la **salida** de una función hace falta un parche por cada `ret` o salto de cola
    (`replace_toast_return`, `_tail_a`, `_tail_b`, `tools/patches.py:167-175`, `:1201-1203`).

### 3.8 Llamar a funciones del juego desde el host

- `guest_call(c, addr)` (`runtime/dispatch.c:174-180`): pone `x30 = 0`, `pc = addr`, despacha y restaura `x30`. Solo
  eso: **no** guarda más registros.
- Hace falta el `Cpu` de un hilo invitado: el `c` que entrega un parche, o `kernel_current_cpu()`
  (`runtime/kernel.c:504`, `NULL` fuera de un hilo invitado).
- Tres formas en uso:
  - copia del `Cpu` y llamada sobre la copia: `Cpu call = *source; call.x[0] = a; …; guest_call(&call, entry);
    return call.x[0];` (`runtime/quick_inventory_sort.c:30-35`);
  - guardar y restaurar: `Cpu saved = *c; …; guest_call(c, fn); r = c->x[0]; *c = saved;`
    (`runtime/menu_wrap.c:20-27`);
  - preparar argumentos en la pila invitada por debajo de `sp` (`runtime/items.c:432-459`).
- Funciones del `sdk` por símbolo: `guest_sym("_ZN2nn2os…")` (`runtime/rt.h:109`; uso en `runtime/hle/movie.c:165`).
- **Modelo para peticiones que nacen en otro hilo del host** (red, UI): `runtime/items.c:420-460`. Una cola con
  mutex y un atómico `g_pending`; el parche `item_tick` (`tools/patches.py:1089-1097`), en la entrada del manejo de
  daño de Link — según su comentario, cada frame en juego y en el hilo del juego — vacía la cola y llama al juego.

---

## 4. Receta: cómo añadir un sitio y un parche nuevo

Requisitos: tus ficheros 1.9.0 en `extracted/`, `tools/venv` con `lz4`, `capstone`, `unicorn`, y (para localizar) el
proyecto Ghidra fuera del repo. En Windows, shell MSYS2 UCRT64 y `tools/venv/Scripts/python.exe`
(AGENTS.md:102-104). Los comandos de `game_sites.py` se ejecutan desde `tools/` o con la ruta completa: importa
`nso` y `analyze` del mismo directorio.

**Paso 1 — Localizar la instrucción.** Con Ghidra:

```
python tools/ghidra/gq.py strings "<regex>" 20
python tools/ghidra/gq.py xrefs <nombre|dirección>
python tools/ghidra/gq.py decompile 0x70xxxxxxxx 0x70yyyyyyyy
python tools/ghidra/gq.py disasm 0x70xxxxxxxx 40
```

Sirven direcciones de runtime (0x70…). Anotar la evidencia (dirección, llamadores) en el comentario del parche, como
hace el resto del fichero. No pegar pseudocódigo ni desensamblado en el repo.

**Paso 2 — Generar el patrón.**

```
cd tools
../tools/venv/Scripts/python game_sites.py pattern ../extracted/exefs/main 0x70xxxxxxxx [code|call|ref|got|imm]
```

Imprime una línea lista para pegar: `Site('<name>', '<kind>', '<patrón>', <at>),  # -> <valor>`
(`tools/game_sites.py:1036-1041`). `pattern_for` (`:926-943`) se queda dentro de la función (por `.eh_frame`), empieza
lo más cerca posible por encima de la instrucción (en el `adrp` para `ref`/`got`), usa de 8 a 24 palabras y comprueba
que solo encuentre ese lugar. Para dos versiones a la vez: `game_sites.py common <main A> <addr A> <main B> <addr B>
[kind]` (`:946-968`, `:1030-1035`).

**Paso 3 — Registrar el sitio** en `tools/game_sites.py`:

- añadir la línea a `SITES` (antes de `main_data`, `:446`), con un comentario de qué es;
- añadir `'<name>': 0x…` a `KNOWN['1.9.0']` (obligatorio para un sitio no opcional:
  `tools/test_game_sites.py:96-100`);
- para una función nueva del fork conviene un grupo propio en `OPTIONAL_GROUPS` (`:455`), todo o nada, como
  `('item_tick', 'pouch_item_get', 'pouch_mgr')` (`:512`).

Comprobar: `../tools/venv/Scripts/python game_sites.py find ../extracted/exefs/main` debe listar cada sitio como `ok`
y salir con 0 (`:1042-1058`; `new` = encontrado pero sin entrada en `KNOWN`).

**Paso 4 — (Si el runtime necesita la dirección)** añadir `X(<name>)` a `GAME_SITES` o, si el grupo es opcional, a
`GAME_SITES_OPTIONAL` en `runtime/game_sites.h`, y usar `g_site.<name>` comprobando que no sea 0. Ejemplo de dato:
`player_info` (`tools/game_sites.py:216`, `runtime/game_sites.h:30`, uso `runtime/control_game.c:102`). Ejemplo de
función llamable: `pouch_sort` (`runtime/game_sites.h:43`).

**Paso 5 — Escribir el parche** en `tools/patches.py` y registrarlo en `PATCHES` (`:1183`). Plantillas existentes:

| Necesidad | Copiar de |
|---|---|
| Gancho por frame / aviso al runtime y seguir | `_vfr_ratio` `:422-425`, `_surf_contact` `:444-447` (este es el ejemplo de estilo que cita AGENTS.md:37) |
| Entrada de función con posible sustitución completa | `_frame_wait` `:415-419`, `_act_query` `:1145-1153`, `_horse_look_ahead` `:1071-1074` |
| Filtrar un valor que el juego guarda | `_stamina` `:1020-1024`, `_link_life` `:1004-1014` |
| Filtrar un valor que el juego carga | `_sprint_cost` `:1032-1035`, `_arrow_gravity` `:434-438` |
| Envolver una llamada `bl` | `_gyro_scene_actor` `:1118-1124`, `_opt_flag_type` `:230-237`, `_replace_call` `:79-84` |
| Envolver una llamada `blr` | `_quick_drop_actor_call` `:66-73` |
| Ejecutar la función original y cambiar su resultado | `_tp_gate` `:1174-1180` |
| Salto condicional nuevo | `_stale_actor_cbz` `:462-470`, `_phys_tick` `:600-605` |
| Llamar al juego desde el gancho con cola | `_item_tick` `:1089-1097` + `runtime/items.c:420-460` |
| Fábrica de parches parametrizada | `_battle_info(which)` `:1044-1054`, `_cam_reverse(axis)` `:786` |

**Paso 6 — Lado runtime**: la función `rt_*` en un `.c` de `runtime/` (no `static`); estado por arranque con
`RT_ON_RESET` (`runtime/hle.h:40`; AGENTS.md:49-50: no retener punteros invitados entre reinicios).

**Paso 7 — Pruebas sin ficheros del juego** (sección 5): `test_game_sites.py` y una prueba sintética del parche
copiando `tools/test_act_site.py` (texto del C y rechazo de un opcode distinto) o
`tools/test_quick_drop_codegen.py` (pasa el parche por `Module._emit_body` con y sin locales y compila).

**Paso 8 — Regenerar y compilar**: `make gen` y luego `make -j8` (AGENTS.md:75-80). `gen.py` solo reescribe los
trozos que cambian (`write_if_changed`, `tools/gen.py:729-738`), así que un parche nuevo recompila pocos ficheros.
Comprobar en el log la línea `[gamelib] … (game version 1.9.0, N sites)` (`runtime/gamelib.c:203`) y probar en juego.

---

## 5. Pruebas

Python: `tools/venv/bin/python` (Linux/macOS) o `tools/venv/Scripts/python.exe` (Windows). Entorno:
`python -m venv tools/venv` y `pip install capstone==5.0.9 unicorn==2.1.4 lz4==4.4.5`
(`docs/SAVAGE-WIND.md:2103-2104`). CI usa `uv run --no-project --with lz4 --with capstone --with unicorn`
(`.github/workflows/tests.yml:90`, `:118`).

| Prueba | Qué comprueba | Necesita | Comando |
|---|---|---|---|
| `tools/test_game_sites.py` | `VERSIONS` == `game_versions.h`; sitios del runtime y de `PATCHES` ⊆ `SITES`; `KNOWN` lista todo lo no opcional; todos los patrones compilan; máscaras; código movido se reencuentra; `code/call/ref/got/data/end`; `resolve()` (tabla, lista de patrones, grupos opcionales); guardas de `pc_map_focus*`. | capstone (vía `analyze`), sin juego | `tools/venv/Scripts/python.exe tools/test_game_sites.py` |
| `tools/test_layout.py` | NSOs sintéticos: `nso.py` (GNU_HASH, RELR), layout, `gen.py all` / por módulo (`--no-sites`), `sw_probe.py`, tablas de salto. `--keep DIR` deja un exefs y un `gen` compilables. | lz4, capstone, unicorn | `… tools/test_layout.py [--keep DIR]` |
| `tools/test_simd.py` | Bajada FP/SIMD portable contra Unicorn (Cortex-A57): validez de codificaciones y resultados. | unicorn, capstone, clang | `… tools/test_simd.py --jobs 8` y `… --locals`; en arm64 `--simd asm` |
| `tools/test_locals.py` | Funciones aleatorias generadas con y sin `--locals` contra Unicorn: llamadas, `blr`, SVC, **sitios de parche con `goto`**, setjmp/longjmp, salidas de cola. | unicorn, clang (`CC`) | `… tools/test_locals.py [--functions N] [--cases K] [--seed S] [--no-callee-saved-trust] [--reg-locals N]` |
| `tools/test_lift.py` | Cada instrucción del NSO real, C levantado contra Unicorn. | **ficheros del juego**, clang | `… tools/test_lift.py extracted/exefs/main 0x7000000000 [count] [seed] [--locals]` |
| `tools/test_act_site.py`, `test_gyro_site.py` | Texto C de un parche con instrucciones inventadas; rechazo de opcode distinto. | nada | `python tools/test_act_site.py` |
| `tools/test_two_button_sites.py` | El grupo `OPT_TWO_SITES` cae entero si falla un patrón. | nada | ejecutar desde `tools/` (importa `game_sites`) |
| `tools/test_quick_*_codegen.py`, `test_ui_layout_codegen.py` | Parches pasados por `gen.Module` sintético. | capstone, unicorn | `… tools/test_quick_drop_codegen.py` |
| `tools/test_gamelib_sites.c` | `take_sites`: opcionales a 0, obligatorios exigidos. | clang | `make test-quick-inventory-sort` (Makefile:896-903) |

Comprobaciones con ficheros del juego: `game_sites.py find extracted/exefs/main` (salida 0) y
`tools/sw_probe.py extracted/exefs` (layout, versión, instrucciones no soportadas).

Biblioteca desfasada: comparar el fichero `<biblioteca>.id` con
`make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id` (AGENTS.md:82-89). No editar el `.id`.

---

## 6. Riesgos, trampas y preguntas abiertas para el modo cooperativo

### Trampas del mecanismo

1. **Una instrucción por parche.** No hay "hook de función" genérico: entrada = parche en la primera instrucción;
   salida = un parche por cada `ret`/salto de cola.
2. **`hle_override` no ve los `bl` internos** (3.4). No sirve para interceptar funciones de `main` llamadas
   directamente.
3. **Offsets de struct dentro del patrón**: quedan sin enmascarar; si otra versión mueve un campo, el patrón deja de
   encontrar el lugar (pasó en 1.9.0, `docs/SAVAGE-WIND.md:1808-1816`). Los offsets que usa el C del host no los
   cubre nada: se revisan a mano por versión (`docs/SAVAGE-WIND.md:1775-1780`).
4. **`goto` solo a etiquetas existentes** y con la forma exacta `goto L_<hex>;` (3.7.7).
5. **x19–x29 / SP**: código del host que los cambie y vuelva normalmente rompe la confianza de `--locals` (3.5).
   Guardar/restaurar el `Cpu` o trabajar sobre una copia.
6. **Hilo**: un gancho corre en el hilo invitado que ejecuta ese código, con su propio `Cpu`. Nada de `guest_call`
   desde un hilo de red; usar el modelo de cola de `runtime/items.c`.
7. **Reinicio en sitio**: todo estado por arranque (punteros invitados, colas, conexiones ligadas a la partida) con
   `RT_ON_RESET`.
8. **Paso de tiempo**: los ganchos por frame corren a 30/60/90/120 FPS; distinguir actualización del juego, frame
   presentado y frame generado (AGENTS.md:52-54). Un cooperativo que sincronice estado por "frame" debe decidir cuál.
9. **Coste de cada cambio**: tocar `patches.py` o `game_sites.py` cambia `GAMELIB_ID` y obliga a cada jugador a
   reconstruir la biblioteca. Dos jugadores con builds distintos del fork tendrán bibliotecas distintas: el protocolo
   de red debería intercambiar una versión propia, no asumir igualdad.
10. **Conflictos con upstream**: `SITES`, `KNOWN`, `OPTIONAL_GROUPS`, `PATCHES` y los X-macros de `game_sites.h` son
    listas únicas que upstream edita a menudo. Mantener los añadidos del fork en bloques contiguos y con prefijo
    propio reduce los conflictos al rebasar sobre `test`.
11. **Direcciones literales**: `runtime/pc_cursor.c:558`, `:708`, `:1320`, `:1332` llaman a direcciones 0x70…
    escritas a mano. Va contra la regla de AGENTS.md:42-44 ("use g_site for runtime addresses"); no imitarlo.
12. **Reglas legales**: nada de salida de Ghidra, desensamblado, C generado ni nombres copiados de proyectos de
    decompilación en el repo (AGENTS.md:23-26; CONTRIBUTING.md:19-21). Las pruebas usan instrucciones inventadas.
13. **Herramientas desfasadas**: `tools/sym.py` (base fija del `sdk`) y las cifras de `docs/SAVAGE-WIND.md` sobre
    sitios y versiones.

### Preguntas abiertas (no respondidas por esta zona del código)

- **Segundo jugador**: no hay ningún sitio para crear actores ni para instanciar un segundo `Player`. Existen sitios
  de lectura y servicio — `player_info` (ref), `player_actor` (call), `world_mgr`, `event_mgr`, `pause_state`,
  `actor_info`, `actor_info_lookup`, `actor_link_*`, `actor_index_*`, `pouch_*`, `link_life_store`, `stamina_store`,
  `lookat_update` (`tools/game_sites.py:209-219`, `:308-338`, `:366-395`) — pero cómo representar al otro jugador
  (actor nuevo, NPC reutilizado, solo un modelo dibujado) hay que investigarlo en Ghidra desde cero.
- **Offsets de `PlayerInfo`**: la documentación cita +0x88 (posición) y +0xa4 (estamina)
  (`docs/SAVAGE-WIND.md:1777`; `tools/patches.py:1017-1018`). El resto de campos necesarios (rotación, animación,
  equipo) no están mapeados aquí.
- **Qué hilo ejecuta cada gancho**: se afirma "hilo del juego" para `item_tick` en comentarios
  (`runtime/items.c:13-14`, `:417`); para sitios nuevos hay que comprobarlo. `rt_dispatch` puede ceder el núcleo
  emulado a otro hilo (`runtime/dispatch.c:167-168`): el planificador (`runtime/kernel.c`) queda fuera de este mapa.
- **Validar punteros invitados** recibidos o guardados entre frames: hay precedente (`rt_actor_link_valid`,
  `runtime/hle/framepace.c:616`; comprobaciones de rango `GUEST_LO..GUEST_HI` en `runtime/menu_wrap.c:17-18`), pero
  no un helper común.
- **Determinismo**: nada en el generador garantiza que dos máquinas simulen igual (objetivo x86-64-v3 frente al
  compatible sin FMA, `gamelib_sync` cambia el modo de fma en caliente, `runtime/gamelib.c:228-240`). Un cooperativo
  por sincronización de estado es viable; uno por entradas deterministas (lockstep) parece arriesgado.
- **Sitios obligatorios dentro de grupos opcionales** (observación de 3.6): confirmar con el mantenedor antes de
  apoyarse en ese patrón.
- **Soporte de versiones**: el fork heredará "solo 1.9.0"; si se quisiera otra, hay que rehacer `KNOWN` y revisar
  offsets (pasos en `docs/SAVAGE-WIND.md:1782-1801`).
